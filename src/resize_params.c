#include "resize_params.h"

#include "log.h"
#include "utils.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int
_load_resize_param_from_token(struct resize_parameter *param, char *token) {
    uint32_t rune;
    char    *p = token + str_to_rune(token, &rune);

    if (*p != ':') {
        return 1;
    }

    p++;

    enum resize_direction direction;
    switch (*p) {
    case 'h':
        direction = RESIZE_HORIZONTAL;
        break;

    case 'v':
        direction = RESIZE_VERTICAL;
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
    param->direction  = direction;

    return 0;
}

int load_resize_parameters(struct resize_parameter **params, char *s) {
    int  s_len = strlen(s);
    char buf[s_len + 1];
    strcpy(buf, s);
    buf[s_len] = '\0';

    size_t                   cap = 8;
    struct resize_parameter *params_buf =
        malloc(sizeof(struct resize_parameter) * cap);
    static const char delims[] = " \t\n";
    char             *strtok_p;

    int   i     = 0;
    char *token = strtok_r(buf, delims, &strtok_p);
    while (token != NULL) {
        if (cap < i + 1) {
            cap *= 1.5;
            params_buf =
                realloc(params_buf, sizeof(struct resize_parameter) * cap);
        }

        if (_load_resize_param_from_token(&params_buf[i], token) != 0) {
            LOG_ERR("Could not parse token `%s`.", token);
            free(params_buf);
            return -1;
        }

        i++;
        token = strtok_r(NULL, delims, &strtok_p);
    }

    *params = params_buf;
    return i;
}

#define NO_GUIDE -1

int resize_parameters_compute_guides(
    struct resize_parameter *params, size_t len, struct focused_window *fw
) {

    for (int i = 0; i < len; i++) {
        struct resize_parameter *param = &params[i];

        int32_t min_limit;
        int32_t max_limit;
        int32_t resizable_min_dir;
        int32_t resizable_max_dir;
        int32_t size;
        int32_t pos;

        if (param->direction == RESIZE_HORIZONTAL) {
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

            guide_pos[0] = new_size + pos;
            guide_pos[1] = NO_GUIDE;

            if (guide_pos[0] > max_limit) {
                param->applicable = false;
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

        param->size = new_size;

        if (param->direction == RESIZE_HORIZONTAL) {
            /*
                  +      +-------------+      +  fw->rect.y
                  |      |             |      |
                  |      |             |      |
                  |      |             |      |
                  +      +-------------+      +  fw->rect.y + fw.rect.h

                  ^                           ^
                  guide_pos[0]                guide_pos[1]
             */
            param->guides[0].x1 = guide_pos[0];
            param->guides[0].x2 = guide_pos[0];
            param->guides[0].y1 = fw->rect.y;
            param->guides[0].y2 = fw->rect.y + fw->rect.h;

            if (guide_pos[1] == NO_GUIDE) {
                param->guides[1].x1 = NO_GUIDE;
            } else {
                param->guides[1].x1 = guide_pos[1];
                param->guides[1].x2 = guide_pos[1];
                param->guides[1].y1 = fw->rect.y;
                param->guides[1].y2 = fw->rect.y + fw->rect.h;
            }

        } else { // RESIZE_VERTICAL
            /*
                     +-------------+ guide_pos[0]


                     +-------------+
                     |             |
                     |             |
                     |             |
                     +-------------+


                     +-------------+ guide_pos[1]

                     ^             ^
                     fw->rect.x    fw->rect.x + fw->rect.w
            */

            param->guides[0].x1 = fw->rect.x;
            param->guides[0].x2 = fw->rect.x + fw->rect.w;
            param->guides[0].y1 = guide_pos[0];
            param->guides[0].y2 = guide_pos[0];

            if (guide_pos[1] == NO_GUIDE) {
                param->guides[1].x1 = NO_GUIDE;
            } else {
                param->guides[1].x1 = fw->rect.x;
                param->guides[1].x2 = fw->rect.x + fw->rect.w;
                param->guides[1].y1 = guide_pos[1];
                param->guides[1].y2 = guide_pos[1];
            }
        }

        param->applicable = true;
    }

    return 0;
}

void log_resize_params(struct resize_parameter *params, int len) {
    for (int i = 0; i < len; i++) {
        LOG_INFO("params[%d]", i);
        LOG_INFO(" .symbol = %d", params[i].symbol);
        LOG_INFO(
            " .direction = %s",
            params[i].direction == RESIZE_VERTICAL ? "VERTICAL" : "HORIZONTAL"
        );
        LOG_INFO(" .value = %d", params[i].value);
        LOG_INFO(" .relative = %s", params[i].relative ? "true" : "false");
        LOG_INFO(" .percentage = %s", params[i].percentage ? "true" : "false");
        LOG_INFO(" .applicable = %s", params[i].applicable ? "true" : "false");

        if (params[i].applicable) {
            LOG_INFO("  .size = %d", params[i].size);
            LOG_INFO(
                "  .guides[0] = %d,%d %d,%d", params[i].guides[0].x1,
                params[i].guides[0].y1, params[i].guides[0].x2,
                params[i].guides[0].y2
            );
            if (params[i].guides[1].x1 < 0) {
                LOG_INFO("  .guides[1] = n/a");
            } else {
                LOG_INFO(
                    "  .guides[1] = %d,%d %d,%d", params[i].guides[1].x1,
                    params[i].guides[1].y1, params[i].guides[1].x2,
                    params[i].guides[1].y2
                );
            }
        }
    }
}
