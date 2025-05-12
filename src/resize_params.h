#ifndef __RESIZE_PARAMS_H_INCLUDED__
#define __RESIZE_PARAMS_H_INCLUDED__

#include "sway_win.h"

#include <stdbool.h>
#include <stdint.h>

enum resize_direction {
    RESIZE_LEFT       = 0,
    RESIZE_RIGHT      = 1,
    RESIZE_TOP        = 2,
    RESIZE_BOTTOM     = 3,
    RESIZE_VERTICAL   = 4,
    RESIZE_HORIZONTAL = 5,
};

#define NUM_DIRECTIONS 6
#define NO_GUIDE       -1

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
    struct resize_parameter *params[NUM_DIRECTIONS];
    size_t                   counts[NUM_DIRECTIONS];
    size_t                   applicable_counts[NUM_DIRECTIONS];
};

const char *resize_direction_to_str(enum resize_direction direction);

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
