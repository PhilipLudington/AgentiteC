/**
 * Carbon View Model System
 *
 * Separates game state from UI presentation with observable values,
 * change detection, and event-driven updates.
 */

#include "agentite/agentite.h"
#include "agentite/viewmodel.h"
#include "agentite/event.h"
#include "agentite/error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * Internal Data Structures
 *============================================================================*/

/**
 * Listener registration
 */
typedef struct {
    uint32_t id;                    /* Listener ID */
    uint32_t observable_id;         /* Observable this listens to (0 = all) */
    Agentite_VMCallback callback;
    void *userdata;
    bool active;
} VMListener;

/**
 * Observable definition
 */
typedef struct {
    uint32_t id;                    /* Observable ID */
    char name[64];                  /* Observable name */
    Agentite_VMValue value;           /* Current value */
    Agentite_VMValue old_value;       /* Previous value (for batch) */
    bool changed;                   /* Changed during batch */

    /* Validator */
    Agentite_VMValidator validator;
    void *validator_userdata;

    /* Formatter */
    Agentite_VMFormatter formatter;
    void *formatter_userdata;

    /* Computed value support */
    bool is_computed;
    Agentite_VMComputed compute;
    void *compute_userdata;
    uint32_t dependencies[8];
    int dep_count;

    bool active;
} VMObservable;

/**
 * View model internal structure
 */
struct Agentite_ViewModel {
    /* Observables */
    VMObservable observables[AGENTITE_VM_MAX_OBSERVABLES];
    int observable_count;
    uint32_t next_observable_id;

    /* Listeners */
    VMListener listeners[AGENTITE_VM_MAX_OBSERVABLES * 4];
    int listener_count;
    uint32_t next_listener_id;

    /* Batch mode */
    bool batching;
    uint32_t batch_changed[AGENTITE_VM_MAX_OBSERVABLES];
    int batch_changed_count;

    /* Event dispatcher (optional) */
    Agentite_EventDispatcher *events;
};

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Find observable by ID
 */
static VMObservable *find_observable(Agentite_ViewModel *vm, uint32_t id) {
    if (!vm || id == AGENTITE_VM_INVALID_ID) return NULL;

    for (int i = 0; i < vm->observable_count; i++) {
        if (vm->observables[i].id == id && vm->observables[i].active) {
            return &vm->observables[i];
        }
    }
    return NULL;
}

/**
 * Find observable by name
 */
static VMObservable *find_observable_by_name(Agentite_ViewModel *vm, const char *name) {
    if (!vm || !name) return NULL;

    for (int i = 0; i < vm->observable_count; i++) {
        if (vm->observables[i].active &&
            strcmp(vm->observables[i].name, name) == 0) {
            return &vm->observables[i];
        }
    }
    return NULL;
}

/**
 * Notify listeners of a change
 */
static void notify_listeners(Agentite_ViewModel *vm, VMObservable *obs,
                             const Agentite_VMValue *old_value) {
    if (!vm || !obs) return;

    Agentite_VMChangeEvent event = {
        .id = obs->id,
        .name = obs->name,
        .type = obs->value.type,
        .old_value = *old_value,
        .new_value = obs->value,
    };

    /* Notify specific listeners */
    for (int i = 0; i < vm->listener_count; i++) {
        VMListener *l = &vm->listeners[i];
        if (l->active && l->callback &&
            (l->observable_id == obs->id || l->observable_id == 0)) {
            l->callback(vm, &event, l->userdata);
        }
    }

    /* Emit event if dispatcher is set */
    if (vm->events) {
        Agentite_Event e = { .type = AGENTITE_EVENT_UI_VALUE_CHANGED };
        e.ui.widget_id = obs->id;
        e.ui.widget_name = obs->name;
        agentite_event_emit(vm->events, &e);
    }

    /* Update computed values that depend on this */
    for (int i = 0; i < vm->observable_count; i++) {
        VMObservable *computed = &vm->observables[i];
        if (!computed->active || !computed->is_computed) continue;

        for (int j = 0; j < computed->dep_count; j++) {
            if (computed->dependencies[j] == obs->id) {
                /* Recalculate */
                Agentite_VMValue new_val = computed->compute(vm, computed->id,
                                                           computed->compute_userdata);
                if (!agentite_vm_values_equal(&computed->value, &new_val)) {
                    Agentite_VMValue old = computed->value;
                    computed->value = new_val;
                    notify_listeners(vm, computed, &old);
                }
                break;
            }
        }
    }
}

