#include "fractional-scale-v1-client-protocol.h"
#include "log.h"
#include "render.h"
#include "resize_params.h"
#include "state.h"
#include "surface_buffer.h"
#include "sway_ipc.h"
#include "sway_win.h"
#include "viewporter-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#include <cairo/cairo.h>
#include <getopt.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

static void send_frame(struct state *state) {
    int32_t scale_120 = state->scale_120;
    if (scale_120 == 0) {
        // Falling back to the output scale if fractional scale is not received.
        scale_120 =
            (state->current_output == NULL ? 1 : state->current_output->scale) *
            120;
    }

    struct surface_buffer *surface_buffer = get_next_buffer(
        state->wl_shm, &state->surface_buffer_pool,
        state->surface_width * scale_120 / 120,
        state->surface_height * scale_120 / 120
    );
    if (surface_buffer == NULL) {
        return;
    }
    surface_buffer->state = SURFACE_BUFFER_BUSY;

    cairo_t *cairo = surface_buffer->cairo;
    cairo_identity_matrix(cairo);
    cairo_scale(cairo, scale_120 / 120.0, scale_120 / 120.0);

    render(state, cairo);

    wl_surface_set_buffer_scale(state->wl_surface, 1);

    wl_surface_attach(state->wl_surface, surface_buffer->wl_buffer, 0, 0);
    wp_viewport_set_destination(
        state->wp_viewport, state->surface_width, state->surface_height
    );
    wl_surface_damage(
        state->wl_surface, 0, 0, state->surface_width, state->surface_height
    );
    wl_surface_commit(state->wl_surface);
}

static void surface_callback_done(
    void *data, struct wl_callback *callback, uint32_t callback_data
) {
    struct state *state = data;
    send_frame(state);

    wl_callback_destroy(state->wl_surface_callback);
    state->wl_surface_callback = NULL;
}

const struct wl_callback_listener surface_callback_listener = {
    .done = surface_callback_done,
};

static void request_frame(struct state *state) {
    if (state->wl_surface_callback != NULL) {
        return;
    }

    state->wl_surface_callback = wl_surface_frame(state->wl_surface);
    wl_callback_add_listener(
        state->wl_surface_callback, &surface_callback_listener, state
    );
    wl_surface_commit(state->wl_surface);
}

static void noop() {}

static void handle_keyboard_keymap(
    void *data, struct wl_keyboard *keyboard, uint32_t format, int fd,
    uint32_t size
) {
    struct seat *seat = data;
    if (seat->xkb_state != NULL) {
        xkb_state_unref(seat->xkb_state);
        seat->xkb_state = NULL;
    }
    if (seat->xkb_keymap != NULL) {
        xkb_keymap_unref(seat->xkb_keymap);
        seat->xkb_keymap = NULL;
    }

    switch (format) {
    case WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP:
        seat->xkb_keymap = xkb_keymap_new_from_names(
            seat->xkb_context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS
        );
        break;

    case WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1:;
        void *buffer = mmap(NULL, size - 1, PROT_READ, MAP_PRIVATE, fd, 0);
        if (buffer == MAP_FAILED) {
            LOG_ERR("Could not mmap keymap data.");
            return;
        }

        seat->xkb_keymap = xkb_keymap_new_from_buffer(
            seat->xkb_context, buffer, size - 1, XKB_KEYMAP_FORMAT_TEXT_V1,
            XKB_KEYMAP_COMPILE_NO_FLAGS
        );

        munmap(buffer, size - 1);
        close(fd);
        break;
    }

    seat->xkb_state = xkb_state_new(seat->xkb_keymap);
}

