/*
 * Carbon Tilemap System Implementation
 *
 * Chunk-based tile storage for efficient large map rendering.
 */

#include "agentite/agentite.h"
#include "agentite/tilemap.h"
#include "agentite/sprite.h"
#include "agentite/camera.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/* Chunk: 32x32 tiles */
typedef struct Agentite_TileChunk {
    Agentite_TileID tiles[AGENTITE_TILEMAP_CHUNK_SIZE * AGENTITE_TILEMAP_CHUNK_SIZE];
    uint32_t tile_count;  /* Non-empty tile count (skip chunk if 0) */
} Agentite_TileChunk;

/* Layer: sparse 2D array of chunks */
struct Agentite_TileLayer {
    char *name;
    Agentite_TileChunk **chunks;  /* Flat array: chunks[cy * chunks_x + cx] */
    int chunks_x;               /* Number of chunks in X */
    int chunks_y;               /* Number of chunks in Y */
    bool visible;
    float opacity;
};

/* Tileset: texture divided into tiles */
struct Agentite_Tileset {
    Agentite_Texture *texture;
    Agentite_Sprite *sprites;     /* Pre-computed sprite per tile */
    int tile_width;
    int tile_height;
    int columns;                /* Tiles per row in texture */
    int rows;                   /* Tile rows in texture */
    int tile_count;
    int spacing;
    int margin;
};

/* Tilemap: layers + tileset */
struct Agentite_Tilemap {
    Agentite_Tileset *tileset;
    Agentite_TileLayer *layers[AGENTITE_TILEMAP_MAX_LAYERS];
    int layer_count;
    int width;                  /* Map width in tiles */
    int height;                 /* Map height in tiles */
    int tile_width;             /* Tile width in pixels */
    int tile_height;            /* Tile height in pixels */
    int chunks_x;               /* Chunks per row */
    int chunks_y;               /* Chunks per column */
};

/* ============================================================================
 * Tileset Functions
 * ============================================================================ */

Agentite_Tileset *agentite_tileset_create(Agentite_Texture *texture,
                                      int tile_width, int tile_height)
{
    return agentite_tileset_create_ex(texture, tile_width, tile_height, 0, 0);
}

Agentite_Tileset *agentite_tileset_create_ex(Agentite_Texture *texture,
                                         int tile_width, int tile_height,
                                         int spacing, int margin)
{
    if (!texture || tile_width <= 0 || tile_height <= 0) return NULL;

    Agentite_Tileset *ts = AGENTITE_ALLOC(Agentite_Tileset);
    if (!ts) return NULL;

    ts->texture = texture;
    ts->tile_width = tile_width;
    ts->tile_height = tile_height;
    ts->spacing = spacing;
    ts->margin = margin;

    /* Calculate tileset dimensions */
    int tex_w, tex_h;
    agentite_texture_get_size(texture, &tex_w, &tex_h);

    int usable_w = tex_w - 2 * margin;
    int usable_h = tex_h - 2 * margin;

    ts->columns = (usable_w + spacing) / (tile_width + spacing);
    ts->rows = (usable_h + spacing) / (tile_height + spacing);
    ts->tile_count = ts->columns * ts->rows;

    if (ts->tile_count <= 0) {
        free(ts);
        return NULL;
    }

    /* Pre-compute sprites for each tile */
    ts->sprites = (Agentite_Sprite*)malloc(ts->tile_count * sizeof(Agentite_Sprite));
    if (!ts->sprites) {
        free(ts);
        return NULL;
    }

    for (int i = 0; i < ts->tile_count; i++) {
        int tx = i % ts->columns;
        int ty = i / ts->columns;

        float src_x = (float)(margin + tx * (tile_width + spacing));
        float src_y = (float)(margin + ty * (tile_height + spacing));

        ts->sprites[i] = agentite_sprite_create(texture, src_x, src_y,
                                              (float)tile_width, (float)tile_height);
        /* Set origin to top-left for tilemap rendering */
        agentite_sprite_set_origin(&ts->sprites[i], 0.0f, 0.0f);
    }

    return ts;
}

void agentite_tileset_destroy(Agentite_Tileset *tileset)
{
    if (!tileset) return;
    free(tileset->sprites);
    free(tileset);
}

