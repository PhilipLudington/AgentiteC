# AgentiteC Engine Systems Reference

This document describes the AgentiteC engine systems, their dependencies, and how they interact.

## System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Agentite_GameContext                            │
│  (Unified container for all systems - recommended for new games)     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐          │
│  │ Agentite_Engine│───▶│ SDL3 Window  │───▶│ SDL_GPU      │          │
│  │   (core)     │    │              │    │ Device       │          │
│  └──────────────┘    └──────────────┘    └──────────────┘          │
│         │                                       │                    │
│         ▼                                       ▼                    │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐          │
│  │Agentite_Input  │    │Agentite_Sprite │◀───│Agentite_Camera │          │
│  │              │    │  Renderer    │    │              │          │
│  └──────────────┘    └──────────────┘    └──────────────┘          │
│                             │                   │                    │
│                             ▼                   │                    │
│                      ┌──────────────┐          │                    │
│                      │Agentite_Tilemap│──────────┘                    │
│                      │              │                                │
│                      └──────────────┘                                │
│                                                                      │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐          │
│  │Agentite_Text   │    │Agentite_Audio  │    │Agentite_World  │          │
│  │  Renderer    │    │              │    │   (ECS)      │          │
│  └──────────────┘    └──────────────┘    └──────────────┘          │
│                                                                      │
│  ┌──────────────┐    ┌──────────────┐                               │
│  │ AUI_Context  │    │Agentite_Path   │                               │
│  │    (UI)      │    │   finder     │                               │
│  └──────────────┘    └──────────────┘                               │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

## Initialization Order

Systems must be initialized in this order due to dependencies:

```c
// 1. Core Engine (creates window, GPU device)
Agentite_Engine *engine = agentite_init(&config);

// 2. Sprite Renderer (requires GPU device, window)
Agentite_SpriteRenderer *sprites = agentite_sprite_init(gpu, window);

// 3. Text Renderer (requires GPU device, window)
Agentite_TextRenderer *text = agentite_text_init(gpu, window);

// 4. Camera (standalone, but connects to sprite renderer)
Agentite_Camera *camera = agentite_camera_create(width, height);
agentite_sprite_set_camera(sprites, camera);

// 5. Input System (standalone)
Agentite_Input *input = agentite_input_init();

// 6. Audio System (standalone)
Agentite_Audio *audio = agentite_audio_init();

// 7. ECS World (standalone)
Agentite_World *ecs = agentite_ecs_init();

// 8. UI System (requires GPU device, window, font)
AUI_Context *ui = aui_init(gpu, window, width, height, font_path, font_size);

// 9. Load fonts (requires text renderer)
Agentite_Font *font = agentite_font_load(text, path, size);
Agentite_SDFFont *sdf = agentite_sdf_font_load(text, atlas, json);

// 10. Create tileset and tilemap (requires sprite renderer)
Agentite_Tileset *tileset = agentite_tileset_create(texture, tile_w, tile_h);
Agentite_Tilemap *tilemap = agentite_tilemap_create(tileset, map_w, map_h);

// 11. Create pathfinder (standalone)
Agentite_Pathfinder *pf = agentite_pathfinder_create(width, height);
```

## Shutdown Order (Reverse)

```c
// Pathfinder
agentite_pathfinder_destroy(pf);

// Tilemap (before tileset)
agentite_tilemap_destroy(tilemap);
agentite_tileset_destroy(tileset);

// Fonts (before text renderer)
agentite_font_destroy(text, font);
agentite_sdf_font_destroy(text, sdf);

// UI
aui_shutdown(ui);

// ECS
agentite_ecs_shutdown(ecs);

// Audio
agentite_audio_shutdown(audio);

// Input
agentite_input_shutdown(input);

// Camera
agentite_camera_destroy(camera);

// Text Renderer
agentite_text_shutdown(text);

// Textures (before sprite renderer)
agentite_texture_destroy(sprites, texture);

// Sprite Renderer
agentite_sprite_shutdown(sprites);

// Engine (last - calls SDL_Quit)
agentite_shutdown(engine);
```

## Frame Structure

```c
while (agentite_is_running(engine)) {
    // === FRAME START ===
    agentite_begin_frame(engine);

    // === INPUT PHASE ===
    agentite_input_begin_frame(input);
    while (SDL_PollEvent(&event)) {
        // UI gets first chance at events
        if (aui_process_event(ui, &event)) continue;
        agentite_input_process_event(input, &event);
        // Handle quit
        if (event.type == SDL_EVENT_QUIT) agentite_quit(engine);
    }
    agentite_input_update(input);

    // === UPDATE PHASE ===
    float dt = agentite_get_delta_time(engine);
    // Game logic here...
    agentite_ecs_progress(ecs, dt);
    agentite_audio_update(audio);
    agentite_camera_update(camera);

    // === RENDER PHASE ===
    // 1. Build batches (before render pass)
    agentite_sprite_begin(sprites, NULL);
    // Draw world sprites, tilemap...
    agentite_tilemap_render(tilemap, sprites, camera);
    // Draw entities...

    // 2. UI frame
    aui_begin_frame(ui, dt);
    // Draw UI widgets...
    aui_end_frame(ui);

    // 3. Text batches
    agentite_text_begin(text);
    // Draw text...
    agentite_text_end(text);

    // 4. Upload to GPU (before render pass)
    SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
    agentite_sprite_upload(sprites, cmd);
    aui_upload(ui, cmd);
    agentite_text_upload(text, cmd);

    // 5. Render pass
    if (agentite_begin_render_pass(engine, r, g, b, a)) {
        SDL_GPURenderPass *pass = agentite_get_render_pass(engine);

        // Render order (back to front):
        agentite_sprite_render(sprites, cmd, pass);  // World sprites
        aui_render(ui, cmd, pass);                 // UI
        agentite_text_render(text, cmd, pass);       // Text overlay

        agentite_end_render_pass(engine);
    }

    // === FRAME END ===
    agentite_end_frame(engine);
}
```

