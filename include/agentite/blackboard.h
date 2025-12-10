#ifndef AGENTITE_BLACKBOARD_H
#define AGENTITE_BLACKBOARD_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Carbon Shared Blackboard System
 *
 * Cross-system communication and data sharing without direct coupling.
 * Provides key-value storage, resource reservations, plan publication,
 * and decision history tracking.
 *
 * Usage:
 *   // Create blackboard
 *   Agentite_Blackboard *bb = agentite_blackboard_create();
 *
 *   // Store values
 *   agentite_blackboard_set_int(bb, "threat_level", 75);
 *   agentite_blackboard_set_float(bb, "resources_ratio", 1.2f);
 *   agentite_blackboard_set_ptr(bb, "primary_target", enemy_entity);
 *
 *   // Reserve resources (prevents double-spending)
 *   if (agentite_blackboard_reserve(bb, "gold", 500, "military_track")) {
 *       // Resource reserved for military use
 *   }
 *
 *   // Publish plans for conflict avoidance
 *   agentite_blackboard_publish_plan(bb, "military", "Attack sector 7");
 *   if (agentite_blackboard_has_conflicting_plan(bb, "sector_7")) {
 *       // Another system has plans for this target
 *   }
 *
 *   // Log decisions for history
 *   agentite_blackboard_log(bb, "Decided to expand to region %d", region_id);
 *
 *   // Cleanup
 *   agentite_blackboard_destroy(bb);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_BB_MAX_ENTRIES       64    /* Maximum key-value entries */
#define AGENTITE_BB_MAX_KEY_LEN       32    /* Maximum key length */
#define AGENTITE_BB_MAX_STRING_LEN    128   /* Maximum string value length */
#define AGENTITE_BB_MAX_RESERVATIONS  16    /* Maximum concurrent reservations */
#define AGENTITE_BB_MAX_PLANS         8     /* Maximum published plans */
#define AGENTITE_BB_MAX_HISTORY       32    /* History buffer size */
#define AGENTITE_BB_HISTORY_ENTRY_LEN 128   /* Maximum history entry length */

/*============================================================================
 * Value Types
 *============================================================================*/

/**
 * Blackboard value types
 */
typedef enum Agentite_BBValueType {
    AGENTITE_BB_TYPE_NONE = 0,
    AGENTITE_BB_TYPE_INT,
    AGENTITE_BB_TYPE_INT64,
    AGENTITE_BB_TYPE_FLOAT,
    AGENTITE_BB_TYPE_DOUBLE,
    AGENTITE_BB_TYPE_BOOL,
    AGENTITE_BB_TYPE_STRING,
    AGENTITE_BB_TYPE_PTR,
    AGENTITE_BB_TYPE_VEC2,
    AGENTITE_BB_TYPE_VEC3,
} Agentite_BBValueType;

/**
 * Blackboard value union
 */
typedef struct Agentite_BBValue {
    Agentite_BBValueType type;
    union {
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        bool b;
        char str[AGENTITE_BB_MAX_STRING_LEN];
        void *ptr;
        float vec2[2];
        float vec3[3];
    };
} Agentite_BBValue;

/**
 * Resource reservation entry
 */
typedef struct Agentite_BBReservation {
    char resource[AGENTITE_BB_MAX_KEY_LEN];
    char owner[AGENTITE_BB_MAX_KEY_LEN];
    int32_t amount;
    int turns_remaining;    /* -1 = indefinite */
} Agentite_BBReservation;

/**
 * Published plan entry
 */
typedef struct Agentite_BBPlan {
    char owner[AGENTITE_BB_MAX_KEY_LEN];
    char description[AGENTITE_BB_MAX_STRING_LEN];
    char target[AGENTITE_BB_MAX_KEY_LEN];  /* Resource/target this plan affects */
    int turns_remaining;    /* -1 = indefinite */
    bool active;
} Agentite_BBPlan;

/**
 * History entry
 */
typedef struct Agentite_BBHistoryEntry {
    char text[AGENTITE_BB_HISTORY_ENTRY_LEN];
    int32_t turn;
    uint32_t timestamp;     /* Monotonic counter */
} Agentite_BBHistoryEntry;

/*============================================================================
 * Blackboard System (forward declaration)
 *============================================================================*/

typedef struct Agentite_Blackboard Agentite_Blackboard;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Value change callback
 *
 * @param bb      Blackboard instance
 * @param key     Key that changed
 * @param old_val Previous value (NULL if new key)
 * @param new_val New value
 * @param userdata User context
 */
typedef void (*Agentite_BBChangeCallback)(Agentite_Blackboard *bb,
                                         const char *key,
                                         const Agentite_BBValue *old_val,
                                         const Agentite_BBValue *new_val,
                                         void *userdata);