/**
 * Define a generic observable
 */
static uint32_t define_observable(Agentite_ViewModel *vm, const char *name,
                                   const Agentite_VMValue *initial) {
    if (!vm || !name) {
        agentite_set_error("agentite_vm_define: null parameter");
        return AGENTITE_VM_INVALID_ID;
    }

    /* Check for duplicate */
    if (find_observable_by_name(vm, name)) {
        agentite_set_error("agentite_vm_define: observable '%s' already exists", name);
        return AGENTITE_VM_INVALID_ID;
    }

    if (vm->observable_count >= AGENTITE_VM_MAX_OBSERVABLES) {
        agentite_set_error("agentite_vm_define: max observables reached");
        return AGENTITE_VM_INVALID_ID;
    }

    VMObservable *obs = &vm->observables[vm->observable_count++];
    memset(obs, 0, sizeof(VMObservable));

    obs->id = ++vm->next_observable_id;
    strncpy(obs->name, name, sizeof(obs->name) - 1);
    obs->value = *initial;
    obs->active = true;

    return obs->id;
}

/**
 * Set value with change detection
 */
static bool set_value(Agentite_ViewModel *vm, uint32_t id,
                      const Agentite_VMValue *new_value) {
    VMObservable *obs = find_observable(vm, id);
    if (!obs) return false;

    /* Type check */
    if (obs->value.type != new_value->type) {
        agentite_set_error("agentite_vm_set: type mismatch");
        return false;
    }

    /* Validate */
    if (obs->validator) {
        if (!obs->validator(vm, id, new_value, obs->validator_userdata)) {
            return false;
        }
    }

    /* Check if changed */
    if (agentite_vm_values_equal(&obs->value, new_value)) {
        return false;
    }

    Agentite_VMValue old_value = obs->value;
    obs->value = *new_value;

    if (vm->batching) {
        /* Record for batch commit */
        if (!obs->changed) {
            obs->old_value = old_value;
            obs->changed = true;
            vm->batch_changed[vm->batch_changed_count++] = id;
        }
    } else {
        /* Notify immediately */
        notify_listeners(vm, obs, &old_value);
    }

    return true;
}

/*============================================================================
 * Creation and Destruction
 *============================================================================*/

Agentite_ViewModel *agentite_vm_create(void) {
    return agentite_vm_create_with_events(NULL);
}

Agentite_ViewModel *agentite_vm_create_with_events(Agentite_EventDispatcher *events) {
    Agentite_ViewModel *vm = AGENTITE_ALLOC(Agentite_ViewModel);
    if (!vm) {
        agentite_set_error("agentite_vm_create: allocation failed");
        return NULL;
    }

    vm->events = events;
    vm->next_observable_id = 0;
    vm->next_listener_id = 0;

    return vm;
}

void agentite_vm_destroy(Agentite_ViewModel *vm) {
    if (vm) {
        free(vm);
    }
}

/*============================================================================
 * Observable Definition
 *============================================================================*/

uint32_t agentite_vm_define_int(Agentite_ViewModel *vm, const char *name, int32_t initial) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_INT, .i32 = initial };
    return define_observable(vm, name, &val);
}

uint32_t agentite_vm_define_int64(Agentite_ViewModel *vm, const char *name, int64_t initial) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_INT64, .i64 = initial };
    return define_observable(vm, name, &val);
}

uint32_t agentite_vm_define_float(Agentite_ViewModel *vm, const char *name, float initial) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_FLOAT, .f32 = initial };
    return define_observable(vm, name, &val);
}

uint32_t agentite_vm_define_double(Agentite_ViewModel *vm, const char *name, double initial) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_DOUBLE, .f64 = initial };
    return define_observable(vm, name, &val);
}

uint32_t agentite_vm_define_bool(Agentite_ViewModel *vm, const char *name, bool initial) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_BOOL, .b = initial };
    return define_observable(vm, name, &val);
}

uint32_t agentite_vm_define_string(Agentite_ViewModel *vm, const char *name, const char *initial) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_STRING };
    if (initial) {
        strncpy(val.str, initial, AGENTITE_VM_MAX_STRING_LENGTH - 1);
        val.str[AGENTITE_VM_MAX_STRING_LENGTH - 1] = '\0';
    }
    return define_observable(vm, name, &val);
}

uint32_t agentite_vm_define_ptr(Agentite_ViewModel *vm, const char *name, void *initial) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_POINTER, .ptr = initial };
    return define_observable(vm, name, &val);
}

