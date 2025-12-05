#include "carbon/containers.h"
#include <SDL3/SDL.h>
#include <time.h>

/*============================================================================
 * Random Number Generator State
 *============================================================================*/

static bool s_random_initialized = false;

/*============================================================================
 * Random Number Utilities
 *============================================================================*/

void carbon_random_seed(uint64_t seed) {
    if (seed == 0) {
        seed = (uint64_t)time(NULL) ^ (uint64_t)SDL_GetPerformanceCounter();
    }
    srand((unsigned int)(seed & 0xFFFFFFFF));
    s_random_initialized = true;
}

static void ensure_random_init(void) {
    if (!s_random_initialized) {
        carbon_random_seed(0);
    }
}

int carbon_rand_int(int min, int max) {
    ensure_random_init();

    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }

    if (min == max) {
        return min;
    }

    /* Use SDL's random for better distribution */
    uint32_t range = (uint32_t)(max - min + 1);
    uint32_t r = (uint32_t)rand();
    return min + (int)(r % range);
}

float carbon_rand_float(float min, float max) {
    ensure_random_init();

    if (min > max) {
        float temp = min;
        min = max;
        max = temp;
    }

    float normalized = (float)rand() / (float)RAND_MAX;
    return min + normalized * (max - min);
}

bool carbon_rand_bool(void) {
    ensure_random_init();
    return (rand() & 1) != 0;
}

size_t carbon_rand_index(size_t count) {
    ensure_random_init();

    if (count == 0) {
        return 0;
    }

    return (size_t)rand() % count;
}

float carbon_rand_normalized(void) {
    ensure_random_init();
    return (float)rand() / (float)RAND_MAX;
}

/*============================================================================
 * Weighted Random Selection
 *============================================================================*/

size_t carbon_weighted_random(const Carbon_WeightedItem *items, size_t count) {
    if (!items || count == 0) {
        return 0;
    }

    /* Calculate total weight */
    float total = 0.0f;
    for (size_t i = 0; i < count; i++) {
        if (items[i].weight > 0) {
            total += items[i].weight;
        }
    }

    if (total <= 0.0f) {
        /* All weights are zero or negative, fall back to uniform random */
        return items[carbon_rand_index(count)].index;
    }

    /* Select random value in [0, total) */
    float target = carbon_rand_float(0.0f, total);

    /* Find which item the target falls into */
    float cumulative = 0.0f;
    for (size_t i = 0; i < count; i++) {
        if (items[i].weight > 0) {
            cumulative += items[i].weight;
            if (target < cumulative) {
                return items[i].index;
            }
        }
    }

    /* Fallback (shouldn't happen with proper float math) */
    return items[count - 1].index;
}

size_t carbon_weighted_random_simple(const float *weights, size_t count) {
    if (!weights || count == 0) {
        return 0;
    }

    /* Calculate total weight */
    float total = 0.0f;
    for (size_t i = 0; i < count; i++) {
        if (weights[i] > 0) {
            total += weights[i];
        }
    }

    if (total <= 0.0f) {
        /* All weights are zero or negative, fall back to uniform random */
        return carbon_rand_index(count);
    }

    /* Select random value in [0, total) */
    float target = carbon_rand_float(0.0f, total);

    /* Find which index the target falls into */
    float cumulative = 0.0f;
    for (size_t i = 0; i < count; i++) {
        if (weights[i] > 0) {
            cumulative += weights[i];
            if (target < cumulative) {
                return i;
            }
        }
    }

    /* Fallback */
    return count - 1;
}

/*============================================================================
 * Shuffle
 *============================================================================*/

void carbon_shuffle(void *array, size_t count, size_t element_size) {
    if (!array || count <= 1 || element_size == 0) {
        return;
    }

    ensure_random_init();

    unsigned char *arr = (unsigned char *)array;

    /* Allocate temporary buffer for swapping */
    unsigned char *temp = malloc(element_size);
    if (!temp) {
        return;  /* Can't shuffle without temp buffer */
    }

    /* Fisher-Yates shuffle */
    for (size_t i = count - 1; i > 0; i--) {
        size_t j = carbon_rand_index(i + 1);

        if (i != j) {
            /* Swap elements at i and j */
            unsigned char *elem_i = arr + (i * element_size);
            unsigned char *elem_j = arr + (j * element_size);

            memcpy(temp, elem_i, element_size);
            memcpy(elem_i, elem_j, element_size);
            memcpy(elem_j, temp, element_size);
        }
    }

    free(temp);
}
