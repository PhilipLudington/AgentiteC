#ifndef CARBON_CONTAINERS_H
#define CARBON_CONTAINERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * Carbon Container Utilities
 *
 * Generic container macros and utilities for common patterns:
 * - Dynamic arrays with type safety
 * - Random selection utilities
 * - Weighted random choice
 * - Fisher-Yates shuffle
 *
 * Usage:
 *   // Dynamic array
 *   Carbon_Array(int) numbers;
 *   carbon_array_init(&numbers);
 *   carbon_array_push(&numbers, 42);
 *   carbon_array_push(&numbers, 17);
 *   int val = numbers.data[0];  // 42
 *   carbon_array_free(&numbers);
 *
 *   // Random selection
 *   int items[] = {1, 2, 3, 4, 5};
 *   int chosen = carbon_random_choice(items, 5);
 *
 *   // Shuffle
 *   carbon_shuffle_array(items, 5);
 */

/*============================================================================
 * Dynamic Array Macros
 *============================================================================*/

/**
 * Declare a dynamic array type.
 * Creates an anonymous struct with data pointer, count, and capacity.
 *
 * Example:
 *   Carbon_Array(float) values;
 *   Carbon_Array(struct Entity) entities;
 */
#define Carbon_Array(T) struct { T *data; size_t count; size_t capacity; }

/**
 * Initialize a dynamic array to empty state.
 */
#define carbon_array_init(arr) \
    do { \
        (arr)->data = NULL; \
        (arr)->count = 0; \
        (arr)->capacity = 0; \
    } while(0)

/**
 * Free a dynamic array and reset to empty state.
 */
#define carbon_array_free(arr) \
    do { \
        free((arr)->data); \
        (arr)->data = NULL; \
        (arr)->count = 0; \
        (arr)->capacity = 0; \
    } while(0)

/**
 * Push an item to the end of the array.
 * Automatically grows the array if needed.
 */
#define carbon_array_push(arr, item) \
    do { \
        if ((arr)->count >= (arr)->capacity) { \
            size_t _new_cap = (arr)->capacity == 0 ? 8 : (arr)->capacity * 2; \
            void *_new_data = realloc((arr)->data, _new_cap * sizeof(*(arr)->data)); \
            if (_new_data) { \
                (arr)->data = _new_data; \
                (arr)->capacity = _new_cap; \
            } \
        } \
        if ((arr)->count < (arr)->capacity) { \
            (arr)->data[(arr)->count++] = (item); \
        } \
    } while(0)

/**
 * Pop and return the last item from the array.
 * Warning: Returns undefined value if array is empty.
 */
#define carbon_array_pop(arr) \
    ((arr)->count > 0 ? (arr)->data[--(arr)->count] : (arr)->data[0])

/**
 * Get the last item without removing it.
 * Warning: Returns undefined value if array is empty.
 */
#define carbon_array_last(arr) \
    ((arr)->data[(arr)->count > 0 ? (arr)->count - 1 : 0])

/**
 * Clear the array (set count to 0, keep capacity).
 */
#define carbon_array_clear(arr) \
    do { (arr)->count = 0; } while(0)

/**
 * Remove item at index, shifting remaining elements.
 * Preserves order. O(n) complexity.
 */
#define carbon_array_remove(arr, index) \
    do { \
        if ((size_t)(index) < (arr)->count) { \
            if ((size_t)(index) < (arr)->count - 1) { \
                memmove(&(arr)->data[index], &(arr)->data[(index) + 1], \
                        ((arr)->count - (index) - 1) * sizeof(*(arr)->data)); \
            } \
            (arr)->count--; \
        } \
    } while(0)

/**
 * Remove item at index by swapping with last element.
 * Does not preserve order. O(1) complexity.
 */
#define carbon_array_remove_swap(arr, index) \
    do { \
        if ((size_t)(index) < (arr)->count) { \
            if ((size_t)(index) < (arr)->count - 1) { \
                (arr)->data[index] = (arr)->data[(arr)->count - 1]; \
            } \
            (arr)->count--; \
        } \
    } while(0)

