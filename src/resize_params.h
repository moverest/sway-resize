#ifndef __RESIZE_PARAMS_H_INCLUDED__
#define __RESIZE_PARAMS_H_INCLUDED__

#include "sway_win.h"

#include <stdbool.h>
#include <stdint.h>

enum resize_direction {
    RESIZE_VERTICAL,
    RESIZE_HORIZONTAL,
};

#define NO_GUIDE -1

struct resize_parameter {
    int32_t  value;
    uint32_t symbol;
    int32_t  guides[2];
    uint32_t size;
    bool     relative;
    bool     percentage;
    bool     applicable;
};

struct resize_parameters {
    size_t                   num_horizontal_params;
    size_t                   num_vertical_params;
    struct resize_parameter *horizontal_params;
    struct resize_parameter *vertical_params;
    size_t                   num_applicable_horizontal_params;
    size_t                   num_applicable_vertical_params;
};

/*
 * Load the resize parameters from a string.
 *
 * Example: a:100 b:+100px c:-100px b:50% c:+10%
 */
struct resize_parameters *load_resize_parameters(char *s);

int resize_parameters_compute_guides(
    struct resize_parameters *params, struct focused_window *fw
);

void log_resize_params(struct resize_parameters *params);

struct resize_parameter *find_resize_param_by_symbol(
    struct resize_parameters *params, uint32_t rune,
    enum resize_direction *direction
);

void free_resize_params(struct resize_parameters *params);

#endif
