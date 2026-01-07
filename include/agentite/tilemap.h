/*
 * Carbon Tilemap System
 *
 * Chunk-based tilemap for large maps with efficient frustum culling.
 *
 * Usage:
 *   // Load tileset texture and create tileset
 *   Agentite_Texture *tex = agentite_texture_load(sr, "assets/tiles.png");
 *   Agentite_Tileset *tileset = agentite_tileset_create(tex, 32, 32);  // 32x32 pixel tiles
 *
 *   // Create tilemap (1000x1000 tiles)
 *   Agentite_Tilemap *tilemap = agentite_tilemap_create(tileset, 1000, 1000);
 *
 *   // Add layers
 *   int ground = agentite_tilemap_add_layer(tilemap, "ground");
 *   int objects = agentite_tilemap_add_layer(tilemap, "objects");
 *
 *   // Set tiles (tile ID 0 = empty, 1+ = valid tile)
 *   agentite_tilemap_fill(tilemap, ground, 0, 0, 1000, 1000, 1);  // Fill with grass
 *   agentite_tilemap_set_tile(tilemap, objects, 50, 50, 17);       // Place tree
 *
 *   // Each frame (during sprite batch):
 *   agentite_sprite_begin(sr, NULL);
 *   agentite_tilemap_render(tilemap, sr, camera);
 *   agentite_sprite_upload(sr, cmd);
 *   // ... render pass ...
 *   agentite_sprite_render(sr, cmd, pass);
 *
 *   // Cleanup
 *   agentite_tilemap_destroy(tilemap);
 *   agentite_tileset_destroy(tileset);
 */

#ifndef AGENTITE_TILEMAP_H
#define AGENTITE_TILEMAP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_Texture Agentite_Texture;
typedef struct Agentite_Sprite Agentite_Sprite;
typedef struct Agentite_SpriteRenderer Agentite_SpriteRenderer;
typedef struct Agentite_Camera Agentite_Camera;

/* ============================================================================
 * Types
 * ============================================================================ */

/* Tile ID: 0 = empty, 1+ = valid tile index (maps to tileset index 0+) */
typedef uint16_t Agentite_TileID;

#define AGENTITE_TILE_EMPTY 0
#define AGENTITE_TILEMAP_CHUNK_SIZE 32   /* 32x32 tiles per chunk */
#define AGENTITE_TILEMAP_MAX_LAYERS 16

/* Opaque types */
typedef struct Agentite_Tileset Agentite_Tileset;
typedef struct Agentite_TileLayer Agentite_TileLayer;
typedef struct Agentite_Tilemap Agentite_Tilemap;

/* ============================================================================
 * Tileset Functions
 * ============================================================================ */

/* Create tileset from texture (assumes regular grid, no spacing) */
Agentite_Tileset *agentite_tileset_create(Agentite_Texture *texture,
                                      int tile_width, int tile_height);

/* Create tileset with spacing and margin */
Agentite_Tileset *agentite_tileset_create_ex(Agentite_Texture *texture,
                                         int tile_width, int tile_height,
                                         int spacing, int margin);

/* Destroy tileset (does NOT destroy the source texture) */
void agentite_tileset_destroy(Agentite_Tileset *tileset);

/* Get tileset tile dimensions */
void agentite_tileset_get_tile_size(const Agentite_Tileset *tileset, int *width, int *height);

/* Get number of tiles in tileset */
int agentite_tileset_get_tile_count(const Agentite_Tileset *tileset);

/* ============================================================================
 * Tilemap Lifecycle Functions
 * ============================================================================ */

/* Create tilemap with specified dimensions (in tiles) */
Agentite_Tilemap *agentite_tilemap_create(Agentite_Tileset *tileset,
                                      int width, int height);

/* Destroy tilemap and all layers */
void agentite_tilemap_destroy(Agentite_Tilemap *tilemap);

/* Get tilemap dimensions in tiles */
void agentite_tilemap_get_size(const Agentite_Tilemap *tilemap, int *width, int *height);

/* Get tile dimensions in pixels */
void agentite_tilemap_get_tile_size(const Agentite_Tilemap *tilemap, int *width, int *height);

/* ============================================================================
 * Layer Functions
 * ============================================================================ */

/* Add a new layer (returns layer index, or -1 on failure) */
int agentite_tilemap_add_layer(Agentite_Tilemap *tilemap, const char *name);

/* Get layer by index (returns NULL if invalid) */
Agentite_TileLayer *agentite_tilemap_get_layer(Agentite_Tilemap *tilemap, int index);

/* Get layer by name (returns NULL if not found) */
Agentite_TileLayer *agentite_tilemap_get_layer_by_name(Agentite_Tilemap *tilemap,
                                                   const char *name);

/* Get layer count */
int agentite_tilemap_get_layer_count(const Agentite_Tilemap *tilemap);

/* Set layer visibility */
void agentite_tilemap_set_layer_visible(Agentite_Tilemap *tilemap, int layer, bool visible);

/* Get layer visibility */
bool agentite_tilemap_get_layer_visible(const Agentite_Tilemap *tilemap, int layer);

/* Set layer opacity (0.0 - 1.0) */
void agentite_tilemap_set_layer_opacity(Agentite_Tilemap *tilemap, int layer, float opacity);

/* Get layer opacity */
float agentite_tilemap_get_layer_opacity(const Agentite_Tilemap *tilemap, int layer);

/* ============================================================================
 * Tile Access Functions
 * ============================================================================ */

/* Set tile at position */
void agentite_tilemap_set_tile(Agentite_Tilemap *tilemap, int layer,
                             int x, int y, Agentite_TileID tile);

/* Get tile at position (returns AGENTITE_TILE_EMPTY if out of bounds) */
Agentite_TileID agentite_tilemap_get_tile(const Agentite_Tilemap *tilemap, int layer,
                                      int x, int y);

/* Fill a rectangular region with a tile */
void agentite_tilemap_fill(Agentite_Tilemap *tilemap, int layer,
                         int x, int y, int width, int height,
                         Agentite_TileID tile);

/* Clear all tiles in a layer (set to AGENTITE_TILE_EMPTY) */
void agentite_tilemap_clear_layer(Agentite_Tilemap *tilemap, int layer);

/* ============================================================================
 * Rendering Functions
 * ============================================================================ */

/* Render all visible layers with camera culling
 * Call during sprite batch (after agentite_sprite_begin, before upload/render) */
void agentite_tilemap_render(Agentite_Tilemap *tilemap,
                           Agentite_SpriteRenderer *sr,
                           Agentite_Camera *camera);

/* Render a single layer */
void agentite_tilemap_render_layer(Agentite_Tilemap *tilemap,
                                 Agentite_SpriteRenderer *sr,
                                 Agentite_Camera *camera,
                                 int layer);

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

/* Convert world coordinates to tile coordinates */
void agentite_tilemap_world_to_tile(const Agentite_Tilemap *tilemap,
                                  float world_x, float world_y,
                                  int *tile_x, int *tile_y);

/* Convert tile coordinates to world coordinates (top-left corner of tile) */
void agentite_tilemap_tile_to_world(const Agentite_Tilemap *tilemap,
                                  int tile_x, int tile_y,
                                  float *world_x, float *world_y);

/* Get tile at world position */
Agentite_TileID agentite_tilemap_get_tile_at_world(const Agentite_Tilemap *tilemap,
                                               int layer,
                                               float world_x, float world_y);

/* Get tilemap bounds in world coordinates */
void agentite_tilemap_get_world_bounds(const Agentite_Tilemap *tilemap,
                                     float *left, float *right,
                                     float *top, float *bottom);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_TILEMAP_H */