/**
 * Reserve capacity for at least `cap` elements.
 */
#define carbon_array_reserve(arr, cap) \
    do { \
        if ((cap) > (arr)->capacity) { \
            void *_new_data = realloc((arr)->data, (cap) * sizeof(*(arr)->data)); \
            if (_new_data) { \
                (arr)->data = _new_data; \
                (arr)->capacity = (cap); \
            } \
        } \
    } while(0)

/**
 * Resize array to exactly `new_count` elements.
 * New elements are zero-initialized.
 */
#define carbon_array_resize(arr, new_count) \
    do { \
        if ((new_count) > (arr)->capacity) { \
            carbon_array_reserve((arr), (new_count)); \
        } \
        if ((new_count) > (arr)->count) { \
            memset(&(arr)->data[(arr)->count], 0, \
                   ((new_count) - (arr)->count) * sizeof(*(arr)->data)); \
        } \
        (arr)->count = (new_count); \
    } while(0)

/**
 * Shrink capacity to match count.
 */
#define carbon_array_shrink(arr) \
    do { \
        if ((arr)->count > 0 && (arr)->count < (arr)->capacity) { \
            void *_new_data = realloc((arr)->data, (arr)->count * sizeof(*(arr)->data)); \
            if (_new_data) { \
                (arr)->data = _new_data; \
                (arr)->capacity = (arr)->count; \
            } \
        } else if ((arr)->count == 0) { \
            free((arr)->data); \
            (arr)->data = NULL; \
            (arr)->capacity = 0; \
        } \
    } while(0)

/**
 * Insert item at index, shifting elements.
 */
#define carbon_array_insert(arr, index, item) \
    do { \
        if ((arr)->count >= (arr)->capacity) { \
            size_t _new_cap = (arr)->capacity == 0 ? 8 : (arr)->capacity * 2; \
            void *_new_data = realloc((arr)->data, _new_cap * sizeof(*(arr)->data)); \
            if (_new_data) { \
                (arr)->data = _new_data; \
                (arr)->capacity = _new_cap; \
            } \
        } \
        if ((arr)->count < (arr)->capacity) { \
            size_t _idx = (index) > (arr)->count ? (arr)->count : (index); \
            if (_idx < (arr)->count) { \
                memmove(&(arr)->data[_idx + 1], &(arr)->data[_idx], \
                        ((arr)->count - _idx) * sizeof(*(arr)->data)); \
            } \
            (arr)->data[_idx] = (item); \
            (arr)->count++; \
        } \
    } while(0)

/**
 * Check if array contains item (linear search).
 * Requires == comparison to work for the type.
 */
#define carbon_array_contains(arr, item) \
    ({ \
        bool _found = false; \
        for (size_t _i = 0; _i < (arr)->count; _i++) { \
            if ((arr)->data[_i] == (item)) { _found = true; break; } \
        } \
        _found; \
    })

/**
 * Find index of item (linear search).
 * Returns SIZE_MAX if not found.
 */
#define carbon_array_find(arr, item) \
    ({ \
        size_t _idx = SIZE_MAX; \
        for (size_t _i = 0; _i < (arr)->count; _i++) { \
            if ((arr)->data[_i] == (item)) { _idx = _i; break; } \
        } \
        _idx; \
    })

/**
 * Iterate over array elements.
 *
 * Example:
 *   carbon_array_foreach(arr, int, item) {
 *       printf("%d\n", item);
 *   }
 */
#define carbon_array_foreach(arr, type, var) \
    for (size_t _foreach_i = 0; _foreach_i < (arr)->count && ((var) = (arr)->data[_foreach_i], 1); _foreach_i++)

/**
 * Iterate with index.
 *
 * Example:
 *   carbon_array_foreach_i(arr, i, int, item) {
 *       printf("[%zu] = %d\n", i, item);
 *   }
 */
