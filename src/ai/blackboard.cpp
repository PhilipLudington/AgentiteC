/**
 * Carbon Shared Blackboard System
 *
 * Cross-system communication and data sharing without direct coupling.
 * Provides key-value storage, resource reservations, plan publication,
 * and decision history tracking.
 */

#include "carbon/carbon.h"
#include "carbon/blackboard.h"
#include "carbon/error.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/*============================================================================
 * Internal Data Structures
 *============================================================================*/

/**
 * Key-value entry
 */
typedef struct {
    char key[CARBON_BB_MAX_KEY_LEN];
    Carbon_BBValue value;
    bool used;
} BBEntry;

/**
 * Subscription entry
 */
typedef struct {
    char key[CARBON_BB_MAX_KEY_LEN];  /* Empty = all keys */
    Carbon_BBChangeCallback callback;
    void *userdata;
    uint32_t id;
    bool used;
} BBSubscription;

#define CARBON_BB_MAX_SUBSCRIPTIONS 8

/**
 * Blackboard internal structure
 */
struct Carbon_Blackboard {
    /* Key-value storage */
    BBEntry entries[CARBON_BB_MAX_ENTRIES];
    int entry_count;

    /* Reservations */
    Carbon_BBReservation reservations[CARBON_BB_MAX_RESERVATIONS];
    int reservation_count;

    /* Plans */
    Carbon_BBPlan plans[CARBON_BB_MAX_PLANS];
    int plan_count;

    /* History (circular buffer) */
    Carbon_BBHistoryEntry history[CARBON_BB_MAX_HISTORY];
    int history_head;       /* Next write position */
    int history_count;      /* Total entries (up to max) */
    uint32_t history_seq;   /* Monotonic counter for ordering */

    /* Subscriptions */
    BBSubscription subscriptions[CARBON_BB_MAX_SUBSCRIPTIONS];
    uint32_t next_sub_id;

    /* Turn tracking */
    int32_t current_turn;
};

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Find entry by key
 */
static BBEntry *find_entry(Carbon_Blackboard *bb, const char *key) {
    for (int i = 0; i < CARBON_BB_MAX_ENTRIES; i++) {
        if (bb->entries[i].used && strcmp(bb->entries[i].key, key) == 0) {
            return &bb->entries[i];
        }
    }
    return NULL;
}

/**
 * Find or create entry
 */
static BBEntry *get_or_create_entry(Carbon_Blackboard *bb, const char *key) {
    /* Find existing */
    BBEntry *entry = find_entry(bb, key);
    if (entry) return entry;

    /* Find free slot */
    for (int i = 0; i < CARBON_BB_MAX_ENTRIES; i++) {
        if (!bb->entries[i].used) {
            entry = &bb->entries[i];
            entry->used = true;
            strncpy(entry->key, key, CARBON_BB_MAX_KEY_LEN - 1);
            entry->key[CARBON_BB_MAX_KEY_LEN - 1] = '\0';
            memset(&entry->value, 0, sizeof(Carbon_BBValue));
            bb->entry_count++;
            return entry;
        }
    }

    carbon_set_error("carbon_blackboard: max entries reached");
    return NULL;
}

/**
 * Notify subscribers of a change
 */
static void notify_change(Carbon_Blackboard *bb, const char *key,
                          const Carbon_BBValue *old_val,
                          const Carbon_BBValue *new_val) {
    for (int i = 0; i < CARBON_BB_MAX_SUBSCRIPTIONS; i++) {
        BBSubscription *sub = &bb->subscriptions[i];
        if (!sub->used) continue;

        /* Check if subscription matches */
        bool matches = (sub->key[0] == '\0') ||  /* All keys */
                       (strcmp(sub->key, key) == 0);

        if (matches && sub->callback) {
            sub->callback(bb, key, old_val, new_val, sub->userdata);
        }
    }
}

/*============================================================================
 * Creation and Destruction
 *============================================================================*/