void agentite_tileset_get_tile_size(const Agentite_Tileset *tileset, int *width, int *height)
{
    if (!tileset) return;
    if (width) *width = tileset->tile_width;
    if (height) *height = tileset->tile_height;
}

int agentite_tileset_get_tile_count(const Agentite_Tileset *tileset)
{
    return tileset ? tileset->tile_count : 0;
}

/* ============================================================================
 * Internal Layer Functions
 * ============================================================================ */

static Agentite_TileLayer *layer_create(const char *name, int chunks_x, int chunks_y)
{
    Agentite_TileLayer *layer = AGENTITE_ALLOC(Agentite_TileLayer);
    if (!layer) return NULL;

    layer->name = name ? strdup(name) : NULL;
    layer->chunks_x = chunks_x;
    layer->chunks_y = chunks_y;
    layer->visible = true;
    layer->opacity = 1.0f;

    /* Allocate chunk pointer array (initially all NULL) */
    int total_chunks = chunks_x * chunks_y;
    layer->chunks = (Agentite_TileChunk**)calloc(total_chunks, sizeof(Agentite_TileChunk *));
    if (!layer->chunks) {
        free(layer->name);
        free(layer);
        return NULL;
    }

    return layer;
}

static void layer_destroy(Agentite_TileLayer *layer)
{
    if (!layer) return;

    /* Free all allocated chunks */
    int total_chunks = layer->chunks_x * layer->chunks_y;
    for (int i = 0; i < total_chunks; i++) {
        free(layer->chunks[i]);
    }
    free(layer->chunks);
    free(layer->name);
    free(layer);
}

static Agentite_TileChunk *layer_get_chunk(Agentite_TileLayer *layer, int cx, int cy)
{
    if (!layer || cx < 0 || cy < 0 || cx >= layer->chunks_x || cy >= layer->chunks_y) {
        return NULL;
    }
    return layer->chunks[cy * layer->chunks_x + cx];
}

static const Agentite_TileChunk *layer_get_chunk_const(const Agentite_TileLayer *layer, int cx, int cy)
{
    if (!layer || cx < 0 || cy < 0 || cx >= layer->chunks_x || cy >= layer->chunks_y) {
        return NULL;
    }
    return layer->chunks[cy * layer->chunks_x + cx];
}

static Agentite_TileChunk *layer_ensure_chunk(Agentite_TileLayer *layer, int cx, int cy)
{
    if (!layer || cx < 0 || cy < 0 || cx >= layer->chunks_x || cy >= layer->chunks_y) {
        return NULL;
    }

    int idx = cy * layer->chunks_x + cx;
    if (!layer->chunks[idx]) {
        layer->chunks[idx] = AGENTITE_ALLOC(Agentite_TileChunk);
    }
    return layer->chunks[idx];
}

/* ============================================================================
 * Tilemap Lifecycle Functions
 * ============================================================================ */

Agentite_Tilemap *agentite_tilemap_create(Agentite_Tileset *tileset,
                                      int width, int height)
{
    if (!tileset || width <= 0 || height <= 0) return NULL;

    Agentite_Tilemap *tm = AGENTITE_ALLOC(Agentite_Tilemap);
    if (!tm) return NULL;

    tm->tileset = tileset;
    tm->width = width;
    tm->height = height;
    tm->tile_width = tileset->tile_width;
    tm->tile_height = tileset->tile_height;

    /* Calculate chunk grid dimensions */
    tm->chunks_x = (width + AGENTITE_TILEMAP_CHUNK_SIZE - 1) / AGENTITE_TILEMAP_CHUNK_SIZE;
    tm->chunks_y = (height + AGENTITE_TILEMAP_CHUNK_SIZE - 1) / AGENTITE_TILEMAP_CHUNK_SIZE;

    return tm;
}

void agentite_tilemap_destroy(Agentite_Tilemap *tilemap)
{
    if (!tilemap) return;

    /* Destroy all layers */
    for (int i = 0; i < tilemap->layer_count; i++) {
        layer_destroy(tilemap->layers[i]);
    }

    free(tilemap);
}

void agentite_tilemap_get_size(const Agentite_Tilemap *tilemap, int *width, int *height)
{
    if (!tilemap) return;
    if (width) *width = tilemap->width;
    if (height) *height = tilemap->height;
}