#define carbon_array_foreach_i(arr, idx_var, type, var) \
    for (size_t idx_var = 0; idx_var < (arr)->count && ((var) = (arr)->data[idx_var], 1); idx_var++)

/*============================================================================
 * Random Number Utilities
 *============================================================================*/

/**
 * Seed the random number generator.
 * Uses SDL_rand internally.
 *
 * @param seed Seed value (0 for time-based seed)
 */
void carbon_random_seed(uint64_t seed);

/**
 * Get a random integer in range [min, max] (inclusive).
 *
 * @param min Minimum value
 * @param max Maximum value
 * @return Random integer in range
 */
int carbon_rand_int(int min, int max);

/**
 * Get a random float in range [min, max).
 *
 * @param min Minimum value (inclusive)
 * @param max Maximum value (exclusive)
 * @return Random float in range
 */
float carbon_rand_float(float min, float max);

/**
 * Get a random boolean.
 *
 * @return Random true or false
 */
bool carbon_rand_bool(void);

/**
 * Get a random index in range [0, count).
 *
 * @param count Upper bound (exclusive)
 * @return Random index
 */
size_t carbon_rand_index(size_t count);

/**
 * Get a random float in range [0.0, 1.0).
 *
 * @return Random float
 */
float carbon_rand_normalized(void);

/**
 * Random choice from array.
 * Returns element at random index.
 *
 * Example:
 *   int items[] = {10, 20, 30};
 *   int chosen = carbon_random_choice(items, 3);  // Returns 10, 20, or 30
 */
#define carbon_random_choice(arr, count) \
    ((count) > 0 ? (arr)[carbon_rand_index(count)] : (arr)[0])

/**
 * Random choice from Carbon_Array.
 */
#define carbon_array_random_choice(arr) \
    ((arr)->count > 0 ? (arr)->data[carbon_rand_index((arr)->count)] : (arr)->data[0])

/*============================================================================
 * Weighted Random Selection
 *============================================================================*/

/**
 * Item with associated weight for weighted random selection.
 */
typedef struct Carbon_WeightedItem {
    size_t index;       /* Index into original array */
    float weight;       /* Selection weight (must be >= 0) */
} Carbon_WeightedItem;

/**
 * Select a random index based on weights.
 * Higher weights = higher probability of selection.
 *
 * @param items Array of weighted items
 * @param count Number of items
 * @return Selected item's index field, or 0 if count is 0
 */
size_t carbon_weighted_random(const Carbon_WeightedItem *items, size_t count);

/**
 * Select from an array of floats interpreted as weights.
 * Returns index of selected weight.
 *
 * @param weights Array of weights
 * @param count Number of weights
 * @return Selected index, or 0 if count is 0
 */
size_t carbon_weighted_random_simple(const float *weights, size_t count);

/*============================================================================
 * Shuffle
 *============================================================================*/

/**
 * Shuffle array in place using Fisher-Yates algorithm.
 *
 * @param array Pointer to first element
 * @param count Number of elements
 * @param element_size Size of each element in bytes
 */
void carbon_shuffle(void *array, size_t count, size_t element_size);

/**
 * Shuffle array macro (type-safe wrapper).
 *
 * Example:
 *   int items[] = {1, 2, 3, 4, 5};
 *   carbon_shuffle_array(items, 5);
 */
#define carbon_shuffle_array(arr, count) \
    carbon_shuffle((arr), (count), sizeof(*(arr)))

/**
 * Shuffle a Carbon_Array in place.
 */
#define carbon_array_shuffle(arr) \
    carbon_shuffle((arr)->data, (arr)->count, sizeof(*(arr)->data))

/*============================================================================
 * Min/Max Utilities
 *============================================================================*/

/**
 * Find minimum value in array.
 */
#define carbon_array_min(arr) \
    ({ \
        __typeof__(*(arr)->data) _min = (arr)->data[0]; \
        for (size_t _i = 1; _i < (arr)->count; _i++) { \
            if ((arr)->data[_i] < _min) _min = (arr)->data[_i]; \
        } \
        _min; \
    })

