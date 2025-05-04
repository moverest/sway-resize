#ifndef __STATE_H_INCLUDED__
#define __STATE_H_INCLUDED__

#include "fractional-scale-v1-client-protocol.h"
#include "resize_params.h"
#include "surface_buffer.h"
#include "viewporter-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon.h>

struct output {
    struct wl_list           link; // type: struct output
    struct wl_output        *wl_output;
    struct zxdg_output_v1   *xdg_output;
    char                    *name;
    int32_t                  scale;
    int32_t                  width;
    int32_t                  height;
    int32_t                  x;
    int32_t                  y;
    enum wl_output_transform transform;
};

struct seat {
    struct wl_list      link; // type: struct seat
    struct wl_seat     *wl_seat;
    struct wl_keyboard *wl_keyboard;
    struct xkb_context *xkb_context;
    struct xkb_keymap  *xkb_keymap;
    struct xkb_state   *xkb_state;
    struct state       *state;
};

struct state {
    struct wl_display                     *wl_display;
    struct wl_registry                    *wl_registry;
    struct wl_compositor                  *wl_compositor;
    struct wl_shm                         *wl_shm;
    struct zwlr_layer_shell_v1            *wl_layer_shell;
    struct wp_viewporter                  *wp_viewporter;
    struct wp_viewport                    *wp_viewport;
    struct wp_fractional_scale_manager_v1 *fractional_scale_mgr;
    struct surface_buffer_pool             surface_buffer_pool;
    struct wl_surface                     *wl_surface;
    struct wl_callback                    *wl_surface_callback;
    struct zwlr_layer_surface_v1          *wl_layer_surface;
    struct zxdg_output_manager_v1         *xdg_output_manager;
    struct wl_list                         outputs;
    struct wl_list                         seats;
    struct output                         *current_output;
    uint32_t                               scale_120;
    uint32_t                               surface_height;
    uint32_t                               surface_width;
    bool                                   running;
    bool                                   surface_configured;
    struct resize_parameter               *resize_params;
    int                                    num_resize_params;
    struct focused_window                  focused_window;
    int                                    selected_resize;
};

#endif