/**
 * Create a new blackboard.
 *
 * @return New blackboard or NULL on failure
 */
Agentite_Blackboard *agentite_blackboard_create(void);

/**
 * Destroy a blackboard and free resources.
 *
 * @param bb Blackboard to destroy
 */
void agentite_blackboard_destroy(Agentite_Blackboard *bb);

/**
 * Clear all entries from the blackboard.
 *
 * @param bb Blackboard to clear
 */
void agentite_blackboard_clear(Agentite_Blackboard *bb);

/*============================================================================
 * Value Storage
 *============================================================================*/

/**
 * Set an integer value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value Value to store
 */
void agentite_blackboard_set_int(Agentite_Blackboard *bb, const char *key, int32_t value);

/**
 * Set a 64-bit integer value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value Value to store
 */
void agentite_blackboard_set_int64(Agentite_Blackboard *bb, const char *key, int64_t value);

/**
 * Set a float value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value Value to store
 */
void agentite_blackboard_set_float(Agentite_Blackboard *bb, const char *key, float value);

/**
 * Set a double value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value Value to store
 */
void agentite_blackboard_set_double(Agentite_Blackboard *bb, const char *key, double value);

/**
 * Set a boolean value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value Value to store
 */
void agentite_blackboard_set_bool(Agentite_Blackboard *bb, const char *key, bool value);

/**
 * Set a string value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value String to store (copied, truncated if too long)
 */
void agentite_blackboard_set_string(Agentite_Blackboard *bb, const char *key, const char *value);

/**
 * Set a pointer value (not owned, not freed).
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value Pointer to store
 */
void agentite_blackboard_set_ptr(Agentite_Blackboard *bb, const char *key, void *value);

/**
 * Set a 2D vector value.
 *
 * @param bb Blackboard
 * @param key Key name
 * @param x   X component
 * @param y   Y component
 */
void agentite_blackboard_set_vec2(Agentite_Blackboard *bb, const char *key, float x, float y);

/**
 * Set a 3D vector value.
 *
 * @param bb Blackboard
 * @param key Key name
 * @param x   X component
 * @param y   Y component
 * @param z   Z component
 */
void agentite_blackboard_set_vec3(Agentite_Blackboard *bb, const char *key, float x, float y, float z);

/*============================================================================
 * Value Retrieval
 *============================================================================*/

/**
 * Check if a key exists.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return true if key exists
 */
bool agentite_blackboard_has(const Agentite_Blackboard *bb, const char *key);

/**
 * Get the type of a value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value type or AGENTITE_BB_TYPE_NONE if not found
 */
Agentite_BBValueType agentite_blackboard_get_type(const Agentite_Blackboard *bb, const char *key);

/**
 * Get an integer value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value or 0 if not found
 */
int32_t agentite_blackboard_get_int(const Agentite_Blackboard *bb, const char *key);

/**
 * Get a 64-bit integer value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value or 0 if not found
 */
int64_t agentite_blackboard_get_int64(const Agentite_Blackboard *bb, const char *key);

/**
 * Get a float value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value or 0.0f if not found
 */
float agentite_blackboard_get_float(const Agentite_Blackboard *bb, const char *key);

/**
 * Get a double value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value or 0.0 if not found
 */
double agentite_blackboard_get_double(const Agentite_Blackboard *bb, const char *key);

/**
 * Get a boolean value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value or false if not found
 */
bool agentite_blackboard_get_bool(const Agentite_Blackboard *bb, const char *key);

/**
 * Get a string value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return String pointer or NULL if not found (do not free)
 */
const char *agentite_blackboard_get_string(const Agentite_Blackboard *bb, const char *key);

/**
 * Get a pointer value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Pointer or NULL if not found
 */
void *agentite_blackboard_get_ptr(const Agentite_Blackboard *bb, const char *key);

/**
 * Get a 2D vector value.
 *
 * @param bb   Blackboard
 * @param key  Key name
 * @param out_x Output X (NULL to skip)
 * @param out_y Output Y (NULL to skip)
 * @return true if found
 */
bool agentite_blackboard_get_vec2(const Agentite_Blackboard *bb, const char *key,
                                 float *out_x, float *out_y);

/**
 * Get a 3D vector value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param out_x Output X (NULL to skip)
 * @param out_y Output Y (NULL to skip)
 * @param out_z Output Z (NULL to skip)
 * @return true if found
 */
bool agentite_blackboard_get_vec3(const Agentite_Blackboard *bb, const char *key,
                                 float *out_x, float *out_y, float *out_z);

/**
 * Get the raw value struct.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value pointer or NULL if not found (do not modify)
 */
const Agentite_BBValue *agentite_blackboard_get_value(const Agentite_Blackboard *bb, const char *key);