Carbon_Blackboard *carbon_blackboard_create(void) {
    Carbon_Blackboard *bb = CARBON_ALLOC(Carbon_Blackboard);
    if (!bb) {
        carbon_set_error("carbon_blackboard_create: allocation failed");
        return NULL;
    }
    bb->next_sub_id = 1;
    return bb;
}

void carbon_blackboard_destroy(Carbon_Blackboard *bb) {
    if (bb) {
        free(bb);
    }
}

void carbon_blackboard_clear(Carbon_Blackboard *bb) {
    if (!bb) return;

    for (int i = 0; i < CARBON_BB_MAX_ENTRIES; i++) {
        bb->entries[i].used = false;
    }
    bb->entry_count = 0;
}

/*============================================================================
 * Value Storage
 *============================================================================*/

void carbon_blackboard_set_int(Carbon_Blackboard *bb, const char *key, int32_t value) {
    if (!bb || !key) return;

    BBEntry *entry = get_or_create_entry(bb, key);
    if (!entry) return;

    Carbon_BBValue old_val = entry->value;
    entry->value.type = CARBON_BB_TYPE_INT;
    entry->value.i32 = value;

    notify_change(bb, key, old_val.type != CARBON_BB_TYPE_NONE ? &old_val : NULL,
                  &entry->value);
}

void carbon_blackboard_set_int64(Carbon_Blackboard *bb, const char *key, int64_t value) {
    if (!bb || !key) return;

    BBEntry *entry = get_or_create_entry(bb, key);
    if (!entry) return;

    Carbon_BBValue old_val = entry->value;
    entry->value.type = CARBON_BB_TYPE_INT64;
    entry->value.i64 = value;

    notify_change(bb, key, old_val.type != CARBON_BB_TYPE_NONE ? &old_val : NULL,
                  &entry->value);
}

void carbon_blackboard_set_float(Carbon_Blackboard *bb, const char *key, float value) {
    if (!bb || !key) return;

    BBEntry *entry = get_or_create_entry(bb, key);
    if (!entry) return;

    Carbon_BBValue old_val = entry->value;
    entry->value.type = CARBON_BB_TYPE_FLOAT;
    entry->value.f32 = value;

    notify_change(bb, key, old_val.type != CARBON_BB_TYPE_NONE ? &old_val : NULL,
                  &entry->value);
}

void carbon_blackboard_set_double(Carbon_Blackboard *bb, const char *key, double value) {
    if (!bb || !key) return;

    BBEntry *entry = get_or_create_entry(bb, key);
    if (!entry) return;

    Carbon_BBValue old_val = entry->value;
    entry->value.type = CARBON_BB_TYPE_DOUBLE;
    entry->value.f64 = value;

    notify_change(bb, key, old_val.type != CARBON_BB_TYPE_NONE ? &old_val : NULL,
                  &entry->value);
}

void carbon_blackboard_set_bool(Carbon_Blackboard *bb, const char *key, bool value) {
    if (!bb || !key) return;

    BBEntry *entry = get_or_create_entry(bb, key);
    if (!entry) return;

    Carbon_BBValue old_val = entry->value;
    entry->value.type = CARBON_BB_TYPE_BOOL;
    entry->value.b = value;

    notify_change(bb, key, old_val.type != CARBON_BB_TYPE_NONE ? &old_val : NULL,
                  &entry->value);
}

void carbon_blackboard_set_string(Carbon_Blackboard *bb, const char *key, const char *value) {
    if (!bb || !key) return;

    BBEntry *entry = get_or_create_entry(bb, key);
    if (!entry) return;

    Carbon_BBValue old_val = entry->value;
    entry->value.type = CARBON_BB_TYPE_STRING;
    if (value) {
        strncpy(entry->value.str, value, CARBON_BB_MAX_STRING_LEN - 1);
        entry->value.str[CARBON_BB_MAX_STRING_LEN - 1] = '\0';
    } else {
        entry->value.str[0] = '\0';
    }

    notify_change(bb, key, old_val.type != CARBON_BB_TYPE_NONE ? &old_val : NULL,
                  &entry->value);
}

