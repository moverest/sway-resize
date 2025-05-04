#include "render.h"

#include "log.h"
#include "utils_cairo.h"

#define BG_COLOR              0x00007744
#define WIN_BG_COLOR          0x00770044
#define GUIDE_LINE_COLOR      0xffffffbb
#define GUIDE_LINE_BG_COLOR   0x00000044
#define GUIDE_LABEL_COLOR     0xeeeeeeff
#define GUIDE_LABEL_FONT_SIZE 15

void _render_line(cairo_t *cairo, struct line *line, char *label) {
    cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
    cairo_move_to(cairo, line->x1 + .5, line->y1 + .5);
    cairo_line_to(cairo, line->x2 + .5, line->y2 + .5);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    cairo_set_font_size(cairo, GUIDE_LABEL_FONT_SIZE);
    cairo_text_extents_t te;
    cairo_text_extents(cairo, label, &te);

    cairo_rectangle(
        cairo, (line->x1 + line->x2 - te.width) / 2,
        (line->y1 + line->y2 - GUIDE_LABEL_FONT_SIZE) / 2, te.width,
        GUIDE_LABEL_FONT_SIZE
    );
    cairo_set_source_u32(cairo, GUIDE_LINE_BG_COLOR);
    cairo_fill(cairo);

    cairo_move_to(
        cairo, (line->x1 + line->x2 - te.x_advance) / 2,
        (line->y1 + line->y2 + te.height) / 2
    );
    cairo_set_source_u32(cairo, GUIDE_LABEL_COLOR);
    cairo_show_text(cairo, label);
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

    for (int i = 0; i < state->num_resize_params; i++) {
        struct resize_parameter *param = &state->resize_params[i];

        if (!param->applicable) {
            continue;
        }

        char symbol[5];
        rune_to_str(param->symbol, symbol);

        _render_line(cairo, &param->guides[0], symbol);

        if (param->guides[1].x1 >= 0) {
            _render_line(cairo, &param->guides[1], symbol);
        }
    }
}
