/*
 * Agentite UI - Layout System
 */

#include "agentite/ui.h"
#include <string.h>

/* Forward declarations */
extern AUI_Id aui_make_id(AUI_Context *ctx, const char *str);

/* ============================================================================
 * Layout Stack Management
 * ============================================================================ */

static AUI_LayoutFrame *aui_current_layout(AUI_Context *ctx)
{
    if (ctx->layout_depth <= 0) return NULL;
    return &ctx->layout_stack[ctx->layout_depth - 1];
}

static AUI_LayoutFrame *aui_push_layout(AUI_Context *ctx)
{
    if (ctx->layout_depth >= 32) {
        SDL_Log("CUI: Layout stack overflow");
        return NULL;
    }
    return &ctx->layout_stack[ctx->layout_depth++];
}

static void aui_pop_layout(AUI_Context *ctx)
{
    if (ctx->layout_depth > 1) {
        ctx->layout_depth--;
    }
}

/* ============================================================================
 * Row/Column Layout
 * ============================================================================ */

void aui_begin_row(AUI_Context *ctx)
{
    aui_begin_row_ex(ctx, ctx->theme.widget_height, ctx->theme.spacing);
}

void aui_begin_row_ex(AUI_Context *ctx, float height, float spacing)
{
    if (!ctx) return;

    AUI_LayoutFrame *parent = aui_current_layout(ctx);
    if (!parent) return;

    AUI_LayoutFrame *frame = aui_push_layout(ctx);
    if (!frame) return;

    frame->bounds.x = parent->cursor_x;
    frame->bounds.y = parent->cursor_y;
    frame->bounds.w = parent->bounds.x + parent->bounds.w - parent->cursor_x - parent->padding;
    frame->bounds.h = height;
    frame->cursor_x = frame->bounds.x;
    frame->cursor_y = frame->bounds.y;
    frame->row_height = height;
    frame->spacing = spacing;
    frame->padding = 0;
    frame->horizontal = true;
    frame->has_clip = false;
}

void aui_end_row(AUI_Context *ctx)
{
    if (!ctx || ctx->layout_depth <= 1) return;

    AUI_LayoutFrame *frame = aui_current_layout(ctx);
    aui_pop_layout(ctx);

    AUI_LayoutFrame *parent = aui_current_layout(ctx);
    if (parent && !parent->horizontal) {
        parent->cursor_y += frame->row_height + parent->spacing;
    }
}

void aui_begin_column(AUI_Context *ctx)
{
    aui_begin_column_ex(ctx, 0, ctx->theme.spacing);
}

void aui_begin_column_ex(AUI_Context *ctx, float width, float spacing)
{
    if (!ctx) return;

    AUI_LayoutFrame *parent = aui_current_layout(ctx);
    if (!parent) return;

    AUI_LayoutFrame *frame = aui_push_layout(ctx);
    if (!frame) return;

    float actual_width = width > 0 ? width :
        (parent->bounds.x + parent->bounds.w - parent->cursor_x - parent->padding);

    frame->bounds.x = parent->cursor_x;
    frame->bounds.y = parent->cursor_y;
    frame->bounds.w = actual_width;
    frame->bounds.h = parent->bounds.y + parent->bounds.h - parent->cursor_y - parent->padding;
    frame->cursor_x = frame->bounds.x;
    frame->cursor_y = frame->bounds.y;
    frame->row_height = 0;
    frame->spacing = spacing;
    frame->padding = 0;
    frame->horizontal = false;
    frame->has_clip = false;
}

void aui_end_column(AUI_Context *ctx)
{
    if (!ctx || ctx->layout_depth <= 1) return;

    AUI_LayoutFrame *frame = aui_current_layout(ctx);
    float used_width = frame->bounds.w;
    aui_pop_layout(ctx);

    AUI_LayoutFrame *parent = aui_current_layout(ctx);
    if (parent && parent->horizontal) {
        parent->cursor_x += used_width + parent->spacing;
    }
}

/* ============================================================================
 * Spacing
 * ============================================================================ */

void aui_spacing(AUI_Context *ctx, float amount)
{
    if (!ctx) return;

    AUI_LayoutFrame *frame = aui_current_layout(ctx);
    if (!frame) return;

    if (frame->horizontal) {
        frame->cursor_x += amount;
    } else {
        frame->cursor_y += amount;
    }
}

void aui_separator(AUI_Context *ctx)
{
    if (!ctx) return;

    AUI_LayoutFrame *frame = aui_current_layout(ctx);
    if (!frame) return;

    float y = frame->cursor_y + ctx->theme.spacing;
    float x1 = frame->bounds.x;
    float x2 = frame->bounds.x + frame->bounds.w;

    aui_draw_line(ctx, x1, y, x2, y, ctx->theme.border, 1.0f);

    frame->cursor_y = y + ctx->theme.spacing;
}

void aui_same_line(AUI_Context *ctx)
{
    if (!ctx) return;

    AUI_LayoutFrame *frame = aui_current_layout(ctx);
    if (!frame) return;

    /* Mark that next widget should stay on same line */
    /* This is a simplified version - proper implementation would
       track the previous widget's position */
    frame->horizontal = true;
}