void carbon_blackboard_set_ptr(Carbon_Blackboard *bb, const char *key, void *value) {
    if (!bb || !key) return;

    BBEntry *entry = get_or_create_entry(bb, key);
    if (!entry) return;

    Carbon_BBValue old_val = entry->value;
    entry->value.type = CARBON_BB_TYPE_PTR;
    entry->value.ptr = value;

    notify_change(bb, key, old_val.type != CARBON_BB_TYPE_NONE ? &old_val : NULL,
                  &entry->value);
}

void carbon_blackboard_set_vec2(Carbon_Blackboard *bb, const char *key, float x, float y) {
    if (!bb || !key) return;

    BBEntry *entry = get_or_create_entry(bb, key);
    if (!entry) return;

    Carbon_BBValue old_val = entry->value;
    entry->value.type = CARBON_BB_TYPE_VEC2;
    entry->value.vec2[0] = x;
    entry->value.vec2[1] = y;

    notify_change(bb, key, old_val.type != CARBON_BB_TYPE_NONE ? &old_val : NULL,
                  &entry->value);
}

void carbon_blackboard_set_vec3(Carbon_Blackboard *bb, const char *key,
                                 float x, float y, float z) {
    if (!bb || !key) return;

    BBEntry *entry = get_or_create_entry(bb, key);
    if (!entry) return;

    Carbon_BBValue old_val = entry->value;
    entry->value.type = CARBON_BB_TYPE_VEC3;
    entry->value.vec3[0] = x;
    entry->value.vec3[1] = y;
    entry->value.vec3[2] = z;

    notify_change(bb, key, old_val.type != CARBON_BB_TYPE_NONE ? &old_val : NULL,
                  &entry->value);
}

/*============================================================================
 * Value Retrieval
 *============================================================================*/

bool carbon_blackboard_has(const Carbon_Blackboard *bb, const char *key) {
    if (!bb || !key) return false;
    return find_entry((Carbon_Blackboard *)bb, key) != NULL;
}

Carbon_BBValueType carbon_blackboard_get_type(const Carbon_Blackboard *bb, const char *key) {
    if (!bb || !key) return CARBON_BB_TYPE_NONE;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    return entry ? entry->value.type : CARBON_BB_TYPE_NONE;
}

int32_t carbon_blackboard_get_int(const Carbon_Blackboard *bb, const char *key) {
    if (!bb || !key) return 0;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    if (!entry) return 0;

    switch (entry->value.type) {
        case CARBON_BB_TYPE_INT:
            return entry->value.i32;
        case CARBON_BB_TYPE_INT64:
            return (int32_t)entry->value.i64;
        case CARBON_BB_TYPE_FLOAT:
            return (int32_t)entry->value.f32;
        case CARBON_BB_TYPE_DOUBLE:
            return (int32_t)entry->value.f64;
        case CARBON_BB_TYPE_BOOL:
            return entry->value.b ? 1 : 0;
        default:
            return 0;
    }
}

int64_t carbon_blackboard_get_int64(const Carbon_Blackboard *bb, const char *key) {
    if (!bb || !key) return 0;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    if (!entry) return 0;

    switch (entry->value.type) {
        case CARBON_BB_TYPE_INT:
            return entry->value.i32;
        case CARBON_BB_TYPE_INT64:
            return entry->value.i64;
        case CARBON_BB_TYPE_FLOAT:
            return (int64_t)entry->value.f32;
        case CARBON_BB_TYPE_DOUBLE:
            return (int64_t)entry->value.f64;
        case CARBON_BB_TYPE_BOOL:
            return entry->value.b ? 1 : 0;
        default:
            return 0;
    }
}

