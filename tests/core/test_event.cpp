/*
 * Carbon Event Dispatcher Tests
 *
 * Tests for the publish-subscribe event system.
 */

#include "catch_amalgamated.hpp"
#include "carbon/event.h"
#include <cstring>
#include <vector>

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

struct EventRecorder {
    std::vector<Carbon_EventType> received_types;
    std::vector<uint32_t> received_values;
    int call_count = 0;

    void reset() {
        received_types.clear();
        received_values.clear();
        call_count = 0;
    }
};

static void recorder_callback(const Carbon_Event *event, void *userdata) {
    EventRecorder *rec = static_cast<EventRecorder *>(userdata);
    rec->received_types.push_back(event->type);
    rec->call_count++;

    // Store event-specific data
    if (event->type == CARBON_EVENT_TURN_STARTED || event->type == CARBON_EVENT_TURN_ENDED) {
        rec->received_values.push_back(event->turn.turn);
    }
}

static void counter_callback(const Carbon_Event *event, void *userdata) {
    (void)event;
    int *counter = static_cast<int *>(userdata);
    (*counter)++;
}

/* ============================================================================
 * Dispatcher Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Event dispatcher creation and destruction", "[event][lifecycle]") {
    Carbon_EventDispatcher *d = carbon_event_dispatcher_create();
    REQUIRE(d != nullptr);
    carbon_event_dispatcher_destroy(d);
}

TEST_CASE("Destroy NULL dispatcher", "[event][lifecycle]") {
    // Should not crash
    carbon_event_dispatcher_destroy(nullptr);
}

/* ============================================================================
 * Subscription Tests
 * ============================================================================ */

TEST_CASE("Subscribe and receive events", "[event][subscribe]") {
    Carbon_EventDispatcher *d = carbon_event_dispatcher_create();
    EventRecorder recorder;

    SECTION("Single subscription") {
        Carbon_ListenerID id = carbon_event_subscribe(
            d, CARBON_EVENT_TURN_STARTED, recorder_callback, &recorder);
        REQUIRE(id != 0);

        carbon_event_emit_turn_started(d, 1);

        REQUIRE(recorder.call_count == 1);
        REQUIRE(recorder.received_types[0] == CARBON_EVENT_TURN_STARTED);
        REQUIRE(recorder.received_values[0] == 1);

        carbon_event_unsubscribe(d, id);
    }

    SECTION("Multiple subscriptions to same event") {
        int counter1 = 0, counter2 = 0;

        Carbon_ListenerID id1 = carbon_event_subscribe(
            d, CARBON_EVENT_GAME_STARTED, counter_callback, &counter1);
        Carbon_ListenerID id2 = carbon_event_subscribe(
            d, CARBON_EVENT_GAME_STARTED, counter_callback, &counter2);

        REQUIRE(id1 != 0);
        REQUIRE(id2 != 0);
        REQUIRE(id1 != id2);

        carbon_event_emit_game_started(d);

        REQUIRE(counter1 == 1);
        REQUIRE(counter2 == 1);

        carbon_event_unsubscribe(d, id1);
        carbon_event_unsubscribe(d, id2);
    }

    SECTION("Subscribe to different events") {
        EventRecorder rec1, rec2;

        Carbon_ListenerID id1 = carbon_event_subscribe(
            d, CARBON_EVENT_TURN_STARTED, recorder_callback, &rec1);
        Carbon_ListenerID id2 = carbon_event_subscribe(
            d, CARBON_EVENT_TURN_ENDED, recorder_callback, &rec2);

        carbon_event_emit_turn_started(d, 1);
        carbon_event_emit_turn_ended(d, 1);

        REQUIRE(rec1.call_count == 1);
        REQUIRE(rec1.received_types[0] == CARBON_EVENT_TURN_STARTED);

        REQUIRE(rec2.call_count == 1);
        REQUIRE(rec2.received_types[0] == CARBON_EVENT_TURN_ENDED);

        carbon_event_unsubscribe(d, id1);
        carbon_event_unsubscribe(d, id2);
    }

    SECTION("Subscribe to all events") {
        Carbon_ListenerID id = carbon_event_subscribe_all(d, recorder_callback, &recorder);
        REQUIRE(id != 0);

        carbon_event_emit_turn_started(d, 1);
        carbon_event_emit_game_paused(d);
        carbon_event_emit_turn_ended(d, 1);

        REQUIRE(recorder.call_count == 3);

        carbon_event_unsubscribe(d, id);
    }

    carbon_event_dispatcher_destroy(d);
}

