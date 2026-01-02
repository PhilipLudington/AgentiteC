# Shader System

The Agentite Shader System provides a flexible shader abstraction layer built on SDL_GPU that enables:
- Loading shaders from SPIR-V files or embedded bytecode
- Automatic format selection (Metal MSL, SPIRV, DXIL)
- Uniform buffer management
- Post-processing pipeline with effect chaining
- Built-in effect shaders (grayscale, blur, glow, etc.)

## Header

```c
#include "agentite/shader.h"
```

## Quick Start

```c
// Create shader system
Agentite_ShaderSystem *ss = agentite_shader_system_create(gpu);

// Load a custom shader from SPIR-V files
Agentite_ShaderDesc desc = AGENTITE_SHADER_DESC_DEFAULT;
desc.num_fragment_samplers = 1;
Agentite_Shader *shader = agentite_shader_load_spirv(ss,
    "assets/shaders/custom.vert.spv",
    "assets/shaders/custom.frag.spv",
    &desc);

// Or use a built-in effect
Agentite_Shader *grayscale = agentite_shader_get_builtin(ss,
    AGENTITE_SHADER_GRAYSCALE);

// Clean up
agentite_shader_destroy(ss, shader);  // Don't destroy builtins
agentite_shader_system_destroy(ss);
```

## Post-Processing

```c
// Create post-processing pipeline
Agentite_PostProcess *pp = agentite_postprocess_create(ss, window, NULL);

// Get render target for scene rendering
SDL_GPUTexture *target = agentite_postprocess_get_target(pp);

// Render your scene to the target...

// Apply effects in render pass
agentite_postprocess_begin(pp, cmd, target);
agentite_postprocess_apply(pp, cmd, pass, grayscale, NULL);
agentite_postprocess_end(pp, cmd, pass);

// For effects with parameters
Agentite_ShaderParams_Vignette vignette_params = {
    .intensity = 0.5f,
    .softness = 0.4f
};
agentite_postprocess_apply(pp, cmd, pass,
    agentite_shader_get_builtin(ss, AGENTITE_SHADER_VIGNETTE),
    &vignette_params);

agentite_postprocess_destroy(pp);
```

## Built-in Effects

| Effect | Parameters | Description |
|--------|------------|-------------|
| `AGENTITE_SHADER_GRAYSCALE` | None | Convert to grayscale |
| `AGENTITE_SHADER_SEPIA` | None | Sepia tone effect |
| `AGENTITE_SHADER_INVERT` | None | Invert colors |
| `AGENTITE_SHADER_BRIGHTNESS` | `Agentite_ShaderParams_Adjust` | Adjust brightness |
| `AGENTITE_SHADER_CONTRAST` | `Agentite_ShaderParams_Adjust` | Adjust contrast |
| `AGENTITE_SHADER_SATURATION` | `Agentite_ShaderParams_Adjust` | Adjust saturation |
| `AGENTITE_SHADER_BLUR_BOX` | `Agentite_ShaderParams_Blur` | Box blur |
| `AGENTITE_SHADER_VIGNETTE` | `Agentite_ShaderParams_Vignette` | Darkened edges |
| `AGENTITE_SHADER_CHROMATIC` | `Agentite_ShaderParams_Chromatic` | Chromatic aberration |
| `AGENTITE_SHADER_SCANLINES` | `Agentite_ShaderParams_Scanlines` | CRT scanlines |
| `AGENTITE_SHADER_PIXELATE` | `Agentite_ShaderParams_Pixelate` | Pixelation |
| `AGENTITE_SHADER_SOBEL` | None | Sobel edge detection |
| `AGENTITE_SHADER_FLASH` | `Agentite_ShaderParams_Flash` | Color flash |

## Custom Shaders

### GLSL Shader Example

Vertex shader (`custom.vert.glsl`):
```glsl
#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 frag_texcoord;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    frag_texcoord = texcoord;
}
```

Fragment shader (`custom.frag.glsl`):
```glsl
#version 450

layout(set = 2, binding = 0) uniform sampler2D source_texture;

layout(set = 3, binding = 0) uniform Params {
    float intensity;
    vec3 _pad;
};

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

void main() {
    vec4 color = texture(source_texture, frag_texcoord);
    // Your effect here
    out_color = color;
}
```

### Compiling Shaders

