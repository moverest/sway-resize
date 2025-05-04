#ifndef __RENDER_H_INCLUDED__
#define __RENDER_H_INCLUDED__

#include "state.h"

#include <cairo/cairo.h>

void render(struct state *state, cairo_t *cairo);

#endif
