#include "resize_params.h"

#include "log.h"
#include "utils.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _load_resize_param_from_token(
    struct resize_parameter *param, char *token,
    enum resize_direction *direction
) {
    uint32_t rune;
    char    *p = token + str_to_rune(token, &rune);

    if (*p != ':') {
        return 1;
    }

    p++;

    switch (*p) {
    case 'h':
        *direction = RESIZE_HORIZONTAL;
        break;

    case 'v':
        *direction = RESIZE_VERTICAL;
        break;

    default:
        return 2;
    }
    p++;

    if (*p != ':') {
        return 1;
    }

    p++;

    bool relative = false;
    bool negative = false;
    if (*p == '+') {
        relative = true;
        p++;
    } else if (*p == '-') {
        relative = true;
        negative = true;
        p++;
    }

    char num_str[16];
    int  i = 0;

    while ('0' <= *p && *p <= '9' && i < sizeof(num_str) - 1) {
        num_str[i++] = *p;
        p++;
    }

    if (i == 0) {
        return 3;
    }

    num_str[i] = '\0';
    int value  = atoi(num_str);

    bool percentage = false;
    if (*p == '%') {
        percentage = true;
        p++;
    }

    if (*p != '\0') {
        return 4;
    }

    param->relative   = relative;
    param->percentage = percentage;
    param->value      = negative ? -value : value;
    param->symbol     = rune;

    return 0;
}

struct resize_parameters *load_resize_parameters(char *s) {
    static const char         delims[] = " \t\n";
    struct resize_parameters *params = malloc(sizeof(struct resize_parameters));

    int  s_len = strlen(s);
    char buf[s_len + 1];
    strcpy(buf, s);
    buf[s_len] = '\0';

    size_t vcap = 8;
    size_t hcap = 8;

    params->vertical_params   = malloc(sizeof(struct resize_parameter) * vcap);
    params->horizontal_params = malloc(sizeof(struct resize_parameter) * hcap);

    params->num_vertical_params   = 0;
    params->num_horizontal_params = 0;

    char *strtok_p;

    char *token = strtok_r(buf, delims, &strtok_p);
    while (token != NULL) {
        struct resize_parameter param;
        enum resize_direction   direction;

        if (_load_resize_param_from_token(&param, token, &direction) != 0) {
            LOG_ERR("Could not parse token `%s`.", token);
            free_resize_params(params);
            return NULL;
        }

        if (direction == RESIZE_VERTICAL) {
            if (vcap < params->num_vertical_params + 1) {
                vcap                    *= 1.5;
                params->vertical_params  = realloc(
                    params->vertical_params,
                    sizeof(struct resize_parameter) * vcap
                );
            }
            memcpy(
                &params->vertical_params[params->num_vertical_params++], &param,
                sizeof(struct resize_parameter)
            );
        } else {
            if (hcap < params->num_horizontal_params + 1) {
                hcap                      *= 1.5;
                params->horizontal_params  = realloc(
                    params->horizontal_params,
                    sizeof(struct resize_parameter) * hcap
                );
            }
            memcpy(
                &params->horizontal_params[params->num_horizontal_params++],
                &param, sizeof(struct resize_parameter)
            );
        }

        token = strtok_r(NULL, delims, &strtok_p);
    }

    params->vertical_params = realloc(
        params->vertical_params,
        sizeof(struct resize_parameter) * params->num_vertical_params
    );
    params->horizontal_params = realloc(
        params->horizontal_params,
        sizeof(struct resize_parameter) * params->num_horizontal_params
    );

    return params;
}