static void handle_keyboard_key(
    void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time,
    uint32_t key, uint32_t key_state
) {
    struct seat  *seat  = data;
    struct state *state = seat->state;
    char          text[64];

    const xkb_keycode_t key_code = key + 8;
    const xkb_keysym_t  key_sym =
        xkb_state_key_get_one_sym(seat->xkb_state, key_code);
    xkb_keysym_to_utf8(key_sym, text, sizeof(text));

    if (key_state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (key_sym == XKB_KEY_Escape) {
            state->running = false;
        }

        if (text[0] == '\0') {
            return;
        }

        uint32_t rune;
        str_to_rune(text, &rune);

        state->selected_resize = find_resize_param_by_symbol(
            state->resize_params, rune, &state->resize_direction
        );
        if (state->selected_resize != NULL) {
            state->running = false;
        }
    }
}

static void handle_keyboard_modifiers(
    void *data, struct wl_keyboard *keyboard, uint32_t serial,
    uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked,
    uint32_t group
) {
    struct seat *seat = data;
    xkb_state_update_mask(
        seat->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group
    );
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
    .keymap      = handle_keyboard_keymap,
    .enter       = noop,
    .leave       = noop,
    .key         = handle_keyboard_key,
    .modifiers   = handle_keyboard_modifiers,
    .repeat_info = noop,
};

static void handle_seat_capabilities(
    void *data, struct wl_seat *wl_seat, uint32_t capabilities
) {
    struct seat *seat = data;
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
        seat->wl_keyboard = wl_seat_get_keyboard(seat->wl_seat);
        wl_keyboard_add_listener(
            seat->wl_keyboard, &wl_keyboard_listener, data
        );
    }
}

const struct wl_seat_listener wl_seat_listener = {
    .name         = noop,
    .capabilities = handle_seat_capabilities,
};

static void free_seats(struct wl_list *seats) {
    struct seat *seat;
    struct seat *tmp;
    wl_list_for_each_safe (seat, tmp, seats, link) {
        if (seat->wl_keyboard != NULL) {
            wl_keyboard_destroy(seat->wl_keyboard);
        }

        if (seat->xkb_state != NULL) {
            xkb_state_unref(seat->xkb_state);
        }
        if (seat->xkb_keymap != NULL) {
            xkb_keymap_unref(seat->xkb_keymap);
        }
        xkb_context_unref(seat->xkb_context);

        wl_seat_destroy(seat->wl_seat);
        wl_list_remove(&seat->link);
        free(seat);
    }
}

static void free_outputs(struct wl_list *outputs) {
    struct output *output;
    struct output *tmp;
    wl_list_for_each_safe (output, tmp, outputs, link) {
        wl_output_destroy(output->wl_output);
        zxdg_output_v1_destroy(output->xdg_output);
        wl_list_remove(&output->link);
        free(output->name);
        free(output);
    }
}

static struct output *find_output_from_wl_output(
    struct wl_list *outputs, struct wl_output *wl_output
) {
    struct output *output;
    wl_list_for_each (output, outputs, link) {
        if (wl_output == output->wl_output) {
            return output;
        }
    }

    return NULL;
}

static void
handle_output_scale(void *data, struct wl_output *wl_output, int32_t scale) {
    struct output *output = data;
    output->scale         = scale;
}

static void handle_output_geometry(
    void *data, struct wl_output *wl_output, int32_t x, int32_t y,
    int32_t physical_width, int32_t physical_height, int32_t subpixel,
    const char *make, const char *model, int32_t transform
) {
    struct output *output = data;
    output->transform     = transform;
}

const static struct wl_output_listener output_listener = {
    .name        = noop,
    .geometry    = handle_output_geometry,
    .mode        = noop,
    .scale       = handle_output_scale,
    .description = noop,
    .done        = noop,
};

static void handle_xdg_output_logical_position(
    void *data, struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y
) {
    struct output *output = data;
    output->x             = x;
    output->y             = y;
}

static void handle_xdg_output_logical_size(
    void *data, struct zxdg_output_v1 *xdg_output, int32_t w, int32_t h
) {
    struct output *output = data;
    output->width         = w;
    output->height        = h;
}

