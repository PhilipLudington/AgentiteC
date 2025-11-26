/*
 * Carbon Tilemap System
 *
 * Chunk-based tilemap for large maps with efficient frustum culling.
 *
 * Usage:
 *   // Load tileset texture and create tileset
 *   Carbon_Texture *tex = carbon_texture_load(sr, "assets/tiles.png");
 *   Carbon_Tileset *tileset = carbon_tileset_create(tex, 32, 32);  // 32x32 pixel tiles
 *
 *   // Create tilemap (1000x1000 tiles)
 *   Carbon_Tilemap *tilemap = carbon_tilemap_create(tileset, 1000, 1000);
 *
 *   // Add layers
 *   int ground = carbon_tilemap_add_layer(tilemap, "ground");
 *   int objects = carbon_tilemap_add_layer(tilemap, "objects");
 *
 *   // Set tiles (tile ID 0 = empty, 1+ = valid tile)
 *   carbon_tilemap_fill(tilemap, ground, 0, 0, 1000, 1000, 1);  // Fill with grass
 *   carbon_tilemap_set_tile(tilemap, objects, 50, 50, 17);       // Place tree
 *
 *   // Each frame (during sprite batch):
 *   carbon_sprite_begin(sr, NULL);
 *   carbon_tilemap_render(tilemap, sr, camera);
 *   carbon_sprite_upload(sr, cmd);
 *   // ... render pass ...
 *   carbon_sprite_render(sr, cmd, pass);
 *
 *   // Cleanup
 *   carbon_tilemap_destroy(tilemap);
 *   carbon_tileset_destroy(tileset);
 */

#ifndef CARBON_TILEMAP_H
#define CARBON_TILEMAP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Carbon_Texture Carbon_Texture;
typedef struct Carbon_Sprite Carbon_Sprite;
typedef struct Carbon_SpriteRenderer Carbon_SpriteRenderer;
typedef struct Carbon_Camera Carbon_Camera;

/* ============================================================================
 * Types
 * ============================================================================ */

/* Tile ID: 0 = empty, 1+ = valid tile index (maps to tileset index 0+) */
typedef uint16_t Carbon_TileID;

#define CARBON_TILE_EMPTY 0
#define CARBON_TILEMAP_CHUNK_SIZE 32   /* 32x32 tiles per chunk */
#define CARBON_TILEMAP_MAX_LAYERS 16

/* Opaque types */
typedef struct Carbon_Tileset Carbon_Tileset;
typedef struct Carbon_TileLayer Carbon_TileLayer;
typedef struct Carbon_Tilemap Carbon_Tilemap;

/* ============================================================================
 * Tileset Functions
 * ============================================================================ */

/* Create tileset from texture (assumes regular grid, no spacing) */
Carbon_Tileset *carbon_tileset_create(Carbon_Texture *texture,
                                      int tile_width, int tile_height);

/* Create tileset with spacing and margin */
Carbon_Tileset *carbon_tileset_create_ex(Carbon_Texture *texture,
                                         int tile_width, int tile_height,
                                         int spacing, int margin);

/* Destroy tileset (does NOT destroy the source texture) */
void carbon_tileset_destroy(Carbon_Tileset *tileset);

/* Get tileset tile dimensions */
void carbon_tileset_get_tile_size(Carbon_Tileset *tileset, int *width, int *height);

/* Get number of tiles in tileset */
int carbon_tileset_get_tile_count(Carbon_Tileset *tileset);

/* ============================================================================
 * Tilemap Lifecycle Functions
 * ============================================================================ */

/* Create tilemap with specified dimensions (in tiles) */
Carbon_Tilemap *carbon_tilemap_create(Carbon_Tileset *tileset,
                                      int width, int height);

/* Destroy tilemap and all layers */
void carbon_tilemap_destroy(Carbon_Tilemap *tilemap);

/* Get tilemap dimensions in tiles */
void carbon_tilemap_get_size(Carbon_Tilemap *tilemap, int *width, int *height);

/* Get tile dimensions in pixels */
void carbon_tilemap_get_tile_size(Carbon_Tilemap *tilemap, int *width, int *height);

/* ============================================================================
 * Layer Functions
 * ============================================================================ */

/* Add a new layer (returns layer index, or -1 on failure) */
int carbon_tilemap_add_layer(Carbon_Tilemap *tilemap, const char *name);

/* Get layer by index (returns NULL if invalid) */
Carbon_TileLayer *carbon_tilemap_get_layer(Carbon_Tilemap *tilemap, int index);

/* Get layer by name (returns NULL if not found) */
Carbon_TileLayer *carbon_tilemap_get_layer_by_name(Carbon_Tilemap *tilemap,
                                                   const char *name);

/* Get layer count */
int carbon_tilemap_get_layer_count(Carbon_Tilemap *tilemap);

/* Set layer visibility */
void carbon_tilemap_set_layer_visible(Carbon_Tilemap *tilemap, int layer, bool visible);

/* Get layer visibility */
bool carbon_tilemap_get_layer_visible(Carbon_Tilemap *tilemap, int layer);

/* Set layer opacity (0.0 - 1.0) */
void carbon_tilemap_set_layer_opacity(Carbon_Tilemap *tilemap, int layer, float opacity);

/* Get layer opacity */
float carbon_tilemap_get_layer_opacity(Carbon_Tilemap *tilemap, int layer);

/* ============================================================================
 * Tile Access Functions
 * ============================================================================ */

/* Set tile at position */
void carbon_tilemap_set_tile(Carbon_Tilemap *tilemap, int layer,
                             int x, int y, Carbon_TileID tile);

/* Get tile at position (returns CARBON_TILE_EMPTY if out of bounds) */
Carbon_TileID carbon_tilemap_get_tile(Carbon_Tilemap *tilemap, int layer,
                                      int x, int y);

/* Fill a rectangular region with a tile */
void carbon_tilemap_fill(Carbon_Tilemap *tilemap, int layer,
                         int x, int y, int width, int height,
                         Carbon_TileID tile);

/* Clear all tiles in a layer (set to CARBON_TILE_EMPTY) */
void carbon_tilemap_clear_layer(Carbon_Tilemap *tilemap, int layer);

/* ============================================================================
 * Rendering Functions
 * ============================================================================ */

/* Render all visible layers with camera culling
 * Call during sprite batch (after carbon_sprite_begin, before upload/render) */
void carbon_tilemap_render(Carbon_Tilemap *tilemap,
                           Carbon_SpriteRenderer *sr,
                           Carbon_Camera *camera);

/* Render a single layer */
void carbon_tilemap_render_layer(Carbon_Tilemap *tilemap,
                                 Carbon_SpriteRenderer *sr,
                                 Carbon_Camera *camera,
                                 int layer);

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

/* Convert world coordinates to tile coordinates */
void carbon_tilemap_world_to_tile(Carbon_Tilemap *tilemap,
                                  float world_x, float world_y,
                                  int *tile_x, int *tile_y);

/* Convert tile coordinates to world coordinates (top-left corner of tile) */
void carbon_tilemap_tile_to_world(Carbon_Tilemap *tilemap,
                                  int tile_x, int tile_y,
                                  float *world_x, float *world_y);

/* Get tile at world position */
Carbon_TileID carbon_tilemap_get_tile_at_world(Carbon_Tilemap *tilemap,
                                               int layer,
                                               float world_x, float world_y);

/* Get tilemap bounds in world coordinates */
void carbon_tilemap_get_world_bounds(Carbon_Tilemap *tilemap,
                                     float *left, float *right,
                                     float *top, float *bottom);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_TILEMAP_H */