uint32_t agentite_vm_define_vec2(Agentite_ViewModel *vm, const char *name, float x, float y) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_VEC2, .vec2 = { x, y } };
    return define_observable(vm, name, &val);
}

uint32_t agentite_vm_define_vec3(Agentite_ViewModel *vm, const char *name,
                                float x, float y, float z) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_VEC3, .vec3 = { x, y, z } };
    return define_observable(vm, name, &val);
}

uint32_t agentite_vm_define_vec4(Agentite_ViewModel *vm, const char *name,
                                float x, float y, float z, float w) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_VEC4, .vec4 = { x, y, z, w } };
    return define_observable(vm, name, &val);
}

uint32_t agentite_vm_define_color(Agentite_ViewModel *vm, const char *name,
                                 float r, float g, float b, float a) {
    return agentite_vm_define_vec4(vm, name, r, g, b, a);
}

/*============================================================================
 * Value Setters
 *============================================================================*/

bool agentite_vm_set_int(Agentite_ViewModel *vm, uint32_t id, int32_t value) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_INT, .i32 = value };
    return set_value(vm, id, &val);
}

bool agentite_vm_set_int64(Agentite_ViewModel *vm, uint32_t id, int64_t value) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_INT64, .i64 = value };
    return set_value(vm, id, &val);
}

bool agentite_vm_set_float(Agentite_ViewModel *vm, uint32_t id, float value) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_FLOAT, .f32 = value };
    return set_value(vm, id, &val);
}

bool agentite_vm_set_double(Agentite_ViewModel *vm, uint32_t id, double value) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_DOUBLE, .f64 = value };
    return set_value(vm, id, &val);
}

bool agentite_vm_set_bool(Agentite_ViewModel *vm, uint32_t id, bool value) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_BOOL, .b = value };
    return set_value(vm, id, &val);
}

bool agentite_vm_set_string(Agentite_ViewModel *vm, uint32_t id, const char *value) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_STRING };
    if (value) {
        strncpy(val.str, value, AGENTITE_VM_MAX_STRING_LENGTH - 1);
        val.str[AGENTITE_VM_MAX_STRING_LENGTH - 1] = '\0';
    }
    return set_value(vm, id, &val);
}

bool agentite_vm_set_ptr(Agentite_ViewModel *vm, uint32_t id, void *value) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_POINTER, .ptr = value };
    return set_value(vm, id, &val);
}

bool agentite_vm_set_vec2(Agentite_ViewModel *vm, uint32_t id, float x, float y) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_VEC2, .vec2 = { x, y } };
    return set_value(vm, id, &val);
}

bool agentite_vm_set_vec3(Agentite_ViewModel *vm, uint32_t id, float x, float y, float z) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_VEC3, .vec3 = { x, y, z } };
    return set_value(vm, id, &val);
}

bool agentite_vm_set_vec4(Agentite_ViewModel *vm, uint32_t id,
                         float x, float y, float z, float w) {
    Agentite_VMValue val = { .type = AGENTITE_VM_TYPE_VEC4, .vec4 = { x, y, z, w } };
    return set_value(vm, id, &val);
}

bool agentite_vm_set_color(Agentite_ViewModel *vm, uint32_t id,
                          float r, float g, float b, float a) {
    return agentite_vm_set_vec4(vm, id, r, g, b, a);
}

bool agentite_vm_set_value(Agentite_ViewModel *vm, uint32_t id, const Agentite_VMValue *value) {
    if (!value) return false;
    return set_value(vm, id, value);
}

/*============================================================================
 * Value Getters
 *============================================================================*/

int32_t agentite_vm_get_int(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs || obs->value.type != AGENTITE_VM_TYPE_INT) return 0;
    return obs->value.i32;
}

int64_t agentite_vm_get_int64(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs || obs->value.type != AGENTITE_VM_TYPE_INT64) return 0;
    return obs->value.i64;
}

float agentite_vm_get_float(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs || obs->value.type != AGENTITE_VM_TYPE_FLOAT) return 0.0f;
    return obs->value.f32;
}

double agentite_vm_get_double(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs || obs->value.type != AGENTITE_VM_TYPE_DOUBLE) return 0.0;
    return obs->value.f64;
}

bool agentite_vm_get_bool(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs || obs->value.type != AGENTITE_VM_TYPE_BOOL) return false;
    return obs->value.b;
}

