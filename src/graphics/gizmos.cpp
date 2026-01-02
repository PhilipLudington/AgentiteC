/*
 * Agentite Gizmo Rendering System Implementation
 *
 * Provides immediate-mode gizmo drawing for editor tools and debug visualization.
 * Uses a batched line renderer with GPU pipeline.
 */

#include "agentite/gizmos.h"
#include "agentite/camera.h"
#include "agentite/error.h"
#include <cglm/cglm.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define GIZMO_MAX_LINES 16384           /* Max lines per frame */
#define GIZMO_MAX_TRIANGLES 4096        /* Max triangles (for filled shapes) */
#define GIZMO_CIRCLE_SEGMENTS 32        /* Segments for circle approximation */
#define GIZMO_SPHERE_RINGS 3            /* Number of rings for sphere wireframe */
#define GIZMO_ARROW_HEAD_SIZE 0.15f     /* Arrow head size as fraction of length */
#define GIZMO_ARROW_HEAD_ANGLE 0.5f     /* Arrow head half-angle in radians */

/* ============================================================================
 * Vertex Types
 * ============================================================================ */

/* Line vertex (position + color) */
typedef struct GizmoLineVertex {
    float pos[3];       /* World/screen position */
    float color[4];     /* RGBA color */
} GizmoLineVertex;

/* Triangle vertex for filled shapes */
typedef struct GizmoTriVertex {
    float pos[3];       /* World/screen position */
    float color[4];     /* RGBA color */
} GizmoTriVertex;

/* ============================================================================
 * Embedded MSL Shader Source
 * ============================================================================ */

static const char gizmo_shader_msl[] =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct Uniforms {\n"
"    float4x4 view_projection;\n"
"    float2 screen_size;\n"
"    float2 padding;\n"
"};\n"
"\n"
"struct VertexIn {\n"
"    float3 position [[attribute(0)]];\n"
"    float4 color [[attribute(1)]];\n"
"};\n"
"\n"
"struct VertexOut {\n"
"    float4 position [[position]];\n"
"    float4 color;\n"
"};\n"
"\n"
"vertex VertexOut gizmo_vertex(\n"
"    VertexIn in [[stage_in]],\n"
"    constant Uniforms& uniforms [[buffer(0)]]\n"
") {\n"
"    VertexOut out;\n"
"    float4 world_pos = float4(in.position, 1.0);\n"
"    out.position = uniforms.view_projection * world_pos;\n"
"    out.color = in.color;\n"
"    return out;\n"
"}\n"
"\n"
"fragment float4 gizmo_fragment(\n"
"    VertexOut in [[stage_in]]\n"
") {\n"
"    return in.color;\n"
"}\n";

/* SPIRV shaders would go here for Vulkan/D3D12 support */
/* For now, we only support Metal */

/* ============================================================================
 * Internal Types
 * ============================================================================ */

struct Agentite_Gizmos {
    SDL_GPUDevice *gpu;
    int screen_width;
    int screen_height;

    /* Configuration */
    Agentite_GizmoConfig config;

    /* GPU resources */
    SDL_GPUGraphicsPipeline *line_pipeline;
    SDL_GPUGraphicsPipeline *tri_pipeline;
    SDL_GPUBuffer *line_vertex_buffer;
    SDL_GPUBuffer *tri_vertex_buffer;

    /* CPU-side batch buffers */
    GizmoLineVertex *line_vertices;
    uint32_t line_vertex_count;

    GizmoTriVertex *tri_vertices;
    uint32_t tri_vertex_count;

    /* Camera reference (borrowed, not owned) */
    Agentite_Camera *camera;

    /* Input state */
    float mouse_x;
    float mouse_y;
    bool mouse_down;
    bool mouse_pressed;

    /* Interaction state */
    bool is_hovered;
    bool is_active;
    Agentite_GizmoAxis active_axis;
    vec3 drag_start_pos;        /* World position when drag started */
    float drag_start_mouse_x;   /* Screen position when drag started */
    float drag_start_mouse_y;

    /* Frame state */
    bool frame_started;
};

/* ============================================================================
 * Internal: Color Helpers
 * ============================================================================ */

static void color_unpack(uint32_t color, float *r, float *g, float *b, float *a)
{
    *r = ((color >> 24) & 0xFF) / 255.0f;
    *g = ((color >> 16) & 0xFF) / 255.0f;
    *b = ((color >> 8) & 0xFF) / 255.0f;
    *a = (color & 0xFF) / 255.0f;
}

/* ============================================================================
 * Internal: Pipeline Creation
 * ============================================================================ */

static bool gizmo_create_line_pipeline(Agentite_Gizmos *g)
{
    if (!g || !g->gpu) return false;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(g->gpu);

    SDL_GPUShader *vertex_shader = NULL;
    SDL_GPUShader *fragment_shader = NULL;

    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        /* Create vertex shader (Metal) */
        SDL_GPUShaderCreateInfo vs_info = {};
        vs_info.code = (const Uint8 *)gizmo_shader_msl;
        vs_info.code_size = sizeof(gizmo_shader_msl);
        vs_info.entrypoint = "gizmo_vertex";
        vs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        vs_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs_info.num_samplers = 0;
        vs_info.num_storage_textures = 0;
        vs_info.num_storage_buffers = 0;
        vs_info.num_uniform_buffers = 1;
        vertex_shader = SDL_CreateGPUShader(g->gpu, &vs_info);
        if (!vertex_shader) {
            agentite_set_error_from_sdl("Gizmo: Failed to create vertex shader");
            return false;
        }

        /* Create fragment shader (Metal) */
        SDL_GPUShaderCreateInfo fs_info = {};
        fs_info.code = (const Uint8 *)gizmo_shader_msl;
        fs_info.code_size = sizeof(gizmo_shader_msl);
        fs_info.entrypoint = "gizmo_fragment";
        fs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs_info.num_samplers = 0;
        fs_info.num_storage_textures = 0;
        fs_info.num_storage_buffers = 0;
        fs_info.num_uniform_buffers = 0;
        fragment_shader = SDL_CreateGPUShader(g->gpu, &fs_info);
        if (!fragment_shader) {
            agentite_set_error_from_sdl("Gizmo: Failed to create fragment shader");
            SDL_ReleaseGPUShader(g->gpu, vertex_shader);
            return false;
        }
    } else {
        agentite_set_error("Gizmo: No supported shader format (need MSL)");
        return false;
    }

    /* Define vertex attributes */
    SDL_GPUVertexAttribute attributes[2] = {};
    attributes[0].location = 0;
    attributes[0].buffer_slot = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attributes[0].offset = offsetof(GizmoLineVertex, pos);
    attributes[1].location = 1;
    attributes[1].buffer_slot = 0;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[1].offset = offsetof(GizmoLineVertex, color);

    SDL_GPUVertexBufferDescription vb_desc = {};
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(GizmoLineVertex);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vb_desc.instance_step_rate = 0;

    SDL_GPUVertexInputState vertex_input = {};
    vertex_input.vertex_buffer_descriptions = &vb_desc;
    vertex_input.num_vertex_buffers = 1;
    vertex_input.vertex_attributes = attributes;
    vertex_input.num_vertex_attributes = 2;

    /* Alpha blending */
    SDL_GPUColorTargetBlendState blend_state = {};
    blend_state.enable_blend = true;
    blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                                   SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

    SDL_GPUColorTargetDescription color_target = {};
    color_target.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    color_target.blend_state = blend_state;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;
    pipeline_info.vertex_input_state = vertex_input;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pipeline_info.rasterizer_state.enable_depth_clip = false;
    pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    pipeline_info.multisample_state.sample_mask = 0;
    pipeline_info.depth_stencil_state.enable_depth_test = false;
    pipeline_info.depth_stencil_state.enable_depth_write = false;
    pipeline_info.depth_stencil_state.enable_stencil_test = false;
    pipeline_info.target_info.color_target_descriptions = &color_target;
    pipeline_info.target_info.num_color_targets = 1;
    pipeline_info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID;
    pipeline_info.target_info.has_depth_stencil_target = false;

    g->line_pipeline = SDL_CreateGPUGraphicsPipeline(g->gpu, &pipeline_info);

    SDL_ReleaseGPUShader(g->gpu, vertex_shader);
    SDL_ReleaseGPUShader(g->gpu, fragment_shader);

    if (!g->line_pipeline) {
        agentite_set_error_from_sdl("Gizmo: Failed to create line pipeline");
        return false;
    }

    SDL_Log("Gizmo: Line pipeline created successfully");
    return true;
}

