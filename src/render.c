#include "render.h"

#include "resize_params.h"
#include "utils_cairo.h"

#include <stddef.h>

#define BG_COLOR              0x00003388
#define WIN_BORDER_COLOR      0x88aa88ee
#define WIN_BG_COLOR          0x00330088
#define GUIDE_LINE_COLOR      0xbb9999ee
#define GUIDE_LABEL_COLOR     0xeeeeeeff
#define GUIDE_LABEL_FONT_SIZE 15

static void _render_vertical_guide(
    cairo_t *cairo, uint32_t x, uint32_t start_y, uint32_t end_y, char *label
) {
    cairo_set_font_size(cairo, GUIDE_LABEL_FONT_SIZE);
    cairo_text_extents_t te;
    cairo_text_extents(cairo, label, &te);

    uint32_t label_x = x - te.x_advance / 2;
    uint32_t label_y =
        end_y + (start_y < end_y ? GUIDE_LABEL_FONT_SIZE + 3.5 : -5);

    static const double dashes[] = {1., 1.};
    cairo_set_dash(cairo, dashes, 2, 0);

    cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
    cairo_move_to(cairo, x + .5, start_y + .5);
    cairo_line_to(cairo, x + .5, end_y + .5);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    cairo_set_dash(cairo, NULL, 0, 0);

    cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
    cairo_set_line_width(cairo, 2);
    cairo_move_to(cairo, x - 4.5, end_y);
    cairo_line_to(cairo, x + 5.5, end_y);
    cairo_stroke(cairo);

    cairo_set_source_u32(cairo, GUIDE_LABEL_COLOR);
    cairo_move_to(cairo, label_x, label_y);
    cairo_show_text(cairo, label);
}

static void _render_horizontal_guide(
    cairo_t *cairo, uint32_t y, uint32_t start_x, uint32_t end_x, char *label
) {
    cairo_set_font_size(cairo, GUIDE_LABEL_FONT_SIZE);
    cairo_text_extents_t te;
    cairo_text_extents(cairo, label, &te);

    uint32_t label_x = end_x + (start_x < end_x ? 3.5 : -te.x_advance - 3.5);
    uint32_t label_y = y + te.height / 2;

    static const double dashes[] = {1., 1.};
    cairo_set_dash(cairo, dashes, 2, 0);

    cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
    cairo_move_to(cairo, start_x + .5, y + .5);
    cairo_line_to(cairo, end_x + .5, y + .5);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    cairo_set_dash(cairo, NULL, 0, 0);

    cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
    cairo_set_line_width(cairo, 2);
    cairo_move_to(cairo, end_x + .5, y - 4.5);
    cairo_line_to(cairo, end_x + .5, y + 5.5);
    cairo_stroke(cairo);

    cairo_set_source_u32(cairo, GUIDE_LABEL_COLOR);
    cairo_move_to(cairo, label_x, label_y);
    cairo_show_text(cairo, label);
}

#define GUIDE_PADDING 10

static void _render_guides(
    cairo_t *cairo, struct resize_parameter *params, size_t num_params,
    struct focused_window *focused_window, enum resize_direction direction,
    size_t num_applicable
) {
    int      y = 0;
    uint32_t start_pos =
        direction == RESIZE_VERTICAL
            ? focused_window->rect.x +
                  (focused_window->rect.w - GUIDE_PADDING * num_applicable) / 2
            : focused_window->rect.y +
                  (focused_window->rect.h - GUIDE_PADDING * num_applicable) / 2;

    for (int i = 0; i < num_params; i++) {
        struct resize_parameter *param = &params[i];

        if (!param->applicable) {
            continue;
        }

        char label[5];
        rune_to_str(param->symbol, label);

        cairo_set_font_size(cairo, GUIDE_LABEL_FONT_SIZE);
        cairo_text_extents_t te;
        cairo_text_extents(cairo, label, &te);

        uint32_t pos = start_pos + GUIDE_PADDING * (y + 1);
        if (direction == RESIZE_VERTICAL) {
            if (param->guides[0] != NO_GUIDE) {
                _render_vertical_guide(
                    cairo, pos, focused_window->rect.y, param->guides[0], label
                );
            }
            if (param->guides[1] != NO_GUIDE) {
                _render_vertical_guide(
                    cairo, pos,
                    focused_window->rect.y + focused_window->rect.h - 1,
                    param->guides[1], label
                );
            }
        } else {
            if (param->guides[0] != NO_GUIDE) {
                _render_horizontal_guide(
                    cairo, pos, focused_window->rect.x, param->guides[0], label
                );
            }
            if (param->guides[1] != NO_GUIDE) {
                _render_horizontal_guide(
                    cairo, pos,
                    focused_window->rect.x + focused_window->rect.w - 1,
                    param->guides[1], label
                );
            }
        }
        y++;
    }
}

void render(struct state *state, cairo_t *cairo) {
    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_u32(cairo, BG_COLOR);
    cairo_paint(cairo);

    cairo_rectangle(
        cairo, state->focused_window.rect.x + .5,
        state->focused_window.rect.y + .5, state->focused_window.rect.w,
        state->focused_window.rect.h
    );
    cairo_set_source_u32(cairo, WIN_BG_COLOR);
    cairo_fill(cairo);

    _render_guides(
        cairo, state->resize_params->vertical_params,
        state->resize_params->num_vertical_params, &state->focused_window,
        RESIZE_VERTICAL, state->resize_params->num_applicable_vertical_params
    );
    _render_guides(
        cairo, state->resize_params->horizontal_params,
        state->resize_params->num_horizontal_params, &state->focused_window,
        RESIZE_HORIZONTAL,
        state->resize_params->num_applicable_horizontal_params
    );

    cairo_set_line_width(cairo, 1);
    cairo_rectangle(
        cairo, state->focused_window.rect.x + .5,
        state->focused_window.rect.y + .5, state->focused_window.rect.w - 1,
        state->focused_window.rect.h - 1
    );
    cairo_set_source_u32(cairo, WIN_BORDER_COLOR);
    cairo_stroke(cairo);
}
