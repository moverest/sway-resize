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
    case 't':
        *direction = RESIZE_TOP;
        break;
    case 'b':
        *direction = RESIZE_BOTTOM;
        break;
    case 'l':
        *direction = RESIZE_LEFT;
        break;
    case 'r':
        *direction = RESIZE_RIGHT;
        break;
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

    size_t caps[NUM_DIRECTIONS];
    for (int i = 0; i < NUM_DIRECTIONS; i++) {
        caps[i]           = 0;
        params->params[i] = NULL;
        params->counts[i] = 0;
    }

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

        if (caps[direction] == 0) {
            caps[direction] = 8;
            params->params[direction] =
                malloc(sizeof(struct resize_parameter) * caps[direction]);
        } else if (caps[direction] < params->counts[direction] + 1) {
            caps[direction]           *= 1.5;
            params->params[direction]  = realloc(
                params->params[direction],
                sizeof(struct resize_parameter) * caps[direction]
            );
        }
        memcpy(
            &params->params[direction][params->counts[direction]++], &param,
            sizeof(struct resize_parameter)
        );

        token = strtok_r(NULL, delims, &strtok_p);
    }

    for (int i = 0; i < NUM_DIRECTIONS; i++) {
        if (params->params[i] != NULL) {
            params->params[i] = realloc(
                params->params[i],
                sizeof(struct resize_parameter) * params->counts[i]
            );
        }
    }
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
    params->applicable_counts[RESIZE_HORIZONTAL] =
        _resize_parameters_compute_guides(
            params->params[RESIZE_HORIZONTAL],
            params->counts[RESIZE_HORIZONTAL], fw, RESIZE_HORIZONTAL
        );
    params->applicable_counts[RESIZE_VERTICAL] =
        _resize_parameters_compute_guides(
            params->params[RESIZE_VERTICAL], params->counts[RESIZE_VERTICAL],
            fw, RESIZE_VERTICAL
        );

    return params->applicable_counts[RESIZE_VERTICAL] < 0 ||
           params->applicable_counts[RESIZE_HORIZONTAL] < 0;
}

static void
_log_resize_params(struct resize_parameter *params, int len, const char *name) {
    if (len == 0) {
        LOG_INFO("params[%s] = []", name);
        return;
    }

    for (int i = 0; i < len; i++) {
        char symbol[5];
        rune_to_str(params[i].symbol, symbol);
        LOG_INFO("params[%s][%d]", name, i);
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

const char *resize_direction_to_str(enum resize_direction direction) {
    const static char *invalid_str = "RESIZE_DIRECTION_INVALID";
    if (direction < 0 || direction >= NUM_DIRECTIONS) {
        return invalid_str;
    }

#define DIR_STR(dir) [dir] = #dir

    const static char *direction_strs[NUM_DIRECTIONS] = {
        DIR_STR(RESIZE_TOP),        DIR_STR(RESIZE_BOTTOM),
        DIR_STR(RESIZE_LEFT),       DIR_STR(RESIZE_RIGHT),
        DIR_STR(RESIZE_HORIZONTAL), DIR_STR(RESIZE_VERTICAL),
    };

    return direction_strs[direction];
}

void log_resize_params(struct resize_parameters *params) {
    for (int direction = 0; direction < NUM_DIRECTIONS; direction++) {
        _log_resize_params(
            params->params[direction], params->counts[direction],
            resize_direction_to_str(direction)
        );
    }
}

struct resize_parameter *find_resize_param_by_symbol(
    struct resize_parameters *params, uint32_t rune,
    enum resize_direction *out_direction
) {
    for (int direction = 0; direction < NUM_DIRECTIONS; direction++) {
        for (int i = 0; i < params->counts[direction]; i++) {
            if (params->params[direction][i].symbol == rune) {
                *out_direction = direction;
                return &params->params[direction][i];
            }
        }
    }

    return NULL;
}

void free_resize_params(struct resize_parameters *params) {
    for (int direction = 0; direction < NUM_DIRECTIONS; direction++) {
        if (params->params[direction] != NULL) {
            free(params->params[direction]);
        }
    }
    free(params);
}