static bool gizmo_create_tri_pipeline(Agentite_Gizmos *g)
{
    if (!g || !g->gpu) return false;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(g->gpu);

    SDL_GPUShader *vertex_shader = NULL;
    SDL_GPUShader *fragment_shader = NULL;

    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        SDL_GPUShaderCreateInfo vs_info = {};
        vs_info.code = (const Uint8 *)gizmo_shader_msl;
        vs_info.code_size = sizeof(gizmo_shader_msl);
        vs_info.entrypoint = "gizmo_vertex";
        vs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        vs_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs_info.num_samplers = 0;
        vs_info.num_storage_textures = 0;
        vs_info.num_storage_buffers = 0;
        vs_info.num_uniform_buffers = 1;
        vertex_shader = SDL_CreateGPUShader(g->gpu, &vs_info);
        if (!vertex_shader) {
            SDL_Log("Gizmo: Failed to create tri vertex shader");
            return false;
        }

        SDL_GPUShaderCreateInfo fs_info = {};
        fs_info.code = (const Uint8 *)gizmo_shader_msl;
        fs_info.code_size = sizeof(gizmo_shader_msl);
        fs_info.entrypoint = "gizmo_fragment";
        fs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs_info.num_samplers = 0;
        fs_info.num_storage_textures = 0;
        fs_info.num_storage_buffers = 0;
        fs_info.num_uniform_buffers = 0;
        fragment_shader = SDL_CreateGPUShader(g->gpu, &fs_info);
        if (!fragment_shader) {
            SDL_ReleaseGPUShader(g->gpu, vertex_shader);
            return false;
        }
    } else {
        return false;
    }

    SDL_GPUVertexAttribute attributes[2] = {};
    attributes[0].location = 0;
    attributes[0].buffer_slot = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attributes[0].offset = offsetof(GizmoTriVertex, pos);
    attributes[1].location = 1;
    attributes[1].buffer_slot = 0;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[1].offset = offsetof(GizmoTriVertex, color);

    SDL_GPUVertexBufferDescription vb_desc = {};
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(GizmoTriVertex);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vb_desc.instance_step_rate = 0;

    SDL_GPUVertexInputState vertex_input = {};
    vertex_input.vertex_buffer_descriptions = &vb_desc;
    vertex_input.num_vertex_buffers = 1;
    vertex_input.vertex_attributes = attributes;
    vertex_input.num_vertex_attributes = 2;

    SDL_GPUColorTargetBlendState blend_state = {};
    blend_state.enable_blend = true;
    blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                                   SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

    SDL_GPUColorTargetDescription color_target = {};
    color_target.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    color_target.blend_state = blend_state;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;
    pipeline_info.vertex_input_state = vertex_input;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pipeline_info.rasterizer_state.enable_depth_clip = false;
    pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    pipeline_info.multisample_state.sample_mask = 0;
    pipeline_info.depth_stencil_state.enable_depth_test = false;
    pipeline_info.depth_stencil_state.enable_depth_write = false;
    pipeline_info.depth_stencil_state.enable_stencil_test = false;
    pipeline_info.target_info.color_target_descriptions = &color_target;
    pipeline_info.target_info.num_color_targets = 1;
    pipeline_info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID;
    pipeline_info.target_info.has_depth_stencil_target = false;

    g->tri_pipeline = SDL_CreateGPUGraphicsPipeline(g->gpu, &pipeline_info);

    SDL_ReleaseGPUShader(g->gpu, vertex_shader);
    SDL_ReleaseGPUShader(g->gpu, fragment_shader);

    if (!g->tri_pipeline) {
        SDL_Log("Gizmo: Failed to create tri pipeline");
        return false;
    }

    SDL_Log("Gizmo: Triangle pipeline created successfully");
    return true;
}

