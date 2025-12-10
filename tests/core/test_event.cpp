/*
 * Carbon Event Dispatcher Tests
 *
 * Tests for the publish-subscribe event system.
 */

#include "catch_amalgamated.hpp"
#include "agentite/event.h"
#include <cstring>
#include <vector>

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

struct EventRecorder {
    std::vector<Agentite_EventType> received_types;
    std::vector<uint32_t> received_values;
    int call_count = 0;

    void reset() {
        received_types.clear();
        received_values.clear();
        call_count = 0;
    }
};

static void recorder_callback(const Agentite_Event *event, void *userdata) {
    EventRecorder *rec = static_cast<EventRecorder *>(userdata);
    rec->received_types.push_back(event->type);
    rec->call_count++;

    // Store event-specific data
    if (event->type == AGENTITE_EVENT_TURN_STARTED || event->type == AGENTITE_EVENT_TURN_ENDED) {
        rec->received_values.push_back(event->turn.turn);
    }
}

static void counter_callback(const Agentite_Event *event, void *userdata) {
    (void)event;
    int *counter = static_cast<int *>(userdata);
    (*counter)++;
}

/* ============================================================================
 * Dispatcher Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Event dispatcher creation and destruction", "[event][lifecycle]") {
    Agentite_EventDispatcher *d = agentite_event_dispatcher_create();
    REQUIRE(d != nullptr);
    agentite_event_dispatcher_destroy(d);
}

TEST_CASE("Destroy NULL dispatcher", "[event][lifecycle]") {
    // Should not crash
    agentite_event_dispatcher_destroy(nullptr);
}

/* ============================================================================
 * Subscription Tests
 * ============================================================================ */

TEST_CASE("Subscribe and receive events", "[event][subscribe]") {
    Agentite_EventDispatcher *d = agentite_event_dispatcher_create();
    EventRecorder recorder;

    SECTION("Single subscription") {
        Agentite_ListenerID id = agentite_event_subscribe(
            d, AGENTITE_EVENT_TURN_STARTED, recorder_callback, &recorder);
        REQUIRE(id != 0);

        agentite_event_emit_turn_started(d, 1);

        REQUIRE(recorder.call_count == 1);
        REQUIRE(recorder.received_types[0] == AGENTITE_EVENT_TURN_STARTED);
        REQUIRE(recorder.received_values[0] == 1);

        agentite_event_unsubscribe(d, id);
    }

    SECTION("Multiple subscriptions to same event") {
        int counter1 = 0, counter2 = 0;

        Agentite_ListenerID id1 = agentite_event_subscribe(
            d, AGENTITE_EVENT_GAME_STARTED, counter_callback, &counter1);
        Agentite_ListenerID id2 = agentite_event_subscribe(
            d, AGENTITE_EVENT_GAME_STARTED, counter_callback, &counter2);

        REQUIRE(id1 != 0);
        REQUIRE(id2 != 0);
        REQUIRE(id1 != id2);

        agentite_event_emit_game_started(d);

        REQUIRE(counter1 == 1);
        REQUIRE(counter2 == 1);

        agentite_event_unsubscribe(d, id1);
        agentite_event_unsubscribe(d, id2);
    }

    SECTION("Subscribe to different events") {
        EventRecorder rec1, rec2;

        Agentite_ListenerID id1 = agentite_event_subscribe(
            d, AGENTITE_EVENT_TURN_STARTED, recorder_callback, &rec1);
        Agentite_ListenerID id2 = agentite_event_subscribe(
            d, AGENTITE_EVENT_TURN_ENDED, recorder_callback, &rec2);

        agentite_event_emit_turn_started(d, 1);
        agentite_event_emit_turn_ended(d, 1);

        REQUIRE(rec1.call_count == 1);
        REQUIRE(rec1.received_types[0] == AGENTITE_EVENT_TURN_STARTED);

        REQUIRE(rec2.call_count == 1);
        REQUIRE(rec2.received_types[0] == AGENTITE_EVENT_TURN_ENDED);

        agentite_event_unsubscribe(d, id1);
        agentite_event_unsubscribe(d, id2);
    }

    SECTION("Subscribe to all events") {
        Agentite_ListenerID id = agentite_event_subscribe_all(d, recorder_callback, &recorder);
        REQUIRE(id != 0);

        agentite_event_emit_turn_started(d, 1);
        agentite_event_emit_game_paused(d);
        agentite_event_emit_turn_ended(d, 1);

        REQUIRE(recorder.call_count == 3);

        agentite_event_unsubscribe(d, id);
    }

    agentite_event_dispatcher_destroy(d);
}

