/*
 * Carbon UI - ID Generation and State Management
 */

#include "carbon/ui.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * FNV-1a Hash Implementation
 * ============================================================================ */

#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME        16777619u

CUI_Id cui_id(const char *str)
{
    uint32_t hash = FNV_OFFSET_BASIS;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= FNV_PRIME;
    }
    return hash;
}

CUI_Id cui_id_int(const char *str, int n)
{
    uint32_t hash = cui_id(str);
    hash ^= (uint32_t)n;
    hash *= FNV_PRIME;
    return hash;
}

/* Internal: Generate ID with context's ID stack */
CUI_Id cui_make_id(CUI_Context *ctx, const char *str)
{
    CUI_Id id = cui_id(str);
    /* Combine with parent IDs from stack */
    for (int i = 0; i < ctx->id_stack_depth; i++) {
        id ^= ctx->id_stack[i];
        id *= FNV_PRIME;
    }
    return id;
}

CUI_Id cui_make_id_int(CUI_Context *ctx, const char *str, int n)
{
    CUI_Id id = cui_id_int(str, n);
    for (int i = 0; i < ctx->id_stack_depth; i++) {
        id ^= ctx->id_stack[i];
        id *= FNV_PRIME;
    }
    return id;
}

/* ============================================================================
 * ID Stack (for scoping)
 * ============================================================================ */

void cui_push_id(CUI_Context *ctx, const char *str)
{
    if (ctx->id_stack_depth < 32) {
        ctx->id_stack[ctx->id_stack_depth++] = cui_id(str);
    }
}

void cui_push_id_int(CUI_Context *ctx, int n)
{
    if (ctx->id_stack_depth < 32) {
        uint32_t hash = FNV_OFFSET_BASIS;
        hash ^= (uint32_t)n;
        hash *= FNV_PRIME;
        ctx->id_stack[ctx->id_stack_depth++] = hash;
    }
}

void cui_pop_id(CUI_Context *ctx)
{
    if (ctx->id_stack_depth > 0) {
        ctx->id_stack_depth--;
    }
}

/* ============================================================================
 * State Hash Table
 * ============================================================================ */

/* Get hash table bucket index */
static int cui_state_bucket(CUI_Id id)
{
    return (int)(id & 0xFF);
}

CUI_WidgetState *cui_get_state(CUI_Context *ctx, CUI_Id id)
{
    if (id == CUI_ID_NONE) {
        return NULL;
    }

    int bucket = cui_state_bucket(id);
    CUI_StateEntry *entry = ctx->state_table[bucket];

    /* Search for existing entry */
    while (entry) {
        if (entry->state.id == id) {
            entry->state.last_frame = ctx->frame_count;
            return &entry->state;
        }
        entry = entry->next;
    }

    /* Create new entry */
    entry = (CUI_StateEntry *)calloc(1, sizeof(CUI_StateEntry));
    if (!entry) {
        return NULL;
    }

    entry->state.id = id;
    entry->state.last_frame = ctx->frame_count;
    entry->next = ctx->state_table[bucket];
    ctx->state_table[bucket] = entry;

    return &entry->state;
}

/* Free all state entries (called on shutdown) */
void cui_state_clear(CUI_Context *ctx)
{
    for (int i = 0; i < 256; i++) {
        CUI_StateEntry *entry = ctx->state_table[i];
        while (entry) {
            CUI_StateEntry *next = entry->next;
            free(entry);
            entry = next;
        }
        ctx->state_table[i] = NULL;
    }
}

/* Garbage collect old entries (call periodically) */
void cui_state_gc(CUI_Context *ctx, uint64_t max_age)
{
    for (int i = 0; i < 256; i++) {
        CUI_StateEntry **pp = &ctx->state_table[i];
        while (*pp) {
            CUI_StateEntry *entry = *pp;
            if (ctx->frame_count - entry->state.last_frame > max_age) {
                /* Remove stale entry */
                *pp = entry->next;
                free(entry);
            } else {
                pp = &entry->next;
            }
        }
    }
}