/* ============================================================================
 * Internal: Line Drawing Helpers
 * ============================================================================ */

static void gizmo_add_line_vertex(Agentite_Gizmos *g, float x, float y, float z,
                                   float r, float gc, float b, float a)
{
    if (g->line_vertex_count >= GIZMO_MAX_LINES * 2) return;

    GizmoLineVertex *v = &g->line_vertices[g->line_vertex_count++];
    v->pos[0] = x;
    v->pos[1] = y;
    v->pos[2] = z;
    v->color[0] = r;
    v->color[1] = gc;
    v->color[2] = b;
    v->color[3] = a;
}

static void gizmo_add_line_3d(Agentite_Gizmos *g,
                               float x1, float y1, float z1,
                               float x2, float y2, float z2,
                               uint32_t color)
{
    float r, gc, b, a;
    color_unpack(color, &r, &gc, &b, &a);

    gizmo_add_line_vertex(g, x1, y1, z1, r, gc, b, a);
    gizmo_add_line_vertex(g, x2, y2, z2, r, gc, b, a);
}

static void gizmo_add_tri_vertex(Agentite_Gizmos *g, float x, float y, float z,
                                  float r, float gc, float b, float a)
{
    if (g->tri_vertex_count >= GIZMO_MAX_TRIANGLES * 3) return;

    GizmoTriVertex *v = &g->tri_vertices[g->tri_vertex_count++];
    v->pos[0] = x;
    v->pos[1] = y;
    v->pos[2] = z;
    v->color[0] = r;
    v->color[1] = gc;
    v->color[2] = b;
    v->color[3] = a;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

Agentite_Gizmos *agentite_gizmos_create(SDL_GPUDevice *device,
                                         const Agentite_GizmoConfig *config)
{
    if (!device) {
        agentite_set_error("Gizmo: NULL device");
        return NULL;
    }

    Agentite_Gizmos *g = (Agentite_Gizmos *)calloc(1, sizeof(Agentite_Gizmos));
    if (!g) {
        agentite_set_error("Gizmo: Failed to allocate renderer");
        return NULL;
    }

    g->gpu = device;
    g->screen_width = 1280;
    g->screen_height = 720;

    /* Apply configuration */
    if (config) {
        g->config = *config;
    } else {
        Agentite_GizmoConfig defaults = AGENTITE_GIZMO_CONFIG_DEFAULT;
        g->config = defaults;
    }

    /* Allocate CPU-side buffers */
    g->line_vertices = (GizmoLineVertex *)malloc(GIZMO_MAX_LINES * 2 * sizeof(GizmoLineVertex));
    g->tri_vertices = (GizmoTriVertex *)malloc(GIZMO_MAX_TRIANGLES * 3 * sizeof(GizmoTriVertex));
    if (!g->line_vertices || !g->tri_vertices) {
        agentite_set_error("Gizmo: Failed to allocate batch buffers");
        agentite_gizmos_destroy(g);
        return NULL;
    }

    /* Create GPU buffers */
    SDL_GPUBufferCreateInfo line_vb_info = {};
    line_vb_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    line_vb_info.size = (Uint32)(GIZMO_MAX_LINES * 2 * sizeof(GizmoLineVertex));
    line_vb_info.props = 0;
    g->line_vertex_buffer = SDL_CreateGPUBuffer(device, &line_vb_info);
    if (!g->line_vertex_buffer) {
        agentite_set_error_from_sdl("Gizmo: Failed to create line vertex buffer");
        agentite_gizmos_destroy(g);
        return NULL;
    }

    SDL_GPUBufferCreateInfo tri_vb_info = {};
    tri_vb_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    tri_vb_info.size = (Uint32)(GIZMO_MAX_TRIANGLES * 3 * sizeof(GizmoTriVertex));
    tri_vb_info.props = 0;
    g->tri_vertex_buffer = SDL_CreateGPUBuffer(device, &tri_vb_info);
    if (!g->tri_vertex_buffer) {
        agentite_set_error_from_sdl("Gizmo: Failed to create tri vertex buffer");
        agentite_gizmos_destroy(g);
        return NULL;
    }

    /* Create pipelines */
    if (!gizmo_create_line_pipeline(g)) {
        agentite_gizmos_destroy(g);
        return NULL;
    }

    if (!gizmo_create_tri_pipeline(g)) {
        /* Non-fatal - filled shapes won't work */
        SDL_Log("Gizmo: Warning - triangle pipeline creation failed");
    }

    SDL_Log("Gizmo: Renderer initialized");
    return g;
}

void agentite_gizmos_destroy(Agentite_Gizmos *gizmos)
{
    if (!gizmos) return;

    if (gizmos->line_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(gizmos->gpu, gizmos->line_pipeline);
    }
    if (gizmos->tri_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(gizmos->gpu, gizmos->tri_pipeline);
    }
    if (gizmos->line_vertex_buffer) {
        SDL_ReleaseGPUBuffer(gizmos->gpu, gizmos->line_vertex_buffer);
    }
    if (gizmos->tri_vertex_buffer) {
        SDL_ReleaseGPUBuffer(gizmos->gpu, gizmos->tri_vertex_buffer);
    }

    free(gizmos->line_vertices);
    free(gizmos->tri_vertices);
    free(gizmos);

    SDL_Log("Gizmo: Renderer destroyed");
}

/* ============================================================================
 * Frame Management
 * ============================================================================ */

void agentite_gizmos_begin(Agentite_Gizmos *gizmos, Agentite_Camera *camera)
{
    if (!gizmos) return;

    gizmos->camera = camera;
    gizmos->line_vertex_count = 0;
    gizmos->tri_vertex_count = 0;
    gizmos->is_hovered = false;
    gizmos->frame_started = true;
}

void agentite_gizmos_end(Agentite_Gizmos *gizmos)
{
    if (!gizmos) return;
    gizmos->frame_started = false;
}

void agentite_gizmos_set_screen_size(Agentite_Gizmos *gizmos, int width, int height)
{
    if (!gizmos) return;
    gizmos->screen_width = width;
    gizmos->screen_height = height;
}

void agentite_gizmos_upload(Agentite_Gizmos *gizmos, SDL_GPUCommandBuffer *cmd)
{
    if (!gizmos || !cmd) return;

    /* Upload line vertices */
    if (gizmos->line_vertex_count > 0) {
        SDL_GPUTransferBufferCreateInfo transfer_info = {};
        transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_info.size = (Uint32)(gizmos->line_vertex_count * sizeof(GizmoLineVertex));
        transfer_info.props = 0;
        SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(gizmos->gpu, &transfer_info);
        if (transfer) {
            void *mapped = SDL_MapGPUTransferBuffer(gizmos->gpu, transfer, false);
            if (mapped) {
                memcpy(mapped, gizmos->line_vertices,
                       gizmos->line_vertex_count * sizeof(GizmoLineVertex));
                SDL_UnmapGPUTransferBuffer(gizmos->gpu, transfer);
            }

            SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
            if (copy_pass) {
                SDL_GPUTransferBufferLocation src = {};
                src.transfer_buffer = transfer;
                src.offset = 0;
                SDL_GPUBufferRegion dst = {};
                dst.buffer = gizmos->line_vertex_buffer;
                dst.offset = 0;
                dst.size = (Uint32)(gizmos->line_vertex_count * sizeof(GizmoLineVertex));
                SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);
                SDL_EndGPUCopyPass(copy_pass);
            }

            SDL_ReleaseGPUTransferBuffer(gizmos->gpu, transfer);
        }
    }

    /* Upload triangle vertices */
    if (gizmos->tri_vertex_count > 0) {
        SDL_GPUTransferBufferCreateInfo transfer_info = {};
        transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_info.size = (Uint32)(gizmos->tri_vertex_count * sizeof(GizmoTriVertex));
        transfer_info.props = 0;
        SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(gizmos->gpu, &transfer_info);
        if (transfer) {
            void *mapped = SDL_MapGPUTransferBuffer(gizmos->gpu, transfer, false);
            if (mapped) {
                memcpy(mapped, gizmos->tri_vertices,
                       gizmos->tri_vertex_count * sizeof(GizmoTriVertex));
                SDL_UnmapGPUTransferBuffer(gizmos->gpu, transfer);
            }

            SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
            if (copy_pass) {
                SDL_GPUTransferBufferLocation src = {};
                src.transfer_buffer = transfer;
                src.offset = 0;
                SDL_GPUBufferRegion dst = {};
                dst.buffer = gizmos->tri_vertex_buffer;
                dst.offset = 0;
                dst.size = (Uint32)(gizmos->tri_vertex_count * sizeof(GizmoTriVertex));
                SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);
                SDL_EndGPUCopyPass(copy_pass);
            }

            SDL_ReleaseGPUTransferBuffer(gizmos->gpu, transfer);
        }
    }
}