const char *agentite_vm_get_string(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs || obs->value.type != AGENTITE_VM_TYPE_STRING) return "";
    return obs->value.str;
}

void *agentite_vm_get_ptr(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs || obs->value.type != AGENTITE_VM_TYPE_POINTER) return NULL;
    return obs->value.ptr;
}

Agentite_VMVec2 agentite_vm_get_vec2(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs || obs->value.type != AGENTITE_VM_TYPE_VEC2) {
        return (Agentite_VMVec2){ 0, 0 };
    }
    return obs->value.vec2;
}

Agentite_VMVec3 agentite_vm_get_vec3(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs || obs->value.type != AGENTITE_VM_TYPE_VEC3) {
        return (Agentite_VMVec3){ 0, 0, 0 };
    }
    return obs->value.vec3;
}

Agentite_VMVec4 agentite_vm_get_vec4(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs || obs->value.type != AGENTITE_VM_TYPE_VEC4) {
        return (Agentite_VMVec4){ 0, 0, 0, 0 };
    }
    return obs->value.vec4;
}

bool agentite_vm_get_value(const Agentite_ViewModel *vm, uint32_t id, Agentite_VMValue *out) {
    if (!out) return false;
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs) return false;
    *out = obs->value;
    return true;
}

/*============================================================================
 * Lookup and Query
 *============================================================================*/

uint32_t agentite_vm_find(const Agentite_ViewModel *vm, const char *name) {
    VMObservable *obs = find_observable_by_name((Agentite_ViewModel *)vm, name);
    return obs ? obs->id : AGENTITE_VM_INVALID_ID;
}

const char *agentite_vm_get_name(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    return obs ? obs->name : NULL;
}

Agentite_VMType agentite_vm_get_type(const Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    return obs ? obs->value.type : AGENTITE_VM_TYPE_NONE;
}

bool agentite_vm_exists(const Agentite_ViewModel *vm, uint32_t id) {
    return find_observable((Agentite_ViewModel *)vm, id) != NULL;
}

int agentite_vm_count(const Agentite_ViewModel *vm) {
    if (!vm) return 0;
    int count = 0;
    for (int i = 0; i < vm->observable_count; i++) {
        if (vm->observables[i].active) count++;
    }
    return count;
}

/*============================================================================
 * Change Notification
 *============================================================================*/

