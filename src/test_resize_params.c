#include "log.h"
#include "resize_params.h"

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
        .vertical_params = (struct resize_parameter *)expected_vertical_params,
        .horizontal_params =
            (struct resize_parameter *)expected_horizontal_params,
        .num_vertical_params =
            sizeof(expected_vertical_params) / sizeof(struct resize_parameter),
        .num_horizontal_params = sizeof(expected_horizontal_params) /
                                 sizeof(struct resize_parameter),
    };

    if (params->num_vertical_params != expect_params.num_vertical_params) {
        LOG_ERR("Wrong size %zu.", params->num_vertical_params);
        return 1;
    }

    if (params->num_horizontal_params != expect_params.num_horizontal_params) {
        LOG_ERR("Wrong size %zu.", params->num_horizontal_params);
        return 1;
    }

#define CHECK_FIELD(params, expected_params, field)                            \
    if (params[i].field != expected_params[i].field) {                         \
        LOG_ERR("%s[%d].%s = %d wrong.", #params, i, #field, params[i].field); \
        return 2;                                                              \
    }

#define CHECK_PARAMS(params, expected_params, len)        \
    for (int i = 0; i < len; i++) {                       \
        CHECK_FIELD(params, expected_params, value);      \
        CHECK_FIELD(params, expected_params, symbol);     \
        CHECK_FIELD(params, expected_params, relative);   \
        CHECK_FIELD(params, expected_params, percentage); \
    }

    CHECK_PARAMS(
        params->vertical_params, expected_vertical_params,
        params->num_vertical_params
    );
    CHECK_PARAMS(
        params->horizontal_params, expected_horizontal_params,
        params->num_horizontal_params
    );

    free_resize_params(params);
    return 0;
}
