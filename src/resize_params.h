#ifndef __RESIZE_PARAMS_H_INCLUDED__
#define __RESIZE_PARAMS_H_INCLUDED__

#include "sway_win.h"

#include <stdbool.h>
#include <stdint.h>

struct line {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
};

enum resize_direction {
    RESIZE_VERTICAL,
    RESIZE_HORIZONTAL,
};

struct resize_parameter {
    int32_t               value;
    uint32_t              symbol;
    struct line           guides[2];
    enum resize_direction direction;
    bool                  relative;
    bool                  percentage;
    bool                  applicable;
    uint32_t              size;
};

/*
 * Load the resize parameters from a string.
 *
 * Example: a:100 b:+100px c:-100px b:50% c:+10%
 */
int load_resize_parameters(struct resize_parameter **params, char *s);
int resize_parameters_compute_guides(
    struct resize_parameter *params, size_t len, struct focused_window *fw
);
void log_resize_params(struct resize_parameter *params, int len);

#endif