float carbon_blackboard_get_float(const Carbon_Blackboard *bb, const char *key) {
    if (!bb || !key) return 0.0f;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    if (!entry) return 0.0f;

    switch (entry->value.type) {
        case CARBON_BB_TYPE_INT:
            return (float)entry->value.i32;
        case CARBON_BB_TYPE_INT64:
            return (float)entry->value.i64;
        case CARBON_BB_TYPE_FLOAT:
            return entry->value.f32;
        case CARBON_BB_TYPE_DOUBLE:
            return (float)entry->value.f64;
        case CARBON_BB_TYPE_BOOL:
            return entry->value.b ? 1.0f : 0.0f;
        default:
            return 0.0f;
    }
}

double carbon_blackboard_get_double(const Carbon_Blackboard *bb, const char *key) {
    if (!bb || !key) return 0.0;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    if (!entry) return 0.0;

    switch (entry->value.type) {
        case CARBON_BB_TYPE_INT:
            return (double)entry->value.i32;
        case CARBON_BB_TYPE_INT64:
            return (double)entry->value.i64;
        case CARBON_BB_TYPE_FLOAT:
            return (double)entry->value.f32;
        case CARBON_BB_TYPE_DOUBLE:
            return entry->value.f64;
        case CARBON_BB_TYPE_BOOL:
            return entry->value.b ? 1.0 : 0.0;
        default:
            return 0.0;
    }
}

bool carbon_blackboard_get_bool(const Carbon_Blackboard *bb, const char *key) {
    if (!bb || !key) return false;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    if (!entry) return false;

    switch (entry->value.type) {
        case CARBON_BB_TYPE_INT:
            return entry->value.i32 != 0;
        case CARBON_BB_TYPE_INT64:
            return entry->value.i64 != 0;
        case CARBON_BB_TYPE_FLOAT:
            return entry->value.f32 != 0.0f;
        case CARBON_BB_TYPE_DOUBLE:
            return entry->value.f64 != 0.0;
        case CARBON_BB_TYPE_BOOL:
            return entry->value.b;
        case CARBON_BB_TYPE_PTR:
            return entry->value.ptr != NULL;
        case CARBON_BB_TYPE_STRING:
            return entry->value.str[0] != '\0';
        default:
            return false;
    }
}

const char *carbon_blackboard_get_string(const Carbon_Blackboard *bb, const char *key) {
    if (!bb || !key) return NULL;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    if (!entry || entry->value.type != CARBON_BB_TYPE_STRING) return NULL;

    return entry->value.str;
}

void *carbon_blackboard_get_ptr(const Carbon_Blackboard *bb, const char *key) {
    if (!bb || !key) return NULL;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    if (!entry || entry->value.type != CARBON_BB_TYPE_PTR) return NULL;

    return entry->value.ptr;
}

bool carbon_blackboard_get_vec2(const Carbon_Blackboard *bb, const char *key,
                                 float *out_x, float *out_y) {
    if (!bb || !key) return false;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    if (!entry || entry->value.type != CARBON_BB_TYPE_VEC2) return false;

    if (out_x) *out_x = entry->value.vec2[0];
    if (out_y) *out_y = entry->value.vec2[1];
    return true;
}

bool carbon_blackboard_get_vec3(const Carbon_Blackboard *bb, const char *key,
                                 float *out_x, float *out_y, float *out_z) {
    if (!bb || !key) return false;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    if (!entry || entry->value.type != CARBON_BB_TYPE_VEC3) return false;

    if (out_x) *out_x = entry->value.vec3[0];
    if (out_y) *out_y = entry->value.vec3[1];
    if (out_z) *out_z = entry->value.vec3[2];
    return true;
}

const Carbon_BBValue *carbon_blackboard_get_value(const Carbon_Blackboard *bb, const char *key) {
    if (!bb || !key) return NULL;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    return entry ? &entry->value : NULL;
}

bool carbon_blackboard_remove(Carbon_Blackboard *bb, const char *key) {
    if (!bb || !key) return false;

    BBEntry *entry = find_entry(bb, key);
    if (!entry) return false;

    entry->used = false;
    bb->entry_count--;
    return true;
}

/*============================================================================
 * Integer Operations
 *============================================================================*/

