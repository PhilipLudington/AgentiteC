#ifndef CARBON_BLACKBOARD_H
#define CARBON_BLACKBOARD_H

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
 *   Carbon_Blackboard *bb = carbon_blackboard_create();
 *
 *   // Store values
 *   carbon_blackboard_set_int(bb, "threat_level", 75);
 *   carbon_blackboard_set_float(bb, "resources_ratio", 1.2f);
 *   carbon_blackboard_set_ptr(bb, "primary_target", enemy_entity);
 *
 *   // Reserve resources (prevents double-spending)
 *   if (carbon_blackboard_reserve(bb, "gold", 500, "military_track")) {
 *       // Resource reserved for military use
 *   }
 *
 *   // Publish plans for conflict avoidance
 *   carbon_blackboard_publish_plan(bb, "military", "Attack sector 7");
 *   if (carbon_blackboard_has_conflicting_plan(bb, "sector_7")) {
 *       // Another system has plans for this target
 *   }
 *
 *   // Log decisions for history
 *   carbon_blackboard_log(bb, "Decided to expand to region %d", region_id);
 *
 *   // Cleanup
 *   carbon_blackboard_destroy(bb);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_BB_MAX_ENTRIES       64    /* Maximum key-value entries */
#define CARBON_BB_MAX_KEY_LEN       32    /* Maximum key length */
#define CARBON_BB_MAX_STRING_LEN    128   /* Maximum string value length */
#define CARBON_BB_MAX_RESERVATIONS  16    /* Maximum concurrent reservations */
#define CARBON_BB_MAX_PLANS         8     /* Maximum published plans */
#define CARBON_BB_MAX_HISTORY       32    /* History buffer size */
#define CARBON_BB_HISTORY_ENTRY_LEN 128   /* Maximum history entry length */

/*============================================================================
 * Value Types
 *============================================================================*/

/**
 * Blackboard value types
 */
typedef enum Carbon_BBValueType {
    CARBON_BB_TYPE_NONE = 0,
    CARBON_BB_TYPE_INT,
    CARBON_BB_TYPE_INT64,
    CARBON_BB_TYPE_FLOAT,
    CARBON_BB_TYPE_DOUBLE,
    CARBON_BB_TYPE_BOOL,
    CARBON_BB_TYPE_STRING,
    CARBON_BB_TYPE_PTR,
    CARBON_BB_TYPE_VEC2,
    CARBON_BB_TYPE_VEC3,
} Carbon_BBValueType;

/**
 * Blackboard value union
 */
typedef struct Carbon_BBValue {
    Carbon_BBValueType type;
    union {
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        bool b;
        char str[CARBON_BB_MAX_STRING_LEN];
        void *ptr;
        float vec2[2];
        float vec3[3];
    };
} Carbon_BBValue;

/**
 * Resource reservation entry
 */
typedef struct Carbon_BBReservation {
    char resource[CARBON_BB_MAX_KEY_LEN];
    char owner[CARBON_BB_MAX_KEY_LEN];
    int32_t amount;
    int turns_remaining;    /* -1 = indefinite */
} Carbon_BBReservation;

/**
 * Published plan entry
 */
typedef struct Carbon_BBPlan {
    char owner[CARBON_BB_MAX_KEY_LEN];
    char description[CARBON_BB_MAX_STRING_LEN];
    char target[CARBON_BB_MAX_KEY_LEN];  /* Resource/target this plan affects */
    int turns_remaining;    /* -1 = indefinite */
    bool active;
} Carbon_BBPlan;

/**
 * History entry
 */
typedef struct Carbon_BBHistoryEntry {
    char text[CARBON_BB_HISTORY_ENTRY_LEN];
    int32_t turn;
    uint32_t timestamp;     /* Monotonic counter */
} Carbon_BBHistoryEntry;

/*============================================================================
 * Blackboard System (forward declaration)
 *============================================================================*/

typedef struct Carbon_Blackboard Carbon_Blackboard;

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
typedef void (*Carbon_BBChangeCallback)(Carbon_Blackboard *bb,
                                         const char *key,
                                         const Carbon_BBValue *old_val,
                                         const Carbon_BBValue *new_val,
                                         void *userdata);