static int _resize_parameters_compute_guides(
    struct resize_parameter *params, size_t len, struct focused_window *fw,
    enum resize_direction direction
) {
    int32_t min_limit;
    int32_t max_limit;
    int32_t resizable_min_dir;
    int32_t resizable_max_dir;
    int32_t size;
    int32_t pos;

    size_t num_applicable = 0;

    if (direction == RESIZE_HORIZONTAL) {
        min_limit         = fw->resize_left_limit;
        max_limit         = fw->resize_right_limit;
        resizable_min_dir = fw->resize_left;
        resizable_max_dir = fw->resize_right;
        size              = fw->rect.w;
        pos               = fw->rect.x;
    } else {
        min_limit         = fw->resize_top_limit;
        max_limit         = fw->resize_bottom_limit;
        resizable_min_dir = fw->resize_top;
        resizable_max_dir = fw->resize_bottom;
        size              = fw->rect.h;
        pos               = fw->rect.y;
    }

    for (int i = 0; i < len; i++) {
        struct resize_parameter *param = &params[i];

        if (!resizable_min_dir && !resizable_max_dir) {
            param->applicable = false;
            continue;
        }

        int32_t new_size;

        if (param->relative) {
            new_size = param->percentage ? (100 + param->value) * size / 100
                                         : size + param->value;
        } else {
            new_size = param->percentage
                           ? (max_limit - min_limit) * param->value / 100
                           : param->value;
        }

        if (abs(new_size - size) < 2) {
            param->applicable = false;
            continue;
        }

        int32_t guide_pos[2];

        if (new_size < 15) {
            param->applicable = false;
            continue;
        }

        if (!resizable_min_dir) {
            /*
                pos
                min_limit                    max_limit
                |                            |
            ----|--------|-------|-----------|------->
                ^________^       ^
                   size          guide_pos[0]
                ^________________^
                     new_size

             */

            guide_pos[0] = NO_GUIDE;
            guide_pos[1] = new_size + pos;

            if (guide_pos[1] > max_limit) {
                param->applicable = false;
                continue;
            }

        } else if (!resizable_max_dir) {
            /*
                min_limit                    max_limit
                |                pos         |
            ----|--------|-------|-----------|------->
                         .       ^___________^
                         .           size    .
                         ^___________________^
                                new_size
             */

            guide_pos[0] = pos + size - new_size;
            guide_pos[1] = NO_GUIDE;

            if (guide_pos[0] < min_limit) {
                param->applicable = false;
                continue;
            }
        } else {
            /*
                min_limit                                           max_limit
                |                pos                                |
            ----|--------|-------|-----------|-------|--------------|------->
                         .       ^___________^       .
                         .           size            .
                         ^___________________________^
                                   new_size
             */

            int32_t grow = (new_size - size) / 2;
            guide_pos[0] = pos - grow;

            if (guide_pos[0] < min_limit) {
                param->applicable = false;
                continue;
            }

            guide_pos[1] = pos + size + grow;
            if (guide_pos[1] > max_limit) {
                param->applicable = false;
                continue;
            }

            new_size = guide_pos[1] - guide_pos[0];
        }

        param->size       = new_size;
        param->guides[0]  = guide_pos[0];
        param->guides[1]  = guide_pos[1];
        param->applicable = true;

        num_applicable++;
    }

    return num_applicable;
}

int resize_parameters_compute_guides(
    struct resize_parameters *params, struct focused_window *fw
) {
    params->num_applicable_horizontal_params =
        _resize_parameters_compute_guides(
            params->horizontal_params, params->num_horizontal_params, fw,
            RESIZE_HORIZONTAL
        );
    params->num_applicable_vertical_params = _resize_parameters_compute_guides(
        params->vertical_params, params->num_vertical_params, fw,
        RESIZE_VERTICAL
    );

    return params->num_applicable_vertical_params < 0 ||
           params->num_applicable_horizontal_params < 0;
}

static void
_log_resize_params(struct resize_parameter *params, int len, char *name) {
    for (int i = 0; i < len; i++) {
        char symbol[5];
        rune_to_str(params[i].symbol, symbol);
        LOG_INFO("%s[%d]", name, i);
        LOG_INFO(" .symbol = %s (%d)", symbol, params[i].symbol);
        LOG_INFO(" .value = %d", params[i].value);
        LOG_INFO(" .relative = %s", params[i].relative ? "true" : "false");
        LOG_INFO(" .percentage = %s", params[i].percentage ? "true" : "false");
        LOG_INFO(" .applicable = %s", params[i].applicable ? "true" : "false");

        if (params[i].applicable) {
            LOG_INFO("  .size = %d", params[i].size);
            LOG_INFO("  .guides[0] = %d", params[i].guides[0]);
            LOG_INFO("  .guides[1] = %d", params[i].guides[1]);
        }
    }
}

void log_resize_params(struct resize_parameters *params) {
    _log_resize_params(
        params->vertical_params, params->num_vertical_params, "vertical_params"
    );
    _log_resize_params(
        params->horizontal_params, params->num_horizontal_params,
        "horizontal_params"
    );
}

struct resize_parameter *find_resize_param_by_symbol(
    struct resize_parameters *params, uint32_t rune,
    enum resize_direction *direction
) {
    for (int i = 0; i < params->num_vertical_params; i++) {
        if (params->vertical_params[i].symbol == rune) {
            *direction = RESIZE_VERTICAL;
            return &params->vertical_params[i];
        }
    }

    for (int i = 0; i < params->num_horizontal_params; i++) {
        if (params->horizontal_params[i].symbol == rune) {
            *direction = RESIZE_HORIZONTAL;
            return &params->horizontal_params[i];
        }
    }

    return NULL;
}

void free_resize_params(struct resize_parameters *params) {
    free(params->vertical_params);
    free(params->horizontal_params);
    free(params);
}
