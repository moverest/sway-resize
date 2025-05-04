#ifndef __SWAY_WIN_H_INCLUDED__
#define __SWAY_WIN_H_INCLUDED__

#include "utils.h"

#include <jansson.h>
#include <stdbool.h>
#include <stdint.h>

struct focused_window {
    int         id;
    const char *output;
    struct rect rect;
    struct rect output_rect;
    int32_t     resize_top_limit;
    int32_t     resize_bottom_limit;
    int32_t     resize_left_limit;
    int32_t     resize_right_limit;
    bool        resize_top;
    bool        resize_bottom;
    bool        resize_left;
    bool        resize_right;
    bool        floating;
};

int  find_focused_window(struct focused_window *fw, json_t *tree);
void log_focused_window(struct focused_window *fw);

#endif