/**
 * Remove a key from the blackboard.
 *
 * @param bb  Blackboard
 * @param key Key to remove
 * @return true if key existed and was removed
 */
bool agentite_blackboard_remove(Agentite_Blackboard *bb, const char *key);

/*============================================================================
 * Integer Operations
 *============================================================================*/

/**
 * Increment an integer value.
 *
 * @param bb     Blackboard
 * @param key    Key name
 * @param amount Amount to add (can be negative)
 * @return New value (creates with 0 if not exists)
 */
int32_t agentite_blackboard_inc_int(Agentite_Blackboard *bb, const char *key, int32_t amount);

/**
 * Get integer with default value.
 *
 * @param bb           Blackboard
 * @param key          Key name
 * @param default_val  Value to return if not found
 * @return Value or default
 */
int32_t agentite_blackboard_get_int_or(const Agentite_Blackboard *bb, const char *key,
                                      int32_t default_val);

/**
 * Get float with default value.
 *
 * @param bb           Blackboard
 * @param key          Key name
 * @param default_val  Value to return if not found
 * @return Value or default
 */
float agentite_blackboard_get_float_or(const Agentite_Blackboard *bb, const char *key,
                                      float default_val);

/*============================================================================
 * Resource Reservations
 *============================================================================*/

/**
 * Reserve a resource amount.
 * Prevents double-spending by multiple AI tracks.
 *
 * @param bb       Blackboard
 * @param resource Resource identifier
 * @param amount   Amount to reserve
 * @param owner    Owner identifier (track name, system name)
 * @return true if reservation succeeded
 */
bool agentite_blackboard_reserve(Agentite_Blackboard *bb, const char *resource,
                                int32_t amount, const char *owner);

/**
 * Reserve with expiration.
 *
 * @param bb       Blackboard
 * @param resource Resource identifier
 * @param amount   Amount to reserve
 * @param owner    Owner identifier
 * @param turns    Turns until expiration (-1 = indefinite)
 * @return true if reservation succeeded
 */
bool agentite_blackboard_reserve_ex(Agentite_Blackboard *bb, const char *resource,
                                   int32_t amount, const char *owner, int turns);

/**
 * Release a reservation.
 *
 * @param bb       Blackboard
 * @param resource Resource identifier
 * @param owner    Owner who made the reservation
 */
void agentite_blackboard_release(Agentite_Blackboard *bb, const char *resource,
                                const char *owner);

/**
 * Release all reservations by an owner.
 *
 * @param bb    Blackboard
 * @param owner Owner identifier
 */
void agentite_blackboard_release_all(Agentite_Blackboard *bb, const char *owner);

/**
 * Get total reserved amount for a resource.
 *
 * @param bb       Blackboard
 * @param resource Resource identifier
 * @return Total reserved amount
 */
int32_t agentite_blackboard_get_reserved(const Agentite_Blackboard *bb, const char *resource);

/**
 * Get available amount (total - reserved).
 *
 * @param bb       Blackboard
 * @param resource Resource key in blackboard
 * @return Available amount (may be negative if over-reserved)
 */
int32_t agentite_blackboard_get_available(const Agentite_Blackboard *bb, const char *resource);

/**
 * Check if a resource has any reservations.
 *
 * @param bb       Blackboard
 * @param resource Resource identifier
 * @return true if any reservations exist
 */
bool agentite_blackboard_has_reservation(const Agentite_Blackboard *bb, const char *resource);

/**
 * Get reservation by owner.
 *
 * @param bb       Blackboard
 * @param resource Resource identifier
 * @param owner    Owner identifier
 * @return Reserved amount or 0 if no reservation
 */
int32_t agentite_blackboard_get_reservation(const Agentite_Blackboard *bb, const char *resource,
                                           const char *owner);

/*============================================================================
 * Plan Publication
 *============================================================================*/

/**
 * Publish a plan for conflict avoidance.
 *
 * @param bb          Blackboard
 * @param owner       Plan owner (track name, system name)
 * @param description Plan description
 */
void agentite_blackboard_publish_plan(Agentite_Blackboard *bb, const char *owner,
                                     const char *description);

/**
 * Publish a plan with target and expiration.
 *
 * @param bb          Blackboard
 * @param owner       Plan owner
 * @param description Plan description
 * @param target      Resource/target this plan affects
 * @param turns       Turns until expiration (-1 = indefinite)
 */
void agentite_blackboard_publish_plan_ex(Agentite_Blackboard *bb, const char *owner,
                                        const char *description, const char *target,
                                        int turns);

/**
 * Cancel a published plan.
 *
 * @param bb    Blackboard
 * @param owner Plan owner
 */
void agentite_blackboard_cancel_plan(Agentite_Blackboard *bb, const char *owner);

/**
 * Check if any plan conflicts with a target.
 *
 * @param bb     Blackboard
 * @param target Resource or target to check
 * @return true if a conflicting plan exists
 */