static void handle_xdg_output_name(
    void *data, struct zxdg_output_v1 *xdg_output, const char *name
) {
    struct output *output = data;
    output->name          = strdup(name);
}

const static struct zxdg_output_v1_listener xdg_output_listener = {
    .logical_position = handle_xdg_output_logical_position,
    .logical_size     = handle_xdg_output_logical_size,
    .done             = noop,
    .name             = handle_xdg_output_name,
    .description      = noop,
};

static void load_xdg_outputs(struct state *state) {
    struct output *output;
    wl_list_for_each (output, &state->outputs, link) {
        output->xdg_output = zxdg_output_manager_v1_get_xdg_output(
            state->xdg_output_manager, output->wl_output
        );
        zxdg_output_v1_add_listener(
            output->xdg_output, &xdg_output_listener, output
        );
    }

    wl_display_roundtrip(state->wl_display);
}

static void handle_surface_enter(
    void *data, struct wl_surface *surface, struct wl_output *wl_output
) {
    struct state  *state = data;
    struct output *output =
        find_output_from_wl_output(&state->outputs, wl_output);
    state->current_output = output;
}

static const struct wl_surface_listener surface_listener = {
    .enter                      = handle_surface_enter,
    .leave                      = noop,
    .preferred_buffer_transform = noop,
    .preferred_buffer_scale     = noop,
};

static void handle_registry_global(
    void *data, struct wl_registry *registry, uint32_t name,
    const char *interface, uint32_t version
) {
    struct state *state = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->wl_compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface, 4);

    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->wl_shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);

    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->wl_layer_shell =
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 2);

    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        struct seat *seat = calloc(1, sizeof(struct seat));
        seat->wl_seat = wl_registry_bind(registry, name, &wl_seat_interface, 7);
        seat->wl_keyboard = NULL;
        seat->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        seat->xkb_state   = NULL;
        seat->xkb_keymap  = NULL;
        seat->state       = state;

        wl_seat_add_listener(seat->wl_seat, &wl_seat_listener, seat);
        wl_list_insert(&state->seats, &seat->link);

    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct wl_output *wl_output =
            wl_registry_bind(registry, name, &wl_output_interface, 3);
        struct output *output = calloc(1, sizeof(struct output));
        output->wl_output     = wl_output;
        output->scale         = 1;

        wl_output_add_listener(output->wl_output, &output_listener, output);
        wl_list_insert(&state->outputs, &output->link);

    } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        state->xdg_output_manager = wl_registry_bind(
            registry, name, &zxdg_output_manager_v1_interface, 2
        );

    } else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
        state->wp_viewporter =
            wl_registry_bind(registry, name, &wp_viewporter_interface, 1);
    } else if (strcmp(
                   interface, wp_fractional_scale_manager_v1_interface.name
               ) == 0) {
        state->fractional_scale_mgr = wl_registry_bind(
            registry, name, &wp_fractional_scale_manager_v1_interface, 1
        );
    }
}

const struct wl_registry_listener wl_registry_listener = {
    .global        = handle_registry_global,
    .global_remove = noop,
};

static void handle_layer_surface_configure(
    void *data, struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
    uint32_t width, uint32_t height
) {
    struct state *state   = data;
    state->surface_width  = width;
    state->surface_height = height;
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

    if (!state->surface_configured) {
        send_frame(state);
    }
    state->surface_configured = true;
}

static void handle_layer_surface_closed(
    void *data, struct zwlr_layer_surface_v1 *layer_surface
) {
    struct state *state = data;
    state->running      = false;
}

const struct zwlr_layer_surface_v1_listener wl_layer_surface_listener = {
    .configure = handle_layer_surface_configure,
    .closed    = handle_layer_surface_closed,
};

static void fractional_scale_preferred(
    void *data, struct wp_fractional_scale_v1 *fractional_scale, uint32_t scale
) {
    struct state *state     = data;
    int32_t       old_scale = state->scale_120;
    state->scale_120        = scale;

    if (old_scale != 0 && old_scale != scale) {
        request_frame(state);
    }
}

