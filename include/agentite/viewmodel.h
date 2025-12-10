#ifndef AGENTITE_VIEWMODEL_H
#define AGENTITE_VIEWMODEL_H

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
 *   Agentite_ViewModel *vm = agentite_vm_create();
 *
 *   // Define observables
 *   uint32_t health_id = agentite_vm_define_int(vm, "player_health", 100);
 *   uint32_t gold_id = agentite_vm_define_int(vm, "gold", 0);
 *   uint32_t name_id = agentite_vm_define_string(vm, "player_name", "Hero");
 *
 *   // Subscribe to changes
 *   agentite_vm_subscribe(vm, health_id, on_health_changed, ui);
 *
 *   // Update values (triggers callbacks if changed)
 *   agentite_vm_set_int(vm, health_id, 75);
 *
 *   // Batch updates
 *   agentite_vm_begin_batch(vm);
 *   agentite_vm_set_int(vm, health_id, 50);
 *   agentite_vm_set_int(vm, gold_id, 100);
 *   agentite_vm_commit_batch(vm);  // Triggers all callbacks once
 *
 *   // Cleanup
 *   agentite_vm_destroy(vm);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_VM_MAX_OBSERVABLES    256   /* Maximum observable values */
#define AGENTITE_VM_MAX_LISTENERS      64    /* Maximum listeners per observable */
#define AGENTITE_VM_MAX_STRING_LENGTH  256   /* Maximum string value length */
#define AGENTITE_VM_INVALID_ID         0     /* Invalid observable ID */

/*============================================================================
 * Observable Types
 *============================================================================*/

/**
 * Types of observable values
 */
typedef enum Agentite_VMType {
    AGENTITE_VM_TYPE_NONE = 0,
    AGENTITE_VM_TYPE_INT,         /* int32_t */
    AGENTITE_VM_TYPE_INT64,       /* int64_t */
    AGENTITE_VM_TYPE_FLOAT,       /* float */
    AGENTITE_VM_TYPE_DOUBLE,      /* double */
    AGENTITE_VM_TYPE_BOOL,        /* bool */
    AGENTITE_VM_TYPE_STRING,      /* char[] (copied) */
    AGENTITE_VM_TYPE_POINTER,     /* void* (not owned) */
    AGENTITE_VM_TYPE_VEC2,        /* float[2] */
    AGENTITE_VM_TYPE_VEC3,        /* float[3] */
    AGENTITE_VM_TYPE_VEC4,        /* float[4] / color */
    AGENTITE_VM_TYPE_COUNT
} Agentite_VMType;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Vector types for convenience
 */
typedef struct Agentite_VMVec2 {
    float x, y;
} Agentite_VMVec2;

typedef struct Agentite_VMVec3 {
    float x, y, z;
} Agentite_VMVec3;

typedef struct Agentite_VMVec4 {
    float x, y, z, w;
} Agentite_VMVec4;

/**
 * Observable value (tagged union)
 */
typedef struct Agentite_VMValue {
    Agentite_VMType type;
    union {
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        bool b;
        char str[AGENTITE_VM_MAX_STRING_LENGTH];
        void *ptr;
        Agentite_VMVec2 vec2;
        Agentite_VMVec3 vec3;
        Agentite_VMVec4 vec4;
    };
} Agentite_VMValue;

/**
 * Change event data passed to callbacks
 */
typedef struct Agentite_VMChangeEvent {
    uint32_t id;                    /* Observable ID */
    const char *name;               /* Observable name */
    Agentite_VMType type;             /* Value type */
    Agentite_VMValue old_value;       /* Previous value */
    Agentite_VMValue new_value;       /* New value */
} Agentite_VMChangeEvent;

/*============================================================================
 * Callback Types
 *============================================================================*/

typedef struct Agentite_ViewModel Agentite_ViewModel;

/**
 * Callback for value changes.
 *
 * @param vm      View model
 * @param event   Change event with old and new values
 * @param userdata User data passed during subscription
 */