/* ============================================================================
 * Unsubscribe Tests
 * ============================================================================ */

TEST_CASE("Unsubscribe stops receiving events", "[event][unsubscribe]") {
    Agentite_EventDispatcher *d = agentite_event_dispatcher_create();
    int counter = 0;

    Agentite_ListenerID id = agentite_event_subscribe(
        d, AGENTITE_EVENT_GAME_STARTED, counter_callback, &counter);

    agentite_event_emit_game_started(d);
    REQUIRE(counter == 1);

    agentite_event_unsubscribe(d, id);

    agentite_event_emit_game_started(d);
    REQUIRE(counter == 1);  // Still 1, no longer receiving

    agentite_event_dispatcher_destroy(d);
}

TEST_CASE("Unsubscribe with invalid ID", "[event][unsubscribe]") {
    Agentite_EventDispatcher *d = agentite_event_dispatcher_create();

    // Should not crash
    agentite_event_unsubscribe(d, 0);
    agentite_event_unsubscribe(d, 99999);

    agentite_event_dispatcher_destroy(d);
}

TEST_CASE("Clear all listeners", "[event][unsubscribe]") {
    Agentite_EventDispatcher *d = agentite_event_dispatcher_create();
    int counter1 = 0, counter2 = 0;

    agentite_event_subscribe(d, AGENTITE_EVENT_GAME_STARTED, counter_callback, &counter1);
    agentite_event_subscribe(d, AGENTITE_EVENT_GAME_PAUSED, counter_callback, &counter2);

    agentite_event_emit_game_started(d);
    agentite_event_emit_game_paused(d);
    REQUIRE(counter1 == 1);
    REQUIRE(counter2 == 1);

    agentite_event_clear_all(d);

    agentite_event_emit_game_started(d);
    agentite_event_emit_game_paused(d);
    REQUIRE(counter1 == 1);  // Unchanged
    REQUIRE(counter2 == 1);  // Unchanged

    agentite_event_dispatcher_destroy(d);
}

/* ============================================================================
 * Event Data Tests
 * ============================================================================ */

TEST_CASE("Event data is passed correctly", "[event][data]") {
    Agentite_EventDispatcher *d = agentite_event_dispatcher_create();

    SECTION("Turn events carry turn number") {
        EventRecorder recorder;
        agentite_event_subscribe(d, AGENTITE_EVENT_TURN_STARTED, recorder_callback, &recorder);

        agentite_event_emit_turn_started(d, 42);

        REQUIRE(recorder.received_values.size() == 1);
        REQUIRE(recorder.received_values[0] == 42);
    }

    SECTION("Window resize carries dimensions") {
        struct {
            int width = 0;
            int height = 0;
        } recorded;

        auto capture_resize = [](const Agentite_Event *e, void *ud) {
            auto *r = static_cast<decltype(&recorded)>(ud);
            r->width = e->window_resize.width;
            r->height = e->window_resize.height;
        };

        agentite_event_subscribe(d, AGENTITE_EVENT_WINDOW_RESIZE, capture_resize, &recorded);
        agentite_event_emit_window_resize(d, 1920, 1080);

        REQUIRE(recorded.width == 1920);
        REQUIRE(recorded.height == 1080);
    }

    SECTION("Resource change carries values") {
        struct {
            int type = 0;
            int32_t old_val = 0;
            int32_t new_val = 0;
        } recorded;

        auto capture_resource = [](const Agentite_Event *e, void *ud) {
            auto *r = static_cast<decltype(&recorded)>(ud);
            r->type = e->resource.resource_type;
            r->old_val = e->resource.old_value;
            r->new_val = e->resource.new_value;
        };

        agentite_event_subscribe(d, AGENTITE_EVENT_RESOURCE_CHANGED, capture_resource, &recorded);
        agentite_event_emit_resource_changed(d, 1, 100, 150);

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

        auto capture_custom = [](const Agentite_Event *e, void *ud) {
            auto **ptr = static_cast<CustomData **>(ud);
            *ptr = static_cast<CustomData *>(e->custom.data);
        };

        agentite_event_subscribe(d, AGENTITE_EVENT_CUSTOM, capture_custom, &received);
        agentite_event_emit_custom(d, 999, &data, sizeof(data));

        REQUIRE(received != nullptr);
        REQUIRE(received->x == 42);
        REQUIRE(received->y == Catch::Approx(3.14f));
    }

    agentite_event_dispatcher_destroy(d);
}

