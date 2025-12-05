#ifndef CARBON_VIEWMODEL_H
#define CARBON_VIEWMODEL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Carbon View Model System
 *
 * Separates game state from UI presentation with observable values,
 * change detection, and event-driven updates. Provides a clean interface
 * between game logic and UI rendering.
 *
 * Usage:
 *   // Create view model
 *   Carbon_ViewModel *vm = carbon_vm_create();
 *
 *   // Define observables
 *   uint32_t health_id = carbon_vm_define_int(vm, "player_health", 100);
 *   uint32_t gold_id = carbon_vm_define_int(vm, "gold", 0);
 *   uint32_t name_id = carbon_vm_define_string(vm, "player_name", "Hero");
 *
 *   // Subscribe to changes
 *   carbon_vm_subscribe(vm, health_id, on_health_changed, ui);
 *
 *   // Update values (triggers callbacks if changed)
 *   carbon_vm_set_int(vm, health_id, 75);
 *
 *   // Batch updates
 *   carbon_vm_begin_batch(vm);
 *   carbon_vm_set_int(vm, health_id, 50);
 *   carbon_vm_set_int(vm, gold_id, 100);
 *   carbon_vm_commit_batch(vm);  // Triggers all callbacks once
 *
 *   // Cleanup
 *   carbon_vm_destroy(vm);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_VM_MAX_OBSERVABLES    256   /* Maximum observable values */
#define CARBON_VM_MAX_LISTENERS      64    /* Maximum listeners per observable */
#define CARBON_VM_MAX_STRING_LENGTH  256   /* Maximum string value length */
#define CARBON_VM_INVALID_ID         0     /* Invalid observable ID */

/*============================================================================
 * Observable Types
 *============================================================================*/

/**
 * Types of observable values
 */
typedef enum Carbon_VMType {
    CARBON_VM_TYPE_NONE = 0,
    CARBON_VM_TYPE_INT,         /* int32_t */
    CARBON_VM_TYPE_INT64,       /* int64_t */
    CARBON_VM_TYPE_FLOAT,       /* float */
    CARBON_VM_TYPE_DOUBLE,      /* double */
    CARBON_VM_TYPE_BOOL,        /* bool */
    CARBON_VM_TYPE_STRING,      /* char[] (copied) */
    CARBON_VM_TYPE_POINTER,     /* void* (not owned) */
    CARBON_VM_TYPE_VEC2,        /* float[2] */
    CARBON_VM_TYPE_VEC3,        /* float[3] */
    CARBON_VM_TYPE_VEC4,        /* float[4] / color */
    CARBON_VM_TYPE_COUNT
} Carbon_VMType;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Vector types for convenience
 */
typedef struct Carbon_VMVec2 {
    float x, y;
} Carbon_VMVec2;

typedef struct Carbon_VMVec3 {
    float x, y, z;
} Carbon_VMVec3;

typedef struct Carbon_VMVec4 {
    float x, y, z, w;
} Carbon_VMVec4;

/**
 * Observable value (tagged union)
 */
typedef struct Carbon_VMValue {
    Carbon_VMType type;
    union {
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        bool b;
        char str[CARBON_VM_MAX_STRING_LENGTH];
        void *ptr;
        Carbon_VMVec2 vec2;
        Carbon_VMVec3 vec3;
        Carbon_VMVec4 vec4;
    };
} Carbon_VMValue;

/**
 * Change event data passed to callbacks
 */
typedef struct Carbon_VMChangeEvent {
    uint32_t id;                    /* Observable ID */
    const char *name;               /* Observable name */
    Carbon_VMType type;             /* Value type */
    Carbon_VMValue old_value;       /* Previous value */
    Carbon_VMValue new_value;       /* New value */
} Carbon_VMChangeEvent;

/*============================================================================
 * Callback Types
 *============================================================================*/

typedef struct Carbon_ViewModel Carbon_ViewModel;

/**
 * Callback for value changes.
 *
 * @param vm      View model
 * @param event   Change event with old and new values
 * @param userdata User data passed during subscription
 */
typedef void (*Carbon_VMCallback)(Carbon_ViewModel *vm,
                                   const Carbon_VMChangeEvent *event,
                                   void *userdata);

/**
 * Validator callback (optional).
 * Return false to reject the value change.
 *
 * @param vm        View model
 * @param id        Observable ID
 * @param new_value Proposed new value
 * @param userdata  User data
 * @return true to accept, false to reject
 */