void agentite_tilemap_get_tile_size(const Agentite_Tilemap *tilemap, int *width, int *height)
{
    if (!tilemap) return;
    if (width) *width = tilemap->tile_width;
    if (height) *height = tilemap->tile_height;
}

/* ============================================================================
 * Layer Functions
 * ============================================================================ */

int agentite_tilemap_add_layer(Agentite_Tilemap *tilemap, const char *name)
{
    if (!tilemap || tilemap->layer_count >= AGENTITE_TILEMAP_MAX_LAYERS) {
        return -1;
    }

    Agentite_TileLayer *layer = layer_create(name, tilemap->chunks_x, tilemap->chunks_y);
    if (!layer) return -1;

    int index = tilemap->layer_count;
    tilemap->layers[index] = layer;
    tilemap->layer_count++;

    return index;
}

Agentite_TileLayer *agentite_tilemap_get_layer(Agentite_Tilemap *tilemap, int index)
{
    if (!tilemap || index < 0 || index >= tilemap->layer_count) {
        return NULL;
    }
    return tilemap->layers[index];
}

/* Internal const helper for getters */
static const Agentite_TileLayer *tilemap_get_layer_const(const Agentite_Tilemap *tilemap, int index)
{
    if (!tilemap || index < 0 || index >= tilemap->layer_count) {
        return NULL;
    }
    return tilemap->layers[index];
}

Agentite_TileLayer *agentite_tilemap_get_layer_by_name(Agentite_Tilemap *tilemap,
                                                   const char *name)
{
    if (!tilemap || !name) return NULL;

    for (int i = 0; i < tilemap->layer_count; i++) {
        if (tilemap->layers[i]->name && strcmp(tilemap->layers[i]->name, name) == 0) {
            return tilemap->layers[i];
        }
    }
    return NULL;
}

int agentite_tilemap_get_layer_count(const Agentite_Tilemap *tilemap)
{
    return tilemap ? tilemap->layer_count : 0;
}

void agentite_tilemap_set_layer_visible(Agentite_Tilemap *tilemap, int layer, bool visible)
{
    Agentite_TileLayer *l = agentite_tilemap_get_layer(tilemap, layer);
    if (l) l->visible = visible;
}

bool agentite_tilemap_get_layer_visible(const Agentite_Tilemap *tilemap, int layer)
{
    const Agentite_TileLayer *l = tilemap_get_layer_const(tilemap, layer);
    return l ? l->visible : false;
}

void agentite_tilemap_set_layer_opacity(Agentite_Tilemap *tilemap, int layer, float opacity)
{
    Agentite_TileLayer *l = agentite_tilemap_get_layer(tilemap, layer);
    if (l) {
        if (opacity < 0.0f) opacity = 0.0f;
        if (opacity > 1.0f) opacity = 1.0f;
        l->opacity = opacity;
    }
}

float agentite_tilemap_get_layer_opacity(const Agentite_Tilemap *tilemap, int layer)
{
    const Agentite_TileLayer *l = tilemap_get_layer_const(tilemap, layer);
    return l ? l->opacity : 0.0f;
}

/* ============================================================================
 * Tile Access Functions
 * ============================================================================ */

void agentite_tilemap_set_tile(Agentite_Tilemap *tilemap, int layer,
                             int x, int y, Agentite_TileID tile)
{
    if (!tilemap || x < 0 || y < 0 || x >= tilemap->width || y >= tilemap->height) {
        return;
    }

    Agentite_TileLayer *l = agentite_tilemap_get_layer(tilemap, layer);
    if (!l) return;

    /* Calculate chunk and local coordinates */
    int cx = x / AGENTITE_TILEMAP_CHUNK_SIZE;
    int cy = y / AGENTITE_TILEMAP_CHUNK_SIZE;
    int lx = x % AGENTITE_TILEMAP_CHUNK_SIZE;
    int ly = y % AGENTITE_TILEMAP_CHUNK_SIZE;

    Agentite_TileChunk *chunk = layer_ensure_chunk(l, cx, cy);
    if (!chunk) return;

    int idx = ly * AGENTITE_TILEMAP_CHUNK_SIZE + lx;
    Agentite_TileID old_tile = chunk->tiles[idx];

    /* Update tile count */
    if (old_tile == AGENTITE_TILE_EMPTY && tile != AGENTITE_TILE_EMPTY) {
        chunk->tile_count++;
    } else if (old_tile != AGENTITE_TILE_EMPTY && tile == AGENTITE_TILE_EMPTY) {
        chunk->tile_count--;
    }

    chunk->tiles[idx] = tile;
}

