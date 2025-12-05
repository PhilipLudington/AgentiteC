/*
 * Carbon UI - Core Context and Lifecycle
 */

#include "carbon/ui.h"
#include "carbon/error.h"
#include <stdlib.h>
#include <string.h>

/* Forward declarations from other UI modules */
extern void cui_state_clear(CUI_Context *ctx);
extern void cui_state_gc(CUI_Context *ctx, uint64_t max_age);
extern bool cui_create_pipeline(CUI_Context *ctx);
extern void cui_destroy_pipeline(CUI_Context *ctx);
extern bool cui_load_font(CUI_Context *ctx, const char *path, float size);
extern void cui_free_font(CUI_Context *ctx);

/* ============================================================================
 * Default Theme
 * ============================================================================ */

static void cui_init_theme(CUI_Context *ctx)
{
    ctx->theme.bg_panel          = 0xF21A1A2E;  /* Dark blue, slight transparency */
    ctx->theme.bg_widget         = 0xFF3D3D4A;  /* Gray */
    ctx->theme.bg_widget_hover   = 0xFF4D4D5A;  /* Lighter gray */
    ctx->theme.bg_widget_active  = 0xFF2D2D3A;  /* Darker gray */
    ctx->theme.bg_widget_disabled = 0xFF252530; /* Very dark */
    ctx->theme.border            = 0xFF4A4A5A;  /* Medium gray */
    ctx->theme.text              = 0xFFE0E0E0;  /* Light gray */
    ctx->theme.text_dim          = 0xFF808080;  /* Dim gray */
    ctx->theme.accent            = 0xFFEF9A4D;  /* Blue (ABGR) */
    ctx->theme.checkbox_check    = 0xFFFFFFFF;  /* White */
    ctx->theme.slider_track      = 0xFF2A2A3A;  /* Dark */
    ctx->theme.slider_grab       = 0xFFEF9A4D;  /* Accent blue */
    ctx->theme.scrollbar         = 0x80404050;  /* Semi-transparent */
    ctx->theme.scrollbar_grab    = 0xC0606070;  /* Lighter */
    ctx->theme.corner_radius     = 4.0f;
    ctx->theme.border_width      = 1.0f;
    ctx->theme.widget_height     = 28.0f;
    ctx->theme.spacing           = 4.0f;
    ctx->theme.padding           = 8.0f;
    ctx->theme.scrollbar_width   = 12.0f;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

CUI_Context *cui_init(SDL_GPUDevice *gpu, SDL_Window *window, int width, int height,
                      const char *font_path, float font_size)
{
    CUI_Context *ctx = (CUI_Context *)calloc(1, sizeof(CUI_Context));
    if (!ctx) {
        carbon_set_error("CUI: Failed to allocate context");
        return NULL;
    }

    ctx->gpu = gpu;
    ctx->window = window;
    ctx->width = width;
    ctx->height = height;

    /* Initialize theme */
    cui_init_theme(ctx);

    /* Allocate vertex/index buffers (CPU side) */
    ctx->vertex_capacity = 65536;
    ctx->index_capacity = 98304;  /* 1.5x vertices for quads */
    ctx->vertices = (CUI_Vertex *)malloc(ctx->vertex_capacity * sizeof(CUI_Vertex));
    ctx->indices = (uint16_t *)malloc(ctx->index_capacity * sizeof(uint16_t));

    if (!ctx->vertices || !ctx->indices) {
        carbon_set_error("CUI: Failed to allocate vertex/index arrays");
        cui_shutdown(ctx);
        return NULL;
    }

    /* Create GPU pipeline and resources */
    if (!cui_create_pipeline(ctx)) {
        carbon_set_error("CUI: Failed to create GPU pipeline");
        cui_shutdown(ctx);
        return NULL;
    }

    /* Load font */
    if (font_path && !cui_load_font(ctx, font_path, font_size)) {
        carbon_set_error("CUI: Failed to load font '%s'", font_path);
        cui_shutdown(ctx);
        return NULL;
    }

    /* Initialize layout with full screen */
    ctx->layout_stack[0].bounds = (CUI_Rect){0, 0, (float)width, (float)height};
    ctx->layout_stack[0].cursor_x = 0;
    ctx->layout_stack[0].cursor_y = 0;
    ctx->layout_stack[0].spacing = ctx->theme.spacing;
    ctx->layout_stack[0].padding = ctx->theme.padding;
    ctx->layout_stack[0].horizontal = false;
    ctx->layout_depth = 1;

    SDL_Log("CUI: Initialized (%dx%d)", width, height);
    return ctx;
}

void cui_shutdown(CUI_Context *ctx)
{
    if (!ctx) return;

    cui_destroy_pipeline(ctx);
    cui_free_font(ctx);
    cui_state_clear(ctx);

    free(ctx->vertices);
    free(ctx->indices);
    free(ctx);

    SDL_Log("CUI: Shutdown complete");
}

void cui_begin_frame(CUI_Context *ctx, float delta_time)
{
    if (!ctx) return;

    ctx->delta_time = delta_time;
    ctx->frame_count++;

    /* Reset draw buffers */
    ctx->vertex_count = 0;
    ctx->index_count = 0;

    /* Reset layout to root */
    ctx->layout_depth = 1;
    ctx->layout_stack[0].cursor_x = ctx->layout_stack[0].padding;
    ctx->layout_stack[0].cursor_y = ctx->layout_stack[0].padding;

    /* Reset scissor stack */
    ctx->scissor_depth = 0;

    /* Clear hot widget (will be set during widget processing) */
    ctx->hot = CUI_ID_NONE;

    /* Garbage collect old state entries every 60 frames */
    if (ctx->frame_count % 60 == 0) {
        cui_state_gc(ctx, 300);  /* Remove entries not used for 5 seconds */
    }
}

/* Forward declaration for deferred popup rendering */
extern void cui_draw_rect(CUI_Context *ctx, float x, float y, float w, float h, uint32_t color);
extern void cui_draw_rect_outline(CUI_Context *ctx, float x, float y, float w, float h, uint32_t color, float thickness);
extern float cui_draw_text(CUI_Context *ctx, const char *text, float x, float y, uint32_t color);
extern float cui_text_height(CUI_Context *ctx);

void cui_end_frame(CUI_Context *ctx)
{
    if (!ctx) return;

    /* Store previous mouse position */
    ctx->input.mouse_prev_x = ctx->input.mouse_x;
    ctx->input.mouse_prev_y = ctx->input.mouse_y;

    /* Handle text input start/stop based on focus changes */
    if (ctx->focused != ctx->prev_focused) {
        if (ctx->focused != CUI_ID_NONE && ctx->window) {
            /* A widget gained focus - start text input */
            SDL_StartTextInput(ctx->window);
        } else if (ctx->prev_focused != CUI_ID_NONE && ctx->window) {
            /* Focus was lost - stop text input */
            SDL_StopTextInput(ctx->window);
        }
        ctx->prev_focused = ctx->focused;
    }

    /* Draw deferred popup (renders on top of everything) */
    if (ctx->open_popup != CUI_ID_NONE && ctx->popup_items && ctx->popup_selected) {
        /* Draw popup background */
        cui_draw_rect(ctx, ctx->popup_rect.x, ctx->popup_rect.y,
                      ctx->popup_rect.w, ctx->popup_rect.h,
                      ctx->theme.bg_panel);
        cui_draw_rect_outline(ctx, ctx->popup_rect.x, ctx->popup_rect.y,
                              ctx->popup_rect.w, ctx->popup_rect.h,
                              ctx->theme.border, 1.0f);

        /* Draw popup items */
        for (int i = 0; i < ctx->popup_count; i++) {
            CUI_Rect item_rect = {
                ctx->popup_rect.x,
                ctx->popup_rect.y + i * ctx->theme.widget_height,
                ctx->popup_rect.w,
                ctx->theme.widget_height
            };

            bool item_hovered = cui_rect_contains(item_rect,
                                                   ctx->input.mouse_x,
                                                   ctx->input.mouse_y);

            if (item_hovered) {
                cui_draw_rect(ctx, item_rect.x, item_rect.y, item_rect.w, item_rect.h,
                              ctx->theme.bg_widget_hover);

                if (ctx->input.mouse_pressed[0]) {
                    *ctx->popup_selected = i;
                    ctx->open_popup = CUI_ID_NONE;
                    ctx->popup_changed = true;
                }
            }

            float item_text_y = item_rect.y + (item_rect.h - cui_text_height(ctx)) * 0.5f;
            cui_draw_text(ctx, ctx->popup_items[i], item_rect.x + ctx->theme.padding,
                          item_text_y, ctx->theme.text);
        }
    }

    /* Close popup if clicked outside */
    if (ctx->open_popup != CUI_ID_NONE && ctx->input.mouse_pressed[0]) {
        if (!cui_rect_contains(ctx->popup_rect,
                               ctx->input.mouse_x, ctx->input.mouse_y)) {
            ctx->open_popup = CUI_ID_NONE;
        }
    }

    /* Clear per-frame input state (pressed/released are one-shot) */
    for (int i = 0; i < 3; i++) {
        ctx->input.mouse_pressed[i] = false;
        ctx->input.mouse_released[i] = false;
    }
    memset(ctx->input.keys_pressed, 0, sizeof(ctx->input.keys_pressed));

    /* Clear text input */
    ctx->input.text_input[0] = '\0';
    ctx->input.text_input_len = 0;

    /* Reset scroll (consumed this frame) */
    ctx->input.scroll_x = 0;
    ctx->input.scroll_y = 0;
}

void cui_set_screen_size(CUI_Context *ctx, int width, int height)
{
    if (!ctx) return;
    ctx->width = width;
    ctx->height = height;
    ctx->layout_stack[0].bounds.w = (float)width;
    ctx->layout_stack[0].bounds.h = (float)height;
}

/* ============================================================================
 * Event Processing
 * ============================================================================ */

bool cui_process_event(CUI_Context *ctx, const SDL_Event *event)
{
    if (!ctx || !event) return false;

    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
        ctx->input.mouse_x = event->motion.x;
        ctx->input.mouse_y = event->motion.y;
        return false;  /* Don't consume motion events */

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button <= 3) {
            int btn = event->button.button - 1;
            ctx->input.mouse_down[btn] = true;
            ctx->input.mouse_pressed[btn] = true;
        }
        return ctx->hot != CUI_ID_NONE;  /* Consume if over UI */

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button <= 3) {
            int btn = event->button.button - 1;
            ctx->input.mouse_down[btn] = false;
            ctx->input.mouse_released[btn] = true;
        }
        return ctx->active != CUI_ID_NONE;

    case SDL_EVENT_MOUSE_WHEEL:
        ctx->input.scroll_x = event->wheel.x;
        ctx->input.scroll_y = event->wheel.y;
        return ctx->hot != CUI_ID_NONE;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.scancode < 512) {
            ctx->input.keys_down[event->key.scancode] = true;
            ctx->input.keys_pressed[event->key.scancode] = true;
        }
        ctx->input.shift = (event->key.mod & SDL_KMOD_SHIFT) != 0;
        ctx->input.ctrl = (event->key.mod & SDL_KMOD_CTRL) != 0;
        ctx->input.alt = (event->key.mod & SDL_KMOD_ALT) != 0;
        return ctx->focused != CUI_ID_NONE;

    case SDL_EVENT_KEY_UP:
        if (event->key.scancode < 512) {
            ctx->input.keys_down[event->key.scancode] = false;
        }
        ctx->input.shift = (event->key.mod & SDL_KMOD_SHIFT) != 0;
        ctx->input.ctrl = (event->key.mod & SDL_KMOD_CTRL) != 0;
        ctx->input.alt = (event->key.mod & SDL_KMOD_ALT) != 0;
        return false;

    case SDL_EVENT_TEXT_INPUT:
        if (ctx->focused != CUI_ID_NONE) {
            size_t len = strlen(event->text.text);
            if (ctx->input.text_input_len + len < sizeof(ctx->input.text_input) - 1) {
                memcpy(ctx->input.text_input + ctx->input.text_input_len,
                       event->text.text, len);
                ctx->input.text_input_len += (int)len;
                ctx->input.text_input[ctx->input.text_input_len] = '\0';
            }
            return true;
        }
        return false;

    default:
        return false;
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