/**
 * Create a new blackboard.
 *
 * @return New blackboard or NULL on failure
 */
Carbon_Blackboard *carbon_blackboard_create(void);

/**
 * Destroy a blackboard and free resources.
 *
 * @param bb Blackboard to destroy
 */
void carbon_blackboard_destroy(Carbon_Blackboard *bb);

/**
 * Clear all entries from the blackboard.
 *
 * @param bb Blackboard to clear
 */
void carbon_blackboard_clear(Carbon_Blackboard *bb);

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
void carbon_blackboard_set_int(Carbon_Blackboard *bb, const char *key, int32_t value);

/**
 * Set a 64-bit integer value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value Value to store
 */
void carbon_blackboard_set_int64(Carbon_Blackboard *bb, const char *key, int64_t value);

/**
 * Set a float value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value Value to store
 */
void carbon_blackboard_set_float(Carbon_Blackboard *bb, const char *key, float value);

/**
 * Set a double value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value Value to store
 */
void carbon_blackboard_set_double(Carbon_Blackboard *bb, const char *key, double value);

/**
 * Set a boolean value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value Value to store
 */
void carbon_blackboard_set_bool(Carbon_Blackboard *bb, const char *key, bool value);

/**
 * Set a string value.
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value String to store (copied, truncated if too long)
 */
void carbon_blackboard_set_string(Carbon_Blackboard *bb, const char *key, const char *value);

/**
 * Set a pointer value (not owned, not freed).
 *
 * @param bb    Blackboard
 * @param key   Key name
 * @param value Pointer to store
 */
void carbon_blackboard_set_ptr(Carbon_Blackboard *bb, const char *key, void *value);

/**
 * Set a 2D vector value.
 *
 * @param bb Blackboard
 * @param key Key name
 * @param x   X component
 * @param y   Y component
 */
void carbon_blackboard_set_vec2(Carbon_Blackboard *bb, const char *key, float x, float y);

/**
 * Set a 3D vector value.
 *
 * @param bb Blackboard
 * @param key Key name
 * @param x   X component
 * @param y   Y component
 * @param z   Z component
 */
void carbon_blackboard_set_vec3(Carbon_Blackboard *bb, const char *key, float x, float y, float z);

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
bool carbon_blackboard_has(const Carbon_Blackboard *bb, const char *key);

/**
 * Get the type of a value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value type or CARBON_BB_TYPE_NONE if not found
 */
Carbon_BBValueType carbon_blackboard_get_type(const Carbon_Blackboard *bb, const char *key);

/**
 * Get an integer value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value or 0 if not found
 */
int32_t carbon_blackboard_get_int(const Carbon_Blackboard *bb, const char *key);

/**
 * Get a 64-bit integer value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value or 0 if not found
 */
int64_t carbon_blackboard_get_int64(const Carbon_Blackboard *bb, const char *key);

/**
 * Get a float value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value or 0.0f if not found
 */
float carbon_blackboard_get_float(const Carbon_Blackboard *bb, const char *key);

/**
 * Get a double value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value or 0.0 if not found
 */
double carbon_blackboard_get_double(const Carbon_Blackboard *bb, const char *key);

/**
 * Get a boolean value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value or false if not found
 */
bool carbon_blackboard_get_bool(const Carbon_Blackboard *bb, const char *key);

/**
 * Get a string value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return String pointer or NULL if not found (do not free)
 */
const char *carbon_blackboard_get_string(const Carbon_Blackboard *bb, const char *key);

/**
 * Get a pointer value.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Pointer or NULL if not found
 */
void *carbon_blackboard_get_ptr(const Carbon_Blackboard *bb, const char *key);

/**
 * Get a 2D vector value.
 *
 * @param bb   Blackboard
 * @param key  Key name
 * @param out_x Output X (NULL to skip)
 * @param out_y Output Y (NULL to skip)
 * @return true if found
 */