typedef bool (*Carbon_VMValidator)(Carbon_ViewModel *vm,
                                    uint32_t id,
                                    const Carbon_VMValue *new_value,
                                    void *userdata);

/**
 * Formatter callback for string conversion.
 *
 * @param vm      View model
 * @param id      Observable ID
 * @param value   Current value
 * @param buffer  Output buffer
 * @param size    Buffer size
 * @param userdata User data
 * @return Number of characters written
 */
typedef int (*Carbon_VMFormatter)(Carbon_ViewModel *vm,
                                   uint32_t id,
                                   const Carbon_VMValue *value,
                                   char *buffer,
                                   size_t size,
                                   void *userdata);

/*============================================================================
 * View Model Creation/Destruction
 *============================================================================*/

/**
 * Create a new view model.
 *
 * @return New view model or NULL on failure
 */
Carbon_ViewModel *carbon_vm_create(void);

/**
 * Create a view model with event dispatcher integration.
 * Emits CARBON_EVENT_UI_VALUE_CHANGED events on changes.
 *
 * @param events Event dispatcher (can be NULL)
 * @return New view model or NULL on failure
 */
typedef struct Carbon_EventDispatcher Carbon_EventDispatcher;
Carbon_ViewModel *carbon_vm_create_with_events(Carbon_EventDispatcher *events);

/**
 * Destroy a view model and free resources.
 *
 * @param vm View model to destroy
 */
void carbon_vm_destroy(Carbon_ViewModel *vm);

/*============================================================================
 * Observable Definition
 *============================================================================*/

/**
 * Define an integer observable.
 *
 * @param vm      View model
 * @param name    Observable name (for lookup)
 * @param initial Initial value
 * @return Observable ID or CARBON_VM_INVALID_ID on failure
 */
uint32_t carbon_vm_define_int(Carbon_ViewModel *vm, const char *name, int32_t initial);

/**
 * Define a 64-bit integer observable.
 */
uint32_t carbon_vm_define_int64(Carbon_ViewModel *vm, const char *name, int64_t initial);

/**
 * Define a float observable.
 */
uint32_t carbon_vm_define_float(Carbon_ViewModel *vm, const char *name, float initial);

/**
 * Define a double observable.
 */
uint32_t carbon_vm_define_double(Carbon_ViewModel *vm, const char *name, double initial);

/**
 * Define a boolean observable.
 */
uint32_t carbon_vm_define_bool(Carbon_ViewModel *vm, const char *name, bool initial);

/**
 * Define a string observable.
 */
uint32_t carbon_vm_define_string(Carbon_ViewModel *vm, const char *name, const char *initial);

/**
 * Define a pointer observable (not owned by view model).
 */
uint32_t carbon_vm_define_ptr(Carbon_ViewModel *vm, const char *name, void *initial);

/**
 * Define a 2D vector observable.
 */
uint32_t carbon_vm_define_vec2(Carbon_ViewModel *vm, const char *name, float x, float y);

/**
 * Define a 3D vector observable.
 */
uint32_t carbon_vm_define_vec3(Carbon_ViewModel *vm, const char *name, float x, float y, float z);

/**
 * Define a 4D vector/color observable.
 */
uint32_t carbon_vm_define_vec4(Carbon_ViewModel *vm, const char *name,
                                float x, float y, float z, float w);

/**
 * Define a color observable (alias for vec4).
 */
uint32_t carbon_vm_define_color(Carbon_ViewModel *vm, const char *name,
                                 float r, float g, float b, float a);

/*============================================================================
 * Value Setters
 *============================================================================*/

/**
 * Set an integer value.
 * Triggers callbacks if value changed (unless in batch mode).
 *
 * @param vm    View model
 * @param id    Observable ID
 * @param value New value
 * @return true if value was changed
 */
bool carbon_vm_set_int(Carbon_ViewModel *vm, uint32_t id, int32_t value);
bool carbon_vm_set_int64(Carbon_ViewModel *vm, uint32_t id, int64_t value);
bool carbon_vm_set_float(Carbon_ViewModel *vm, uint32_t id, float value);
bool carbon_vm_set_double(Carbon_ViewModel *vm, uint32_t id, double value);
bool carbon_vm_set_bool(Carbon_ViewModel *vm, uint32_t id, bool value);
bool carbon_vm_set_string(Carbon_ViewModel *vm, uint32_t id, const char *value);
bool carbon_vm_set_ptr(Carbon_ViewModel *vm, uint32_t id, void *value);
bool carbon_vm_set_vec2(Carbon_ViewModel *vm, uint32_t id, float x, float y);
bool carbon_vm_set_vec3(Carbon_ViewModel *vm, uint32_t id, float x, float y, float z);
bool carbon_vm_set_vec4(Carbon_ViewModel *vm, uint32_t id, float x, float y, float z, float w);
bool carbon_vm_set_color(Carbon_ViewModel *vm, uint32_t id, float r, float g, float b, float a);

