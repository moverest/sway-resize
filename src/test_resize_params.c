#include "log.h"
#include "resize_params.h"

#include <stdlib.h>

int main() {
    struct resize_parameter *params;
    int                      params_len =
        load_resize_parameters(&params, "a:h:8% b:v:+5 c:v:+45% d:v:50%");

    if (params_len != 4) {
        LOG_ERR("Wrong size %d.", params_len);
        return 1;
    }

    static const struct resize_parameter expected_params[] = {
        {
            .value      = 8,
            .symbol     = 'a',
            .relative   = false,
            .percentage = true,
            .direction  = RESIZE_HORIZONTAL,
        },
        {
            .value      = 5,
            .symbol     = 'b',
            .relative   = true,
            .percentage = false,
            .direction  = RESIZE_VERTICAL,
        },
        {
            .value      = 45,
            .symbol     = 'c',
            .relative   = true,
            .percentage = true,
            .direction  = RESIZE_VERTICAL,
        },
        {
            .value      = 50,
            .symbol     = 'd',
            .relative   = false,
            .percentage = true,
            .direction  = RESIZE_VERTICAL,
        },
    };

    for (int i = 0; i < params_len; i++) {

#define CHECK_FIELD(field)                                                \
    if (params[i].field != expected_params[i].field) {                    \
        LOG_ERR("params[%d].%s = %d wrong.", i, #field, params[i].field); \
        return 2;                                                         \
    }

        CHECK_FIELD(value);
        CHECK_FIELD(symbol);
        CHECK_FIELD(relative);
        CHECK_FIELD(percentage);
        CHECK_FIELD(direction);
    }

    free(params);
    return 0;
}