void agentite_gizmos_render(Agentite_Gizmos *gizmos, SDL_GPUCommandBuffer *cmd,
                            SDL_GPURenderPass *pass)
{
    if (!gizmos || !cmd || !pass) return;

    /* Build uniforms */
    struct {
        float view_projection[16];
        float screen_size[2];
        float padding[2];
    } uniforms;

    if (gizmos->camera) {
        const float *vp = agentite_camera_get_vp_matrix(gizmos->camera);
        memcpy(uniforms.view_projection, vp, sizeof(float) * 16);
    } else {
        mat4 ortho;
        glm_ortho(0.0f, (float)gizmos->screen_width,
                  (float)gizmos->screen_height, 0.0f,
                  -1.0f, 1.0f, ortho);
        memcpy(uniforms.view_projection, ortho, sizeof(float) * 16);
    }

    uniforms.screen_size[0] = (float)gizmos->screen_width;
    uniforms.screen_size[1] = (float)gizmos->screen_height;
    uniforms.padding[0] = 0.0f;
    uniforms.padding[1] = 0.0f;

    /* Render triangles first (filled shapes behind lines) */
    if (gizmos->tri_vertex_count > 0 && gizmos->tri_pipeline) {
        SDL_BindGPUGraphicsPipeline(pass, gizmos->tri_pipeline);

        SDL_GPUBufferBinding vb_binding = {};
        vb_binding.buffer = gizmos->tri_vertex_buffer;
        vb_binding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

        SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

        SDL_DrawGPUPrimitives(pass, gizmos->tri_vertex_count, 1, 0, 0);
    }

    /* Render lines */
    if (gizmos->line_vertex_count > 0 && gizmos->line_pipeline) {
        SDL_BindGPUGraphicsPipeline(pass, gizmos->line_pipeline);

        SDL_GPUBufferBinding vb_binding = {};
        vb_binding.buffer = gizmos->line_vertex_buffer;
        vb_binding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

        SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

        SDL_DrawGPUPrimitives(pass, gizmos->line_vertex_count, 1, 0, 0);
    }
}

/* ============================================================================
 * Input Handling
 * ============================================================================ */

void agentite_gizmos_update_input(Agentite_Gizmos *gizmos,
                                   float mouse_x, float mouse_y,
                                   bool mouse_down, bool mouse_pressed)
{
    if (!gizmos) return;

    gizmos->mouse_x = mouse_x;
    gizmos->mouse_y = mouse_y;
    gizmos->mouse_down = mouse_down;
    gizmos->mouse_pressed = mouse_pressed;

    /* Release active state when mouse is released */
    if (!mouse_down) {
        gizmos->is_active = false;
        gizmos->active_axis = AGENTITE_AXIS_NONE;
    }
}

bool agentite_gizmos_is_active(Agentite_Gizmos *gizmos)
{
    return gizmos ? gizmos->is_active : false;
}