## System Dependencies

### Agentite_Engine
- **Depends on**: SDL3
- **Provides**: Window, GPU device, frame timing
- **Required by**: All other systems

### Agentite_SpriteRenderer
- **Depends on**: GPU device, window
- **Provides**: Batched sprite rendering, texture management
- **Required by**: Tilemap, Animation (for rendering)

### Agentite_Camera
- **Depends on**: None (standalone math)
- **Provides**: View-projection matrix, coordinate conversion
- **Connects to**: SpriteRenderer (for world transforms)

### Agentite_TextRenderer
- **Depends on**: GPU device, window
- **Provides**: Bitmap and SDF text rendering
- **Manages**: Font atlases, glyph batching

### Agentite_Tilemap
- **Depends on**: SpriteRenderer, Camera
- **Provides**: Chunk-based tile rendering, frustum culling
- **Requires**: Tileset (texture + tile dimensions)

### Agentite_Input
- **Depends on**: SDL3 events
- **Provides**: Action mapping, keyboard/mouse/gamepad state
- **Standalone**: No GPU dependency

### Agentite_Audio
- **Depends on**: SDL3 audio
- **Provides**: Sound/music playback, mixing
- **Standalone**: No GPU dependency

### Agentite_World (ECS)
- **Depends on**: Flecs library
- **Provides**: Entity management, component storage, systems
- **Standalone**: No GPU dependency

### AUI_Context (UI)
- **Depends on**: GPU device, window, font file
- **Provides**: Immediate-mode widgets, panels
- **Consumes**: SDL events (for input)

### Agentite_Pathfinder
- **Depends on**: None
- **Provides**: A* pathfinding for tile grids
- **Integrates with**: Tilemap (for obstacle data)

## Common Patterns

### Camera-Aware Rendering
```c
// Set camera for world-space rendering
agentite_sprite_set_camera(sprites, camera);

// Draw world sprites (transformed by camera)
agentite_sprite_draw(sprites, &sprite, world_x, world_y);

// Disable camera for screen-space UI
agentite_sprite_set_camera(sprites, NULL);
agentite_sprite_draw(sprites, &ui_sprite, screen_x, screen_y);
```

### Multi-Layer Rendering
```c
// Render order:
// 1. Background (tilemap layer 0)
// 2. Ground sprites (tilemap layer 1 + entities)
// 3. Foreground (effects, particles)
// 4. UI (panels, text)

agentite_sprite_begin(sprites, NULL);

// Background tilemap
agentite_tilemap_render(tilemap, sprites, camera);

// Entity sprites (sorted by y for depth)
for (int i = 0; i < entity_count; i++) {
    agentite_sprite_draw(sprites, &entities[i].sprite,
                       entities[i].x, entities[i].y);
}

// UI is rendered separately after sprite batch
```

### Coordinate Conversion
```c
// Screen → World (for picking)
float world_x, world_y;
agentite_camera_screen_to_world(camera, mouse_x, mouse_y, &world_x, &world_y);

// World → Tile
int tile_x = (int)(world_x / TILE_SIZE);
int tile_y = (int)(world_y / TILE_SIZE);

// World → Screen (for HUD positioning)
float screen_x, screen_y;
agentite_camera_world_to_screen(camera, entity_x, entity_y, &screen_x, &screen_y);
```

### Error Handling
```c
#include "agentite/error.h"

Agentite_Texture *tex = agentite_texture_load(sprites, "missing.png");
if (!tex) {
    SDL_Log("Failed to load texture: %s", agentite_get_last_error());
    // Handle error...
}
```

## Using Agentite_GameContext

For new projects, use `Agentite_GameContext` to manage all systems:

```c
#include "agentite/game_context.h"

// Configure
Agentite_GameContextConfig config = AGENTITE_GAME_CONTEXT_DEFAULT;
config.window_title = "My Game";
config.font_path = "assets/fonts/font.ttf";

// Create (initializes everything)
Agentite_GameContext *ctx = agentite_game_context_create(&config);

// Access systems
ctx->engine;   // Agentite_Engine*
ctx->sprites;  // Agentite_SpriteRenderer*
ctx->camera;   // Agentite_Camera*
ctx->text;     // Agentite_TextRenderer*
ctx->input;    // Agentite_Input*
ctx->audio;    // Agentite_Audio* (if enabled)
ctx->ecs;      // Agentite_World* (if enabled)
ctx->ui;       // AUI_Context* (if enabled)
ctx->font;     // Agentite_Font* (if font_path provided)

// Simplified frame structure
while (agentite_game_context_is_running(ctx)) {
    agentite_game_context_begin_frame(ctx);
    agentite_game_context_poll_events(ctx);

    // Update game...

    SDL_GPUCommandBuffer *cmd = agentite_game_context_begin_render(ctx);
    // Build batches, upload...

    if (agentite_game_context_begin_render_pass(ctx, r, g, b, a)) {
        // Render...
        agentite_game_context_end_render_pass(ctx);
    }

    agentite_game_context_end_frame(ctx);
}

// Cleanup (destroys everything)
agentite_game_context_destroy(ctx);
```
