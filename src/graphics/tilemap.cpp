/*
 * Carbon Tilemap System Implementation
 *
 * Chunk-based tile storage for efficient large map rendering.
 */

#include "carbon/carbon.h"
#include "carbon/tilemap.h"
#include "carbon/sprite.h"
#include "carbon/camera.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/* Chunk: 32x32 tiles */
typedef struct Carbon_TileChunk {
    Carbon_TileID tiles[CARBON_TILEMAP_CHUNK_SIZE * CARBON_TILEMAP_CHUNK_SIZE];
    uint32_t tile_count;  /* Non-empty tile count (skip chunk if 0) */
} Carbon_TileChunk;

/* Layer: sparse 2D array of chunks */
struct Carbon_TileLayer {
    char *name;
    Carbon_TileChunk **chunks;  /* Flat array: chunks[cy * chunks_x + cx] */
    int chunks_x;               /* Number of chunks in X */
    int chunks_y;               /* Number of chunks in Y */
    bool visible;
    float opacity;
};

/* Tileset: texture divided into tiles */
struct Carbon_Tileset {
    Carbon_Texture *texture;
    Carbon_Sprite *sprites;     /* Pre-computed sprite per tile */
    int tile_width;
    int tile_height;
    int columns;                /* Tiles per row in texture */
    int rows;                   /* Tile rows in texture */
    int tile_count;
    int spacing;
    int margin;
};

