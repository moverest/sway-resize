#include "log.h"
#include "resize_params.h"
#include "utils.h"

#include <stdlib.h>

int main() {
    struct resize_parameters *params =
        load_resize_parameters("a:h:8% b:v:+5 c:v:+45% d:v:50%");

    static const struct resize_parameter expected_vertical_params[] = {
        {
            .value      = 5,
            .symbol     = 'b',
            .relative   = true,
            .percentage = false,
        },
        {
            .value      = 45,
            .symbol     = 'c',
            .relative   = true,
            .percentage = true,
        },
        {
            .value      = 50,
            .symbol     = 'd',
            .relative   = false,
            .percentage = true,
        },
    };

    static const struct resize_parameter expected_horizontal_params[] = {
        {
            .value      = 8,
            .symbol     = 'a',
            .relative   = false,
            .percentage = true,
        },
    };

    static const struct resize_parameters expect_params = {
        .params =
            {
                [RESIZE_HORIZONTAL] =
                    (struct resize_parameter *)expected_horizontal_params,
                [RESIZE_VERTICAL] =
                    (struct resize_parameter *)expected_horizontal_params,
                [RESIZE_RIGHT]  = NULL,
                [RESIZE_LEFT]   = NULL,
                [RESIZE_TOP]    = NULL,
                [RESIZE_BOTTOM] = NULL,
            },
        .counts =
            {
                [RESIZE_HORIZONTAL] = ARRAY_LEN(expected_horizontal_params),
                [RESIZE_VERTICAL]   = ARRAY_LEN(expected_vertical_params),
                [RESIZE_RIGHT]      = 0,
                [RESIZE_LEFT]       = 0,
                [RESIZE_TOP]        = 0,
                [RESIZE_BOTTOM]     = 0,
            },
    };

    for (int direction = 0; direction < NUM_DIRECTIONS; direction++) {
        if (params->counts[direction] != expect_params.counts[direction]) {
            LOG_ERR(
                "Wrong size for params[%s]: %zu. Expected %zu",
                resize_direction_to_str(direction), params->counts[direction],
                expect_params.counts[direction]
            );
            return 1;
        }

#define CHECK_FIELD(params, expected_params, field)      \
    if (params->params[direction][i].field !=            \
        expected_params->params[direction][i].field) {   \
        LOG_ERR(                                         \
            "%s[%d].%s = %d wrong.", #params, i, #field, \
            params->params[direction][i].field           \
        );                                               \
        return 2;                                        \
    }

#define CHECK_PARAMS(params, expected_params, len)        \
    for (int i = 0; i < len; i++) {                       \
        CHECK_FIELD(params, expected_params, value);      \
        CHECK_FIELD(params, expected_params, symbol);     \
        CHECK_FIELD(params, expected_params, relative);   \
        CHECK_FIELD(params, expected_params, percentage); \
    }
    }

    free_resize_params(params);
    return 0;
}