bool agentite_gizmos_is_hovered(Agentite_Gizmos *gizmos)
{
    return gizmos ? gizmos->is_hovered : false;
}

/* ============================================================================
 * Transform Gizmos
 * ============================================================================ */

/* Helper: Project world point to screen */
static void world_to_screen(Agentite_Gizmos *g, vec3 world, float *screen_x, float *screen_y)
{
    if (g->camera) {
        agentite_camera_world_to_screen(g->camera, world[0], world[1], screen_x, screen_y);
    } else {
        *screen_x = world[0];
        *screen_y = world[1];
    }
}

/* Helper: Calculate screen distance from point to line segment */
static float point_to_line_distance(float px, float py,
                                     float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len_sq = dx * dx + dy * dy;

    if (len_sq < 0.0001f) {
        /* Line segment is a point */
        return sqrtf((px - x1) * (px - x1) + (py - y1) * (py - y1));
    }

    /* Project point onto line, clamped to segment */
    float t = ((px - x1) * dx + (py - y1) * dy) / len_sq;
    t = fmaxf(0.0f, fminf(1.0f, t));

    float proj_x = x1 + t * dx;
    float proj_y = y1 + t * dy;

    return sqrtf((px - proj_x) * (px - proj_x) + (py - proj_y) * (py - proj_y));
}

Agentite_GizmoResult agentite_gizmo_translate(Agentite_Gizmos *gizmos,
                                               vec3 position,
                                               mat4 *orientation)
{
    Agentite_GizmoResult result = {};
    glm_vec3_zero(result.delta);

    if (!gizmos || !gizmos->frame_started) return result;

    float handle_size = gizmos->config.handle_size;

    /* Calculate screen-space gizmo size if enabled */
    float scale = 1.0f;
    if (gizmos->config.screen_space_size && gizmos->camera) {
        /* Get camera zoom for consistent screen size */
        float zoom = agentite_camera_get_zoom(gizmos->camera);
        scale = 1.0f / zoom;
    }

    float axis_len = handle_size * scale;

    /* Calculate axis directions (default to world axes) */
    vec3 axis_x = {1, 0, 0};
    vec3 axis_y = {0, 1, 0};
    vec3 axis_z = {0, 0, 1};

    if (orientation) {
        glm_vec3_copy((*orientation)[0], axis_x);
        glm_vec3_copy((*orientation)[1], axis_y);
        glm_vec3_copy((*orientation)[2], axis_z);
    }

    /* Calculate axis endpoints */
    vec3 end_x, end_y, end_z;
    glm_vec3_scale(axis_x, axis_len, end_x);
    glm_vec3_add(position, end_x, end_x);
    glm_vec3_scale(axis_y, axis_len, end_y);
    glm_vec3_add(position, end_y, end_y);
    glm_vec3_scale(axis_z, axis_len, end_z);
    glm_vec3_add(position, end_z, end_z);

    /* Project to screen for hit testing */
    float center_sx, center_sy;
    float end_x_sx, end_x_sy;
    float end_y_sx, end_y_sy;

    world_to_screen(gizmos, position, &center_sx, &center_sy);
    world_to_screen(gizmos, end_x, &end_x_sx, &end_x_sy);
    world_to_screen(gizmos, end_y, &end_y_sx, &end_y_sy);

    /* Hit test each axis */
    float threshold = gizmos->config.hover_threshold;
    float dist_x = point_to_line_distance(gizmos->mouse_x, gizmos->mouse_y,
                                           center_sx, center_sy, end_x_sx, end_x_sy);
    float dist_y = point_to_line_distance(gizmos->mouse_x, gizmos->mouse_y,
                                           center_sx, center_sy, end_y_sx, end_y_sy);

    Agentite_GizmoAxis hovered_axis = AGENTITE_AXIS_NONE;
    if (dist_x < threshold && dist_x < dist_y) {
        hovered_axis = AGENTITE_AXIS_X;
    } else if (dist_y < threshold) {
        hovered_axis = AGENTITE_AXIS_Y;
    }

    /* Handle interaction */
    if (gizmos->is_active && gizmos->active_axis != AGENTITE_AXIS_NONE) {
        /* Continue dragging */
        result.active = true;
        result.axis = gizmos->active_axis;

        /* Calculate delta based on mouse movement */
        float delta_x = gizmos->mouse_x - gizmos->drag_start_mouse_x;
        float delta_y = gizmos->mouse_y - gizmos->drag_start_mouse_y;

        /* Convert screen delta to world delta (simplified for 2D) */
        /* Both screen and world use Y-down convention in this engine */
        if (gizmos->active_axis == AGENTITE_AXIS_X) {
            result.delta[0] = delta_x * scale;
        } else if (gizmos->active_axis == AGENTITE_AXIS_Y) {
            result.delta[1] = delta_y * scale;
        }

        /* Update drag start for next frame */
        gizmos->drag_start_mouse_x = gizmos->mouse_x;
        gizmos->drag_start_mouse_y = gizmos->mouse_y;
    } else if (hovered_axis != AGENTITE_AXIS_NONE) {
        result.hovered = true;
        result.axis = hovered_axis;
        gizmos->is_hovered = true;

        /* Start dragging on mouse press */
        if (gizmos->mouse_pressed) {
            gizmos->is_active = true;
            gizmos->active_axis = hovered_axis;
            glm_vec3_copy(position, gizmos->drag_start_pos);
            gizmos->drag_start_mouse_x = gizmos->mouse_x;
            gizmos->drag_start_mouse_y = gizmos->mouse_y;
        }
    }

    /* Determine colors */
    uint32_t x_color = gizmos->config.colors.x_color;
    uint32_t y_color = gizmos->config.colors.y_color;

    if (gizmos->is_active) {
        if (gizmos->active_axis == AGENTITE_AXIS_X) {
            x_color = gizmos->config.colors.active_color;
        } else if (gizmos->active_axis == AGENTITE_AXIS_Y) {
            y_color = gizmos->config.colors.active_color;
        }
    } else if (result.hovered) {
        if (result.axis == AGENTITE_AXIS_X) {
            x_color = gizmos->config.colors.hover_color;
        } else if (result.axis == AGENTITE_AXIS_Y) {
            y_color = gizmos->config.colors.hover_color;
        }
    }

    /* Draw the gizmo axes */
    agentite_gizmos_arrow(gizmos, position, end_x, x_color);
    agentite_gizmos_arrow(gizmos, position, end_y, y_color);

    return result;
}