Agentite_TileID agentite_tilemap_get_tile(const Agentite_Tilemap *tilemap, int layer,
                                      int x, int y)
{
    if (!tilemap || x < 0 || y < 0 || x >= tilemap->width || y >= tilemap->height) {
        return AGENTITE_TILE_EMPTY;
    }

    const Agentite_TileLayer *l = tilemap_get_layer_const(tilemap, layer);
    if (!l) return AGENTITE_TILE_EMPTY;

    int cx = x / AGENTITE_TILEMAP_CHUNK_SIZE;
    int cy = y / AGENTITE_TILEMAP_CHUNK_SIZE;
    int lx = x % AGENTITE_TILEMAP_CHUNK_SIZE;
    int ly = y % AGENTITE_TILEMAP_CHUNK_SIZE;

    const Agentite_TileChunk *chunk = layer_get_chunk_const(l, cx, cy);
    if (!chunk) return AGENTITE_TILE_EMPTY;

    return chunk->tiles[ly * AGENTITE_TILEMAP_CHUNK_SIZE + lx];
}

void agentite_tilemap_fill(Agentite_Tilemap *tilemap, int layer,
                         int x, int y, int width, int height,
                         Agentite_TileID tile)
{
    if (!tilemap) return;

    /* Clamp to map bounds */
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > tilemap->width) width = tilemap->width - x;
    if (y + height > tilemap->height) height = tilemap->height - y;
    if (width <= 0 || height <= 0) return;

    for (int ty = y; ty < y + height; ty++) {
        for (int tx = x; tx < x + width; tx++) {
            agentite_tilemap_set_tile(tilemap, layer, tx, ty, tile);
        }
    }
}

void agentite_tilemap_clear_layer(Agentite_Tilemap *tilemap, int layer)
{
    Agentite_TileLayer *l = agentite_tilemap_get_layer(tilemap, layer);
    if (!l) return;

    int total_chunks = l->chunks_x * l->chunks_y;
    for (int i = 0; i < total_chunks; i++) {
        if (l->chunks[i]) {
            memset(l->chunks[i]->tiles, 0, sizeof(l->chunks[i]->tiles));
            l->chunks[i]->tile_count = 0;
        }
    }
}

/* ============================================================================
 * Rendering Functions
 * ============================================================================ */

