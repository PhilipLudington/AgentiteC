/**
 * Agentite Engine - ECS Entity Inspector Implementation
 */

#include "agentite/ecs_inspector.h"
#include "agentite/ecs.h"
#include "agentite/ecs_reflect.h"
#include "agentite/ui.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define INSPECTOR_MAX_ENTITIES 4096
#define INSPECTOR_REFRESH_INTERVAL 0.5f  /* Seconds between cache refresh */
#define INSPECTOR_NAME_FILTER_MAX 128
#define INSPECTOR_FIELD_BUFFER_SIZE 256

/* ============================================================================
 * Inspector Structure
 * ============================================================================ */

struct Agentite_Inspector {
    /* References (borrowed) */
    Agentite_World *world;
    Agentite_ReflectRegistry *registry;

    /* Configuration */
    Agentite_InspectorConfig config;

    /* Entity cache */
    ecs_entity_t *entities;
    int entity_count;
    int entity_capacity;
    float time_since_refresh;

    /* Selection */
    ecs_entity_t selected;

    /* Filters */
    char name_filter[INSPECTOR_NAME_FILTER_MAX];
    ecs_entity_t required_component;

    /* UI state */
    float entity_list_scroll;
    float inspector_scroll;

    /* Scrollbar interaction state */
    bool list_scrollbar_dragging;
    float list_scrollbar_drag_offset;
    bool panel_scrollbar_dragging;
    float panel_scrollbar_drag_offset;
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/* Case-insensitive substring search */
static bool str_contains_ci(const char *haystack, const char *needle)
{
    if (!haystack || !needle || !*needle) return true;
    if (!*haystack) return false;

    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);