/* Tilemap: layers + tileset */
struct Carbon_Tilemap {
    Carbon_Tileset *tileset;
    Carbon_TileLayer *layers[CARBON_TILEMAP_MAX_LAYERS];
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

Carbon_Tileset *carbon_tileset_create(Carbon_Texture *texture,
                                      int tile_width, int tile_height)
{
    return carbon_tileset_create_ex(texture, tile_width, tile_height, 0, 0);
}

Carbon_Tileset *carbon_tileset_create_ex(Carbon_Texture *texture,
                                         int tile_width, int tile_height,
                                         int spacing, int margin)
{
    if (!texture || tile_width <= 0 || tile_height <= 0) return NULL;

    Carbon_Tileset *ts = CARBON_ALLOC(Carbon_Tileset);
    if (!ts) return NULL;

    ts->texture = texture;
    ts->tile_width = tile_width;
    ts->tile_height = tile_height;
    ts->spacing = spacing;
    ts->margin = margin;

    /* Calculate tileset dimensions */
    int tex_w, tex_h;
    carbon_texture_get_size(texture, &tex_w, &tex_h);

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
    ts->sprites = (Carbon_Sprite*)malloc(ts->tile_count * sizeof(Carbon_Sprite));
    if (!ts->sprites) {
        free(ts);
        return NULL;
    }

    for (int i = 0; i < ts->tile_count; i++) {
        int tx = i % ts->columns;
        int ty = i / ts->columns;

        float src_x = (float)(margin + tx * (tile_width + spacing));
        float src_y = (float)(margin + ty * (tile_height + spacing));

        ts->sprites[i] = carbon_sprite_create(texture, src_x, src_y,
                                              (float)tile_width, (float)tile_height);
        /* Set origin to top-left for tilemap rendering */
        carbon_sprite_set_origin(&ts->sprites[i], 0.0f, 0.0f);
    }

    return ts;
}

void carbon_tileset_destroy(Carbon_Tileset *tileset)
{
    if (!tileset) return;
    free(tileset->sprites);
    free(tileset);
}

void carbon_tileset_get_tile_size(Carbon_Tileset *tileset, int *width, int *height)
{
    if (!tileset) return;
    if (width) *width = tileset->tile_width;
    if (height) *height = tileset->tile_height;
}

int carbon_tileset_get_tile_count(Carbon_Tileset *tileset)
{
    return tileset ? tileset->tile_count : 0;
}

/* ============================================================================
 * Internal Layer Functions
 * ============================================================================ */

static Carbon_TileLayer *layer_create(const char *name, int chunks_x, int chunks_y)
{
    Carbon_TileLayer *layer = CARBON_ALLOC(Carbon_TileLayer);
    if (!layer) return NULL;

    layer->name = name ? strdup(name) : NULL;
    layer->chunks_x = chunks_x;
    layer->chunks_y = chunks_y;
    layer->visible = true;
    layer->opacity = 1.0f;

    /* Allocate chunk pointer array (initially all NULL) */
    int total_chunks = chunks_x * chunks_y;
    layer->chunks = (Carbon_TileChunk**)calloc(total_chunks, sizeof(Carbon_TileChunk *));
    if (!layer->chunks) {
        free(layer->name);
        free(layer);
        return NULL;
    }

    return layer;
}

static void layer_destroy(Carbon_TileLayer *layer)
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

static Carbon_TileChunk *layer_get_chunk(Carbon_TileLayer *layer, int cx, int cy)
{
    if (!layer || cx < 0 || cy < 0 || cx >= layer->chunks_x || cy >= layer->chunks_y) {
        return NULL;
    }
    return layer->chunks[cy * layer->chunks_x + cx];
}

static Carbon_TileChunk *layer_ensure_chunk(Carbon_TileLayer *layer, int cx, int cy)
{
    if (!layer || cx < 0 || cy < 0 || cx >= layer->chunks_x || cy >= layer->chunks_y) {
        return NULL;
    }

    int idx = cy * layer->chunks_x + cx;
    if (!layer->chunks[idx]) {
        layer->chunks[idx] = CARBON_ALLOC(Carbon_TileChunk);
    }
    return layer->chunks[idx];
}

/* ============================================================================
 * Tilemap Lifecycle Functions
 * ============================================================================ */

Carbon_Tilemap *carbon_tilemap_create(Carbon_Tileset *tileset,
                                      int width, int height)
{
    if (!tileset || width <= 0 || height <= 0) return NULL;

    Carbon_Tilemap *tm = CARBON_ALLOC(Carbon_Tilemap);
    if (!tm) return NULL;

    tm->tileset = tileset;
    tm->width = width;
    tm->height = height;
    tm->tile_width = tileset->tile_width;
    tm->tile_height = tileset->tile_height;

    /* Calculate chunk grid dimensions */
    tm->chunks_x = (width + CARBON_TILEMAP_CHUNK_SIZE - 1) / CARBON_TILEMAP_CHUNK_SIZE;
    tm->chunks_y = (height + CARBON_TILEMAP_CHUNK_SIZE - 1) / CARBON_TILEMAP_CHUNK_SIZE;

    return tm;
}

void carbon_tilemap_destroy(Carbon_Tilemap *tilemap)
{
    if (!tilemap) return;

    /* Destroy all layers */
    for (int i = 0; i < tilemap->layer_count; i++) {
        layer_destroy(tilemap->layers[i]);
    }

    free(tilemap);
}

void carbon_tilemap_get_size(Carbon_Tilemap *tilemap, int *width, int *height)
{
    if (!tilemap) return;
    if (width) *width = tilemap->width;
    if (height) *height = tilemap->height;
}

void carbon_tilemap_get_tile_size(Carbon_Tilemap *tilemap, int *width, int *height)
{
    if (!tilemap) return;
    if (width) *width = tilemap->tile_width;
    if (height) *height = tilemap->tile_height;
}

/* ============================================================================
 * Layer Functions
 * ============================================================================ */

int carbon_tilemap_add_layer(Carbon_Tilemap *tilemap, const char *name)
{
    if (!tilemap || tilemap->layer_count >= CARBON_TILEMAP_MAX_LAYERS) {
        return -1;
    }

    Carbon_TileLayer *layer = layer_create(name, tilemap->chunks_x, tilemap->chunks_y);
    if (!layer) return -1;

    int index = tilemap->layer_count;
    tilemap->layers[index] = layer;
    tilemap->layer_count++;

    return index;
}

Carbon_TileLayer *carbon_tilemap_get_layer(Carbon_Tilemap *tilemap, int index)
{
    if (!tilemap || index < 0 || index >= tilemap->layer_count) {
        return NULL;
    }
    return tilemap->layers[index];
}

Carbon_TileLayer *carbon_tilemap_get_layer_by_name(Carbon_Tilemap *tilemap,
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

int carbon_tilemap_get_layer_count(Carbon_Tilemap *tilemap)
{
    return tilemap ? tilemap->layer_count : 0;
}

void carbon_tilemap_set_layer_visible(Carbon_Tilemap *tilemap, int layer, bool visible)
{
    Carbon_TileLayer *l = carbon_tilemap_get_layer(tilemap, layer);
    if (l) l->visible = visible;
}

bool carbon_tilemap_get_layer_visible(Carbon_Tilemap *tilemap, int layer)
{
    Carbon_TileLayer *l = carbon_tilemap_get_layer(tilemap, layer);
    return l ? l->visible : false;
}

void carbon_tilemap_set_layer_opacity(Carbon_Tilemap *tilemap, int layer, float opacity)
{
    Carbon_TileLayer *l = carbon_tilemap_get_layer(tilemap, layer);
    if (l) {
        if (opacity < 0.0f) opacity = 0.0f;
        if (opacity > 1.0f) opacity = 1.0f;
        l->opacity = opacity;
    }
}

float carbon_tilemap_get_layer_opacity(Carbon_Tilemap *tilemap, int layer)
{
    Carbon_TileLayer *l = carbon_tilemap_get_layer(tilemap, layer);
    return l ? l->opacity : 0.0f;
}

/* ============================================================================
 * Tile Access Functions
 * ============================================================================ */

void carbon_tilemap_set_tile(Carbon_Tilemap *tilemap, int layer,
                             int x, int y, Carbon_TileID tile)
{
    if (!tilemap || x < 0 || y < 0 || x >= tilemap->width || y >= tilemap->height) {
        return;
    }

    Carbon_TileLayer *l = carbon_tilemap_get_layer(tilemap, layer);
    if (!l) return;

    /* Calculate chunk and local coordinates */
    int cx = x / CARBON_TILEMAP_CHUNK_SIZE;
    int cy = y / CARBON_TILEMAP_CHUNK_SIZE;
    int lx = x % CARBON_TILEMAP_CHUNK_SIZE;
    int ly = y % CARBON_TILEMAP_CHUNK_SIZE;

    Carbon_TileChunk *chunk = layer_ensure_chunk(l, cx, cy);
    if (!chunk) return;

    int idx = ly * CARBON_TILEMAP_CHUNK_SIZE + lx;
    Carbon_TileID old_tile = chunk->tiles[idx];

    /* Update tile count */
    if (old_tile == CARBON_TILE_EMPTY && tile != CARBON_TILE_EMPTY) {
        chunk->tile_count++;
    } else if (old_tile != CARBON_TILE_EMPTY && tile == CARBON_TILE_EMPTY) {
        chunk->tile_count--;
    }

    chunk->tiles[idx] = tile;
}

Carbon_TileID carbon_tilemap_get_tile(Carbon_Tilemap *tilemap, int layer,
                                      int x, int y)
{
    if (!tilemap || x < 0 || y < 0 || x >= tilemap->width || y >= tilemap->height) {
        return CARBON_TILE_EMPTY;
    }

    Carbon_TileLayer *l = carbon_tilemap_get_layer(tilemap, layer);
    if (!l) return CARBON_TILE_EMPTY;

    int cx = x / CARBON_TILEMAP_CHUNK_SIZE;
    int cy = y / CARBON_TILEMAP_CHUNK_SIZE;
    int lx = x % CARBON_TILEMAP_CHUNK_SIZE;
    int ly = y % CARBON_TILEMAP_CHUNK_SIZE;

    Carbon_TileChunk *chunk = layer_get_chunk(l, cx, cy);
    if (!chunk) return CARBON_TILE_EMPTY;

    return chunk->tiles[ly * CARBON_TILEMAP_CHUNK_SIZE + lx];
}

void carbon_tilemap_fill(Carbon_Tilemap *tilemap, int layer,
                         int x, int y, int width, int height,
                         Carbon_TileID tile)
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
            carbon_tilemap_set_tile(tilemap, layer, tx, ty, tile);
        }
    }
}

