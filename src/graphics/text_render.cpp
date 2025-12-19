/*
 * Carbon Text Rendering Implementation
 *
 * Handles text batching, uploading, and rendering.
 */

#include "text_internal.h"

/* ============================================================================
 * Internal: Glyph Rendering
 * ============================================================================ */

void text_add_glyph(Agentite_TextRenderer *tr,
                    float x0, float y0, float x1, float y1,
                    float u0, float v0, float u1, float v1,
                    float r, float g, float b, float a)
{
    /* Check total capacity across all batches */
    if (tr->vertex_count + 4 > TEXT_VERTEX_CAPACITY) {
        SDL_Log("Text: Total vertex buffer overflow, glyph dropped");
        return;
    }

    uint32_t base = tr->vertex_count;
    TextVertex *v = &tr->vertices[base];

    /* Top-left */
    v[0].pos[0] = x0; v[0].pos[1] = y0;
    v[0].uv[0] = u0; v[0].uv[1] = v0;
    v[0].color[0] = r; v[0].color[1] = g; v[0].color[2] = b; v[0].color[3] = a;

    /* Top-right */
    v[1].pos[0] = x1; v[1].pos[1] = y0;
    v[1].uv[0] = u1; v[1].uv[1] = v0;
    v[1].color[0] = r; v[1].color[1] = g; v[1].color[2] = b; v[1].color[3] = a;

    /* Bottom-right */
    v[2].pos[0] = x1; v[2].pos[1] = y1;
    v[2].uv[0] = u1; v[2].uv[1] = v1;
    v[2].color[0] = r; v[2].color[1] = g; v[2].color[2] = b; v[2].color[3] = a;

    /* Bottom-left */
    v[3].pos[0] = x0; v[3].pos[1] = y1;
    v[3].uv[0] = u0; v[3].uv[1] = v1;
    v[3].color[0] = r; v[3].color[1] = g; v[3].color[2] = b; v[3].color[3] = a;

    tr->glyph_count++;
    tr->vertex_count += 4;
    tr->index_count += 6;
}

/* ============================================================================
 * Rendering Functions
 * ============================================================================ */

void agentite_text_begin(Agentite_TextRenderer *tr)
{
    if (!tr) return;

    /* If this is the first batch after upload/render, reset the queue */
    if (tr->queued_batch_count == 0) {
        tr->vertex_count = 0;
        tr->index_count = 0;
    }

    /* Track where this batch starts in the shared buffers */
    tr->current_batch_vertex_start = tr->vertex_count;
    tr->current_batch_index_start = tr->index_count;
    tr->glyph_count = 0;

    tr->current_font = NULL;
    tr->current_sdf_font = NULL;
    tr->is_sdf_batch = false;
    tr->current_sdf_scale = 1.0f;
    memset(&tr->current_effects, 0, sizeof(tr->current_effects));
    tr->batch_started = true;
}

void agentite_text_draw(Agentite_TextRenderer *tr, Agentite_Font *font,
                      const char *text, float x, float y)
{
    agentite_text_draw_ex(tr, font, text, x, y, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                        AGENTITE_TEXT_ALIGN_LEFT);
}

void agentite_text_draw_colored(Agentite_TextRenderer *tr, Agentite_Font *font,
                              const char *text, float x, float y,
                              float r, float g, float b, float a)
{
    agentite_text_draw_ex(tr, font, text, x, y, 1.0f, r, g, b, a,
                        AGENTITE_TEXT_ALIGN_LEFT);
}

void agentite_text_draw_scaled(Agentite_TextRenderer *tr, Agentite_Font *font,
                             const char *text, float x, float y,
                             float scale)
{
    agentite_text_draw_ex(tr, font, text, x, y, scale, 1.0f, 1.0f, 1.0f, 1.0f,
                        AGENTITE_TEXT_ALIGN_LEFT);
}

void agentite_text_draw_ex(Agentite_TextRenderer *tr, Agentite_Font *font,
                         const char *text, float x, float y,
                         float scale,
                         float r, float g, float b, float a,
                         Agentite_TextAlign align)
{
    if (!tr || !font || !text || !tr->batch_started) return;

    /* Auto-batch: if font changes, end current batch and start a new one */
    if (tr->current_font && tr->current_font != font) {
        /* End current batch (queues it) */
        agentite_text_end(tr);
        /* Start new batch */
        agentite_text_begin(tr);
    }
    tr->current_font = font;

    /* Handle alignment */
    float offset_x = 0.0f;
    if (align != AGENTITE_TEXT_ALIGN_LEFT) {
        float text_width = agentite_text_measure(font, text) * scale;
        if (align == AGENTITE_TEXT_ALIGN_CENTER) {
            offset_x = -text_width / 2.0f;
        } else if (align == AGENTITE_TEXT_ALIGN_RIGHT) {
            offset_x = -text_width;
        }
    }

    float cursor_x = x + offset_x;
    float cursor_y = y;

    const char *p = text;
    while (*p) {
        unsigned char c = (unsigned char)*p;

        if (c == '\n') {
            cursor_x = x + offset_x;
            cursor_y += font->line_height * scale;
            p++;
            continue;
        }

        if (c >= FIRST_CHAR && c <= LAST_CHAR) {
            GlyphInfo *glyph = &font->glyphs[c - FIRST_CHAR];

            /* Calculate screen position */
            float gx0 = cursor_x + glyph->x0 * scale;
            float gy0 = cursor_y + glyph->y0 * scale;
            float gx1 = cursor_x + glyph->x1 * scale;
            float gy1 = cursor_y + glyph->y1 * scale;

            /* Add glyph quad */
            text_add_glyph(tr, gx0, gy0, gx1, gy1,
                          glyph->u0, glyph->v0, glyph->u1, glyph->v1,
                          r, g, b, a);

            cursor_x += glyph->advance_x * scale;
        }

        p++;
    }
}