bool carbon_blackboard_get_vec2(const Carbon_Blackboard *bb, const char *key,
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
bool carbon_blackboard_get_vec3(const Carbon_Blackboard *bb, const char *key,
                                 float *out_x, float *out_y, float *out_z);

/**
 * Get the raw value struct.
 *
 * @param bb  Blackboard
 * @param key Key name
 * @return Value pointer or NULL if not found (do not modify)
 */
const Carbon_BBValue *carbon_blackboard_get_value(const Carbon_Blackboard *bb, const char *key);

/**
 * Remove a key from the blackboard.
 *
 * @param bb  Blackboard
 * @param key Key to remove
 * @return true if key existed and was removed
 */
bool carbon_blackboard_remove(Carbon_Blackboard *bb, const char *key);

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
int32_t carbon_blackboard_inc_int(Carbon_Blackboard *bb, const char *key, int32_t amount);

/**
 * Get integer with default value.
 *
 * @param bb           Blackboard
 * @param key          Key name
 * @param default_val  Value to return if not found
 * @return Value or default
 */
int32_t carbon_blackboard_get_int_or(const Carbon_Blackboard *bb, const char *key,
                                      int32_t default_val);

/**
 * Get float with default value.
 *
 * @param bb           Blackboard
 * @param key          Key name
 * @param default_val  Value to return if not found
 * @return Value or default
 */
float carbon_blackboard_get_float_or(const Carbon_Blackboard *bb, const char *key,
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
bool carbon_blackboard_reserve(Carbon_Blackboard *bb, const char *resource,
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
bool carbon_blackboard_reserve_ex(Carbon_Blackboard *bb, const char *resource,
                                   int32_t amount, const char *owner, int turns);

/**
 * Release a reservation.
 *
 * @param bb       Blackboard
 * @param resource Resource identifier
 * @param owner    Owner who made the reservation
 */
void carbon_blackboard_release(Carbon_Blackboard *bb, const char *resource,
                                const char *owner);

/**
 * Release all reservations by an owner.
 *
 * @param bb    Blackboard
 * @param owner Owner identifier
 */
void carbon_blackboard_release_all(Carbon_Blackboard *bb, const char *owner);

/**
 * Get total reserved amount for a resource.
 *
 * @param bb       Blackboard
 * @param resource Resource identifier
 * @return Total reserved amount
 */
int32_t carbon_blackboard_get_reserved(const Carbon_Blackboard *bb, const char *resource);

/**
 * Get available amount (total - reserved).
 *
 * @param bb       Blackboard
 * @param resource Resource key in blackboard
 * @return Available amount (may be negative if over-reserved)
 */
int32_t carbon_blackboard_get_available(const Carbon_Blackboard *bb, const char *resource);

/**
 * Check if a resource has any reservations.
 *
 * @param bb       Blackboard
 * @param resource Resource identifier
 * @return true if any reservations exist
 */
bool carbon_blackboard_has_reservation(const Carbon_Blackboard *bb, const char *resource);

/**
 * Get reservation by owner.
 *
 * @param bb       Blackboard
 * @param resource Resource identifier
 * @param owner    Owner identifier
 * @return Reserved amount or 0 if no reservation
 */
int32_t carbon_blackboard_get_reservation(const Carbon_Blackboard *bb, const char *resource,
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
void carbon_blackboard_publish_plan(Carbon_Blackboard *bb, const char *owner,
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
void carbon_blackboard_publish_plan_ex(Carbon_Blackboard *bb, const char *owner,
                                        const char *description, const char *target,
                                        int turns);

/**
 * Cancel a published plan.
 *
 * @param bb    Blackboard
 * @param owner Plan owner
 */
void carbon_blackboard_cancel_plan(Carbon_Blackboard *bb, const char *owner);

/**
 * Check if any plan conflicts with a target.
 *
 * @param bb     Blackboard
 * @param target Resource or target to check
 * @return true if a conflicting plan exists
 */
bool carbon_blackboard_has_conflicting_plan(const Carbon_Blackboard *bb, const char *target);

/**
 * Get plan by owner.
 *
 * @param bb    Blackboard
 * @param owner Plan owner
 * @return Plan or NULL if not found
 */
const Carbon_BBPlan *carbon_blackboard_get_plan(const Carbon_Blackboard *bb, const char *owner);

/**
 * Get all active plans.
 *
 * @param bb        Blackboard
 * @param out_plans Output array
 * @param max       Maximum plans to return
 * @return Number of active plans
 */
int carbon_blackboard_get_all_plans(const Carbon_Blackboard *bb,
                                     const Carbon_BBPlan **out_plans, int max);

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
void carbon_blackboard_log(Carbon_Blackboard *bb, const char *fmt, ...);

/**
 * Log with explicit turn number.
 *
 * @param bb   Blackboard
 * @param turn Game turn number
 * @param fmt  Printf-style format string
 * @param ...  Format arguments
 */
void carbon_blackboard_log_turn(Carbon_Blackboard *bb, int32_t turn, const char *fmt, ...);

/**
 * Get history entries.
 *
 * @param bb          Blackboard
 * @param out_entries Output array of entry pointers
 * @param max         Maximum entries to return
 * @return Number of entries returned (newest first)
 */
int carbon_blackboard_get_history(const Carbon_Blackboard *bb,
                                   const Carbon_BBHistoryEntry **out_entries, int max);

/**
 * Get history entries as strings.
 *
 * @param bb          Blackboard
 * @param out_strings Output array of string pointers
 * @param max         Maximum strings to return
 * @return Number of strings returned (newest first)
 */
int carbon_blackboard_get_history_strings(const Carbon_Blackboard *bb,
                                           const char **out_strings, int max);

/**
 * Clear history buffer.
 *
 * @param bb Blackboard
 */
void carbon_blackboard_clear_history(Carbon_Blackboard *bb);

/**
 * Get history count.
 *
 * @param bb Blackboard
 * @return Number of entries in history
 */
int carbon_blackboard_get_history_count(const Carbon_Blackboard *bb);

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
uint32_t carbon_blackboard_subscribe(Carbon_Blackboard *bb, const char *key,
                                      Carbon_BBChangeCallback callback, void *userdata);

/**
 * Unsubscribe from changes.
 *
 * @param bb Blackboard
 * @param id Subscription ID
 */
void carbon_blackboard_unsubscribe(Carbon_Blackboard *bb, uint32_t id);

/*============================================================================
 * Turn Management
 *============================================================================*/

/**
 * Set current turn (for logging).
 *
 * @param bb   Blackboard
 * @param turn Current turn number
 */
void carbon_blackboard_set_turn(Carbon_Blackboard *bb, int32_t turn);

/**
 * Get current turn.
 *
 * @param bb Blackboard
 * @return Current turn number
 */
int32_t carbon_blackboard_get_turn(const Carbon_Blackboard *bb);

/**
 * Update reservations and plans (call each turn).
 * Decrements turns_remaining and removes expired entries.
 *
 * @param bb Blackboard
 */
void carbon_blackboard_update(Carbon_Blackboard *bb);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get number of entries in blackboard.
 *
 * @param bb Blackboard
 * @return Entry count
 */
int carbon_blackboard_count(const Carbon_Blackboard *bb);

/**
 * Get all keys.
 *
 * @param bb       Blackboard
 * @param out_keys Output array of key pointers
 * @param max      Maximum keys to return
 * @return Number of keys returned
 */
int carbon_blackboard_get_keys(const Carbon_Blackboard *bb, const char **out_keys, int max);

/**
 * Copy values from one blackboard to another.
 *
 * @param dest Destination blackboard
 * @param src  Source blackboard
 */
void carbon_blackboard_copy(Carbon_Blackboard *dest, const Carbon_Blackboard *src);

/**
 * Merge values from source into destination.
 * Existing keys in dest are overwritten.
 *
 * @param dest Destination blackboard
 * @param src  Source blackboard
 */
void carbon_blackboard_merge(Carbon_Blackboard *dest, const Carbon_Blackboard *src);

#endif /* CARBON_BLACKBOARD_H */
