# Carbon Engine Systems Reference

This document describes the Carbon engine systems, their dependencies, and how they interact.

## System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Carbon_GameContext                            │
│  (Unified container for all systems - recommended for new games)     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐          │
│  │ Carbon_Engine│───▶│ SDL3 Window  │───▶│ SDL_GPU      │          │
│  │   (core)     │    │              │    │ Device       │          │
│  └──────────────┘    └──────────────┘    └──────────────┘          │
│         │                                       │                    │
│         ▼                                       ▼                    │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐          │
│  │Carbon_Input  │    │Carbon_Sprite │◀───│Carbon_Camera │          │
│  │              │    │  Renderer    │    │              │          │
│  └──────────────┘    └──────────────┘    └──────────────┘          │
│                             │                   │                    │
│                             ▼                   │                    │
│                      ┌──────────────┐          │                    │
│                      │Carbon_Tilemap│──────────┘                    │
│                      │              │                                │
│                      └──────────────┘                                │
│                                                                      │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐          │
│  │Carbon_Text   │    │Carbon_Audio  │    │Carbon_World  │          │
│  │  Renderer    │    │              │    │   (ECS)      │          │
│  └──────────────┘    └──────────────┘    └──────────────┘          │
│                                                                      │
│  ┌──────────────┐    ┌──────────────┐                               │
│  │ CUI_Context  │    │Carbon_Path   │                               │
│  │    (UI)      │    │   finder     │                               │
│  └──────────────┘    └──────────────┘                               │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

## Initialization Order

Systems must be initialized in this order due to dependencies:

```c
// 1. Core Engine (creates window, GPU device)
Carbon_Engine *engine = carbon_init(&config);

// 2. Sprite Renderer (requires GPU device, window)
Carbon_SpriteRenderer *sprites = carbon_sprite_init(gpu, window);

// 3. Text Renderer (requires GPU device, window)
Carbon_TextRenderer *text = carbon_text_init(gpu, window);

// 4. Camera (standalone, but connects to sprite renderer)
Carbon_Camera *camera = carbon_camera_create(width, height);
carbon_sprite_set_camera(sprites, camera);

// 5. Input System (standalone)
Carbon_Input *input = carbon_input_init();

// 6. Audio System (standalone)
Carbon_Audio *audio = carbon_audio_init();

// 7. ECS World (standalone)
Carbon_World *ecs = carbon_ecs_init();

// 8. UI System (requires GPU device, window, font)
CUI_Context *ui = cui_init(gpu, window, width, height, font_path, font_size);

// 9. Load fonts (requires text renderer)
Carbon_Font *font = carbon_font_load(text, path, size);
Carbon_SDFFont *sdf = carbon_sdf_font_load(text, atlas, json);

// 10. Create tileset and tilemap (requires sprite renderer)
Carbon_Tileset *tileset = carbon_tileset_create(texture, tile_w, tile_h);
Carbon_Tilemap *tilemap = carbon_tilemap_create(tileset, map_w, map_h);

// 11. Create pathfinder (standalone)
Carbon_Pathfinder *pf = carbon_pathfinder_create(width, height);
```

## Shutdown Order (Reverse)

```c
// Pathfinder
carbon_pathfinder_destroy(pf);

// Tilemap (before tileset)
carbon_tilemap_destroy(tilemap);
carbon_tileset_destroy(tileset);

// Fonts (before text renderer)
carbon_font_destroy(text, font);
carbon_sdf_font_destroy(text, sdf);

// UI
cui_shutdown(ui);

// ECS
carbon_ecs_shutdown(ecs);

// Audio
carbon_audio_shutdown(audio);

// Input
carbon_input_shutdown(input);

// Camera
carbon_camera_destroy(camera);

// Text Renderer
carbon_text_shutdown(text);

// Textures (before sprite renderer)
carbon_texture_destroy(sprites, texture);

// Sprite Renderer
carbon_sprite_shutdown(sprites);

// Engine (last - calls SDL_Quit)
carbon_shutdown(engine);
```

## Frame Structure

```c
while (carbon_is_running(engine)) {
    // === FRAME START ===
    carbon_begin_frame(engine);

    // === INPUT PHASE ===
    carbon_input_begin_frame(input);
    while (SDL_PollEvent(&event)) {
        // UI gets first chance at events
        if (cui_process_event(ui, &event)) continue;
        carbon_input_process_event(input, &event);
        // Handle quit
        if (event.type == SDL_EVENT_QUIT) carbon_quit(engine);
    }
    carbon_input_update(input);

    // === UPDATE PHASE ===
    float dt = carbon_get_delta_time(engine);
    // Game logic here...
    carbon_ecs_progress(ecs, dt);
    carbon_audio_update(audio);
    carbon_camera_update(camera);

    // === RENDER PHASE ===
    // 1. Build batches (before render pass)
    carbon_sprite_begin(sprites, NULL);
    // Draw world sprites, tilemap...
    carbon_tilemap_render(tilemap, sprites, camera);
    // Draw entities...

    // 2. UI frame
    cui_begin_frame(ui, dt);
    // Draw UI widgets...
    cui_end_frame(ui);

    // 3. Text batches
    carbon_text_begin(text);
    // Draw text...
    carbon_text_end(text);

    // 4. Upload to GPU (before render pass)
    SDL_GPUCommandBuffer *cmd = carbon_acquire_command_buffer(engine);
    carbon_sprite_upload(sprites, cmd);
    cui_upload(ui, cmd);
    carbon_text_upload(text, cmd);

    // 5. Render pass
    if (carbon_begin_render_pass(engine, r, g, b, a)) {
        SDL_GPURenderPass *pass = carbon_get_render_pass(engine);

        // Render order (back to front):
        carbon_sprite_render(sprites, cmd, pass);  // World sprites
        cui_render(ui, cmd, pass);                 // UI
        carbon_text_render(text, cmd, pass);       // Text overlay

        carbon_end_render_pass(engine);
    }

    // 6. End sprite batch
    carbon_sprite_end(sprites, NULL, NULL);

    // === FRAME END ===
    carbon_end_frame(engine);
}
```