/**
 * Set value from a generic Carbon_VMValue struct.
 */
bool carbon_vm_set_value(Carbon_ViewModel *vm, uint32_t id, const Carbon_VMValue *value);

/*============================================================================
 * Value Getters
 *============================================================================*/

/**
 * Get an integer value.
 *
 * @param vm View model
 * @param id Observable ID
 * @return Value or 0 if invalid
 */
int32_t carbon_vm_get_int(const Carbon_ViewModel *vm, uint32_t id);
int64_t carbon_vm_get_int64(const Carbon_ViewModel *vm, uint32_t id);
float carbon_vm_get_float(const Carbon_ViewModel *vm, uint32_t id);
double carbon_vm_get_double(const Carbon_ViewModel *vm, uint32_t id);
bool carbon_vm_get_bool(const Carbon_ViewModel *vm, uint32_t id);
const char *carbon_vm_get_string(const Carbon_ViewModel *vm, uint32_t id);
void *carbon_vm_get_ptr(const Carbon_ViewModel *vm, uint32_t id);
Carbon_VMVec2 carbon_vm_get_vec2(const Carbon_ViewModel *vm, uint32_t id);
Carbon_VMVec3 carbon_vm_get_vec3(const Carbon_ViewModel *vm, uint32_t id);
Carbon_VMVec4 carbon_vm_get_vec4(const Carbon_ViewModel *vm, uint32_t id);

/**
 * Get the full value struct for an observable.
 *
 * @param vm  View model
 * @param id  Observable ID
 * @param out Output value struct
 * @return true if successful
 */
bool carbon_vm_get_value(const Carbon_ViewModel *vm, uint32_t id, Carbon_VMValue *out);

/*============================================================================
 * Lookup and Query
 *============================================================================*/

/**
 * Find an observable by name.
 *
 * @param vm   View model
 * @param name Observable name
 * @return Observable ID or CARBON_VM_INVALID_ID if not found
 */
uint32_t carbon_vm_find(const Carbon_ViewModel *vm, const char *name);

/**
 * Get the name of an observable.
 *
 * @param vm View model
 * @param id Observable ID
 * @return Name or NULL if invalid
 */
const char *carbon_vm_get_name(const Carbon_ViewModel *vm, uint32_t id);

/**
 * Get the type of an observable.
 *
 * @param vm View model
 * @param id Observable ID
 * @return Type or CARBON_VM_TYPE_NONE if invalid
 */
Carbon_VMType carbon_vm_get_type(const Carbon_ViewModel *vm, uint32_t id);

/**
 * Check if an observable exists.
 *
 * @param vm View model
 * @param id Observable ID
 * @return true if exists
 */
bool carbon_vm_exists(const Carbon_ViewModel *vm, uint32_t id);

/**
 * Get the number of defined observables.
 *
 * @param vm View model
 * @return Count
 */
int carbon_vm_count(const Carbon_ViewModel *vm);

/*============================================================================
 * Change Notification
 *============================================================================*/

/**
 * Subscribe to changes on a specific observable.
 *
 * @param vm       View model
 * @param id       Observable ID
 * @param callback Function to call on change
 * @param userdata User data to pass to callback
 * @return Listener ID for unsubscription, or 0 on failure
 */
uint32_t carbon_vm_subscribe(Carbon_ViewModel *vm,
                              uint32_t id,
                              Carbon_VMCallback callback,
                              void *userdata);

/**
 * Subscribe to all observable changes.
 *
 * @param vm       View model
 * @param callback Function to call on any change
 * @param userdata User data to pass to callback
 * @return Listener ID for unsubscription, or 0 on failure
 */
uint32_t carbon_vm_subscribe_all(Carbon_ViewModel *vm,
                                  Carbon_VMCallback callback,
                                  void *userdata);

/**
 * Unsubscribe a listener.
 *
 * @param vm         View model
 * @param listener_id Listener ID from subscribe
 */
void carbon_vm_unsubscribe(Carbon_ViewModel *vm, uint32_t listener_id);

/**
 * Force notification even if value unchanged.
 *
 * @param vm View model
 * @param id Observable ID
 */
void carbon_vm_notify(Carbon_ViewModel *vm, uint32_t id);