Agentite_GizmoResult agentite_gizmo_rotate(Agentite_Gizmos *gizmos,
                                            vec3 position,
                                            mat4 *orientation)
{
    Agentite_GizmoResult result = {};
    glm_vec3_zero(result.delta);

    if (!gizmos || !gizmos->frame_started) return result;

    /* TODO: Implement rotation gizmo */
    /* For now, just draw circles around each axis */

    float radius = gizmos->config.handle_size;
    if (gizmos->config.screen_space_size && gizmos->camera) {
        float zoom = agentite_camera_get_zoom(gizmos->camera);
        radius /= zoom;
    }

    vec3 normal_x = {1, 0, 0};
    vec3 normal_y = {0, 1, 0};
    vec3 normal_z = {0, 0, 1};

    agentite_gizmos_circle(gizmos, position, normal_x, radius, gizmos->config.colors.x_color);
    agentite_gizmos_circle(gizmos, position, normal_y, radius, gizmos->config.colors.y_color);
    agentite_gizmos_circle(gizmos, position, normal_z, radius, gizmos->config.colors.z_color);

    return result;
}

Agentite_GizmoResult agentite_gizmo_scale(Agentite_Gizmos *gizmos,
                                           vec3 position,
                                           mat4 *orientation)
{
    Agentite_GizmoResult result = {};
    glm_vec3_zero(result.delta);

    if (!gizmos || !gizmos->frame_started) return result;

    /* TODO: Implement scale gizmo */
    /* For now, just draw lines with boxes at the ends */

    float handle_size = gizmos->config.handle_size;
    if (gizmos->config.screen_space_size && gizmos->camera) {
        float zoom = agentite_camera_get_zoom(gizmos->camera);
        handle_size /= zoom;
    }

    vec3 end_x = {position[0] + handle_size, position[1], position[2]};
    vec3 end_y = {position[0], position[1] + handle_size, position[2]};
    vec3 end_z = {position[0], position[1], position[2] + handle_size};

    agentite_gizmos_line(gizmos, position, end_x, gizmos->config.colors.x_color);
    agentite_gizmos_line(gizmos, position, end_y, gizmos->config.colors.y_color);
    agentite_gizmos_line(gizmos, position, end_z, gizmos->config.colors.z_color);

    /* Draw small boxes at endpoints */
    float box_size = handle_size * 0.1f;
    vec3 box_dim = {box_size, box_size, box_size};
    agentite_gizmos_box(gizmos, end_x, box_dim, gizmos->config.colors.x_color);
    agentite_gizmos_box(gizmos, end_y, box_dim, gizmos->config.colors.y_color);
    agentite_gizmos_box(gizmos, end_z, box_dim, gizmos->config.colors.z_color);

    return result;
}

Agentite_GizmoResult agentite_gizmo_transform(Agentite_Gizmos *gizmos,
                                               Agentite_GizmoMode mode,
                                               vec3 position,
                                               mat4 *orientation)
{
    switch (mode) {
        case AGENTITE_GIZMO_TRANSLATE:
            return agentite_gizmo_translate(gizmos, position, orientation);
        case AGENTITE_GIZMO_ROTATE:
            return agentite_gizmo_rotate(gizmos, position, orientation);
        case AGENTITE_GIZMO_SCALE:
            return agentite_gizmo_scale(gizmos, position, orientation);
        default:
            break;
    }

    Agentite_GizmoResult result = {};
    return result;
}

/* ============================================================================
 * Debug Drawing - 3D World Space
 * ============================================================================ */

void agentite_gizmos_line(Agentite_Gizmos *gizmos, vec3 from, vec3 to, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    gizmo_add_line_3d(gizmos, from[0], from[1], from[2], to[0], to[1], to[2], color);
}

void agentite_gizmos_ray(Agentite_Gizmos *gizmos, vec3 origin, vec3 dir,
                          float length, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    vec3 end;
    glm_vec3_scale(dir, length, end);
    glm_vec3_add(origin, end, end);

    gizmo_add_line_3d(gizmos, origin[0], origin[1], origin[2], end[0], end[1], end[2], color);
}

void agentite_gizmos_arrow(Agentite_Gizmos *gizmos, vec3 from, vec3 to, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    /* Draw main line */
    gizmo_add_line_3d(gizmos, from[0], from[1], from[2], to[0], to[1], to[2], color);

    /* Calculate arrow head */
    vec3 dir;
    glm_vec3_sub(to, from, dir);
    float len = glm_vec3_norm(dir);
    if (len < 0.0001f) return;

    glm_vec3_scale(dir, 1.0f / len, dir); /* Normalize */

    float head_len = len * GIZMO_ARROW_HEAD_SIZE;

    /* Find perpendicular vector for arrow head - use 2D perpendicular for XY plane */
    vec3 perp;
    if (fabsf(dir[2]) > 0.9f) {
        /* Arrow mostly in Z direction - use X as perpendicular */
        perp[0] = 1.0f;
        perp[1] = 0.0f;
        perp[2] = 0.0f;
    } else {
        /* Arrow in XY plane - perpendicular is (-dy, dx, 0) normalized */
        float perp_len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1]);
        if (perp_len > 0.0001f) {
            perp[0] = -dir[1] / perp_len;
            perp[1] = dir[0] / perp_len;
            perp[2] = 0.0f;
        } else {
            perp[0] = 1.0f;
            perp[1] = 0.0f;
            perp[2] = 0.0f;
        }
    }

    /* Arrow head points */
    vec3 head_base;
    glm_vec3_scale(dir, -head_len, head_base);
    glm_vec3_add(to, head_base, head_base);

    vec3 head_left, head_right;
    glm_vec3_scale(perp, head_len * 0.5f, head_left);
    glm_vec3_add(head_base, head_left, head_left);
    glm_vec3_scale(perp, -head_len * 0.5f, head_right);
    glm_vec3_add(head_base, head_right, head_right);

    gizmo_add_line_3d(gizmos, to[0], to[1], to[2], head_left[0], head_left[1], head_left[2], color);
    gizmo_add_line_3d(gizmos, to[0], to[1], to[2], head_right[0], head_right[1], head_right[2], color);
}