int32_t carbon_blackboard_inc_int(Carbon_Blackboard *bb, const char *key, int32_t amount) {
    if (!bb || !key) return 0;

    BBEntry *entry = get_or_create_entry(bb, key);
    if (!entry) return 0;

    if (entry->value.type == CARBON_BB_TYPE_NONE) {
        entry->value.type = CARBON_BB_TYPE_INT;
        entry->value.i32 = 0;
    }

    if (entry->value.type == CARBON_BB_TYPE_INT) {
        entry->value.i32 += amount;
        return entry->value.i32;
    }

    return 0;
}

int32_t carbon_blackboard_get_int_or(const Carbon_Blackboard *bb, const char *key,
                                      int32_t default_val) {
    if (!bb || !key) return default_val;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    if (!entry) return default_val;

    return carbon_blackboard_get_int(bb, key);
}

float carbon_blackboard_get_float_or(const Carbon_Blackboard *bb, const char *key,
                                      float default_val) {
    if (!bb || !key) return default_val;

    BBEntry *entry = find_entry((Carbon_Blackboard *)bb, key);
    if (!entry) return default_val;

    return carbon_blackboard_get_float(bb, key);
}

/*============================================================================
 * Resource Reservations
 *============================================================================*/

bool carbon_blackboard_reserve(Carbon_Blackboard *bb, const char *resource,
                                int32_t amount, const char *owner) {
    return carbon_blackboard_reserve_ex(bb, resource, amount, owner, -1);
}

bool carbon_blackboard_reserve_ex(Carbon_Blackboard *bb, const char *resource,
                                   int32_t amount, const char *owner, int turns) {
    if (!bb || !resource || !owner || amount <= 0) return false;

    /* Check for existing reservation by same owner */
    for (int i = 0; i < bb->reservation_count; i++) {
        Carbon_BBReservation *res = &bb->reservations[i];
        if (strcmp(res->resource, resource) == 0 && strcmp(res->owner, owner) == 0) {
            /* Update existing reservation */
            res->amount = amount;
            res->turns_remaining = turns;
            return true;
        }
    }

    /* Create new reservation */
    if (bb->reservation_count >= CARBON_BB_MAX_RESERVATIONS) {
        carbon_set_error("carbon_blackboard_reserve: max reservations reached");
        return false;
    }

    Carbon_BBReservation *res = &bb->reservations[bb->reservation_count++];
    strncpy(res->resource, resource, CARBON_BB_MAX_KEY_LEN - 1);
    res->resource[CARBON_BB_MAX_KEY_LEN - 1] = '\0';
    strncpy(res->owner, owner, CARBON_BB_MAX_KEY_LEN - 1);
    res->owner[CARBON_BB_MAX_KEY_LEN - 1] = '\0';
    res->amount = amount;
    res->turns_remaining = turns;

    return true;
}

void carbon_blackboard_release(Carbon_Blackboard *bb, const char *resource,
                                const char *owner) {
    if (!bb || !resource || !owner) return;

    for (int i = 0; i < bb->reservation_count; i++) {
        Carbon_BBReservation *res = &bb->reservations[i];
        if (strcmp(res->resource, resource) == 0 && strcmp(res->owner, owner) == 0) {
            /* Swap with last and decrement */
            bb->reservations[i] = bb->reservations[--bb->reservation_count];
            return;
        }
    }
}

void carbon_blackboard_release_all(Carbon_Blackboard *bb, const char *owner) {
    if (!bb || !owner) return;

    for (int i = bb->reservation_count - 1; i >= 0; i--) {
        if (strcmp(bb->reservations[i].owner, owner) == 0) {
            bb->reservations[i] = bb->reservations[--bb->reservation_count];
        }
    }
}

int32_t carbon_blackboard_get_reserved(const Carbon_Blackboard *bb, const char *resource) {
    if (!bb || !resource) return 0;

    int32_t total = 0;
    for (int i = 0; i < bb->reservation_count; i++) {
        if (strcmp(bb->reservations[i].resource, resource) == 0) {
            total += bb->reservations[i].amount;
        }
    }
    return total;
}