/* ============================================================================
 * Deferred Event Tests
 * ============================================================================ */

TEST_CASE("Deferred events", "[event][deferred]") {
    Agentite_EventDispatcher *d = agentite_event_dispatcher_create();
    int counter = 0;

    agentite_event_subscribe(d, AGENTITE_EVENT_GAME_STARTED, counter_callback, &counter);

    SECTION("Deferred events don't fire immediately") {
        Agentite_Event e = {};
        e.type = AGENTITE_EVENT_GAME_STARTED;
        agentite_event_emit_deferred(d, &e);

        REQUIRE(counter == 0);  // Not yet fired
    }

    SECTION("Deferred events fire on flush") {
        Agentite_Event e = {};
        e.type = AGENTITE_EVENT_GAME_STARTED;
        agentite_event_emit_deferred(d, &e);
        agentite_event_emit_deferred(d, &e);
        agentite_event_emit_deferred(d, &e);

        REQUIRE(counter == 0);

        agentite_event_flush_deferred(d);

        REQUIRE(counter == 3);
    }

    SECTION("Flush with no deferred events is safe") {
        agentite_event_flush_deferred(d);  // Should not crash
        REQUIRE(counter == 0);
    }

    agentite_event_dispatcher_destroy(d);
}

/* ============================================================================
 * Listener Count Tests
 * ============================================================================ */

TEST_CASE("Listener count tracking", "[event][count]") {
    Agentite_EventDispatcher *d = agentite_event_dispatcher_create();
    int dummy = 0;

    REQUIRE(agentite_event_listener_count(d, AGENTITE_EVENT_GAME_STARTED) == 0);

    Agentite_ListenerID id1 = agentite_event_subscribe(
        d, AGENTITE_EVENT_GAME_STARTED, counter_callback, &dummy);
    REQUIRE(agentite_event_listener_count(d, AGENTITE_EVENT_GAME_STARTED) == 1);

    Agentite_ListenerID id2 = agentite_event_subscribe(
        d, AGENTITE_EVENT_GAME_STARTED, counter_callback, &dummy);
    REQUIRE(agentite_event_listener_count(d, AGENTITE_EVENT_GAME_STARTED) == 2);

    agentite_event_unsubscribe(d, id1);
    REQUIRE(agentite_event_listener_count(d, AGENTITE_EVENT_GAME_STARTED) == 1);

    agentite_event_unsubscribe(d, id2);
    REQUIRE(agentite_event_listener_count(d, AGENTITE_EVENT_GAME_STARTED) == 0);

    agentite_event_dispatcher_destroy(d);
}

/* ============================================================================
 * Event Type Name Tests
 * ============================================================================ */

TEST_CASE("Event type names", "[event][names]") {
    REQUIRE(strcmp(agentite_event_type_name(AGENTITE_EVENT_NONE), "NONE") == 0);
    REQUIRE(strcmp(agentite_event_type_name(AGENTITE_EVENT_TURN_STARTED), "TURN_STARTED") == 0);
    REQUIRE(strcmp(agentite_event_type_name(AGENTITE_EVENT_GAME_STARTED), "GAME_STARTED") == 0);

    // Unknown type should return something sensible
    const char *unknown = agentite_event_type_name((Agentite_EventType)9999);
    REQUIRE(unknown != nullptr);
}

/* ============================================================================
 * Frame Number Tests
 * ============================================================================ */

TEST_CASE("Event timestamp", "[event][timestamp]") {
    Agentite_EventDispatcher *d = agentite_event_dispatcher_create();

    struct {
        uint32_t timestamp = 0;
    } recorded;

    auto capture_timestamp = [](const Agentite_Event *e, void *ud) {
        auto *r = static_cast<decltype(&recorded)>(ud);
        r->timestamp = e->timestamp;
    };

    agentite_event_subscribe(d, AGENTITE_EVENT_GAME_STARTED, capture_timestamp, &recorded);

    agentite_event_set_frame(d, 100);
    agentite_event_emit_game_started(d);
    REQUIRE(recorded.timestamp == 100);

    agentite_event_set_frame(d, 200);
    agentite_event_emit_game_started(d);
    REQUIRE(recorded.timestamp == 200);

    agentite_event_dispatcher_destroy(d);
}