    if (needle_len > haystack_len) return false;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower((unsigned char)haystack[i + j]) !=
                tolower((unsigned char)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

/* Get display name for an entity */
static const char *get_entity_display_name(ecs_world_t *w, ecs_entity_t e,
                                            char *buffer, int size)
{
    const char *name = ecs_get_name(w, e);
    if (name && *name) {
        return name;
    }
    snprintf(buffer, size, "Entity %llu", (unsigned long long)e);
    return buffer;
}

/* Check if entity passes current filters */
static bool entity_passes_filters(Agentite_Inspector *inspector,
                                   ecs_world_t *w, ecs_entity_t e)
{
    /* Check component filter */
    if (inspector->required_component != 0) {
        if (!ecs_has_id(w, e, inspector->required_component)) {
            return false;
        }
    }

    /* Check name filter */
    if (inspector->name_filter[0] != '\0') {
        char name_buf[64];
        const char *name = get_entity_display_name(w, e, name_buf, sizeof(name_buf));
        if (!str_contains_ci(name, inspector->name_filter)) {
            return false;
        }
    }

    return true;
}

/* Check if entity has any components we have reflection data for */
static bool has_reflected_components(Agentite_Inspector *inspector,
                                      ecs_world_t *w, ecs_entity_t e)
{
    const ecs_type_t *type = ecs_get_type(w, e);
    if (!type || type->count == 0) return false;

    for (int i = 0; i < (int)type->count; i++) {
        ecs_entity_t comp_id = type->array[i];

        /* Skip relationship pairs */
        if (ECS_IS_PAIR(comp_id)) continue;

        /* Check if we have reflection for this component */
        const Agentite_ComponentMeta *meta = agentite_reflect_get(
            inspector->registry, comp_id);
        if (meta) return true;
    }

    return false;
}

/* Refresh the entity cache */
static void refresh_entity_cache(Agentite_Inspector *inspector)
{
    if (!inspector || !inspector->world) return;

    ecs_world_t *w = agentite_ecs_get_world(inspector->world);
    if (!w) return;

    inspector->entity_count = 0;

    /* Use a query with EcsAny to get all entities with any component */
    ecs_query_desc_t query_desc = {0};
    query_desc.terms[0].id = EcsAny;

    ecs_query_t *query = ecs_query_init(w, &query_desc);
    if (!query) return;

    ecs_iter_t it = ecs_query_iter(w, query);
    while (ecs_query_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            ecs_entity_t e = it.entities[i];

            /* Skip if at capacity */
            if (inspector->entity_count >= inspector->entity_capacity) break;
            if (inspector->entity_count >= inspector->config.max_entities) break;

            /* Skip internal flecs entities (modules, builtins) */
            if (e < 256) continue;

            /* Skip module entities */
            if (ecs_has_id(w, e, EcsModule)) continue;

            /* Only show entities with at least one reflected component */
            if (!has_reflected_components(inspector, w, e)) continue;

            /* Apply user filters */
            if (!entity_passes_filters(inspector, w, e)) continue;

            inspector->entities[inspector->entity_count++] = e;
        }
    }

    ecs_query_fini(query);
    inspector->time_since_refresh = 0.0f;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

Agentite_Inspector *agentite_inspector_create(
    Agentite_World *world,
    Agentite_ReflectRegistry *registry,
    const Agentite_InspectorConfig *config)
{
    if (!world || !registry) return NULL;

    Agentite_Inspector *inspector = (Agentite_Inspector *)
        calloc(1, sizeof(Agentite_Inspector));
    if (!inspector) return NULL;

    inspector->world = world;
    inspector->registry = registry;

    /* Apply config */
    if (config) {
        inspector->config = *config;
    } else {
        Agentite_InspectorConfig defaults = AGENTITE_INSPECTOR_CONFIG_DEFAULT;
        inspector->config = defaults;
    }

    /* Allocate entity cache */
    inspector->entity_capacity = INSPECTOR_MAX_ENTITIES;
    inspector->entities = (ecs_entity_t *)
        calloc(inspector->entity_capacity, sizeof(ecs_entity_t));
    if (!inspector->entities) {
        free(inspector);
        return NULL;
    }

    /* Initial refresh */
    refresh_entity_cache(inspector);

    return inspector;
}

void agentite_inspector_destroy(Agentite_Inspector *inspector)
{
    if (!inspector) return;
    free(inspector->entities);
    free(inspector);
}

/* ============================================================================
 * Entity Selection
 * ============================================================================ */

void agentite_inspector_select(Agentite_Inspector *inspector, ecs_entity_t entity)
{
    if (!inspector) return;
    inspector->selected = entity;
    inspector->inspector_scroll = 0.0f;  /* Reset scroll on new selection */
}

ecs_entity_t agentite_inspector_get_selected(const Agentite_Inspector *inspector)
{
    if (!inspector) return 0;
    return inspector->selected;
}

void agentite_inspector_clear_selection(Agentite_Inspector *inspector)
{
    if (!inspector) return;
    inspector->selected = 0;
}

/* ============================================================================
 * Entity Filtering
 * ============================================================================ */

void agentite_inspector_set_name_filter(Agentite_Inspector *inspector,
                                         const char *filter)
{
    if (!inspector) return;

    if (filter && *filter) {
        strncpy(inspector->name_filter, filter, INSPECTOR_NAME_FILTER_MAX - 1);
        inspector->name_filter[INSPECTOR_NAME_FILTER_MAX - 1] = '\0';
    } else {
        inspector->name_filter[0] = '\0';
    }

    /* Force cache refresh */
    refresh_entity_cache(inspector);
}

void agentite_inspector_require_component(Agentite_Inspector *inspector,
                                           ecs_entity_t component_id)
{
    if (!inspector) return;
    inspector->required_component = component_id;

    /* Force cache refresh */
    refresh_entity_cache(inspector);
}

void agentite_inspector_clear_filters(Agentite_Inspector *inspector)
{
    if (!inspector) return;
    inspector->name_filter[0] = '\0';
    inspector->required_component = 0;

    /* Force cache refresh */
    refresh_entity_cache(inspector);
}

/* ============================================================================
 * Cache Control
 * ============================================================================ */

void agentite_inspector_refresh(Agentite_Inspector *inspector)
{
    refresh_entity_cache(inspector);
}

/* ============================================================================
 * UI Drawing - Entity List
 * ============================================================================ */

bool agentite_inspector_entity_list(Agentite_Inspector *inspector,
                                     AUI_Context *ui,
                                     float x, float y, float w, float h)
{
    if (!inspector || !ui) return false;

    bool selection_changed = false;
    ecs_world_t *world = agentite_ecs_get_world(inspector->world);
    if (!world) return false;

    /* Auto-refresh cache periodically */
    inspector->time_since_refresh += ui->delta_time;
    if (inspector->time_since_refresh >= INSPECTOR_REFRESH_INTERVAL) {
        refresh_entity_cache(inspector);
    }

    /* Draw panel background */
    aui_draw_rect(ui, x, y, w, h, ui->theme.bg_panel);

    /* Filter textbox */
    float filter_y = y + ui->theme.padding;
    float filter_w = w - ui->theme.padding * 2;
    float filter_h = ui->theme.widget_height;

    aui_push_id(ui, "inspector_filter");

    /* Draw filter input background */
    float filter_x = x + ui->theme.padding;
    aui_draw_rect_rounded(ui, filter_x, filter_y, filter_w, filter_h,
                          ui->theme.bg_widget, ui->theme.corner_radius);

    /* Simple filter label (full textbox would need more state management) */
    float label_x = filter_x + ui->theme.padding;
    float label_y = filter_y + (filter_h - aui_text_height(ui)) * 0.5f;

    if (inspector->name_filter[0] != '\0') {
        aui_draw_text(ui, inspector->name_filter, label_x, label_y, ui->theme.text);
    } else {
        aui_draw_text(ui, "Filter...", label_x, label_y, ui->theme.text_dim);
    }

    aui_pop_id(ui);

    /* Entity count label */
    float count_y = filter_y + filter_h + ui->theme.spacing;
    char count_buf[64];
    snprintf(count_buf, sizeof(count_buf), "%d entities", inspector->entity_count);
    aui_draw_text(ui, count_buf, x + ui->theme.padding, count_y, ui->theme.text_dim);

    /* Entity list area */
    float list_y = count_y + aui_text_height(ui) + ui->theme.spacing;
    float list_h = (y + h) - list_y - ui->theme.padding;
    float item_h = ui->theme.widget_height;
    float visible_items = list_h / item_h;

    /* Clamp scroll */
    float max_scroll = (inspector->entity_count * item_h) - list_h;
    if (max_scroll < 0) max_scroll = 0;

    /* Calculate content width (exclude scrollbar area if needed) */
    bool needs_scrollbar = max_scroll > 0;
    float scrollbar_space = needs_scrollbar ? (ui->theme.scrollbar_width + 4) : 0;
    float content_w = w - scrollbar_space;
    if (inspector->entity_list_scroll > max_scroll) {
        inspector->entity_list_scroll = max_scroll;
    }
    if (inspector->entity_list_scroll < 0) {
        inspector->entity_list_scroll = 0;
    }

    /* Handle scroll input in list area */
    if (ui->input.mouse_x >= x && ui->input.mouse_x < x + w &&
        ui->input.mouse_y >= list_y && ui->input.mouse_y < list_y + list_h) {
        inspector->entity_list_scroll -= ui->input.scroll_y * item_h * 3;
        if (inspector->entity_list_scroll < 0) inspector->entity_list_scroll = 0;
        if (inspector->entity_list_scroll > max_scroll) {
            inspector->entity_list_scroll = max_scroll;
        }
    }

    /* Scissor for list content */
    aui_push_scissor(ui, x, list_y, w, list_h);

    /* Draw visible entities */
    int first_visible = (int)(inspector->entity_list_scroll / item_h);
    int last_visible = first_visible + (int)visible_items + 2;
    if (last_visible > inspector->entity_count) {
        last_visible = inspector->entity_count;
    }

    char name_buf[64];
    for (int i = first_visible; i < last_visible; i++) {
        if (i < 0 || i >= inspector->entity_count) continue;

        ecs_entity_t e = inspector->entities[i];

        /* Skip dead entities */
        if (!ecs_is_alive(world, e)) continue;

        float item_y = list_y + (i * item_h) - inspector->entity_list_scroll;

        /* Check if visible */
        if (item_y + item_h < list_y || item_y > list_y + list_h) continue;

        bool is_selected = (e == inspector->selected);
        bool hovered = (ui->input.mouse_x >= x && ui->input.mouse_x < x + content_w &&
                        ui->input.mouse_y >= item_y && ui->input.mouse_y < item_y + item_h);

        /* Draw background */
        if (is_selected) {
            aui_draw_rect(ui, x, item_y, content_w, item_h, ui->theme.accent);
        } else if (hovered) {
            aui_draw_rect(ui, x, item_y, content_w, item_h, ui->theme.bg_widget_hover);
        }

        /* Handle click */
        if (hovered && ui->input.mouse_pressed[0]) {
            if (inspector->selected != e) {
                inspector->selected = e;
                inspector->inspector_scroll = 0.0f;
                selection_changed = true;
            }
        }

        /* Draw entity name */
        const char *display = get_entity_display_name(world, e, name_buf, sizeof(name_buf));
        float text_x = x + ui->theme.padding;
        float text_y = item_y + (item_h - aui_text_height(ui)) * 0.5f;
        uint32_t text_color = is_selected ? ui->theme.text_highlight : ui->theme.text;
        aui_draw_text(ui, display, text_x, text_y, text_color);

        /* Optionally show entity ID */
        if (inspector->config.show_entity_ids) {
            char id_buf[32];
            snprintf(id_buf, sizeof(id_buf), "%llu", (unsigned long long)e);
            float id_w = aui_text_width(ui, id_buf);
            aui_draw_text(ui, id_buf, x + content_w - id_w - ui->theme.padding,
                         text_y, ui->theme.text_dim);
        }
    }

    aui_pop_scissor(ui);

    /* Draw scrollbar if needed */
    if (needs_scrollbar) {
        float scrollbar_x = x + w - ui->theme.scrollbar_width - 2;
        float scrollbar_h = list_h;
        float thumb_h = (visible_items / (float)inspector->entity_count) * scrollbar_h;
        if (thumb_h < 20) thumb_h = 20;
        float thumb_travel = scrollbar_h - thumb_h;
        float scroll_ratio = (max_scroll > 0) ?
                             (inspector->entity_list_scroll / max_scroll) : 0;
        float thumb_y = list_y + scroll_ratio * thumb_travel;

        /* Scrollbar rects */
        AUI_Rect track_rect = {scrollbar_x, list_y, ui->theme.scrollbar_width, scrollbar_h};
        AUI_Rect thumb_rect = {scrollbar_x + 1, thumb_y, ui->theme.scrollbar_width - 2, thumb_h};

        /* Handle scrollbar interaction */
        bool thumb_hovered = aui_rect_contains(thumb_rect, ui->input.mouse_x, ui->input.mouse_y);
        bool track_hovered = aui_rect_contains(track_rect, ui->input.mouse_x, ui->input.mouse_y);

        /* Start dragging on click */
        if (track_hovered && ui->input.mouse_pressed[0]) {
            inspector->list_scrollbar_dragging = true;
            if (thumb_hovered) {
                /* Clicked on thumb - store offset from thumb top */
                inspector->list_scrollbar_drag_offset = ui->input.mouse_y - thumb_y;
            } else {
                /* Clicked on track - center thumb on click position */
                inspector->list_scrollbar_drag_offset = thumb_h * 0.5f;
                /* Jump to click position */
                float target_thumb_y = ui->input.mouse_y - inspector->list_scrollbar_drag_offset;
                float new_ratio = (target_thumb_y - list_y) / thumb_travel;
                if (new_ratio < 0) new_ratio = 0;
                if (new_ratio > 1) new_ratio = 1;
                inspector->entity_list_scroll = new_ratio * max_scroll;
            }
        }

        /* Handle active drag */
        if (inspector->list_scrollbar_dragging) {
            if (ui->input.mouse_down[0]) {
                float target_thumb_y = ui->input.mouse_y - inspector->list_scrollbar_drag_offset;
                float new_ratio = (target_thumb_y - list_y) / thumb_travel;
                if (new_ratio < 0) new_ratio = 0;
                if (new_ratio > 1) new_ratio = 1;
                inspector->entity_list_scroll = new_ratio * max_scroll;
            } else {
                inspector->list_scrollbar_dragging = false;
            }
        }

        /* Draw track */
        aui_draw_rect(ui, track_rect.x, track_rect.y, track_rect.w, track_rect.h,
                      ui->theme.scrollbar);

        /* Draw thumb - highlight when dragging or hovered */
        uint32_t thumb_color = (inspector->list_scrollbar_dragging || thumb_hovered)
                               ? ui->theme.accent
                               : ui->theme.scrollbar_grab;
        aui_draw_rect_rounded(ui, thumb_rect.x, thumb_rect.y, thumb_rect.w, thumb_rect.h,
                              thumb_color, 3);
    } else {
        inspector->list_scrollbar_dragging = false;
    }

    return selection_changed;
}

/* ============================================================================
 * UI Drawing - Inspector Panel
 * ============================================================================ */

void agentite_inspector_panel(Agentite_Inspector *inspector,
                               AUI_Context *ui,
                               float x, float y, float w, float h)
{
    if (!inspector || !ui) return;

    ecs_world_t *world = agentite_ecs_get_world(inspector->world);
    if (!world) return;

    /* Draw panel background */
    aui_draw_rect(ui, x, y, w, h, ui->theme.bg_panel);

    /* No selection message */
    if (inspector->selected == 0) {
        const char *msg = "No entity selected";
        float msg_w = aui_text_width(ui, msg);
        float msg_x = x + (w - msg_w) * 0.5f;
        float msg_y = y + h * 0.5f - aui_text_height(ui) * 0.5f;
        aui_draw_text(ui, msg, msg_x, msg_y, ui->theme.text_dim);
        return;
    }

    /* Verify entity is still alive */
    if (!ecs_is_alive(world, inspector->selected)) {
        const char *msg = "Entity deleted";
        float msg_w = aui_text_width(ui, msg);
        float msg_x = x + (w - msg_w) * 0.5f;
        float msg_y = y + h * 0.5f - aui_text_height(ui) * 0.5f;
        aui_draw_text(ui, msg, msg_x, msg_y, ui->theme.text_dim);
        return;
    }

    /* Header with entity info */
    float content_x = x + ui->theme.padding;
    float content_w = w - ui->theme.padding * 2;
    float cursor_y = y + ui->theme.padding;

    char name_buf[64];
    const char *entity_name = get_entity_display_name(world, inspector->selected,
                                                       name_buf, sizeof(name_buf));

    /* Entity name/ID header */
    aui_draw_text(ui, entity_name, content_x, cursor_y, ui->theme.text_highlight);
    cursor_y += aui_text_height(ui) + ui->theme.spacing;

    /* Entity ID subheader */
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "ID: %llu", (unsigned long long)inspector->selected);
    aui_draw_text(ui, id_buf, content_x, cursor_y, ui->theme.text_dim);
    cursor_y += aui_text_height(ui) + ui->theme.spacing * 2;

    /* Separator */
    aui_draw_rect(ui, content_x, cursor_y, content_w, 1, ui->theme.border);
    cursor_y += ui->theme.spacing * 2;

    /* Content area with scroll */
    float content_area_y = cursor_y;
    float content_area_h = (y + h) - cursor_y - ui->theme.padding;

    /* Get entity's type (list of components) */
    const ecs_type_t *type = ecs_get_type(world, inspector->selected);
    if (!type || type->count == 0) {
        aui_draw_text(ui, "No components", content_x, cursor_y, ui->theme.text_dim);
        return;
    }

    /* Calculate total content height for scrolling */
    float total_content_h = 0;
    for (int i = 0; i < (int)type->count; i++) {
        ecs_entity_t comp_id = type->array[i];

        /* Skip relationship components, prefabs, etc. */
        if (ECS_IS_PAIR(comp_id)) continue;

        const char *comp_name = ecs_get_name(world, comp_id);
        if (!comp_name) continue;

        /* Header height */
        total_content_h += ui->theme.widget_height + ui->theme.spacing;

        /* Get reflection data */
        const Agentite_ComponentMeta *meta = agentite_reflect_get(
            inspector->registry, comp_id);
        if (meta) {
            /* Field heights */
            total_content_h += meta->field_count *
                              (aui_text_height(ui) + ui->theme.spacing);
        } else {
            /* "No reflection data" message */
            total_content_h += aui_text_height(ui) + ui->theme.spacing;
        }

        total_content_h += ui->theme.spacing;  /* Gap between components */
    }

    /* Handle scroll */
    float max_scroll = total_content_h - content_area_h;
    if (max_scroll < 0) max_scroll = 0;

    if (ui->input.mouse_x >= x && ui->input.mouse_x < x + w &&
        ui->input.mouse_y >= content_area_y &&
        ui->input.mouse_y < content_area_y + content_area_h) {
        inspector->inspector_scroll -= ui->input.scroll_y * 30;
        if (inspector->inspector_scroll < 0) inspector->inspector_scroll = 0;
        if (inspector->inspector_scroll > max_scroll) {
            inspector->inspector_scroll = max_scroll;
        }
    }

    /* Scissor for content */
    aui_push_scissor(ui, x, content_area_y, w, content_area_h);

    float draw_y = content_area_y - inspector->inspector_scroll;

    /* Draw each component */
    for (int i = 0; i < (int)type->count; i++) {
        ecs_entity_t comp_id = type->array[i];

        /* Skip relationship components, prefabs, etc. */
        if (ECS_IS_PAIR(comp_id)) continue;

        const char *comp_name = ecs_get_name(world, comp_id);
        if (!comp_name) continue;

        /* Component header */
        float header_h = ui->theme.widget_height;

        /* Only draw if visible */
        if (draw_y + header_h >= content_area_y &&
            draw_y < content_area_y + content_area_h) {

            /* Header background */
            aui_draw_rect(ui, content_x, draw_y, content_w, header_h,
                         ui->theme.bg_widget);

            /* Component name */
            float text_y = draw_y + (header_h - aui_text_height(ui)) * 0.5f;
            aui_draw_text(ui, comp_name, content_x + ui->theme.padding,
                         text_y, ui->theme.text);

            /* Optional: show component size */
            if (inspector->config.show_component_sizes) {
                const Agentite_ComponentMeta *meta = agentite_reflect_get(
                    inspector->registry, comp_id);
                if (meta) {
                    char size_buf[32];
                    snprintf(size_buf, sizeof(size_buf), "%zu bytes", meta->size);
                    float size_w = aui_text_width(ui, size_buf);
                    aui_draw_text(ui, size_buf,
                                 content_x + content_w - size_w - ui->theme.padding,
                                 text_y, ui->theme.text_dim);
                }
            }
        }

        draw_y += header_h + ui->theme.spacing;

        /* Get reflection data for field display */
        const Agentite_ComponentMeta *meta = agentite_reflect_get(
            inspector->registry, comp_id);

        if (meta) {
            /* Get component data */
            const void *comp_data = ecs_get_id(world, inspector->selected, comp_id);
            if (comp_data) {
                /* Draw each field */
                for (int f = 0; f < meta->field_count; f++) {
                    const Agentite_FieldDesc *field = &meta->fields[f];
                    float row_h = aui_text_height(ui);

                    /* Only draw if visible */
                    if (draw_y + row_h >= content_area_y &&
                        draw_y < content_area_y + content_area_h) {

                        /* Field name */
                        float field_x = content_x + ui->theme.padding * 2;
                        aui_draw_text(ui, field->name, field_x, draw_y,
                                     ui->theme.text_dim);

                        /* Optional: show field type */
                        float value_x = content_x + content_w * 0.4f;
                        if (inspector->config.show_field_types) {
                            const char *type_name = agentite_reflect_type_name(field->type);
                            char type_buf[32];
                            snprintf(type_buf, sizeof(type_buf), "(%s)", type_name);
                            aui_draw_text(ui, type_buf,
                                         content_x + content_w * 0.3f, draw_y,
                                         ui->theme.text_dim);
                            value_x = content_x + content_w * 0.5f;
                        }

                        /* Field value */
                        char value_buf[INSPECTOR_FIELD_BUFFER_SIZE];
                        const uint8_t *field_ptr = (const uint8_t *)comp_data + field->offset;
                        agentite_reflect_format_field(field, field_ptr,
                                                       value_buf, sizeof(value_buf));
                        aui_draw_text(ui, value_buf, value_x, draw_y, ui->theme.text);
                    }

                    draw_y += row_h + ui->theme.spacing;
                }
            }
        } else {
            /* No reflection data available */
            if (draw_y + aui_text_height(ui) >= content_area_y &&
                draw_y < content_area_y + content_area_h) {
                aui_draw_text(ui, "(no reflection data)",
                             content_x + ui->theme.padding * 2, draw_y,
                             ui->theme.text_dim);
            }
            draw_y += aui_text_height(ui) + ui->theme.spacing;
        }

        draw_y += ui->theme.spacing;  /* Gap between components */
    }

    aui_pop_scissor(ui);

    /* Draw scrollbar if needed */
    if (max_scroll > 0) {
        float scrollbar_x = x + w - ui->theme.scrollbar_width - 2;
        float scrollbar_h = content_area_h;
        float visible_ratio = content_area_h / total_content_h;
        float thumb_h = visible_ratio * scrollbar_h;
        if (thumb_h < 20) thumb_h = 20;
        float thumb_travel = scrollbar_h - thumb_h;
        float scroll_ratio = (max_scroll > 0) ?
                             (inspector->inspector_scroll / max_scroll) : 0;
        float thumb_y = content_area_y + scroll_ratio * thumb_travel;

        /* Scrollbar rects */
        AUI_Rect track_rect = {scrollbar_x, content_area_y,
                               ui->theme.scrollbar_width, scrollbar_h};
        AUI_Rect thumb_rect = {scrollbar_x + 1, thumb_y,
                               ui->theme.scrollbar_width - 2, thumb_h};

        /* Handle scrollbar interaction */
        bool thumb_hovered = aui_rect_contains(thumb_rect, ui->input.mouse_x, ui->input.mouse_y);
        bool track_hovered = aui_rect_contains(track_rect, ui->input.mouse_x, ui->input.mouse_y);

        /* Start dragging on click */
        if (track_hovered && ui->input.mouse_pressed[0]) {
            inspector->panel_scrollbar_dragging = true;
            if (thumb_hovered) {
                inspector->panel_scrollbar_drag_offset = ui->input.mouse_y - thumb_y;
            } else {
                inspector->panel_scrollbar_drag_offset = thumb_h * 0.5f;
                float target_thumb_y = ui->input.mouse_y - inspector->panel_scrollbar_drag_offset;
                float new_ratio = (target_thumb_y - content_area_y) / thumb_travel;
                if (new_ratio < 0) new_ratio = 0;
                if (new_ratio > 1) new_ratio = 1;
                inspector->inspector_scroll = new_ratio * max_scroll;
            }
        }

        /* Handle active drag */
        if (inspector->panel_scrollbar_dragging) {
            if (ui->input.mouse_down[0]) {
                float target_thumb_y = ui->input.mouse_y - inspector->panel_scrollbar_drag_offset;
                float new_ratio = (target_thumb_y - content_area_y) / thumb_travel;
                if (new_ratio < 0) new_ratio = 0;
                if (new_ratio > 1) new_ratio = 1;
                inspector->inspector_scroll = new_ratio * max_scroll;
            } else {
                inspector->panel_scrollbar_dragging = false;
            }
        }

        /* Draw track */
        aui_draw_rect(ui, track_rect.x, track_rect.y, track_rect.w, track_rect.h,
                      ui->theme.scrollbar);

        /* Draw thumb */
        uint32_t thumb_color = (inspector->panel_scrollbar_dragging || thumb_hovered)
                               ? ui->theme.accent
                               : ui->theme.scrollbar_grab;
        aui_draw_rect_rounded(ui, thumb_rect.x, thumb_rect.y, thumb_rect.w, thumb_rect.h,
                              thumb_color, 3);
    } else {
        inspector->panel_scrollbar_dragging = false;
    }
}

/* ============================================================================
 * UI Drawing - Combined
 * ============================================================================ */

void agentite_inspector_draw(Agentite_Inspector *inspector,
                              AUI_Context *ui,
                              float x, float y, float w, float h)
{
    if (!inspector || !ui) return;

    float list_w = inspector->config.entity_list_width;
    float panel_w = w - list_w - ui->theme.spacing;

    /* Entity list on the left */
    agentite_inspector_entity_list(inspector, ui, x, y, list_w, h);

    /* Inspector panel on the right */
    float panel_x = x + list_w + ui->theme.spacing;
    agentite_inspector_panel(inspector, ui, panel_x, y, panel_w, h);
}
