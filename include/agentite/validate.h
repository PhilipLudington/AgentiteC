#ifndef AGENTITE_VALIDATE_H
#define AGENTITE_VALIDATE_H

#include "agentite/error.h"
#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>

/**
 * Carbon Validation Framework
 *
 * Macro-based validation utilities for early-return error checking.
 * Integrates with Carbon's error system for consistent error reporting.
 *
 * Usage:
 *   bool do_something(const char *name, int count) {
 *       AGENTITE_VALIDATE_PTR_RET(name, false);
 *       AGENTITE_VALIDATE_RANGE_RET(count, 1, 100, false);
 *       // ... actual implementation ...
 *       return true;
 *   }
 *
 *   void do_something_void(void *ptr) {
 *       AGENTITE_VALIDATE_PTR(ptr);
 *       // ... implementation ...
 *   }
 */

/*============================================================================
 * Pointer Validation
 *============================================================================*/

/**
 * Validate pointer is not NULL (void return).
 * Sets error and returns if pointer is NULL.
 */
#define AGENTITE_VALIDATE_PTR(ptr) \
    do { \
        if (!(ptr)) { \
            agentite_set_error("%s: null pointer: %s", __func__, #ptr); \
            return; \
        } \
    } while(0)

/**
 * Validate pointer is not NULL (with return value).
 * Sets error and returns specified value if pointer is NULL.
 */
#define AGENTITE_VALIDATE_PTR_RET(ptr, ret) \
    do { \
        if (!(ptr)) { \
            agentite_set_error("%s: null pointer: %s", __func__, #ptr); \
            return (ret); \
        } \
    } while(0)

/**
 * Validate multiple pointers at once (void return).
 * More efficient than multiple AGENTITE_VALIDATE_PTR calls.
 */
#define AGENTITE_VALIDATE_PTRS2(p1, p2) \
    do { \
        if (!(p1)) { agentite_set_error("%s: null pointer: %s", __func__, #p1); return; } \
        if (!(p2)) { agentite_set_error("%s: null pointer: %s", __func__, #p2); return; } \
    } while(0)

#define AGENTITE_VALIDATE_PTRS3(p1, p2, p3) \
    do { \
        if (!(p1)) { agentite_set_error("%s: null pointer: %s", __func__, #p1); return; } \
        if (!(p2)) { agentite_set_error("%s: null pointer: %s", __func__, #p2); return; } \
        if (!(p3)) { agentite_set_error("%s: null pointer: %s", __func__, #p3); return; } \
    } while(0)

/**
 * Validate multiple pointers (with return value).
 */
#define AGENTITE_VALIDATE_PTRS2_RET(p1, p2, ret) \
    do { \
        if (!(p1)) { agentite_set_error("%s: null pointer: %s", __func__, #p1); return (ret); } \
        if (!(p2)) { agentite_set_error("%s: null pointer: %s", __func__, #p2); return (ret); } \
    } while(0)

#define AGENTITE_VALIDATE_PTRS3_RET(p1, p2, p3, ret) \
    do { \
        if (!(p1)) { agentite_set_error("%s: null pointer: %s", __func__, #p1); return (ret); } \
        if (!(p2)) { agentite_set_error("%s: null pointer: %s", __func__, #p2); return (ret); } \
        if (!(p3)) { agentite_set_error("%s: null pointer: %s", __func__, #p3); return (ret); } \
    } while(0)

/*============================================================================
 * ID/Handle Validation
 *============================================================================*/

/**
 * Validate ID is not invalid (void return).
 * Useful for entity IDs, handle values, etc.
 */
#define AGENTITE_VALIDATE_ID(id, invalid_value) \
    do { \
        if ((id) == (invalid_value)) { \
            agentite_set_error("%s: invalid ID: %s", __func__, #id); \
            return; \
        } \
    } while(0)

/**
 * Validate ID is not invalid (with return value).
 */
#define AGENTITE_VALIDATE_ID_RET(id, invalid_value, ret) \
    do { \
        if ((id) == (invalid_value)) { \
            agentite_set_error("%s: invalid ID: %s", __func__, #id); \
            return (ret); \
        } \
    } while(0)

/**
 * Validate index is within bounds (void return).
 */
#define AGENTITE_VALIDATE_INDEX(index, count) \
    do { \
        if ((size_t)(index) >= (size_t)(count)) { \
            agentite_set_error("%s: index out of bounds: %s (%zu >= %zu)", \
                            __func__, #index, (size_t)(index), (size_t)(count)); \
            return; \
        } \
    } while(0)

/**
 * Validate index is within bounds (with return value).
 */
#define AGENTITE_VALIDATE_INDEX_RET(index, count, ret) \
    do { \
        if ((size_t)(index) >= (size_t)(count)) { \
            agentite_set_error("%s: index out of bounds: %s (%zu >= %zu)", \
                            __func__, #index, (size_t)(index), (size_t)(count)); \
            return (ret); \
        } \
    } while(0)

/*============================================================================
 * Range Validation
 *============================================================================*/

/**
 * Validate value is within range [min, max] (void return).
 */
#define AGENTITE_VALIDATE_RANGE(val, min, max) \
    do { \
        if ((val) < (min) || (val) > (max)) { \
            agentite_set_error("%s: %s out of range [%d, %d]: %d", \
                            __func__, #val, (int)(min), (int)(max), (int)(val)); \
            return; \
        } \
    } while(0)

/**
 * Validate value is within range [min, max] (with return value).
 */
#define AGENTITE_VALIDATE_RANGE_RET(val, min, max, ret) \
    do { \
        if ((val) < (min) || (val) > (max)) { \
            agentite_set_error("%s: %s out of range [%d, %d]: %d", \
                            __func__, #val, (int)(min), (int)(max), (int)(val)); \
            return (ret); \
        } \
    } while(0)

/**
 * Validate float value is within range (void return).
 */
#define AGENTITE_VALIDATE_RANGE_F(val, min, max) \
    do { \
        if ((val) < (min) || (val) > (max)) { \
            agentite_set_error("%s: %s out of range [%.2f, %.2f]: %.2f", \
                            __func__, #val, (float)(min), (float)(max), (float)(val)); \
            return; \
        } \
    } while(0)

/**
 * Validate float value is within range (with return value).
 */
#define AGENTITE_VALIDATE_RANGE_F_RET(val, min, max, ret) \
    do { \
        if ((val) < (min) || (val) > (max)) { \
            agentite_set_error("%s: %s out of range [%.2f, %.2f]: %.2f", \
                            __func__, #val, (float)(min), (float)(max), (float)(val)); \
            return (ret); \
        } \
    } while(0)

/**
 * Validate value is positive (> 0).
 */
#define AGENTITE_VALIDATE_POSITIVE(val) \
    do { \
        if ((val) <= 0) { \
            agentite_set_error("%s: %s must be positive: %d", __func__, #val, (int)(val)); \
            return; \
        } \
    } while(0)

#define AGENTITE_VALIDATE_POSITIVE_RET(val, ret) \
    do { \
        if ((val) <= 0) { \
            agentite_set_error("%s: %s must be positive: %d", __func__, #val, (int)(val)); \
            return (ret); \
        } \
    } while(0)

/**
 * Validate value is non-negative (>= 0).
 */
#define AGENTITE_VALIDATE_NON_NEGATIVE(val) \
    do { \
        if ((val) < 0) { \
            agentite_set_error("%s: %s must be non-negative: %d", __func__, #val, (int)(val)); \
            return; \
        } \
    } while(0)

#define AGENTITE_VALIDATE_NON_NEGATIVE_RET(val, ret) \
    do { \
        if ((val) < 0) { \
            agentite_set_error("%s: %s must be non-negative: %d", __func__, #val, (int)(val)); \
            return (ret); \
        } \
    } while(0)

/*============================================================================
 * String Validation
 *============================================================================*/

/**
 * Validate string is not NULL or empty (void return).
 */
#define AGENTITE_VALIDATE_STRING(str) \
    do { \
        if (!(str) || (str)[0] == '\0') { \
            agentite_set_error("%s: null or empty string: %s", __func__, #str); \
            return; \
        } \
    } while(0)

/**
 * Validate string is not NULL or empty (with return value).
 */
#define AGENTITE_VALIDATE_STRING_RET(str, ret) \
    do { \
        if (!(str) || (str)[0] == '\0') { \
            agentite_set_error("%s: null or empty string: %s", __func__, #str); \
            return (ret); \
        } \
    } while(0)

/*============================================================================
 * Condition Validation
 *============================================================================*/

/**
 * Validate arbitrary condition (void return).
 */
#define AGENTITE_VALIDATE_COND(cond, msg) \
    do { \
        if (!(cond)) { \
            agentite_set_error("%s: %s", __func__, (msg)); \
            return; \
        } \
    } while(0)

/**
 * Validate arbitrary condition (with return value).
 */
#define AGENTITE_VALIDATE_COND_RET(cond, msg, ret) \
    do { \
        if (!(cond)) { \
            agentite_set_error("%s: %s", __func__, (msg)); \
            return (ret); \
        } \
    } while(0)

/*============================================================================
 * ECS Entity Validation
 *============================================================================*/

/**
 * Validate ECS entity is valid (non-zero).
 * Note: For full validation with ecs_is_alive(), use AGENTITE_VALIDATE_ENTITY_ALIVE.
 */
#define AGENTITE_VALIDATE_ENTITY(entity) \
    do { \
        if ((entity) == 0) { \
            agentite_set_error("%s: invalid entity: %s", __func__, #entity); \
            return; \
        } \
    } while(0)

#define AGENTITE_VALIDATE_ENTITY_RET(entity, ret) \
    do { \
        if ((entity) == 0) { \
            agentite_set_error("%s: invalid entity: %s", __func__, #entity); \
            return (ret); \
        } \
    } while(0)

/**
 * Validate ECS entity is alive in world.
 * Requires Flecs headers and a valid world pointer.
 */
#define AGENTITE_VALIDATE_ENTITY_ALIVE(world, entity) \
    do { \
        if ((entity) == 0 || !ecs_is_alive((world), (entity))) { \
            agentite_set_error("%s: entity not alive: %s", __func__, #entity); \
            return; \
        } \
    } while(0)

#define AGENTITE_VALIDATE_ENTITY_ALIVE_RET(world, entity, ret) \
    do { \
        if ((entity) == 0 || !ecs_is_alive((world), (entity))) { \
            agentite_set_error("%s: entity not alive: %s", __func__, #entity); \
            return (ret); \
        } \
    } while(0)

/*============================================================================
 * Debug Assertions
 *============================================================================*/

/**
 * Debug-only assertion (compiled out in release builds).
 * Use for internal consistency checks, not for user input validation.
 */
#ifdef DEBUG
#define AGENTITE_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            SDL_Log("ASSERT FAILED: %s at %s:%d in %s", \
                    #cond, __FILE__, __LINE__, __func__); \
            abort(); \
        } \
    } while(0)

#define AGENTITE_ASSERT_MSG(cond, msg) \
    do { \
        if (!(cond)) { \
            SDL_Log("ASSERT FAILED: %s - %s at %s:%d in %s", \
                    #cond, (msg), __FILE__, __LINE__, __func__); \
            abort(); \
        } \
    } while(0)

/**
 * Debug-only unreachable marker.
 */
#define AGENTITE_UNREACHABLE() \
    do { \
        SDL_Log("UNREACHABLE code reached at %s:%d in %s", \
                __FILE__, __LINE__, __func__); \
        abort(); \
    } while(0)

#else
#define AGENTITE_ASSERT(cond) ((void)0)
#define AGENTITE_ASSERT_MSG(cond, msg) ((void)0)
#define AGENTITE_UNREACHABLE() ((void)0)
#endif

/*============================================================================
 * Soft Validation (Warnings)
 *============================================================================*/

/**
 * Log warning but continue execution.
 * Use when invalid input should be handled gracefully.
 */
#define AGENTITE_WARN_IF_NULL(ptr) \
    do { \
        if (!(ptr)) { \
            SDL_Log("WARNING: %s: null pointer: %s", __func__, #ptr); \
        } \
    } while(0)

#define AGENTITE_WARN_IF(cond, msg) \
    do { \
        if (cond) { \
            SDL_Log("WARNING: %s: %s", __func__, (msg)); \
        } \
    } while(0)

/*============================================================================
 * Return Value Helpers
 *============================================================================*/

/**
 * Common return value constants for use with validation macros.
 */
#define AGENTITE_INVALID_ID    0
#define AGENTITE_INVALID_INDEX ((size_t)-1)

/**
 * Quick check and return NULL pattern.
 */
#define AGENTITE_RETURN_NULL_IF(cond) \
    do { if (cond) return NULL; } while(0)

/**
 * Quick check and return false pattern.
 */
#define AGENTITE_RETURN_FALSE_IF(cond) \
    do { if (cond) return false; } while(0)

/**
 * Quick check and return pattern.
 */
#define AGENTITE_RETURN_IF(cond) \
    do { if (cond) return; } while(0)

#define AGENTITE_RETURN_VAL_IF(cond, val) \
    do { if (cond) return (val); } while(0)

#endif /* AGENTITE_VALIDATE_H */