/**
 * Find maximum value in array.
 */
#define carbon_array_max(arr) \
    ({ \
        __typeof__(*(arr)->data) _max = (arr)->data[0]; \
        for (size_t _i = 1; _i < (arr)->count; _i++) { \
            if ((arr)->data[_i] > _max) _max = (arr)->data[_i]; \
        } \
        _max; \
    })

/**
 * Find index of minimum value.
 */
#define carbon_array_min_index(arr) \
    ({ \
        size_t _min_idx = 0; \
        for (size_t _i = 1; _i < (arr)->count; _i++) { \
            if ((arr)->data[_i] < (arr)->data[_min_idx]) _min_idx = _i; \
        } \
        _min_idx; \
    })

/**
 * Find index of maximum value.
 */
#define carbon_array_max_index(arr) \
    ({ \
        size_t _max_idx = 0; \
        for (size_t _i = 1; _i < (arr)->count; _i++) { \
            if ((arr)->data[_i] > (arr)->data[_max_idx]) _max_idx = _i; \
        } \
        _max_idx; \
    })

/*============================================================================
 * Sum/Average Utilities
 *============================================================================*/

/**
 * Sum all elements in numeric array.
 * Returns 0 for empty arrays.
 */
#define carbon_array_sum(arr) \
    ({ \
        __typeof__(*(arr)->data) _sum = 0; \
        for (size_t _i = 0; _i < (arr)->count; _i++) { \
            _sum += (arr)->data[_i]; \
        } \
        _sum; \
    })

/**
 * Average of all elements in numeric array.
 * Returns 0 for empty arrays.
 */
#define carbon_array_avg(arr) \
    ((arr)->count > 0 ? (double)carbon_array_sum(arr) / (double)(arr)->count : 0.0)

/*============================================================================
 * Type-safe Stack Operations
 *============================================================================*/

/**
 * Stack is just an alias for array used in stack context.
 */
#define Carbon_Stack(T) Carbon_Array(T)

#define carbon_stack_init(s)     carbon_array_init(s)
#define carbon_stack_free(s)     carbon_array_free(s)
#define carbon_stack_push(s, v)  carbon_array_push(s, v)
#define carbon_stack_pop(s)      carbon_array_pop(s)
#define carbon_stack_peek(s)     carbon_array_last(s)
#define carbon_stack_empty(s)    ((s)->count == 0)
#define carbon_stack_size(s)     ((s)->count)

/*============================================================================
 * Ring Buffer
 *============================================================================*/

/**
 * Fixed-size ring buffer.
 */
#define Carbon_RingBuffer(T, N) struct { T data[N]; size_t head; size_t tail; size_t count; }

#define carbon_ring_init(rb) \
    do { (rb)->head = 0; (rb)->tail = 0; (rb)->count = 0; } while(0)

#define carbon_ring_capacity(rb) \
    (sizeof((rb)->data) / sizeof((rb)->data[0]))

#define carbon_ring_full(rb) \
    ((rb)->count >= carbon_ring_capacity(rb))

#define carbon_ring_empty(rb) \
    ((rb)->count == 0)

#define carbon_ring_push(rb, item) \
    do { \
        if (!carbon_ring_full(rb)) { \
            (rb)->data[(rb)->tail] = (item); \
            (rb)->tail = ((rb)->tail + 1) % carbon_ring_capacity(rb); \
            (rb)->count++; \
        } \
    } while(0)

#define carbon_ring_pop(rb) \
    ({ \
        __typeof__((rb)->data[0]) _val = (rb)->data[(rb)->head]; \
        if ((rb)->count > 0) { \
            (rb)->head = ((rb)->head + 1) % carbon_ring_capacity(rb); \
            (rb)->count--; \
        } \
        _val; \
    })

#define carbon_ring_peek(rb) \
    ((rb)->data[(rb)->head])

#endif /* CARBON_CONTAINERS_H */