uint32_t cui_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) |
           ((uint32_t)g << 8) | (uint32_t)r;
}

uint32_t cui_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return cui_rgba(r, g, b, 255);
}

uint32_t cui_color_lerp(uint32_t a, uint32_t b, float t)
{
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;

    uint8_t ar = (a >> 0) & 0xFF;
    uint8_t ag = (a >> 8) & 0xFF;
    uint8_t ab = (a >> 16) & 0xFF;
    uint8_t aa = (a >> 24) & 0xFF;

    uint8_t br = (b >> 0) & 0xFF;
    uint8_t bg = (b >> 8) & 0xFF;
    uint8_t bb = (b >> 16) & 0xFF;
    uint8_t ba = (b >> 24) & 0xFF;

    uint8_t rr = (uint8_t)(ar + (br - ar) * t);
    uint8_t rg = (uint8_t)(ag + (bg - ag) * t);
    uint8_t rb = (uint8_t)(ab + (bb - ab) * t);
    uint8_t ra = (uint8_t)(aa + (ba - aa) * t);

    return cui_rgba(rr, rg, rb, ra);
}

uint32_t cui_color_alpha(uint32_t color, float alpha)
{
    uint8_t a = (uint8_t)(((color >> 24) & 0xFF) * alpha);
    return (color & 0x00FFFFFF) | ((uint32_t)a << 24);
}

bool cui_rect_contains(CUI_Rect rect, float x, float y)
{
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

CUI_Rect cui_rect_intersect(CUI_Rect a, CUI_Rect b)
{
    float x1 = (a.x > b.x) ? a.x : b.x;
    float y1 = (a.y > b.y) ? a.y : b.y;
    float x2 = (a.x + a.w < b.x + b.w) ? a.x + a.w : b.x + b.w;
    float y2 = (a.y + a.h < b.y + b.h) ? a.y + a.h : b.y + b.h;

    CUI_Rect result;
    result.x = x1;
    result.y = y1;
    result.w = (x2 > x1) ? x2 - x1 : 0;
    result.h = (y2 > y1) ? y2 - y1 : 0;
    return result;
}