/* ============================================================================
 * Rect Allocation
 * ============================================================================ */

AUI_Rect aui_get_available_rect(AUI_Context *ctx)
{
    AUI_Rect rect = {0, 0, 0, 0};
    if (!ctx) return rect;

    AUI_LayoutFrame *frame = aui_current_layout(ctx);
    if (!frame) return rect;

    rect.x = frame->cursor_x;
    rect.y = frame->cursor_y;
    rect.w = frame->bounds.x + frame->bounds.w - frame->cursor_x - frame->padding;
    rect.h = frame->bounds.y + frame->bounds.h - frame->cursor_y - frame->padding;

    return rect;
}

/* Allocate a rect for a widget */
AUI_Rect aui_allocate_rect(AUI_Context *ctx, float width, float height)
{
    AUI_Rect rect = {0, 0, 0, 0};
    if (!ctx) return rect;

    AUI_LayoutFrame *frame = aui_current_layout(ctx);
    if (!frame) return rect;

    /* Calculate actual dimensions */
    float actual_w = width;
    float actual_h = height;

    if (actual_w <= 0) {
        /* Fill available width */
        actual_w = frame->bounds.x + frame->bounds.w - frame->cursor_x - frame->padding;
    }
    if (actual_h <= 0) {
        actual_h = ctx->theme.widget_height;
    }

    rect.x = frame->cursor_x;
    rect.y = frame->cursor_y;
    rect.w = actual_w;
    rect.h = actual_h;

    /* Advance cursor */
    if (frame->horizontal) {
        frame->cursor_x += actual_w + frame->spacing;
        if (actual_h > frame->row_height) {
            frame->row_height = actual_h;
        }
    } else {
        frame->cursor_y += actual_h + frame->spacing;
    }

    return rect;
}

/* ============================================================================
 * Scrollable Regions
 * ============================================================================ */

void aui_begin_scroll(AUI_Context *ctx, const char *id, float width, float height)
{
    if (!ctx) return;

    AUI_Id scroll_id = aui_make_id(ctx, id);
    AUI_WidgetState *state = aui_get_state(ctx, scroll_id);

    AUI_LayoutFrame *parent = aui_current_layout(ctx);
    if (!parent) return;

    /* Allocate the scroll region rect */
    AUI_Rect outer = aui_allocate_rect(ctx, width, height);

    /* Draw background */
    aui_draw_rect(ctx, outer.x, outer.y, outer.w, outer.h, ctx->theme.bg_panel);

    /* Push clipping */
    aui_push_scissor(ctx, outer.x, outer.y, outer.w - ctx->theme.scrollbar_width, outer.h);

    /* Push layout for content */
    AUI_LayoutFrame *frame = aui_push_layout(ctx);
    if (!frame) return;

    frame->bounds = outer;
    frame->bounds.w -= ctx->theme.scrollbar_width;  /* Reserve space for scrollbar */
    frame->cursor_x = outer.x + ctx->theme.padding;
    frame->cursor_y = outer.y + ctx->theme.padding - (state ? state->scroll_y : 0);
    frame->spacing = ctx->theme.spacing;
    frame->padding = ctx->theme.padding;
    frame->horizontal = false;
    frame->clip = outer;
    frame->has_clip = true;

    /* Store scroll info for end_scroll */
    aui_push_id(ctx, id);
}

void aui_end_scroll(AUI_Context *ctx)
{
    if (!ctx || ctx->layout_depth <= 1) return;

    AUI_LayoutFrame *frame = aui_current_layout(ctx);
    aui_pop_scissor(ctx);

    /* Calculate content height */
    float content_start = frame->bounds.y + frame->padding;
    float content_height = frame->cursor_y - content_start +
                           (frame->has_clip ? aui_get_state(ctx, 0)->scroll_y : 0);
    float visible_height = frame->bounds.h - frame->padding * 2;

    /* Get scroll state - we need to recover the ID */
    aui_pop_id(ctx);

    /* Draw scrollbar if needed */
    if (content_height > visible_height) {
        float scrollbar_x = frame->bounds.x + frame->bounds.w;
        float scrollbar_h = frame->bounds.h;
        float thumb_h = (visible_height / content_height) * scrollbar_h;
        if (thumb_h < 20) thumb_h = 20;

        /* Get scroll state for this region */
        /* Note: Proper implementation would store the scroll ID */
        float scroll_ratio = 0;  /* state->scroll_y / (content_height - visible_height) */
        float thumb_y = frame->bounds.y + scroll_ratio * (scrollbar_h - thumb_h);

        /* Draw track */
        aui_draw_rect(ctx, scrollbar_x, frame->bounds.y,
                      ctx->theme.scrollbar_width, scrollbar_h,
                      ctx->theme.scrollbar);

        /* Draw thumb */
        aui_draw_rect(ctx, scrollbar_x + 2, thumb_y,
                      ctx->theme.scrollbar_width - 4, thumb_h,
                      ctx->theme.scrollbar_grab);
    }

    aui_pop_layout(ctx);
}