int32_t carbon_blackboard_get_available(const Carbon_Blackboard *bb, const char *resource) {
    if (!bb || !resource) return 0;

    int32_t total = carbon_blackboard_get_int(bb, resource);
    int32_t reserved = carbon_blackboard_get_reserved(bb, resource);
    return total - reserved;
}

bool carbon_blackboard_has_reservation(const Carbon_Blackboard *bb, const char *resource) {
    if (!bb || !resource) return false;

    for (int i = 0; i < bb->reservation_count; i++) {
        if (strcmp(bb->reservations[i].resource, resource) == 0) {
            return true;
        }
    }
    return false;
}

int32_t carbon_blackboard_get_reservation(const Carbon_Blackboard *bb, const char *resource,
                                           const char *owner) {
    if (!bb || !resource || !owner) return 0;

    for (int i = 0; i < bb->reservation_count; i++) {
        const Carbon_BBReservation *res = &bb->reservations[i];
        if (strcmp(res->resource, resource) == 0 && strcmp(res->owner, owner) == 0) {
            return res->amount;
        }
    }
    return 0;
}

/*============================================================================
 * Plan Publication
 *============================================================================*/

void carbon_blackboard_publish_plan(Carbon_Blackboard *bb, const char *owner,
                                     const char *description) {
    carbon_blackboard_publish_plan_ex(bb, owner, description, "", -1);
}

void carbon_blackboard_publish_plan_ex(Carbon_Blackboard *bb, const char *owner,
                                        const char *description, const char *target,
                                        int turns) {
    if (!bb || !owner) return;

    /* Find existing plan by owner */
    for (int i = 0; i < bb->plan_count; i++) {
        if (strcmp(bb->plans[i].owner, owner) == 0) {
            /* Update existing plan */
            if (description) {
                strncpy(bb->plans[i].description, description,
                        CARBON_BB_MAX_STRING_LEN - 1);
                bb->plans[i].description[CARBON_BB_MAX_STRING_LEN - 1] = '\0';
            }
            if (target) {
                strncpy(bb->plans[i].target, target, CARBON_BB_MAX_KEY_LEN - 1);
                bb->plans[i].target[CARBON_BB_MAX_KEY_LEN - 1] = '\0';
            }
            bb->plans[i].turns_remaining = turns;
            bb->plans[i].active = true;
            return;
        }
    }

    /* Create new plan */
    if (bb->plan_count >= CARBON_BB_MAX_PLANS) {
        carbon_set_error("carbon_blackboard_publish_plan: max plans reached");
        return;
    }

    Carbon_BBPlan *plan = &bb->plans[bb->plan_count++];
    strncpy(plan->owner, owner, CARBON_BB_MAX_KEY_LEN - 1);
    plan->owner[CARBON_BB_MAX_KEY_LEN - 1] = '\0';

    if (description) {
        strncpy(plan->description, description, CARBON_BB_MAX_STRING_LEN - 1);
        plan->description[CARBON_BB_MAX_STRING_LEN - 1] = '\0';
    } else {
        plan->description[0] = '\0';
    }

    if (target) {
        strncpy(plan->target, target, CARBON_BB_MAX_KEY_LEN - 1);
        plan->target[CARBON_BB_MAX_KEY_LEN - 1] = '\0';
    } else {
        plan->target[0] = '\0';
    }

    plan->turns_remaining = turns;
    plan->active = true;
}

void carbon_blackboard_cancel_plan(Carbon_Blackboard *bb, const char *owner) {
    if (!bb || !owner) return;

    for (int i = 0; i < bb->plan_count; i++) {
        if (strcmp(bb->plans[i].owner, owner) == 0) {
            /* Swap with last and decrement */
            bb->plans[i] = bb->plans[--bb->plan_count];
            return;
        }
    }
}