## System Dependencies

### Carbon_Engine
- **Depends on**: SDL3
- **Provides**: Window, GPU device, frame timing
- **Required by**: All other systems

### Carbon_SpriteRenderer
- **Depends on**: GPU device, window
- **Provides**: Batched sprite rendering, texture management
- **Required by**: Tilemap, Animation (for rendering)

### Carbon_Camera
- **Depends on**: None (standalone math)
- **Provides**: View-projection matrix, coordinate conversion
- **Connects to**: SpriteRenderer (for world transforms)

### Carbon_TextRenderer
- **Depends on**: GPU device, window
- **Provides**: Bitmap and SDF text rendering
- **Manages**: Font atlases, glyph batching

### Carbon_Tilemap
- **Depends on**: SpriteRenderer, Camera
- **Provides**: Chunk-based tile rendering, frustum culling
- **Requires**: Tileset (texture + tile dimensions)

### Carbon_Input
- **Depends on**: SDL3 events
- **Provides**: Action mapping, keyboard/mouse/gamepad state
- **Standalone**: No GPU dependency

### Carbon_Audio
- **Depends on**: SDL3 audio
- **Provides**: Sound/music playback, mixing
- **Standalone**: No GPU dependency

### Carbon_World (ECS)
- **Depends on**: Flecs library
- **Provides**: Entity management, component storage, systems
- **Standalone**: No GPU dependency

### CUI_Context (UI)
- **Depends on**: GPU device, window, font file
- **Provides**: Immediate-mode widgets, panels
- **Consumes**: SDL events (for input)

### Carbon_Pathfinder
- **Depends on**: None
- **Provides**: A* pathfinding for tile grids
- **Integrates with**: Tilemap (for obstacle data)

## Common Patterns

### Camera-Aware Rendering
```c
// Set camera for world-space rendering
carbon_sprite_set_camera(sprites, camera);

// Draw world sprites (transformed by camera)
carbon_sprite_draw(sprites, &sprite, world_x, world_y);

// Disable camera for screen-space UI
carbon_sprite_set_camera(sprites, NULL);
carbon_sprite_draw(sprites, &ui_sprite, screen_x, screen_y);
```

### Multi-Layer Rendering
```c
// Render order:
// 1. Background (tilemap layer 0)
// 2. Ground sprites (tilemap layer 1 + entities)
// 3. Foreground (effects, particles)
// 4. UI (panels, text)

carbon_sprite_begin(sprites, NULL);

// Background tilemap
carbon_tilemap_render(tilemap, sprites, camera);

// Entity sprites (sorted by y for depth)
for (int i = 0; i < entity_count; i++) {
    carbon_sprite_draw(sprites, &entities[i].sprite,
                       entities[i].x, entities[i].y);
}

// UI is rendered separately after sprite batch
```

### Coordinate Conversion
```c
// Screen → World (for picking)
float world_x, world_y;
carbon_camera_screen_to_world(camera, mouse_x, mouse_y, &world_x, &world_y);

// World → Tile
int tile_x = (int)(world_x / TILE_SIZE);
int tile_y = (int)(world_y / TILE_SIZE);

// World → Screen (for HUD positioning)
float screen_x, screen_y;
carbon_camera_world_to_screen(camera, entity_x, entity_y, &screen_x, &screen_y);
```

### Error Handling
```c
#include "carbon/error.h"

Carbon_Texture *tex = carbon_texture_load(sprites, "missing.png");
if (!tex) {
    SDL_Log("Failed to load texture: %s", carbon_get_last_error());
    // Handle error...
}
```

## Using Carbon_GameContext

For new projects, use `Carbon_GameContext` to manage all systems:

```c
#include "carbon/game_context.h"

// Configure
Carbon_GameContextConfig config = CARBON_GAME_CONTEXT_DEFAULT;
config.window_title = "My Game";
config.font_path = "assets/fonts/font.ttf";

// Create (initializes everything)
Carbon_GameContext *ctx = carbon_game_context_create(&config);

// Access systems
ctx->engine;   // Carbon_Engine*
ctx->sprites;  // Carbon_SpriteRenderer*
ctx->camera;   // Carbon_Camera*
ctx->text;     // Carbon_TextRenderer*
ctx->input;    // Carbon_Input*
ctx->audio;    // Carbon_Audio* (if enabled)
ctx->ecs;      // Carbon_World* (if enabled)
ctx->ui;       // CUI_Context* (if enabled)
ctx->font;     // Carbon_Font* (if font_path provided)

// Simplified frame structure
while (carbon_game_context_is_running(ctx)) {
    carbon_game_context_begin_frame(ctx);
    carbon_game_context_poll_events(ctx);

    // Update game...

    SDL_GPUCommandBuffer *cmd = carbon_game_context_begin_render(ctx);
    // Build batches, upload...

    if (carbon_game_context_begin_render_pass(ctx, r, g, b, a)) {
        // Render...
        carbon_game_context_end_render_pass(ctx);
    }

    carbon_game_context_end_frame(ctx);
}

// Cleanup (destroys everything)
carbon_game_context_destroy(ctx);
```