Use `glslc` from the Vulkan SDK to compile GLSL to SPIR-V:

```bash
glslc -fshader-stage=vertex custom.vert.glsl -o custom.vert.spv
glslc -fshader-stage=fragment custom.frag.glsl -o custom.frag.spv
```

### Loading Custom Shaders

```c
Agentite_ShaderDesc desc = AGENTITE_SHADER_DESC_DEFAULT;
desc.num_fragment_samplers = 1;  // Texture input
desc.num_fragment_uniforms = 1;   // Uniform block
desc.blend_mode = AGENTITE_BLEND_NONE;  // Post-process

Agentite_Shader *shader = agentite_shader_load_spirv(ss,
    "assets/shaders/custom.vert.spv",
    "assets/shaders/custom.frag.spv",
    &desc);
```

### Passing Uniforms

```c
// Method 1: Push per-draw (recommended for post-process)
struct MyParams {
    float intensity;
    float _pad[3];  // 16-byte alignment
};

struct MyParams params = { .intensity = 0.5f };
agentite_shader_push_uniform(cmd,
    AGENTITE_SHADER_STAGE_FRAGMENT, 0,
    &params, sizeof(params));

// Method 2: Uniform buffer (for shared/persistent data)
Agentite_UniformBuffer *ub = agentite_uniform_create(ss, sizeof(MyParams));
agentite_uniform_update(ub, &params, sizeof(params), 0);
```

## Fullscreen Quad Rendering

For custom full-screen effects:

```c
agentite_shader_draw_fullscreen(ss, cmd, pass,
    my_shader, source_texture,
    &my_params, sizeof(my_params));
```

## API Reference

### Shader System Lifecycle

| Function | Description |
|----------|-------------|
| `agentite_shader_system_create(gpu)` | Create shader system |
| `agentite_shader_system_destroy(ss)` | Destroy shader system and all shaders |

### Shader Loading

| Function | Description |
|----------|-------------|
| `agentite_shader_load_spirv(ss, vert_path, frag_path, desc)` | Load from SPIR-V files |
| `agentite_shader_load_memory(ss, vert_data, vert_size, frag_data, frag_size, desc)` | Load from memory |
| `agentite_shader_load_msl(ss, msl_source, desc)` | Load Metal shader |
| `agentite_shader_get_builtin(ss, builtin)` | Get built-in effect |
| `agentite_shader_destroy(ss, shader)` | Destroy a shader |

### Post-Processing

| Function | Description |
|----------|-------------|
| `agentite_postprocess_create(ss, window, config)` | Create pipeline |
| `agentite_postprocess_destroy(pp)` | Destroy pipeline |
| `agentite_postprocess_get_target(pp)` | Get scene render target |
| `agentite_postprocess_begin(pp, cmd, source)` | Begin processing |
| `agentite_postprocess_apply(pp, cmd, pass, shader, params)` | Apply effect |
| `agentite_postprocess_end(pp, cmd, pass)` | End processing |

### Utility

| Function | Description |
|----------|-------------|
| `agentite_shader_get_formats(ss)` | Get supported shader formats |
| `agentite_shader_format_supported(ss, format)` | Check format support |
| `agentite_shader_get_stats(ss, stats)` | Get usage statistics |

## Shader Authoring Workflow

1. **Write GLSL 4.50 shaders** - Use `layout(set = N, binding = M)` for uniforms/samplers
2. **Compile to SPIR-V** - `glslc -fshader-stage=<stage> input.glsl -o output.spv`
3. **Create shader descriptor** - Set sampler/uniform counts, blend mode
4. **Load at runtime** - `agentite_shader_load_spirv()` or `agentite_shader_load_memory()`
5. **For built-in effects** - Use `agentite_shader_get_builtin()`

### GLSL Binding Conventions

| Set | Usage |
|-----|-------|
| 0 | Reserved |
| 1 | Vertex uniforms |
| 2 | Fragment samplers (textures) |
| 3 | Fragment uniforms |

### Uniform Alignment

All uniform structs must be 16-byte aligned. Use padding if needed:

```glsl
layout(set = 3, binding = 0) uniform Params {
    float value1;      // 4 bytes
    float value2;      // 4 bytes
    vec2 _pad;         // 8 bytes padding to reach 16
};
```

## Thread Safety

All shader operations are **NOT thread-safe** and must be called from the main thread.