void agentite_text_upload(Agentite_TextRenderer *tr, SDL_GPUCommandBuffer *cmd)
{
    if (!tr || !cmd || tr->queued_batch_count == 0 || tr->vertex_count == 0) return;

    /* Upload vertex and index data */
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = (Uint32)(tr->vertex_count * sizeof(TextVertex) +
            tr->index_count * sizeof(uint16_t));
    transfer_info.props = 0;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(tr->gpu, &transfer_info);
    if (!transfer) return;

    void *mapped = SDL_MapGPUTransferBuffer(tr->gpu, transfer, false);
    if (mapped) {
        memcpy(mapped, tr->vertices,
               tr->vertex_count * sizeof(TextVertex));
        memcpy((uint8_t *)mapped + tr->vertex_count * sizeof(TextVertex),
               tr->indices, tr->index_count * sizeof(uint16_t));
        SDL_UnmapGPUTransferBuffer(tr->gpu, transfer);
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (copy_pass) {
        /* Upload vertices */
        SDL_GPUTransferBufferLocation src_vert = {};
        src_vert.transfer_buffer = transfer;
        src_vert.offset = 0;
        SDL_GPUBufferRegion dst_vert = {};
        dst_vert.buffer = tr->vertex_buffer;
        dst_vert.offset = 0;
        dst_vert.size = (Uint32)(tr->vertex_count * sizeof(TextVertex));
        SDL_UploadToGPUBuffer(copy_pass, &src_vert, &dst_vert, false);

        /* Upload indices */
        SDL_GPUTransferBufferLocation src_idx = {};
        src_idx.transfer_buffer = transfer;
        src_idx.offset = (Uint32)(tr->vertex_count * sizeof(TextVertex));
        SDL_GPUBufferRegion dst_idx = {};
        dst_idx.buffer = tr->index_buffer;
        dst_idx.offset = 0;
        dst_idx.size = (Uint32)(tr->index_count * sizeof(uint16_t));
        SDL_UploadToGPUBuffer(copy_pass, &src_idx, &dst_idx, false);

        SDL_EndGPUCopyPass(copy_pass);
    }

    SDL_ReleaseGPUTransferBuffer(tr->gpu, transfer);
}

void agentite_text_render(Agentite_TextRenderer *tr, SDL_GPUCommandBuffer *cmd,
                        SDL_GPURenderPass *pass)
{
    if (!tr || !cmd || !pass || tr->queued_batch_count == 0) return;

    /* Build vertex uniforms once (shared by all batches) */
    struct {
        float view_projection[16];
        float screen_size[2];
        float padding[2];
    } uniforms;

    mat4 ortho;
    glm_ortho(0.0f, (float)tr->screen_width,
              (float)tr->screen_height, 0.0f,
              -1.0f, 1.0f, ortho);
    memcpy(uniforms.view_projection, ortho, sizeof(float) * 16);

    uniforms.screen_size[0] = (float)tr->screen_width;
    uniforms.screen_size[1] = (float)tr->screen_height;
    uniforms.padding[0] = 0.0f;
    uniforms.padding[1] = 0.0f;

    /* Bind vertex buffer (shared by all batches) */
    SDL_GPUBufferBinding vb_binding = {};
    vb_binding.buffer = tr->vertex_buffer;
    vb_binding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer (shared by all batches) */
    SDL_GPUBufferBinding ib_binding = {};
    ib_binding.buffer = tr->index_buffer;
    ib_binding.offset = 0;
    SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    /* Render each queued batch */
    for (uint32_t i = 0; i < tr->queued_batch_count; i++) {
        QueuedTextBatch *batch = &tr->queued_batches[i];

        /* Select pipeline based on batch type */
        SDL_GPUGraphicsPipeline *pipeline;
        switch (batch->type) {
            case TEXT_BATCH_MSDF:
                pipeline = tr->msdf_pipeline;
                break;
            case TEXT_BATCH_SDF:
                pipeline = tr->sdf_pipeline;
                break;
            case TEXT_BATCH_BITMAP:
            default:
                pipeline = tr->pipeline;
                break;
        }

        if (!pipeline || !batch->atlas_texture) continue;

        /* Bind pipeline */
        SDL_BindGPUGraphicsPipeline(pass, pipeline);

        /* Push vertex uniforms */
        SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

        /* For SDF/MSDF batches, push fragment uniforms */
        if (batch->type == TEXT_BATCH_SDF || batch->type == TEXT_BATCH_MSDF) {
            SDFFragmentUniforms sdf_uniforms = {
                .params = {
                    batch->sdf_font->distance_range,
                    batch->sdf_scale,
                    batch->effects.weight,
                    0.5f  /* edge_threshold */
                },
                .outline_params = {
                    batch->effects.outline_width,
                    0.0f, 0.0f, 0.0f
                },
                .outline_color = {
                    batch->effects.outline_color[0],
                    batch->effects.outline_color[1],
                    batch->effects.outline_color[2],
                    batch->effects.outline_color[3]
                },
                .glow_params = {
                    batch->effects.glow_width,
                    0.0f, 0.0f, 0.0f
                },
                .glow_color = {
                    batch->effects.glow_color[0],
                    batch->effects.glow_color[1],
                    batch->effects.glow_color[2],
                    batch->effects.glow_color[3]
                },
                .shadow_params = {
                    batch->effects.shadow_offset[0],
                    batch->effects.shadow_offset[1],
                    batch->effects.shadow_softness,
                    0.0f
                },
                .shadow_color = {
                    batch->effects.shadow_color[0],
                    batch->effects.shadow_color[1],
                    batch->effects.shadow_color[2],
                    batch->effects.shadow_color[3]
                },
                .flags = 0,
                ._padding = {0, 0, 0}
            };

            if (batch->effects.outline_enabled) sdf_uniforms.flags |= 1;
            if (batch->effects.glow_enabled) sdf_uniforms.flags |= 2;
            if (batch->effects.shadow_enabled) sdf_uniforms.flags |= 4;

            SDL_PushGPUFragmentUniformData(cmd, 0, &sdf_uniforms, sizeof(sdf_uniforms));
        }

        /* Bind atlas texture for this batch */
        SDL_GPUTextureSamplerBinding tex_binding = {};
        tex_binding.texture = batch->atlas_texture;
        tex_binding.sampler = tr->sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

        /* Draw this batch
         * Note: We use vertex_offset to shift index references to the correct vertices.
         * The indices are pre-generated as 0,1,2,0,2,3 for glyph 0, 4,5,6,4,6,7 for glyph 1, etc.
         * So first_index should be batch->index_offset, and vertex_offset should be 0
         * because indices already reference absolute vertex positions.
         */
        SDL_DrawGPUIndexedPrimitives(pass, batch->index_count, 1,
                                      batch->index_offset, 0, 0);
    }

    /* Reset batch queue for next frame */
    tr->queued_batch_count = 0;
}

void agentite_text_end(Agentite_TextRenderer *tr)
{
    if (!tr) return;
    if (!tr->batch_started) return;

    tr->batch_started = false;

    /* Don't queue empty batches */
    uint32_t batch_vertex_count = tr->vertex_count - tr->current_batch_vertex_start;
    uint32_t batch_index_count = tr->index_count - tr->current_batch_index_start;
    if (batch_vertex_count == 0) return;

    /* Check if we have room in the queue */
    if (tr->queued_batch_count >= TEXT_MAX_QUEUED_BATCHES) {
        SDL_Log("Text: Batch queue full, batch dropped");
        return;
    }

    /* Queue this batch */
    QueuedTextBatch *batch = &tr->queued_batches[tr->queued_batch_count];
    batch->vertex_offset = tr->current_batch_vertex_start;
    batch->index_offset = tr->current_batch_index_start;
    batch->vertex_count = batch_vertex_count;
    batch->index_count = batch_index_count;

    if (tr->is_sdf_batch && tr->current_sdf_font) {
        batch->type = (tr->current_sdf_font->type == AGENTITE_SDF_TYPE_MSDF)
                      ? TEXT_BATCH_MSDF : TEXT_BATCH_SDF;
        batch->sdf_font = tr->current_sdf_font;
        batch->sdf_scale = tr->current_sdf_scale;
        batch->effects = tr->current_effects;
        batch->atlas_texture = tr->current_sdf_font->atlas_texture;
    } else if (tr->current_font) {
        batch->type = TEXT_BATCH_BITMAP;
        batch->atlas_texture = tr->current_font->atlas_texture;
        batch->sdf_font = NULL;
    } else {
        /* No font set - shouldn't happen but handle gracefully */
        return;
    }

    tr->queued_batch_count++;
}

/* ============================================================================
 * Formatted Text (printf-style)
 * ============================================================================ */

void agentite_text_printf(Agentite_TextRenderer *tr, Agentite_Font *font,
                        float x, float y,
                        const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    agentite_text_draw(tr, font, buffer, x, y);
}

void agentite_text_printf_colored(Agentite_TextRenderer *tr, Agentite_Font *font,
                                float x, float y,
                                float r, float g, float b, float a,
                                const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    agentite_text_draw_colored(tr, font, buffer, x, y, r, g, b, a);
}