void agentite_gizmos_box(Agentite_Gizmos *gizmos, vec3 center, vec3 size, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    float hx = size[0] * 0.5f;
    float hy = size[1] * 0.5f;
    float hz = size[2] * 0.5f;

    /* 8 corners */
    vec3 corners[8] = {
        {center[0] - hx, center[1] - hy, center[2] - hz},
        {center[0] + hx, center[1] - hy, center[2] - hz},
        {center[0] + hx, center[1] + hy, center[2] - hz},
        {center[0] - hx, center[1] + hy, center[2] - hz},
        {center[0] - hx, center[1] - hy, center[2] + hz},
        {center[0] + hx, center[1] - hy, center[2] + hz},
        {center[0] + hx, center[1] + hy, center[2] + hz},
        {center[0] - hx, center[1] + hy, center[2] + hz}
    };

    /* 12 edges */
    /* Bottom face */
    gizmo_add_line_3d(gizmos, corners[0][0], corners[0][1], corners[0][2],
                      corners[1][0], corners[1][1], corners[1][2], color);
    gizmo_add_line_3d(gizmos, corners[1][0], corners[1][1], corners[1][2],
                      corners[2][0], corners[2][1], corners[2][2], color);
    gizmo_add_line_3d(gizmos, corners[2][0], corners[2][1], corners[2][2],
                      corners[3][0], corners[3][1], corners[3][2], color);
    gizmo_add_line_3d(gizmos, corners[3][0], corners[3][1], corners[3][2],
                      corners[0][0], corners[0][1], corners[0][2], color);

    /* Top face */
    gizmo_add_line_3d(gizmos, corners[4][0], corners[4][1], corners[4][2],
                      corners[5][0], corners[5][1], corners[5][2], color);
    gizmo_add_line_3d(gizmos, corners[5][0], corners[5][1], corners[5][2],
                      corners[6][0], corners[6][1], corners[6][2], color);
    gizmo_add_line_3d(gizmos, corners[6][0], corners[6][1], corners[6][2],
                      corners[7][0], corners[7][1], corners[7][2], color);
    gizmo_add_line_3d(gizmos, corners[7][0], corners[7][1], corners[7][2],
                      corners[4][0], corners[4][1], corners[4][2], color);

    /* Vertical edges */
    gizmo_add_line_3d(gizmos, corners[0][0], corners[0][1], corners[0][2],
                      corners[4][0], corners[4][1], corners[4][2], color);
    gizmo_add_line_3d(gizmos, corners[1][0], corners[1][1], corners[1][2],
                      corners[5][0], corners[5][1], corners[5][2], color);
    gizmo_add_line_3d(gizmos, corners[2][0], corners[2][1], corners[2][2],
                      corners[6][0], corners[6][1], corners[6][2], color);
    gizmo_add_line_3d(gizmos, corners[3][0], corners[3][1], corners[3][2],
                      corners[7][0], corners[7][1], corners[7][2], color);
}

void agentite_gizmos_sphere(Agentite_Gizmos *gizmos, vec3 center, float radius, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    /* Draw 3 circles (one for each axis plane) */
    vec3 normal_x = {1, 0, 0};
    vec3 normal_y = {0, 1, 0};
    vec3 normal_z = {0, 0, 1};

    agentite_gizmos_circle(gizmos, center, normal_x, radius, color);
    agentite_gizmos_circle(gizmos, center, normal_y, radius, color);
    agentite_gizmos_circle(gizmos, center, normal_z, radius, color);
}

void agentite_gizmos_circle(Agentite_Gizmos *gizmos, vec3 center, vec3 normal,
                             float radius, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    /* Find two perpendicular vectors on the circle's plane */
    vec3 u, v;
    if (fabsf(normal[0]) < 0.9f) {
        vec3 temp = {1, 0, 0};
        glm_vec3_cross(normal, temp, u);
    } else {
        vec3 temp = {0, 1, 0};
        glm_vec3_cross(normal, temp, u);
    }
    glm_vec3_normalize(u);
    glm_vec3_cross(normal, u, v);
    glm_vec3_normalize(v);

    /* Draw circle as line segments */
    float angle_step = (2.0f * GLM_PI) / GIZMO_CIRCLE_SEGMENTS;
    vec3 prev, curr;

    /* First point */
    glm_vec3_scale(u, radius, prev);
    glm_vec3_add(center, prev, prev);

    for (int i = 1; i <= GIZMO_CIRCLE_SEGMENTS; i++) {
        float angle = i * angle_step;
        float cos_a = cosf(angle);
        float sin_a = sinf(angle);

        vec3 offset;
        glm_vec3_scale(u, radius * cos_a, offset);
        vec3 offset2;
        glm_vec3_scale(v, radius * sin_a, offset2);
        glm_vec3_add(offset, offset2, offset);
        glm_vec3_add(center, offset, curr);

        gizmo_add_line_3d(gizmos, prev[0], prev[1], prev[2], curr[0], curr[1], curr[2], color);

        glm_vec3_copy(curr, prev);
    }
}