void agentite_tilemap_render_layer(Agentite_Tilemap *tilemap,
                                 Agentite_SpriteRenderer *sr,
                                 Agentite_Camera *camera,
                                 int layer_idx)
{
    if (!tilemap || !sr) return;

    Agentite_TileLayer *layer = agentite_tilemap_get_layer(tilemap, layer_idx);
    if (!layer || !layer->visible) return;

    /* Get visible world bounds */
    float left, right, top, bottom;
    if (camera) {
        agentite_camera_get_bounds(camera, &left, &right, &top, &bottom);
    } else {
        /* No camera - assume screen bounds at origin */
        left = 0;
        top = 0;
        right = (float)(tilemap->width * tilemap->tile_width);
        bottom = (float)(tilemap->height * tilemap->tile_height);
    }

    /* Convert world bounds to chunk range (with 1-chunk padding for safety) */
    float chunk_world_w = AGENTITE_TILEMAP_CHUNK_SIZE * tilemap->tile_width;
    float chunk_world_h = AGENTITE_TILEMAP_CHUNK_SIZE * tilemap->tile_height;

    int chunk_min_x = (int)floorf(left / chunk_world_w) - 1;
    int chunk_max_x = (int)ceilf(right / chunk_world_w) + 1;
    int chunk_min_y = (int)floorf(top / chunk_world_h) - 1;
    int chunk_max_y = (int)ceilf(bottom / chunk_world_h) + 1;

    /* Clamp to tilemap chunk bounds */
    if (chunk_min_x < 0) chunk_min_x = 0;
    if (chunk_min_y < 0) chunk_min_y = 0;
    if (chunk_max_x > tilemap->chunks_x) chunk_max_x = tilemap->chunks_x;
    if (chunk_max_y > tilemap->chunks_y) chunk_max_y = tilemap->chunks_y;

    float opacity = layer->opacity;
    Agentite_Tileset *ts = tilemap->tileset;

    /* Render visible chunks */
    for (int cy = chunk_min_y; cy < chunk_max_y; cy++) {
        for (int cx = chunk_min_x; cx < chunk_max_x; cx++) {
            Agentite_TileChunk *chunk = layer_get_chunk(layer, cx, cy);
            if (!chunk || chunk->tile_count == 0) continue;

            /* Base world position of this chunk */
            int base_tile_x = cx * AGENTITE_TILEMAP_CHUNK_SIZE;
            int base_tile_y = cy * AGENTITE_TILEMAP_CHUNK_SIZE;

            /* Render tiles in this chunk */
            for (int ly = 0; ly < AGENTITE_TILEMAP_CHUNK_SIZE; ly++) {
                int tile_y = base_tile_y + ly;
                if (tile_y >= tilemap->height) break;

                for (int lx = 0; lx < AGENTITE_TILEMAP_CHUNK_SIZE; lx++) {
                    int tile_x = base_tile_x + lx;
                    if (tile_x >= tilemap->width) break;

                    Agentite_TileID tile_id = chunk->tiles[ly * AGENTITE_TILEMAP_CHUNK_SIZE + lx];
                    if (tile_id == AGENTITE_TILE_EMPTY) continue;

                    /* Tile ID is 1-based, sprite array is 0-based */
                    int sprite_idx = tile_id - 1;
                    if (sprite_idx < 0 || sprite_idx >= ts->tile_count) continue;

                    Agentite_Sprite *sprite = &ts->sprites[sprite_idx];
                    float world_x = (float)(tile_x * tilemap->tile_width);
                    float world_y = (float)(tile_y * tilemap->tile_height);

                    agentite_sprite_draw_tinted(sr, sprite, world_x, world_y,
                                              1.0f, 1.0f, 1.0f, opacity);
                }
            }
        }
    }
}

void agentite_tilemap_render(Agentite_Tilemap *tilemap,
                           Agentite_SpriteRenderer *sr,
                           Agentite_Camera *camera)
{
    if (!tilemap || !sr) return;

    /* Render layers in order (0 = back, N = front) */
    for (int i = 0; i < tilemap->layer_count; i++) {
        agentite_tilemap_render_layer(tilemap, sr, camera, i);
    }
}

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

void agentite_tilemap_world_to_tile(const Agentite_Tilemap *tilemap,
                                  float world_x, float world_y,
                                  int *tile_x, int *tile_y)
{
    if (!tilemap) return;

    if (tile_x) *tile_x = (int)floorf(world_x / tilemap->tile_width);
    if (tile_y) *tile_y = (int)floorf(world_y / tilemap->tile_height);
}

void agentite_tilemap_tile_to_world(const Agentite_Tilemap *tilemap,
                                  int tile_x, int tile_y,
                                  float *world_x, float *world_y)
{
    if (!tilemap) return;

    if (world_x) *world_x = (float)(tile_x * tilemap->tile_width);
    if (world_y) *world_y = (float)(tile_y * tilemap->tile_height);
}

Agentite_TileID agentite_tilemap_get_tile_at_world(const Agentite_Tilemap *tilemap,
                                               int layer,
                                               float world_x, float world_y)
{
    if (!tilemap) return AGENTITE_TILE_EMPTY;

    int tx, ty;
    agentite_tilemap_world_to_tile(tilemap, world_x, world_y, &tx, &ty);
    return agentite_tilemap_get_tile(tilemap, layer, tx, ty);
}

void agentite_tilemap_get_world_bounds(const Agentite_Tilemap *tilemap,
                                     float *left, float *right,
                                     float *top, float *bottom)
{
    if (!tilemap) return;

    if (left) *left = 0.0f;
    if (top) *top = 0.0f;
    if (right) *right = (float)(tilemap->width * tilemap->tile_width);
    if (bottom) *bottom = (float)(tilemap->height * tilemap->tile_height);
}