/**
 * Notify all subscribers for all observables.
 *
 * @param vm View model
 */
void carbon_vm_notify_all(Carbon_ViewModel *vm);

/*============================================================================
 * Batch Updates
 *============================================================================*/

/**
 * Begin a batch update.
 * Changes during batch mode are collected but callbacks are deferred.
 *
 * @param vm View model
 */
void carbon_vm_begin_batch(Carbon_ViewModel *vm);

/**
 * Commit a batch update.
 * Triggers callbacks for all changed observables.
 *
 * @param vm View model
 */
void carbon_vm_commit_batch(Carbon_ViewModel *vm);

/**
 * Cancel a batch update.
 * Reverts all changes made during batch mode.
 *
 * @param vm View model
 */
void carbon_vm_cancel_batch(Carbon_ViewModel *vm);

/**
 * Check if currently in batch mode.
 *
 * @param vm View model
 * @return true if in batch mode
 */
bool carbon_vm_is_batching(const Carbon_ViewModel *vm);

/*============================================================================
 * Validation
 *============================================================================*/

/**
 * Set a validator for an observable.
 * Validators can reject value changes.
 *
 * @param vm        View model
 * @param id        Observable ID
 * @param validator Validator function
 * @param userdata  User data for validator
 */
void carbon_vm_set_validator(Carbon_ViewModel *vm,
                              uint32_t id,
                              Carbon_VMValidator validator,
                              void *userdata);

/*============================================================================
 * Formatting
 *============================================================================*/

/**
 * Set a custom formatter for an observable.
 *
 * @param vm        View model
 * @param id        Observable ID
 * @param formatter Formatter function
 * @param userdata  User data for formatter
 */
void carbon_vm_set_formatter(Carbon_ViewModel *vm,
                              uint32_t id,
                              Carbon_VMFormatter formatter,
                              void *userdata);

/**
 * Format an observable value as a string.
 * Uses custom formatter if set, otherwise default formatting.
 *
 * @param vm     View model
 * @param id     Observable ID
 * @param buffer Output buffer
 * @param size   Buffer size
 * @return Number of characters written
 */
int carbon_vm_format(const Carbon_ViewModel *vm,
                      uint32_t id,
                      char *buffer,
                      size_t size);

/**
 * Format with printf-style format string.
 *
 * @param vm     View model
 * @param id     Observable ID
 * @param buffer Output buffer
 * @param size   Buffer size
 * @param format Printf-style format string
 * @return Number of characters written
 */
int carbon_vm_format_ex(const Carbon_ViewModel *vm,
                         uint32_t id,
                         char *buffer,
                         size_t size,
                         const char *format);

/*============================================================================
 * Computed Values
 *============================================================================*/

/**
 * Computed value callback.
 * Called when any dependency changes to recalculate the computed value.
 *
 * @param vm       View model
 * @param id       Computed observable ID
 * @param userdata User data
 * @return Computed value
 */
typedef Carbon_VMValue (*Carbon_VMComputed)(Carbon_ViewModel *vm,
                                             uint32_t id,
                                             void *userdata);

/**
 * Define a computed observable.
 * Value is recalculated when dependencies change.
 *
 * @param vm           View model
 * @param name         Observable name
 * @param type         Value type
 * @param compute      Computation function
 * @param userdata     User data for compute function
 * @param dependencies Array of dependency IDs
 * @param dep_count    Number of dependencies
 * @return Observable ID or CARBON_VM_INVALID_ID on failure
 */
uint32_t carbon_vm_define_computed(Carbon_ViewModel *vm,
                                    const char *name,
                                    Carbon_VMType type,
                                    Carbon_VMComputed compute,
                                    void *userdata,
                                    const uint32_t *dependencies,
                                    int dep_count);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get a human-readable name for a value type.
 *
 * @param type Value type
 * @return Static string name
 */
const char *carbon_vm_type_name(Carbon_VMType type);

/**
 * Compare two values for equality.
 *
 * @param a First value
 * @param b Second value
 * @return true if equal
 */
bool carbon_vm_values_equal(const Carbon_VMValue *a, const Carbon_VMValue *b);

/**
 * Copy a value.
 *
 * @param dest Destination
 * @param src  Source
 */
void carbon_vm_value_copy(Carbon_VMValue *dest, const Carbon_VMValue *src);

/**
 * Clear/reset a value to default for its type.
 *
 * @param value Value to clear
 */
void carbon_vm_value_clear(Carbon_VMValue *value);

#endif /* CARBON_VIEWMODEL_H */