/* ============================================================================
 * Unsubscribe Tests
 * ============================================================================ */

TEST_CASE("Unsubscribe stops receiving events", "[event][unsubscribe]") {
    Carbon_EventDispatcher *d = carbon_event_dispatcher_create();
    int counter = 0;

    Carbon_ListenerID id = carbon_event_subscribe(
        d, CARBON_EVENT_GAME_STARTED, counter_callback, &counter);

    carbon_event_emit_game_started(d);
    REQUIRE(counter == 1);

    carbon_event_unsubscribe(d, id);

    carbon_event_emit_game_started(d);
    REQUIRE(counter == 1);  // Still 1, no longer receiving

    carbon_event_dispatcher_destroy(d);
}

TEST_CASE("Unsubscribe with invalid ID", "[event][unsubscribe]") {
    Carbon_EventDispatcher *d = carbon_event_dispatcher_create();

    // Should not crash
    carbon_event_unsubscribe(d, 0);
    carbon_event_unsubscribe(d, 99999);

    carbon_event_dispatcher_destroy(d);
}

TEST_CASE("Clear all listeners", "[event][unsubscribe]") {
    Carbon_EventDispatcher *d = carbon_event_dispatcher_create();
    int counter1 = 0, counter2 = 0;

    carbon_event_subscribe(d, CARBON_EVENT_GAME_STARTED, counter_callback, &counter1);
    carbon_event_subscribe(d, CARBON_EVENT_GAME_PAUSED, counter_callback, &counter2);

    carbon_event_emit_game_started(d);
    carbon_event_emit_game_paused(d);
    REQUIRE(counter1 == 1);
    REQUIRE(counter2 == 1);

    carbon_event_clear_all(d);

    carbon_event_emit_game_started(d);
    carbon_event_emit_game_paused(d);
    REQUIRE(counter1 == 1);  // Unchanged
    REQUIRE(counter2 == 1);  // Unchanged

    carbon_event_dispatcher_destroy(d);
}

/* ============================================================================
 * Event Data Tests
 * ============================================================================ */

TEST_CASE("Event data is passed correctly", "[event][data]") {
    Carbon_EventDispatcher *d = carbon_event_dispatcher_create();

    SECTION("Turn events carry turn number") {
        EventRecorder recorder;
        carbon_event_subscribe(d, CARBON_EVENT_TURN_STARTED, recorder_callback, &recorder);

        carbon_event_emit_turn_started(d, 42);

        REQUIRE(recorder.received_values.size() == 1);
        REQUIRE(recorder.received_values[0] == 42);
    }

    SECTION("Window resize carries dimensions") {
        struct {
            int width = 0;
            int height = 0;
        } recorded;

        auto capture_resize = [](const Carbon_Event *e, void *ud) {
            auto *r = static_cast<decltype(&recorded)>(ud);
            r->width = e->window_resize.width;
            r->height = e->window_resize.height;
        };

        carbon_event_subscribe(d, CARBON_EVENT_WINDOW_RESIZE, capture_resize, &recorded);
        carbon_event_emit_window_resize(d, 1920, 1080);

        REQUIRE(recorded.width == 1920);
        REQUIRE(recorded.height == 1080);
    }

    SECTION("Resource change carries values") {
        struct {
            int type = 0;
            int32_t old_val = 0;
            int32_t new_val = 0;
        } recorded;

        auto capture_resource = [](const Carbon_Event *e, void *ud) {
            auto *r = static_cast<decltype(&recorded)>(ud);
            r->type = e->resource.resource_type;
            r->old_val = e->resource.old_value;
            r->new_val = e->resource.new_value;
        };

        carbon_event_subscribe(d, CARBON_EVENT_RESOURCE_CHANGED, capture_resource, &recorded);
        carbon_event_emit_resource_changed(d, 1, 100, 150);

        REQUIRE(recorded.type == 1);
        REQUIRE(recorded.old_val == 100);
        REQUIRE(recorded.new_val == 150);
    }

    SECTION("Custom event carries user data") {
        struct CustomData {
            int x;
            float y;
        };

        CustomData data = {42, 3.14f};
        CustomData *received = nullptr;

        auto capture_custom = [](const Carbon_Event *e, void *ud) {
            auto **ptr = static_cast<CustomData **>(ud);
            *ptr = static_cast<CustomData *>(e->custom.data);
        };

        carbon_event_subscribe(d, CARBON_EVENT_CUSTOM, capture_custom, &received);
        carbon_event_emit_custom(d, 999, &data, sizeof(data));

        REQUIRE(received != nullptr);
        REQUIRE(received->x == 42);
        REQUIRE(received->y == Catch::Approx(3.14f));
    }

    carbon_event_dispatcher_destroy(d);
}

