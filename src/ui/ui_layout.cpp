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

    /* Store scroll state for end_scroll */
    ctx->scroll.id = scroll_id;
    ctx->scroll.outer_rect = outer;
    ctx->scroll.content_start_y = outer.y + ctx->theme.padding;

    /* Draw background */
    aui_draw_rect(ctx, outer.x, outer.y, outer.w, outer.h, ctx->theme.bg_panel);

    /* Push clipping (minus scrollbar width) */
    aui_push_scissor(ctx, outer.x, outer.y, outer.w - ctx->theme.scrollbar_width, outer.h);

    /* Push layout for content */
    AUI_LayoutFrame *frame = aui_push_layout(ctx);
    if (!frame) return;

    float scroll_y = state ? state->scroll_y : 0;

    frame->bounds = outer;
    frame->bounds.w -= ctx->theme.scrollbar_width;  /* Reserve space for scrollbar */
    frame->cursor_x = outer.x + ctx->theme.padding;
    frame->cursor_y = outer.y + ctx->theme.padding - scroll_y;
    frame->spacing = ctx->theme.spacing;
    frame->padding = ctx->theme.padding;
    frame->horizontal = false;
    frame->clip = outer;
    frame->has_clip = true;

    aui_push_id(ctx, id);
}

void aui_end_scroll(AUI_Context *ctx)
{
    if (!ctx || ctx->layout_depth <= 1) return;
    if (ctx->scroll.id == AUI_ID_NONE) return;

    AUI_LayoutFrame *frame = aui_current_layout(ctx);
    aui_pop_scissor(ctx);

    AUI_Id scroll_id = ctx->scroll.id;
    AUI_WidgetState *state = aui_get_state(ctx, scroll_id);
    AUI_Id scrollbar_id = scroll_id + 1;  /* Unique ID for scrollbar */

    /* Calculate content height from how far cursor advanced */
    float scroll_y = state ? state->scroll_y : 0;
    float content_height = (frame->cursor_y + scroll_y) - ctx->scroll.content_start_y;
    float visible_height = ctx->scroll.outer_rect.h - ctx->theme.padding * 2;
    float max_scroll = content_height - visible_height;
    if (max_scroll < 0) max_scroll = 0;

    /* Clamp scroll */
    if (state) {
        if (state->scroll_y < 0) state->scroll_y = 0;
        if (state->scroll_y > max_scroll) state->scroll_y = max_scroll;
        scroll_y = state->scroll_y;
    }

    /* Handle mouse wheel */
    AUI_Rect outer = ctx->scroll.outer_rect;
    bool hovered = aui_rect_contains(outer, ctx->input.mouse_x, ctx->input.mouse_y);
    if (hovered) {
        ctx->hot = scroll_id;
        if (state && ctx->input.scroll_y != 0.0f && max_scroll > 0) {
            float scroll_speed = ctx->theme.widget_height * 2.0f;
            state->scroll_y -= ctx->input.scroll_y * scroll_speed;
            if (state->scroll_y < 0) state->scroll_y = 0;
            if (state->scroll_y > max_scroll) state->scroll_y = max_scroll;
            scroll_y = state->scroll_y;
        }
    }

    /* Draw scrollbar if content exceeds visible area */
    bool needs_scrollbar = content_height > visible_height;
    if (needs_scrollbar && state) {
        float scrollbar_w = ctx->theme.scrollbar_width;
        AUI_Rect scrollbar_rect = {
            outer.x + outer.w - scrollbar_w,
            outer.y,
            scrollbar_w,
            outer.h
        };

        /* Draw scrollbar track */
        aui_draw_rect(ctx, scrollbar_rect.x, scrollbar_rect.y,
                      scrollbar_rect.w, scrollbar_rect.h,
                      ctx->theme.scrollbar);

        /* Calculate thumb size and position */
        float visible_ratio = visible_height / content_height;
        float thumb_h = scrollbar_rect.h * visible_ratio;
        if (thumb_h < 20.0f) thumb_h = 20.0f;  /* Minimum thumb size */

        float thumb_travel = scrollbar_rect.h - thumb_h;
        float scroll_ratio = (max_scroll > 0) ? (scroll_y / max_scroll) : 0;
        float thumb_y = scrollbar_rect.y + thumb_travel * scroll_ratio;

        AUI_Rect thumb_rect = {
            scrollbar_rect.x + 2,
            thumb_y,
            scrollbar_rect.w - 4,
            thumb_h
        };

        /* Handle scrollbar interaction */
        bool thumb_hovered = aui_rect_contains(thumb_rect, ctx->input.mouse_x, ctx->input.mouse_y);
        bool track_hovered = aui_rect_contains(scrollbar_rect, ctx->input.mouse_x, ctx->input.mouse_y);

        if (thumb_hovered || track_hovered) {
            ctx->hot = scrollbar_id;
        }

        /* Start dragging or clicking on scrollbar */
        if (track_hovered && ctx->input.mouse_pressed[0]) {
            ctx->active = scrollbar_id;
            /* Store the offset from thumb center for smooth dragging */
            if (thumb_hovered) {
                /* Clicked on thumb - store offset from thumb top */
                state->cursor_pos = (int)(ctx->input.mouse_y - thumb_y);
            } else {
                /* Clicked on track - center thumb on click position */
                state->cursor_pos = (int)(thumb_h * 0.5f);
            }
        }

        /* Handle active scrollbar (dragging) */
        if (ctx->active == scrollbar_id) {
            if (ctx->input.mouse_down[0]) {
                /* Calculate scroll based on absolute mouse position */
                float target_thumb_y = ctx->input.mouse_y - (float)state->cursor_pos;
                float new_ratio = (target_thumb_y - scrollbar_rect.y) / thumb_travel;
                if (new_ratio < 0) new_ratio = 0;
                if (new_ratio > 1) new_ratio = 1;
                state->scroll_y = new_ratio * max_scroll;
                scroll_y = state->scroll_y;
            } else {
                ctx->active = AUI_ID_NONE;
            }
        }

        /* Recalculate thumb position after potential scroll change */
        scroll_ratio = (max_scroll > 0) ? (scroll_y / max_scroll) : 0;
        thumb_y = scrollbar_rect.y + thumb_travel * scroll_ratio;
        thumb_rect.y = thumb_y;

        /* Draw thumb */
        bool thumb_active = (ctx->active == scrollbar_id);
        uint32_t thumb_color = thumb_active ? ctx->theme.accent :
                               (thumb_hovered ? ctx->theme.bg_widget_hover : ctx->theme.scrollbar_grab);
        aui_draw_rect_rounded(ctx, thumb_rect.x, thumb_rect.y, thumb_rect.w, thumb_rect.h,
                              thumb_color, ctx->theme.corner_radius);
    }

    aui_pop_id(ctx);
    aui_pop_layout(ctx);

    /* Clear scroll state */
    ctx->scroll.id = AUI_ID_NONE;
}