bool carbon_blackboard_has_conflicting_plan(const Carbon_Blackboard *bb, const char *target) {
    if (!bb || !target) return false;

    for (int i = 0; i < bb->plan_count; i++) {
        if (bb->plans[i].active && strcmp(bb->plans[i].target, target) == 0) {
            return true;
        }
    }
    return false;
}

const Carbon_BBPlan *carbon_blackboard_get_plan(const Carbon_Blackboard *bb, const char *owner) {
    if (!bb || !owner) return NULL;

    for (int i = 0; i < bb->plan_count; i++) {
        if (strcmp(bb->plans[i].owner, owner) == 0) {
            return &bb->plans[i];
        }
    }
    return NULL;
}

int carbon_blackboard_get_all_plans(const Carbon_Blackboard *bb,
                                     const Carbon_BBPlan **out_plans, int max) {
    if (!bb || !out_plans || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < bb->plan_count && count < max; i++) {
        if (bb->plans[i].active) {
            out_plans[count++] = &bb->plans[i];
        }
    }
    return count;
}

/*============================================================================
 * History / Decision Log
 *============================================================================*/

void carbon_blackboard_log(Carbon_Blackboard *bb, const char *fmt, ...) {
    if (!bb || !fmt) return;

    va_list args;
    va_start(args, fmt);

    Carbon_BBHistoryEntry *entry = &bb->history[bb->history_head];
    vsnprintf(entry->text, CARBON_BB_HISTORY_ENTRY_LEN, fmt, args);
    entry->turn = bb->current_turn;
    entry->timestamp = bb->history_seq++;

    bb->history_head = (bb->history_head + 1) % CARBON_BB_MAX_HISTORY;
    if (bb->history_count < CARBON_BB_MAX_HISTORY) {
        bb->history_count++;
    }

    va_end(args);
}

void carbon_blackboard_log_turn(Carbon_Blackboard *bb, int32_t turn, const char *fmt, ...) {
    if (!bb || !fmt) return;

    va_list args;
    va_start(args, fmt);

    Carbon_BBHistoryEntry *entry = &bb->history[bb->history_head];
    vsnprintf(entry->text, CARBON_BB_HISTORY_ENTRY_LEN, fmt, args);
    entry->turn = turn;
    entry->timestamp = bb->history_seq++;

    bb->history_head = (bb->history_head + 1) % CARBON_BB_MAX_HISTORY;
    if (bb->history_count < CARBON_BB_MAX_HISTORY) {
        bb->history_count++;
    }

    va_end(args);
}

int carbon_blackboard_get_history(const Carbon_Blackboard *bb,
                                   const Carbon_BBHistoryEntry **out_entries, int max) {
    if (!bb || !out_entries || max <= 0) return 0;

    int count = (bb->history_count < max) ? bb->history_count : max;

    /* Return newest first */
    for (int i = 0; i < count; i++) {
        int idx = (bb->history_head - 1 - i + CARBON_BB_MAX_HISTORY) % CARBON_BB_MAX_HISTORY;
        out_entries[i] = &bb->history[idx];
    }

    return count;
}

int carbon_blackboard_get_history_strings(const Carbon_Blackboard *bb,
                                           const char **out_strings, int max) {
    if (!bb || !out_strings || max <= 0) return 0;

    int count = (bb->history_count < max) ? bb->history_count : max;

    /* Return newest first */
    for (int i = 0; i < count; i++) {
        int idx = (bb->history_head - 1 - i + CARBON_BB_MAX_HISTORY) % CARBON_BB_MAX_HISTORY;
        out_strings[i] = bb->history[idx].text;
    }

    return count;
}

void carbon_blackboard_clear_history(Carbon_Blackboard *bb) {
    if (!bb) return;

    bb->history_head = 0;
    bb->history_count = 0;
}

int carbon_blackboard_get_history_count(const Carbon_Blackboard *bb) {
    return bb ? bb->history_count : 0;
}

/*============================================================================
 * Subscriptions
 *============================================================================*/