const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    .preferred_scale = fractional_scale_preferred,
};

static struct output *
find_output_by_name(struct state *state, const char *name) {
    struct output *output;
    wl_list_for_each (output, &state->outputs, link) {
        if (strcmp(output->name, name) == 0) {
            return output;
        }
    }

    return NULL;
}

static void print_usage() {
    puts("sway-resize [OPTION...]\n");

    puts(" -h, --help          show this help");
    puts(" -v, --version       print version and exit");
}

static void print_version() {
    printf("sway-resize %s\n", VERSION);
}

int main(int argc, char **argv) {
    struct state state = {
        .wl_display           = NULL,
        .wl_registry          = NULL,
        .wl_compositor        = NULL,
        .wl_shm               = NULL,
        .wl_layer_shell       = NULL,
        .wl_surface           = NULL,
        .wl_surface_callback  = NULL,
        .wl_layer_surface     = NULL,
        .surface_configured   = false,
        .wp_viewporter        = NULL,
        .fractional_scale_mgr = NULL,
        .running              = true,
        .scale_120            = 0,
        .selected_resize      = NULL,
    };

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"help-config", no_argument, 0, 'H'},
        {"version", no_argument, 0, 'v'},
        {"guides", required_argument, 0, 'g'},
    };

    char *guides_string = NULL;
    int   option_char   = 0;
    int   option_index  = 0;
    while ((option_char =
                getopt_long(argc, argv, "hvg:", long_options, &option_index)) !=
           EOF) {
        switch (option_char) {
        case 'h':
            print_usage();
            return 0;

        case 'v':
            print_version();
            return 0;

        case 'g':
            guides_string = strdup(optarg);
            break;

        default:
            LOG_ERR("Unknown argument.");
            return 1;
        }
    }

    wl_list_init(&state.outputs);
    wl_list_init(&state.seats);

    if (guides_string == NULL) {
        LOG_ERR("Guides need to be set with -g.");
        return 1;
    }

    state.resize_params = load_resize_parameters(guides_string);
    free(guides_string);

    if (state.resize_params == NULL) {
        LOG_ERR("Failed to load resize guides");
        return 1;
    }

    int sway_ipc_socket = sway_ipc_open_socket();
    if (sway_ipc_socket < 0) {
        LOG_ERR("Could not open Sway socket.");
        return 1;
    }

    sway_ipc_send(sway_ipc_socket, SWAY_MSG_GET_TREE, "", 0);
    struct sway_ipc_msg *sway_tree_msg = sway_ipc_recv(sway_ipc_socket);
    if (sway_tree_msg == NULL) {
        LOG_ERR("Could not receive tree message.");
        return 1;
    }

    json_error_t error;
    json_t      *sway_tree = json_loads(sway_tree_msg->payload, 0, &error);
    if (sway_tree == NULL) {
        LOG_ERR("Could not parse tree.");
        return 1;
    }

    free(sway_tree_msg);

    int err = find_focused_window(&state.focused_window, sway_tree);
    if (err) {
        LOG_ERR("Could not find focused window.");
        return 1;
    }
    json_decref(sway_tree);

    state.wl_display = wl_display_connect(NULL);
    if (state.wl_display == NULL) {
        LOG_ERR("Failed to connect to Wayland compositor.");
        return 1;
    }

    state.wl_registry = wl_display_get_registry(state.wl_display);
    if (state.wl_registry == NULL) {
        LOG_ERR("Failed to get Wayland registry.");
        return 1;
    }

    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
    wl_display_roundtrip(state.wl_display);

    if (state.wl_compositor == NULL) {
        LOG_ERR("Failed to get wl_compositor object.");
        return 1;
    }

    if (state.wl_shm == NULL) {
        LOG_ERR("Failed to get wl_shm object.");
        return 1;
    }

    if (state.wl_layer_shell == NULL) {
        LOG_ERR("Failed to get zwlr_layer_shell_v1 object.");
        return 1;
    }

    if (state.xdg_output_manager == NULL) {
        LOG_ERR("Failed to get xdg_output_manager object.");
        return 1;
    }

    if (state.wp_viewporter == NULL) {
        LOG_ERR("Failed to get wp_viewporter object.");
        return 1;
    }

    load_xdg_outputs(&state);

    // This round trip should load the keymap which is needed to determine the
    // home row keys.
    wl_display_roundtrip(state.wl_display);

    state.current_output =
        find_output_by_name(&state, state.focused_window.output);

    if (!state.current_output) {
        LOG_ERR("Could not find output '%s'.", state.focused_window.output);
        return 1;
    }

    log_focused_window(&state.focused_window);

    resize_parameters_compute_guides(
        state.resize_params, &state.focused_window
    );
    log_resize_params(state.resize_params);

    surface_buffer_pool_init(&state.surface_buffer_pool);

    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
    wl_surface_add_listener(state.wl_surface, &surface_listener, &state);
    state.wl_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        state.wl_layer_shell, state.wl_surface,
        state.current_output == NULL ? NULL : state.current_output->wl_output,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "selection"
    );
    zwlr_layer_surface_v1_add_listener(
        state.wl_layer_surface, &wl_layer_surface_listener, &state
    );
    zwlr_layer_surface_v1_set_exclusive_zone(state.wl_layer_surface, -1);
    zwlr_layer_surface_v1_set_anchor(
        state.wl_layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                    ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                                    ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                    ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
    );
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        state.wl_layer_surface, true
    );

    struct wp_fractional_scale_v1 *fractional_scale = NULL;
    if (state.fractional_scale_mgr) {
        fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(
            state.fractional_scale_mgr, state.wl_surface
        );
        wp_fractional_scale_v1_add_listener(
            fractional_scale, &fractional_scale_listener, &state
        );
    }

    state.wp_viewport =
        wp_viewporter_get_viewport(state.wp_viewporter, state.wl_surface);

    wl_surface_commit(state.wl_surface);
    while (state.running && wl_display_dispatch(state.wl_display)) {}

    zwlr_layer_surface_v1_destroy(state.wl_layer_surface);
    wl_surface_destroy(state.wl_surface);

    surface_buffer_pool_destroy(&state.surface_buffer_pool);
    wl_display_roundtrip(state.wl_display);

    free_seats(&state.seats);
    free_outputs(&state.outputs);

    if (state.fractional_scale_mgr) {
        wp_fractional_scale_v1_destroy(fractional_scale);
        wp_fractional_scale_manager_v1_destroy(state.fractional_scale_mgr);
    }

    wp_viewporter_destroy(state.wp_viewporter);
    wl_shm_destroy(state.wl_shm);
    wl_compositor_destroy(state.wl_compositor);
    wl_registry_destroy(state.wl_registry);
    zwlr_layer_shell_v1_destroy(state.wl_layer_shell);

    wl_display_disconnect(state.wl_display);

    if (state.focused_window.output != NULL) {
        free((void *)state.focused_window.output);
    }

    if (state.selected_resize != NULL) {
        char cmd[256];
        snprintf(
            cmd, sizeof(cmd) - 1, "resize set %s %dpx",
            state.resize_direction == RESIZE_VERTICAL ? "height" : "width",
            state.selected_resize->size
        );
        cmd[sizeof(cmd) - 1] = '\0';

        sway_ipc_send(sway_ipc_socket, SWAY_MSG_RUN_COMMAND, cmd, strlen(cmd));
        struct sway_ipc_msg *res = sway_ipc_recv(sway_ipc_socket);
        puts(res->payload);
        free(res);
    }

    close(sway_ipc_socket);

    return 0;
}