void agentite_gizmos_arc(Agentite_Gizmos *gizmos, vec3 center, vec3 normal,
                          vec3 from, float angle, float radius, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    /* Normalize the 'from' direction */
    vec3 u;
    glm_vec3_normalize_to(from, u);

    /* Get perpendicular direction on the plane */
    vec3 v;
    glm_vec3_cross(normal, u, v);
    glm_vec3_normalize(v);

    /* Draw arc as line segments */
    int segments = (int)(fabsf(angle) / (2.0f * GLM_PI) * GIZMO_CIRCLE_SEGMENTS);
    if (segments < 3) segments = 3;

    float angle_step = angle / segments;
    vec3 prev, curr;

    /* First point */
    glm_vec3_scale(u, radius, prev);
    glm_vec3_add(center, prev, prev);

    for (int i = 1; i <= segments; i++) {
        float a = i * angle_step;
        float cos_a = cosf(a);
        float sin_a = sinf(a);

        vec3 offset;
        glm_vec3_scale(u, radius * cos_a, offset);
        vec3 offset2;
        glm_vec3_scale(v, radius * sin_a, offset2);
        glm_vec3_add(offset, offset2, offset);
        glm_vec3_add(center, offset, curr);

        gizmo_add_line_3d(gizmos, prev[0], prev[1], prev[2], curr[0], curr[1], curr[2], color);

        glm_vec3_copy(curr, prev);
    }
}

void agentite_gizmos_bounds(Agentite_Gizmos *gizmos, vec3 min, vec3 max, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    vec3 center, size;
    glm_vec3_add(min, max, center);
    glm_vec3_scale(center, 0.5f, center);
    glm_vec3_sub(max, min, size);

    agentite_gizmos_box(gizmos, center, size, color);
}

void agentite_gizmos_grid(Agentite_Gizmos *gizmos, vec3 center, vec3 normal,
                           float size, float spacing, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    /* Find two perpendicular vectors on the grid plane */
    vec3 u, v;
    if (fabsf(normal[0]) < 0.9f) {
        vec3 temp = {1, 0, 0};
        glm_vec3_cross(normal, temp, u);
    } else {
        vec3 temp = {0, 1, 0};
        glm_vec3_cross(normal, temp, u);
    }
    glm_vec3_normalize(u);
    glm_vec3_cross(normal, u, v);
    glm_vec3_normalize(v);

    float half_size = size * 0.5f;
    int lines = (int)(size / spacing);

    /* Draw lines along u axis */
    for (int i = -lines / 2; i <= lines / 2; i++) {
        vec3 start, end;
        float offset = i * spacing;

        glm_vec3_scale(v, offset, start);
        glm_vec3_add(center, start, start);
        glm_vec3_copy(start, end);

        vec3 u_half;
        glm_vec3_scale(u, half_size, u_half);
        glm_vec3_sub(start, u_half, start);
        glm_vec3_add(end, u_half, end);

        gizmo_add_line_3d(gizmos, start[0], start[1], start[2], end[0], end[1], end[2], color);
    }

    /* Draw lines along v axis */
    for (int i = -lines / 2; i <= lines / 2; i++) {
        vec3 start, end;
        float offset = i * spacing;

        glm_vec3_scale(u, offset, start);
        glm_vec3_add(center, start, start);
        glm_vec3_copy(start, end);

        vec3 v_half;
        glm_vec3_scale(v, half_size, v_half);
        glm_vec3_sub(start, v_half, start);
        glm_vec3_add(end, v_half, end);

        gizmo_add_line_3d(gizmos, start[0], start[1], start[2], end[0], end[1], end[2], color);
    }
}

/* ============================================================================
 * Debug Drawing - 2D Screen Space
 * ============================================================================ */

void agentite_gizmos_line_2d(Agentite_Gizmos *gizmos, float x1, float y1,
                              float x2, float y2, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    /* For 2D, we draw in screen space (z=0) */
    /* The shader will handle the orthographic projection */
    gizmo_add_line_3d(gizmos, x1, y1, 0.0f, x2, y2, 0.0f, color);
}

void agentite_gizmos_rect_2d(Agentite_Gizmos *gizmos, float x, float y,
                              float w, float h, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    /* Draw 4 edges */
    gizmo_add_line_3d(gizmos, x, y, 0.0f, x + w, y, 0.0f, color);
    gizmo_add_line_3d(gizmos, x + w, y, 0.0f, x + w, y + h, 0.0f, color);
    gizmo_add_line_3d(gizmos, x + w, y + h, 0.0f, x, y + h, 0.0f, color);
    gizmo_add_line_3d(gizmos, x, y + h, 0.0f, x, y, 0.0f, color);
}

void agentite_gizmos_rect_filled_2d(Agentite_Gizmos *gizmos, float x, float y,
                                     float w, float h, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;
    if (!gizmos->tri_pipeline) return;

    float r, g, b, a;
    color_unpack(color, &r, &g, &b, &a);

    /* Two triangles for the rectangle */
    gizmo_add_tri_vertex(gizmos, x, y, 0.0f, r, g, b, a);
    gizmo_add_tri_vertex(gizmos, x + w, y, 0.0f, r, g, b, a);
    gizmo_add_tri_vertex(gizmos, x + w, y + h, 0.0f, r, g, b, a);

    gizmo_add_tri_vertex(gizmos, x, y, 0.0f, r, g, b, a);
    gizmo_add_tri_vertex(gizmos, x + w, y + h, 0.0f, r, g, b, a);
    gizmo_add_tri_vertex(gizmos, x, y + h, 0.0f, r, g, b, a);
}

void agentite_gizmos_circle_2d(Agentite_Gizmos *gizmos, float x, float y,
                                float radius, uint32_t color)
{
    if (!gizmos || !gizmos->frame_started) return;

    float angle_step = (2.0f * GLM_PI) / GIZMO_CIRCLE_SEGMENTS;
    float prev_x = x + radius;
    float prev_y = y;

    for (int i = 1; i <= GIZMO_CIRCLE_SEGMENTS; i++) {
        float angle = i * angle_step;
        float curr_x = x + radius * cosf(angle);
        float curr_y = y + radius * sinf(angle);

        gizmo_add_line_3d(gizmos, prev_x, prev_y, 0.0f, curr_x, curr_y, 0.0f, color);

        prev_x = curr_x;
        prev_y = curr_y;
    }
}