uint32_t carbon_blackboard_subscribe(Carbon_Blackboard *bb, const char *key,
                                      Carbon_BBChangeCallback callback, void *userdata) {
    if (!bb || !callback) return 0;

    /* Find free slot */
    for (int i = 0; i < CARBON_BB_MAX_SUBSCRIPTIONS; i++) {
        if (!bb->subscriptions[i].used) {
            BBSubscription *sub = &bb->subscriptions[i];
            sub->used = true;
            sub->callback = callback;
            sub->userdata = userdata;
            sub->id = bb->next_sub_id++;

            if (key) {
                strncpy(sub->key, key, CARBON_BB_MAX_KEY_LEN - 1);
                sub->key[CARBON_BB_MAX_KEY_LEN - 1] = '\0';
            } else {
                sub->key[0] = '\0';  /* Watch all keys */
            }

            return sub->id;
        }
    }

    carbon_set_error("carbon_blackboard_subscribe: max subscriptions reached");
    return 0;
}

void carbon_blackboard_unsubscribe(Carbon_Blackboard *bb, uint32_t id) {
    if (!bb || id == 0) return;

    for (int i = 0; i < CARBON_BB_MAX_SUBSCRIPTIONS; i++) {
        if (bb->subscriptions[i].used && bb->subscriptions[i].id == id) {
            bb->subscriptions[i].used = false;
            return;
        }
    }
}

/*============================================================================
 * Turn Management
 *============================================================================*/

void carbon_blackboard_set_turn(Carbon_Blackboard *bb, int32_t turn) {
    if (bb) {
        bb->current_turn = turn;
    }
}

int32_t carbon_blackboard_get_turn(const Carbon_Blackboard *bb) {
    return bb ? bb->current_turn : 0;
}

void carbon_blackboard_update(Carbon_Blackboard *bb) {
    if (!bb) return;

    /* Update reservations */
    for (int i = bb->reservation_count - 1; i >= 0; i--) {
        Carbon_BBReservation *res = &bb->reservations[i];
        if (res->turns_remaining > 0) {
            res->turns_remaining--;
            if (res->turns_remaining == 0) {
                /* Expired - remove */
                bb->reservations[i] = bb->reservations[--bb->reservation_count];
            }
        }
    }

    /* Update plans */
    for (int i = bb->plan_count - 1; i >= 0; i--) {
        Carbon_BBPlan *plan = &bb->plans[i];
        if (plan->turns_remaining > 0) {
            plan->turns_remaining--;
            if (plan->turns_remaining == 0) {
                /* Expired - remove */
                bb->plans[i] = bb->plans[--bb->plan_count];
            }
        }
    }
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

int carbon_blackboard_count(const Carbon_Blackboard *bb) {
    return bb ? bb->entry_count : 0;
}

int carbon_blackboard_get_keys(const Carbon_Blackboard *bb, const char **out_keys, int max) {
    if (!bb || !out_keys || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < CARBON_BB_MAX_ENTRIES && count < max; i++) {
        if (bb->entries[i].used) {
            out_keys[count++] = bb->entries[i].key;
        }
    }
    return count;
}

void carbon_blackboard_copy(Carbon_Blackboard *dest, const Carbon_Blackboard *src) {
    if (!dest || !src) return;

    carbon_blackboard_clear(dest);

    for (int i = 0; i < CARBON_BB_MAX_ENTRIES; i++) {
        if (src->entries[i].used) {
            dest->entries[i] = src->entries[i];
        }
    }
    dest->entry_count = src->entry_count;
}

void carbon_blackboard_merge(Carbon_Blackboard *dest, const Carbon_Blackboard *src) {
    if (!dest || !src) return;

    for (int i = 0; i < CARBON_BB_MAX_ENTRIES; i++) {
        if (src->entries[i].used) {
            const BBEntry *src_entry = &src->entries[i];
            BBEntry *dest_entry = get_or_create_entry(dest, src_entry->key);
            if (dest_entry) {
                dest_entry->value = src_entry->value;
            }
        }
    }
}