void carbon_tilemap_clear_layer(Carbon_Tilemap *tilemap, int layer)
{
    Carbon_TileLayer *l = carbon_tilemap_get_layer(tilemap, layer);
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

void carbon_tilemap_render_layer(Carbon_Tilemap *tilemap,
                                 Carbon_SpriteRenderer *sr,
                                 Carbon_Camera *camera,
                                 int layer_idx)
{
    if (!tilemap || !sr) return;

    Carbon_TileLayer *layer = carbon_tilemap_get_layer(tilemap, layer_idx);
    if (!layer || !layer->visible) return;

    /* Get visible world bounds */
    float left, right, top, bottom;
    if (camera) {
        carbon_camera_get_bounds(camera, &left, &right, &top, &bottom);
    } else {
        /* No camera - assume screen bounds at origin */
        left = 0;
        top = 0;
        right = (float)(tilemap->width * tilemap->tile_width);
        bottom = (float)(tilemap->height * tilemap->tile_height);
    }

    /* Convert world bounds to chunk range (with 1-chunk padding for safety) */
    float chunk_world_w = CARBON_TILEMAP_CHUNK_SIZE * tilemap->tile_width;
    float chunk_world_h = CARBON_TILEMAP_CHUNK_SIZE * tilemap->tile_height;

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
    Carbon_Tileset *ts = tilemap->tileset;

    /* Render visible chunks */
    for (int cy = chunk_min_y; cy < chunk_max_y; cy++) {
        for (int cx = chunk_min_x; cx < chunk_max_x; cx++) {
            Carbon_TileChunk *chunk = layer_get_chunk(layer, cx, cy);
            if (!chunk || chunk->tile_count == 0) continue;

            /* Base world position of this chunk */
            int base_tile_x = cx * CARBON_TILEMAP_CHUNK_SIZE;
            int base_tile_y = cy * CARBON_TILEMAP_CHUNK_SIZE;

            /* Render tiles in this chunk */
            for (int ly = 0; ly < CARBON_TILEMAP_CHUNK_SIZE; ly++) {
                int tile_y = base_tile_y + ly;
                if (tile_y >= tilemap->height) break;

                for (int lx = 0; lx < CARBON_TILEMAP_CHUNK_SIZE; lx++) {
                    int tile_x = base_tile_x + lx;
                    if (tile_x >= tilemap->width) break;

                    Carbon_TileID tile_id = chunk->tiles[ly * CARBON_TILEMAP_CHUNK_SIZE + lx];
                    if (tile_id == CARBON_TILE_EMPTY) continue;

                    /* Tile ID is 1-based, sprite array is 0-based */
                    int sprite_idx = tile_id - 1;
                    if (sprite_idx < 0 || sprite_idx >= ts->tile_count) continue;

                    Carbon_Sprite *sprite = &ts->sprites[sprite_idx];
                    float world_x = (float)(tile_x * tilemap->tile_width);
                    float world_y = (float)(tile_y * tilemap->tile_height);

                    carbon_sprite_draw_tinted(sr, sprite, world_x, world_y,
                                              1.0f, 1.0f, 1.0f, opacity);
                }
            }
        }
    }
}

void carbon_tilemap_render(Carbon_Tilemap *tilemap,
                           Carbon_SpriteRenderer *sr,
                           Carbon_Camera *camera)
{
    if (!tilemap || !sr) return;

    /* Render layers in order (0 = back, N = front) */
    for (int i = 0; i < tilemap->layer_count; i++) {
        carbon_tilemap_render_layer(tilemap, sr, camera, i);
    }
}

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

void carbon_tilemap_world_to_tile(Carbon_Tilemap *tilemap,
                                  float world_x, float world_y,
                                  int *tile_x, int *tile_y)
{
    if (!tilemap) return;

    if (tile_x) *tile_x = (int)floorf(world_x / tilemap->tile_width);
    if (tile_y) *tile_y = (int)floorf(world_y / tilemap->tile_height);
}

void carbon_tilemap_tile_to_world(Carbon_Tilemap *tilemap,
                                  int tile_x, int tile_y,
                                  float *world_x, float *world_y)
{
    if (!tilemap) return;

    if (world_x) *world_x = (float)(tile_x * tilemap->tile_width);
    if (world_y) *world_y = (float)(tile_y * tilemap->tile_height);
}

Carbon_TileID carbon_tilemap_get_tile_at_world(Carbon_Tilemap *tilemap,
                                               int layer,
                                               float world_x, float world_y)
{
    if (!tilemap) return CARBON_TILE_EMPTY;

    int tx, ty;
    carbon_tilemap_world_to_tile(tilemap, world_x, world_y, &tx, &ty);
    return carbon_tilemap_get_tile(tilemap, layer, tx, ty);
}

void carbon_tilemap_get_world_bounds(Carbon_Tilemap *tilemap,
                                     float *left, float *right,
                                     float *top, float *bottom)
{
    if (!tilemap) return;

    if (left) *left = 0.0f;
    if (top) *top = 0.0f;
    if (right) *right = (float)(tilemap->width * tilemap->tile_width);
    if (bottom) *bottom = (float)(tilemap->height * tilemap->tile_height);
}
