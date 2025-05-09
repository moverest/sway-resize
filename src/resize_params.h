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
    int32_t     value;
    uint32_t    symbol;
    struct line guides[2];
    bool        relative;
    bool        percentage;
    bool        applicable;
    uint32_t    size;
};

struct resize_parameters {
    size_t                   num_horizontal_params;
    size_t                   num_vertical_params;
    struct resize_parameter *horizontal_params;
    struct resize_parameter *vertical_params;
};

/*
 * Load the resize parameters from a string.
 *
 * Example: a:100 b:+100px c:-100px b:50% c:+10%
 */
struct resize_parameters *load_resize_parameters(char *s);
int                       resize_parameters_compute_guides(
                          struct resize_parameters *params, struct focused_window *fw
                      );
void                     log_resize_params(struct resize_parameters *params);
struct resize_parameter *find_resize_param_by_symbol(
    struct resize_parameters *params, uint32_t rune,
    enum resize_direction *direction
);
void free_resize_params(struct resize_parameters *params);

#endif