typedef void (*Agentite_VMCallback)(Agentite_ViewModel *vm,
                                   const Agentite_VMChangeEvent *event,
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
typedef bool (*Agentite_VMValidator)(Agentite_ViewModel *vm,
                                    uint32_t id,
                                    const Agentite_VMValue *new_value,
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
typedef int (*Agentite_VMFormatter)(Agentite_ViewModel *vm,
                                   uint32_t id,
                                   const Agentite_VMValue *value,
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
Agentite_ViewModel *agentite_vm_create(void);

/**
 * Create a view model with event dispatcher integration.
 * Emits AGENTITE_EVENT_UI_VALUE_CHANGED events on changes.
 *
 * @param events Event dispatcher (can be NULL)
 * @return New view model or NULL on failure
 */
typedef struct Agentite_EventDispatcher Agentite_EventDispatcher;
Agentite_ViewModel *agentite_vm_create_with_events(Agentite_EventDispatcher *events);

/**
 * Destroy a view model and free resources.
 *
 * @param vm View model to destroy
 */
void agentite_vm_destroy(Agentite_ViewModel *vm);

/*============================================================================
 * Observable Definition
 *============================================================================*/

/**
 * Define an integer observable.
 *
 * @param vm      View model
 * @param name    Observable name (for lookup)
 * @param initial Initial value
 * @return Observable ID or AGENTITE_VM_INVALID_ID on failure
 */
uint32_t agentite_vm_define_int(Agentite_ViewModel *vm, const char *name, int32_t initial);

/**
 * Define a 64-bit integer observable.
 */
uint32_t agentite_vm_define_int64(Agentite_ViewModel *vm, const char *name, int64_t initial);

/**
 * Define a float observable.
 */
uint32_t agentite_vm_define_float(Agentite_ViewModel *vm, const char *name, float initial);

/**
 * Define a double observable.
 */
uint32_t agentite_vm_define_double(Agentite_ViewModel *vm, const char *name, double initial);

/**
 * Define a boolean observable.
 */
uint32_t agentite_vm_define_bool(Agentite_ViewModel *vm, const char *name, bool initial);

/**
 * Define a string observable.
 */
uint32_t agentite_vm_define_string(Agentite_ViewModel *vm, const char *name, const char *initial);

/**
 * Define a pointer observable (not owned by view model).
 */
uint32_t agentite_vm_define_ptr(Agentite_ViewModel *vm, const char *name, void *initial);

/**
 * Define a 2D vector observable.
 */
uint32_t agentite_vm_define_vec2(Agentite_ViewModel *vm, const char *name, float x, float y);

/**
 * Define a 3D vector observable.
 */
uint32_t agentite_vm_define_vec3(Agentite_ViewModel *vm, const char *name, float x, float y, float z);

/**
 * Define a 4D vector/color observable.
 */
uint32_t agentite_vm_define_vec4(Agentite_ViewModel *vm, const char *name,
                                float x, float y, float z, float w);

/**
 * Define a color observable (alias for vec4).
 */
uint32_t agentite_vm_define_color(Agentite_ViewModel *vm, const char *name,
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
bool agentite_vm_set_int(Agentite_ViewModel *vm, uint32_t id, int32_t value);
bool agentite_vm_set_int64(Agentite_ViewModel *vm, uint32_t id, int64_t value);
bool agentite_vm_set_float(Agentite_ViewModel *vm, uint32_t id, float value);
bool agentite_vm_set_double(Agentite_ViewModel *vm, uint32_t id, double value);
bool agentite_vm_set_bool(Agentite_ViewModel *vm, uint32_t id, bool value);
bool agentite_vm_set_string(Agentite_ViewModel *vm, uint32_t id, const char *value);
bool agentite_vm_set_ptr(Agentite_ViewModel *vm, uint32_t id, void *value);
bool agentite_vm_set_vec2(Agentite_ViewModel *vm, uint32_t id, float x, float y);
bool agentite_vm_set_vec3(Agentite_ViewModel *vm, uint32_t id, float x, float y, float z);
bool agentite_vm_set_vec4(Agentite_ViewModel *vm, uint32_t id, float x, float y, float z, float w);
bool agentite_vm_set_color(Agentite_ViewModel *vm, uint32_t id, float r, float g, float b, float a);

/**
 * Set value from a generic Agentite_VMValue struct.
 */
bool agentite_vm_set_value(Agentite_ViewModel *vm, uint32_t id, const Agentite_VMValue *value);

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
int32_t agentite_vm_get_int(const Agentite_ViewModel *vm, uint32_t id);
int64_t agentite_vm_get_int64(const Agentite_ViewModel *vm, uint32_t id);
float agentite_vm_get_float(const Agentite_ViewModel *vm, uint32_t id);
double agentite_vm_get_double(const Agentite_ViewModel *vm, uint32_t id);
bool agentite_vm_get_bool(const Agentite_ViewModel *vm, uint32_t id);
const char *agentite_vm_get_string(const Agentite_ViewModel *vm, uint32_t id);
void *agentite_vm_get_ptr(const Agentite_ViewModel *vm, uint32_t id);
Agentite_VMVec2 agentite_vm_get_vec2(const Agentite_ViewModel *vm, uint32_t id);
Agentite_VMVec3 agentite_vm_get_vec3(const Agentite_ViewModel *vm, uint32_t id);
Agentite_VMVec4 agentite_vm_get_vec4(const Agentite_ViewModel *vm, uint32_t id);

/**
 * Get the full value struct for an observable.
 *
 * @param vm  View model
 * @param id  Observable ID
 * @param out Output value struct
 * @return true if successful
 */
bool agentite_vm_get_value(const Agentite_ViewModel *vm, uint32_t id, Agentite_VMValue *out);

/*============================================================================
 * Lookup and Query
 *============================================================================*/

/**
 * Find an observable by name.
 *
 * @param vm   View model
 * @param name Observable name
 * @return Observable ID or AGENTITE_VM_INVALID_ID if not found
 */
uint32_t agentite_vm_find(const Agentite_ViewModel *vm, const char *name);

/**
 * Get the name of an observable.
 *
 * @param vm View model
 * @param id Observable ID
 * @return Name or NULL if invalid
 */
const char *agentite_vm_get_name(const Agentite_ViewModel *vm, uint32_t id);

/**
 * Get the type of an observable.
 *
 * @param vm View model
 * @param id Observable ID
 * @return Type or AGENTITE_VM_TYPE_NONE if invalid
 */
Agentite_VMType agentite_vm_get_type(const Agentite_ViewModel *vm, uint32_t id);

/**
 * Check if an observable exists.
 *
 * @param vm View model
 * @param id Observable ID
 * @return true if exists
 */
bool agentite_vm_exists(const Agentite_ViewModel *vm, uint32_t id);

/**
 * Get the number of defined observables.
 *
 * @param vm View model
 * @return Count
 */
int agentite_vm_count(const Agentite_ViewModel *vm);

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
uint32_t agentite_vm_subscribe(Agentite_ViewModel *vm,
                              uint32_t id,
                              Agentite_VMCallback callback,
                              void *userdata);

/**
 * Subscribe to all observable changes.
 *
 * @param vm       View model
 * @param callback Function to call on any change
 * @param userdata User data to pass to callback
 * @return Listener ID for unsubscription, or 0 on failure
 */
uint32_t agentite_vm_subscribe_all(Agentite_ViewModel *vm,
                                  Agentite_VMCallback callback,
                                  void *userdata);

/**
 * Unsubscribe a listener.
 *
 * @param vm         View model
 * @param listener_id Listener ID from subscribe
 */
void agentite_vm_unsubscribe(Agentite_ViewModel *vm, uint32_t listener_id);

/**
 * Force notification even if value unchanged.
 *
 * @param vm View model
 * @param id Observable ID
 */
void agentite_vm_notify(Agentite_ViewModel *vm, uint32_t id);

/**
 * Notify all subscribers for all observables.
 *
 * @param vm View model
 */
void agentite_vm_notify_all(Agentite_ViewModel *vm);

/*============================================================================
 * Batch Updates
 *============================================================================*/

/**
 * Begin a batch update.
 * Changes during batch mode are collected but callbacks are deferred.
 *
 * @param vm View model
 */
void agentite_vm_begin_batch(Agentite_ViewModel *vm);

/**
 * Commit a batch update.
 * Triggers callbacks for all changed observables.
 *
 * @param vm View model
 */
void agentite_vm_commit_batch(Agentite_ViewModel *vm);

/**
 * Cancel a batch update.
 * Reverts all changes made during batch mode.
 *
 * @param vm View model
 */
void agentite_vm_cancel_batch(Agentite_ViewModel *vm);

/**
 * Check if currently in batch mode.
 *
 * @param vm View model
 * @return true if in batch mode
 */
bool agentite_vm_is_batching(const Agentite_ViewModel *vm);

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
void agentite_vm_set_validator(Agentite_ViewModel *vm,
                              uint32_t id,
                              Agentite_VMValidator validator,
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
void agentite_vm_set_formatter(Agentite_ViewModel *vm,
                              uint32_t id,
                              Agentite_VMFormatter formatter,
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
int agentite_vm_format(const Agentite_ViewModel *vm,
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
int agentite_vm_format_ex(const Agentite_ViewModel *vm,
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
typedef Agentite_VMValue (*Agentite_VMComputed)(Agentite_ViewModel *vm,
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
 * @return Observable ID or AGENTITE_VM_INVALID_ID on failure
 */
uint32_t agentite_vm_define_computed(Agentite_ViewModel *vm,
                                    const char *name,
                                    Agentite_VMType type,
                                    Agentite_VMComputed compute,
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
const char *agentite_vm_type_name(Agentite_VMType type);

/**
 * Compare two values for equality.
 *
 * @param a First value
 * @param b Second value
 * @return true if equal
 */
bool agentite_vm_values_equal(const Agentite_VMValue *a, const Agentite_VMValue *b);

/**
 * Copy a value.
 *
 * @param dest Destination
 * @param src  Source
 */
void agentite_vm_value_copy(Agentite_VMValue *dest, const Agentite_VMValue *src);

/**
 * Clear/reset a value to default for its type.
 *
 * @param value Value to clear
 */
void agentite_vm_value_clear(Agentite_VMValue *value);

#endif /* AGENTITE_VIEWMODEL_H */
