/*
 * Agentite UI - ID Generation and State Management
 */

#include "agentite/ui.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * FNV-1a Hash Implementation
 * ============================================================================ */

#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME        16777619u

AUI_Id aui_id(const char *str)
{
    uint32_t hash = FNV_OFFSET_BASIS;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= FNV_PRIME;
    }
    return hash;
}

AUI_Id aui_id_int(const char *str, int n)
{
    uint32_t hash = aui_id(str);
    hash ^= (uint32_t)n;
    hash *= FNV_PRIME;
    return hash;
}

/* Internal: Generate ID with context's ID stack */
AUI_Id aui_make_id(AUI_Context *ctx, const char *str)
{
    AUI_Id id = aui_id(str);
    /* Combine with parent IDs from stack */
    for (int i = 0; i < ctx->id_stack_depth; i++) {
        id ^= ctx->id_stack[i];
        id *= FNV_PRIME;
    }
    return id;
}

AUI_Id aui_make_id_int(AUI_Context *ctx, const char *str, int n)
{
    AUI_Id id = aui_id_int(str, n);
    for (int i = 0; i < ctx->id_stack_depth; i++) {
        id ^= ctx->id_stack[i];
        id *= FNV_PRIME;
    }
    return id;
}

/* ============================================================================
 * ID Stack (for scoping)
 * ============================================================================ */

void aui_push_id(AUI_Context *ctx, const char *str)
{
    if (ctx->id_stack_depth < 32) {
        ctx->id_stack[ctx->id_stack_depth++] = aui_id(str);
    }
}

void aui_push_id_int(AUI_Context *ctx, int n)
{
    if (ctx->id_stack_depth < 32) {
        uint32_t hash = FNV_OFFSET_BASIS;
        hash ^= (uint32_t)n;
        hash *= FNV_PRIME;
        ctx->id_stack[ctx->id_stack_depth++] = hash;
    }
}

void aui_pop_id(AUI_Context *ctx)
{
    if (ctx->id_stack_depth > 0) {
        ctx->id_stack_depth--;
    }
}

/* ============================================================================
 * State Hash Table
 * ============================================================================ */

/* Get hash table bucket index */
static int aui_state_bucket(AUI_Id id)
{
    return (int)(id & 0xFF);
}

AUI_WidgetState *aui_get_state(AUI_Context *ctx, AUI_Id id)
{
    if (id == AUI_ID_NONE) {
        return NULL;
    }

    int bucket = aui_state_bucket(id);
    AUI_StateEntry *entry = ctx->state_table[bucket];

    /* Search for existing entry */
    while (entry) {
        if (entry->state.id == id) {
            entry->state.last_frame = ctx->frame_count;
            return &entry->state;
        }
        entry = entry->next;
    }

    /* Create new entry */
    entry = (AUI_StateEntry *)calloc(1, sizeof(AUI_StateEntry));
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
void aui_state_clear(AUI_Context *ctx)
{
    for (int i = 0; i < 256; i++) {
        AUI_StateEntry *entry = ctx->state_table[i];
        while (entry) {
            AUI_StateEntry *next = entry->next;
            free(entry);
            entry = next;
        }
        ctx->state_table[i] = NULL;
    }
}

/* Garbage collect old entries (call periodically) */
void aui_state_gc(AUI_Context *ctx, uint64_t max_age)
{
    for (int i = 0; i < 256; i++) {
        AUI_StateEntry **pp = &ctx->state_table[i];
        while (*pp) {
            AUI_StateEntry *entry = *pp;
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
