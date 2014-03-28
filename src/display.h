#pragma once

#include <stdio.h>

// get event declarations
#include "display_common.h"

typedef struct display_t
{
	int isShuttingDown;
  int width;
  int height;
  char msg[256];
  int msg_len;
  int msg_x, msg_y;

  // return value: 0 nothing 1 redraw -1 quit
  int (*onKeyDown)(keycode_t);
  int (*onKeyPressed)(keycode_t);
  int (*onKeyUp)(keycode_t);
  int (*onMouseButtonUp)(mouse_t*);
  int (*onMouseButtonDown)(mouse_t*);
  int (*onMouseMove)(mouse_t*);
  int (*onClose)();
}
display_t;

display_t *display_open(const char title[], int width, int height);
int display_update(display_t *d, uint8_t *pixels);
void display_close(display_t *d);
int display_pump_events(display_t *d);
int display_wait_event(display_t *d);
void display_print(display_t *d, const int px, const int py, const char* msg, ...);
static inline void display_print_usage() {}
void display_title(display_t *d, const char *title);