bool agentite_blackboard_has_conflicting_plan(const Agentite_Blackboard *bb, const char *target);

/**
 * Get plan by owner.
 *
 * @param bb    Blackboard
 * @param owner Plan owner
 * @return Plan or NULL if not found
 */
const Agentite_BBPlan *agentite_blackboard_get_plan(const Agentite_Blackboard *bb, const char *owner);

/**
 * Get all active plans.
 *
 * @param bb        Blackboard
 * @param out_plans Output array
 * @param max       Maximum plans to return
 * @return Number of active plans
 */
int agentite_blackboard_get_all_plans(const Agentite_Blackboard *bb,
                                     const Agentite_BBPlan **out_plans, int max);

/*============================================================================
 * History / Decision Log
 *============================================================================*/

/**
 * Log an entry to the history buffer (circular).
 *
 * @param bb  Blackboard
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void agentite_blackboard_log(Agentite_Blackboard *bb, const char *fmt, ...);

/**
 * Log with explicit turn number.
 *
 * @param bb   Blackboard
 * @param turn Game turn number
 * @param fmt  Printf-style format string
 * @param ...  Format arguments
 */
void agentite_blackboard_log_turn(Agentite_Blackboard *bb, int32_t turn, const char *fmt, ...);

/**
 * Get history entries.
 *
 * @param bb          Blackboard
 * @param out_entries Output array of entry pointers
 * @param max         Maximum entries to return
 * @return Number of entries returned (newest first)
 */
int agentite_blackboard_get_history(const Agentite_Blackboard *bb,
                                   const Agentite_BBHistoryEntry **out_entries, int max);

/**
 * Get history entries as strings.
 *
 * @param bb          Blackboard
 * @param out_strings Output array of string pointers
 * @param max         Maximum strings to return
 * @return Number of strings returned (newest first)
 */
int agentite_blackboard_get_history_strings(const Agentite_Blackboard *bb,
                                           const char **out_strings, int max);

/**
 * Clear history buffer.
 *
 * @param bb Blackboard
 */
void agentite_blackboard_clear_history(Agentite_Blackboard *bb);

/**
 * Get history count.
 *
 * @param bb Blackboard
 * @return Number of entries in history
 */
int agentite_blackboard_get_history_count(const Agentite_Blackboard *bb);

/*============================================================================
 * Subscriptions
 *============================================================================*/

/**
 * Subscribe to value changes for a specific key.
 *
 * @param bb       Blackboard
 * @param key      Key to watch (or NULL for all keys)
 * @param callback Callback function
 * @param userdata User context
 * @return Subscription ID or 0 on failure
 */
uint32_t agentite_blackboard_subscribe(Agentite_Blackboard *bb, const char *key,
                                      Agentite_BBChangeCallback callback, void *userdata);

/**
 * Unsubscribe from changes.
 *
 * @param bb Blackboard
 * @param id Subscription ID
 */
void agentite_blackboard_unsubscribe(Agentite_Blackboard *bb, uint32_t id);

/*============================================================================
 * Turn Management
 *============================================================================*/

/**
 * Set current turn (for logging).
 *
 * @param bb   Blackboard
 * @param turn Current turn number
 */
void agentite_blackboard_set_turn(Agentite_Blackboard *bb, int32_t turn);

/**
 * Get current turn.
 *
 * @param bb Blackboard
 * @return Current turn number
 */
int32_t agentite_blackboard_get_turn(const Agentite_Blackboard *bb);

/**
 * Update reservations and plans (call each turn).
 * Decrements turns_remaining and removes expired entries.
 *
 * @param bb Blackboard
 */
void agentite_blackboard_update(Agentite_Blackboard *bb);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get number of entries in blackboard.
 *
 * @param bb Blackboard
 * @return Entry count
 */
int agentite_blackboard_count(const Agentite_Blackboard *bb);

/**
 * Get all keys.
 *
 * @param bb       Blackboard
 * @param out_keys Output array of key pointers
 * @param max      Maximum keys to return
 * @return Number of keys returned
 */
int agentite_blackboard_get_keys(const Agentite_Blackboard *bb, const char **out_keys, int max);

/**
 * Copy values from one blackboard to another.
 *
 * @param dest Destination blackboard
 * @param src  Source blackboard
 */
void agentite_blackboard_copy(Agentite_Blackboard *dest, const Agentite_Blackboard *src);

/**
 * Merge values from source into destination.
 * Existing keys in dest are overwritten.
 *
 * @param dest Destination blackboard
 * @param src  Source blackboard
 */
void agentite_blackboard_merge(Agentite_Blackboard *dest, const Agentite_Blackboard *src);

#endif /* AGENTITE_BLACKBOARD_H */