uint32_t agentite_vm_subscribe(Agentite_ViewModel *vm,
                              uint32_t id,
                              Agentite_VMCallback callback,
                              void *userdata) {
    if (!vm || !callback) return 0;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < vm->listener_count; i++) {
        if (!vm->listeners[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        if (vm->listener_count >= AGENTITE_VM_MAX_OBSERVABLES * 4) {
            agentite_set_error("agentite_vm_subscribe: max listeners reached");
            return 0;
        }
        slot = vm->listener_count++;
    }

    VMListener *l = &vm->listeners[slot];
    l->id = ++vm->next_listener_id;
    l->observable_id = id;
    l->callback = callback;
    l->userdata = userdata;
    l->active = true;

    return l->id;
}

uint32_t agentite_vm_subscribe_all(Agentite_ViewModel *vm,
                                  Agentite_VMCallback callback,
                                  void *userdata) {
    return agentite_vm_subscribe(vm, 0, callback, userdata);
}

void agentite_vm_unsubscribe(Agentite_ViewModel *vm, uint32_t listener_id) {
    if (!vm || listener_id == 0) return;

    for (int i = 0; i < vm->listener_count; i++) {
        if (vm->listeners[i].id == listener_id) {
            vm->listeners[i].active = false;
            return;
        }
    }
}

void agentite_vm_notify(Agentite_ViewModel *vm, uint32_t id) {
    VMObservable *obs = find_observable(vm, id);
    if (!obs) return;
    notify_listeners(vm, obs, &obs->value);
}

void agentite_vm_notify_all(Agentite_ViewModel *vm) {
    if (!vm) return;

    for (int i = 0; i < vm->observable_count; i++) {
        if (vm->observables[i].active) {
            notify_listeners(vm, &vm->observables[i], &vm->observables[i].value);
        }
    }
}

/*============================================================================
 * Batch Updates
 *============================================================================*/

void agentite_vm_begin_batch(Agentite_ViewModel *vm) {
    if (!vm) return;
    vm->batching = true;
    vm->batch_changed_count = 0;
}

void agentite_vm_commit_batch(Agentite_ViewModel *vm) {
    if (!vm || !vm->batching) return;

    vm->batching = false;

    /* Notify for all changed observables */
    for (int i = 0; i < vm->batch_changed_count; i++) {
        VMObservable *obs = find_observable(vm, vm->batch_changed[i]);
        if (obs && obs->changed) {
            notify_listeners(vm, obs, &obs->old_value);
            obs->changed = false;
        }
    }

    vm->batch_changed_count = 0;
}

void agentite_vm_cancel_batch(Agentite_ViewModel *vm) {
    if (!vm || !vm->batching) return;

    /* Revert all changes */
    for (int i = 0; i < vm->batch_changed_count; i++) {
        VMObservable *obs = find_observable(vm, vm->batch_changed[i]);
        if (obs && obs->changed) {
            obs->value = obs->old_value;
            obs->changed = false;
        }
    }

    vm->batching = false;
    vm->batch_changed_count = 0;
}

bool agentite_vm_is_batching(const Agentite_ViewModel *vm) {
    return vm ? vm->batching : false;
}

/*============================================================================
 * Validation
 *============================================================================*/

void agentite_vm_set_validator(Agentite_ViewModel *vm,
                              uint32_t id,
                              Agentite_VMValidator validator,
                              void *userdata) {
    VMObservable *obs = find_observable(vm, id);
    if (!obs) return;

    obs->validator = validator;
    obs->validator_userdata = userdata;
}

/*============================================================================
 * Formatting
 *============================================================================*/

void agentite_vm_set_formatter(Agentite_ViewModel *vm,
                              uint32_t id,
                              Agentite_VMFormatter formatter,
                              void *userdata) {
    VMObservable *obs = find_observable(vm, id);
    if (!obs) return;

    obs->formatter = formatter;
    obs->formatter_userdata = userdata;
}

int agentite_vm_format(const Agentite_ViewModel *vm,
                      uint32_t id,
                      char *buffer,
                      size_t size) {
    if (!vm || !buffer || size == 0) return 0;

    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs) return 0;

    /* Use custom formatter if set */
    if (obs->formatter) {
        return obs->formatter((Agentite_ViewModel *)vm, id, &obs->value,
                              buffer, size, obs->formatter_userdata);
    }

    /* Default formatting */
    switch (obs->value.type) {
        case AGENTITE_VM_TYPE_INT:
            return snprintf(buffer, size, "%d", obs->value.i32);
        case AGENTITE_VM_TYPE_INT64:
            return snprintf(buffer, size, "%lld", (long long)obs->value.i64);
        case AGENTITE_VM_TYPE_FLOAT:
            return snprintf(buffer, size, "%.2f", obs->value.f32);
        case AGENTITE_VM_TYPE_DOUBLE:
            return snprintf(buffer, size, "%.4f", obs->value.f64);
        case AGENTITE_VM_TYPE_BOOL:
            return snprintf(buffer, size, "%s", obs->value.b ? "true" : "false");
        case AGENTITE_VM_TYPE_STRING:
            return snprintf(buffer, size, "%s", obs->value.str);
        case AGENTITE_VM_TYPE_POINTER:
            return snprintf(buffer, size, "%p", obs->value.ptr);
        case AGENTITE_VM_TYPE_VEC2:
            return snprintf(buffer, size, "(%.2f, %.2f)",
                            obs->value.vec2.x, obs->value.vec2.y);
        case AGENTITE_VM_TYPE_VEC3:
            return snprintf(buffer, size, "(%.2f, %.2f, %.2f)",
                            obs->value.vec3.x, obs->value.vec3.y, obs->value.vec3.z);
        case AGENTITE_VM_TYPE_VEC4:
            return snprintf(buffer, size, "(%.2f, %.2f, %.2f, %.2f)",
                            obs->value.vec4.x, obs->value.vec4.y,
                            obs->value.vec4.z, obs->value.vec4.w);
        default:
            return snprintf(buffer, size, "?");
    }
}

int agentite_vm_format_ex(const Agentite_ViewModel *vm,
                         uint32_t id,
                         char *buffer,
                         size_t size,
                         const char *format) {
    if (!vm || !buffer || size == 0 || !format) return 0;

    VMObservable *obs = find_observable((Agentite_ViewModel *)vm, id);
    if (!obs) return 0;

    switch (obs->value.type) {
        case AGENTITE_VM_TYPE_INT:
            return snprintf(buffer, size, format, obs->value.i32);
        case AGENTITE_VM_TYPE_INT64:
            return snprintf(buffer, size, format, obs->value.i64);
        case AGENTITE_VM_TYPE_FLOAT:
            return snprintf(buffer, size, format, obs->value.f32);
        case AGENTITE_VM_TYPE_DOUBLE:
            return snprintf(buffer, size, format, obs->value.f64);
        case AGENTITE_VM_TYPE_BOOL:
            return snprintf(buffer, size, format, obs->value.b ? 1 : 0);
        case AGENTITE_VM_TYPE_STRING:
            return snprintf(buffer, size, format, obs->value.str);
        default:
            return agentite_vm_format(vm, id, buffer, size);
    }
}

