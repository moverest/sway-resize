#include "sway_win.h"

#include "log.h"

#include <jansson.h>
#include <string.h>

enum orientation {
    ORIENTATION_NONE       = 0,
    ORIENTATION_HORIZONTAL = 1,
    ORIENTATION_VERTICAL   = 2,
};

static enum orientation _get_orientation(json_t *tree) {
    json_t *field = json_object_get(tree, "orientation");
    if (field == NULL) {
        return ORIENTATION_NONE;
    }

    if (!json_is_string(field)) {
        return ORIENTATION_NONE;
    }

    const char *value = json_string_value(field);
    if (strcmp(value, "horizontal") == 0) {
        return ORIENTATION_HORIZONTAL;
    }

    if (strcmp(value, "vertical") == 0) {
        return ORIENTATION_VERTICAL;
    }

    return ORIENTATION_NONE;
}

#define JSON_OBJ_GET_INTEGER(node, var, key)               \
    json_t *var##json = json_object_get(node, key);        \
    if (var##json == NULL) {                               \
        LOG_ERR("Object field '%s' not found.", key);      \
        return -1;                                         \
    }                                                      \
    if (!json_is_integer(var##json)) {                     \
        LOG_ERR("Object field '%s' not an integer.", key); \
        return -1;                                         \
    }                                                      \
    int var = json_integer_value(var##json);

static int _get_rect(json_t *node, struct rect *rect, char *field) {
    json_t *rect_node = json_object_get(node, field);
    if (!json_is_object(rect_node)) {
        return -1;
    }

    JSON_OBJ_GET_INTEGER(rect_node, x, "x");
    JSON_OBJ_GET_INTEGER(rect_node, y, "y");
    JSON_OBJ_GET_INTEGER(rect_node, w, "width");
    JSON_OBJ_GET_INTEGER(rect_node, h, "height");

    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;

    return 0;
}

static int _find_focused_window_rec(struct focused_window *fw, json_t *tree) {

    if (!json_is_object(tree)) {
        LOG_ERR("Node is not an object.");
        return -1;
    }

    json_t     *type_node = json_object_get(tree, "type");
    const char *type      = NULL;
    if (type_node != NULL && json_is_string(type_node)) {
        type = json_string_value(type_node);
        if (strcmp("output", type) == 0) {
            json_t *name = json_object_get(tree, "name");
            if (name != NULL && json_is_string(name)) {
                fw->output = json_string_value(name);
            }

            if (_get_rect(tree, &fw->output_rect, "rect") == 0) {
                fw->resize_top_limit    = fw->output_rect.y;
                fw->resize_bottom_limit = fw->output_rect.y + fw->output_rect.h;
                fw->resize_left_limit   = fw->output_rect.x;
                fw->resize_right_limit  = fw->output_rect.x + fw->output_rect.w;
            }
        }
    }

    json_t *focused = json_object_get(tree, "focused");
    if (focused != NULL && json_is_true(focused)) {
        JSON_OBJ_GET_INTEGER(tree, id, "id");
        fw->id = id;
        return _get_rect(tree, &fw->rect, "rect");
    }

    json_t *focus = json_object_get(tree, "focus");
    if (focus == NULL) {
        return -1;
    }

    if (!json_is_array(focus)) {
        return -1;
    }

    json_t *first_focus = json_array_get(focus, 0);
    if (first_focus == NULL) {
        return -1;
    }
    if (!json_is_integer(first_focus)) {
        return -1;
    }
    int focused_id = json_integer_value(first_focus);

    fw->floating  = false;
    json_t *nodes = json_object_get(tree, "nodes");
    if (nodes == NULL) {
        LOG_ERR("Could not find 'nodes' field.");
        return -1;
    }

    if (!json_is_array(nodes)) {
        LOG_ERR("'nodes' field is not an array.");
        return -1;
    }

    int node_count = json_array_size(nodes);
    for (int i = 0; i < node_count; i++) {
        json_t *node = json_array_get(nodes, i);
        if (node == NULL) {
            LOG_ERR("'nodes[%d]' not found.", i);
            return -1;
        }
        if (!json_is_object(node)) {
            LOG_ERR("'nodes[%d]' is not an object.", i);
            return -1;
        }

        JSON_OBJ_GET_INTEGER(node, id, "id");
        if (id == focused_id) {
            if (node_count > 1 && (type == NULL || (strcmp(type, "root")))) {
                struct rect container_rect;
                _get_rect(tree, &container_rect, "rect");

                switch (_get_orientation(tree)) {
                case ORIENTATION_HORIZONTAL:
                    fw->resize_left       = i > 0;
                    fw->resize_right      = i < node_count - 1;
                    fw->resize_left_limit = container_rect.x;
                    fw->resize_right_limit =
                        container_rect.x + container_rect.w;
                    break;

                case ORIENTATION_VERTICAL:
                    fw->resize_top       = i > 0;
                    fw->resize_bottom    = i < node_count - 1;
                    fw->resize_top_limit = container_rect.y;
                    fw->resize_bottom_limit =
                        container_rect.y + container_rect.h;
                    break;

                default:
                    break;
                }
            }

            return _find_focused_window_rec(fw, node);
        }
    }

    fw->floating           = true;
    json_t *floating_nodes = json_object_get(tree, "floating_nodes");
    if (floating_nodes == NULL) {
        LOG_ERR("Could not find 'floating_nodes' field.");
        return -1;
    }

    if (!json_is_array(nodes)) {
        LOG_ERR("'floating_nodes' field is not an array.");
        return -1;
    }

    int floating_nodes_count = json_array_size(nodes);
    for (int i = 0; i < floating_nodes_count; i++) {
        json_t *node = json_array_get(floating_nodes, i);
        if (node == NULL) {
            LOG_ERR("'floating_nodes[%d]' not found.", i);
            return -1;
        }
        if (!json_is_object(node)) {
            LOG_ERR("'node[%d]' is not an object.", i);
            return -1;
        }

        JSON_OBJ_GET_INTEGER(node, id, "id");
        if (id == focused_id) {
            fw->resize_left         = true;
            fw->resize_right        = true;
            fw->resize_top          = true;
            fw->resize_bottom       = true;
            fw->resize_top_limit    = fw->output_rect.y;
            fw->resize_bottom_limit = fw->output_rect.y + fw->output_rect.h;
            fw->resize_left_limit   = fw->output_rect.x;
            fw->resize_right_limit  = fw->output_rect.x + fw->output_rect.w;

            return _find_focused_window_rec(fw, node);
        }
    }

    return -1;
}

int find_focused_window(struct focused_window *fw, json_t *tree) {
    fw->id                  = -1;
    fw->resize_bottom       = false;
    fw->resize_top          = false;
    fw->resize_left         = false;
    fw->resize_right        = false;
    fw->resize_bottom_limit = -1;
    fw->resize_top_limit    = -1;
    fw->resize_left_limit   = -1;
    fw->resize_right_limit  = -1;
    fw->rect.x              = 0;
    fw->rect.y              = 0;
    fw->rect.w              = 0;
    fw->rect.h              = 0;

    int err = _find_focused_window_rec(fw, tree);
    if (err) {
        return err;
    }

    if (fw->output != NULL) {
        // We kept a reference to the json_t value.
        fw->output = strdup(fw->output);
    }

    fw->rect.x -= fw->output_rect.x;
    fw->rect.y -= fw->output_rect.y;

    fw->resize_left_limit  -= fw->output_rect.x;
    fw->resize_right_limit -= fw->output_rect.x;

    fw->resize_bottom_limit -= fw->output_rect.y;
    fw->resize_top_limit    -= fw->output_rect.y;

    return 0;
}

void log_focused_window(struct focused_window *fw) {
    LOG_INFO("focused_window");
    LOG_INFO(" .id = %d", fw->id);
    LOG_INFO(" .output = %s", fw->output);
    LOG_INFO(
        " .resize_top_limit = %d (%s)", fw->resize_top_limit,
        fw->resize_top ? "resizable" : "fixed"
    );
    LOG_INFO(
        " .resize_bottom_limit = %d (%s)", fw->resize_bottom_limit,
        fw->resize_bottom ? "resizable" : "fixed"
    );
    LOG_INFO(
        " .resize_right_limit = %d (%s)", fw->resize_right_limit,
        fw->resize_right ? "resizable" : "fixed"
    );
    LOG_INFO(
        " .resize_left_limit = %d (%s)", fw->resize_left_limit,
        fw->resize_left ? "resizable" : "fixed"
    );
    LOG_INFO(
        " .rect = %dx%d+%d+%d", fw->rect.w, fw->rect.h, fw->rect.x, fw->rect.y
    );
}