/* ============================================================================
 * Deferred Event Tests
 * ============================================================================ */

TEST_CASE("Deferred events", "[event][deferred]") {
    Carbon_EventDispatcher *d = carbon_event_dispatcher_create();
    int counter = 0;

    carbon_event_subscribe(d, CARBON_EVENT_GAME_STARTED, counter_callback, &counter);

    SECTION("Deferred events don't fire immediately") {
        Carbon_Event e = {};
        e.type = CARBON_EVENT_GAME_STARTED;
        carbon_event_emit_deferred(d, &e);

        REQUIRE(counter == 0);  // Not yet fired
    }

    SECTION("Deferred events fire on flush") {
        Carbon_Event e = {};
        e.type = CARBON_EVENT_GAME_STARTED;
        carbon_event_emit_deferred(d, &e);
        carbon_event_emit_deferred(d, &e);
        carbon_event_emit_deferred(d, &e);

        REQUIRE(counter == 0);

        carbon_event_flush_deferred(d);

        REQUIRE(counter == 3);
    }

    SECTION("Flush with no deferred events is safe") {
        carbon_event_flush_deferred(d);  // Should not crash
        REQUIRE(counter == 0);
    }

    carbon_event_dispatcher_destroy(d);
}

/* ============================================================================
 * Listener Count Tests
 * ============================================================================ */

TEST_CASE("Listener count tracking", "[event][count]") {
    Carbon_EventDispatcher *d = carbon_event_dispatcher_create();
    int dummy = 0;

    REQUIRE(carbon_event_listener_count(d, CARBON_EVENT_GAME_STARTED) == 0);

    Carbon_ListenerID id1 = carbon_event_subscribe(
        d, CARBON_EVENT_GAME_STARTED, counter_callback, &dummy);
    REQUIRE(carbon_event_listener_count(d, CARBON_EVENT_GAME_STARTED) == 1);

    Carbon_ListenerID id2 = carbon_event_subscribe(
        d, CARBON_EVENT_GAME_STARTED, counter_callback, &dummy);
    REQUIRE(carbon_event_listener_count(d, CARBON_EVENT_GAME_STARTED) == 2);

    carbon_event_unsubscribe(d, id1);
    REQUIRE(carbon_event_listener_count(d, CARBON_EVENT_GAME_STARTED) == 1);

    carbon_event_unsubscribe(d, id2);
    REQUIRE(carbon_event_listener_count(d, CARBON_EVENT_GAME_STARTED) == 0);

    carbon_event_dispatcher_destroy(d);
}

/* ============================================================================
 * Event Type Name Tests
 * ============================================================================ */

TEST_CASE("Event type names", "[event][names]") {
    REQUIRE(strcmp(carbon_event_type_name(CARBON_EVENT_NONE), "NONE") == 0);
    REQUIRE(strcmp(carbon_event_type_name(CARBON_EVENT_TURN_STARTED), "TURN_STARTED") == 0);
    REQUIRE(strcmp(carbon_event_type_name(CARBON_EVENT_GAME_STARTED), "GAME_STARTED") == 0);

    // Unknown type should return something sensible
    const char *unknown = carbon_event_type_name((Carbon_EventType)9999);
    REQUIRE(unknown != nullptr);
}

/* ============================================================================
 * Frame Number Tests
 * ============================================================================ */

TEST_CASE("Event timestamp", "[event][timestamp]") {
    Carbon_EventDispatcher *d = carbon_event_dispatcher_create();

    struct {
        uint32_t timestamp = 0;
    } recorded;

    auto capture_timestamp = [](const Carbon_Event *e, void *ud) {
        auto *r = static_cast<decltype(&recorded)>(ud);
        r->timestamp = e->timestamp;
    };

    carbon_event_subscribe(d, CARBON_EVENT_GAME_STARTED, capture_timestamp, &recorded);

    carbon_event_set_frame(d, 100);
    carbon_event_emit_game_started(d);
    REQUIRE(recorded.timestamp == 100);

    carbon_event_set_frame(d, 200);
    carbon_event_emit_game_started(d);
    REQUIRE(recorded.timestamp == 200);

    carbon_event_dispatcher_destroy(d);
}