/*============================================================================
 * Computed Values
 *============================================================================*/

uint32_t agentite_vm_define_computed(Agentite_ViewModel *vm,
                                    const char *name,
                                    Agentite_VMType type,
                                    Agentite_VMComputed compute,
                                    void *userdata,
                                    const uint32_t *dependencies,
                                    int dep_count) {
    if (!vm || !name || !compute) return AGENTITE_VM_INVALID_ID;

    if (dep_count > 8) dep_count = 8;

    /* Initial value */
    Agentite_VMValue initial = { .type = type };
    uint32_t id = define_observable(vm, name, &initial);
    if (id == AGENTITE_VM_INVALID_ID) return id;

    VMObservable *obs = find_observable(vm, id);
    if (!obs) return AGENTITE_VM_INVALID_ID;

    obs->is_computed = true;
    obs->compute = compute;
    obs->compute_userdata = userdata;
    obs->dep_count = dep_count;
    for (int i = 0; i < dep_count; i++) {
        obs->dependencies[i] = dependencies[i];
    }

    /* Calculate initial value */
    obs->value = compute(vm, id, userdata);

    return id;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *agentite_vm_type_name(Agentite_VMType type) {
    switch (type) {
        case AGENTITE_VM_TYPE_NONE:    return "none";
        case AGENTITE_VM_TYPE_INT:     return "int";
        case AGENTITE_VM_TYPE_INT64:   return "int64";
        case AGENTITE_VM_TYPE_FLOAT:   return "float";
        case AGENTITE_VM_TYPE_DOUBLE:  return "double";
        case AGENTITE_VM_TYPE_BOOL:    return "bool";
        case AGENTITE_VM_TYPE_STRING:  return "string";
        case AGENTITE_VM_TYPE_POINTER: return "pointer";
        case AGENTITE_VM_TYPE_VEC2:    return "vec2";
        case AGENTITE_VM_TYPE_VEC3:    return "vec3";
        case AGENTITE_VM_TYPE_VEC4:    return "vec4";
        default: return "unknown";
    }
}

bool agentite_vm_values_equal(const Agentite_VMValue *a, const Agentite_VMValue *b) {
    if (!a || !b) return false;
    if (a->type != b->type) return false;

    switch (a->type) {
        case AGENTITE_VM_TYPE_INT:
            return a->i32 == b->i32;
        case AGENTITE_VM_TYPE_INT64:
            return a->i64 == b->i64;
        case AGENTITE_VM_TYPE_FLOAT:
            return a->f32 == b->f32;
        case AGENTITE_VM_TYPE_DOUBLE:
            return a->f64 == b->f64;
        case AGENTITE_VM_TYPE_BOOL:
            return a->b == b->b;
        case AGENTITE_VM_TYPE_STRING:
            return strcmp(a->str, b->str) == 0;
        case AGENTITE_VM_TYPE_POINTER:
            return a->ptr == b->ptr;
        case AGENTITE_VM_TYPE_VEC2:
            return a->vec2.x == b->vec2.x && a->vec2.y == b->vec2.y;
        case AGENTITE_VM_TYPE_VEC3:
            return a->vec3.x == b->vec3.x && a->vec3.y == b->vec3.y &&
                   a->vec3.z == b->vec3.z;
        case AGENTITE_VM_TYPE_VEC4:
            return a->vec4.x == b->vec4.x && a->vec4.y == b->vec4.y &&
                   a->vec4.z == b->vec4.z && a->vec4.w == b->vec4.w;
        default:
            return false;
    }
}

void agentite_vm_value_copy(Agentite_VMValue *dest, const Agentite_VMValue *src) {
    if (!dest || !src) return;
    *dest = *src;
}

void agentite_vm_value_clear(Agentite_VMValue *value) {
    if (!value) return;
    Agentite_VMType type = value->type;
    memset(value, 0, sizeof(Agentite_VMValue));
    value->type = type;
}
