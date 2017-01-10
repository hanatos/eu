#pragma once

#include <stdio.h>
#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>

// get event declarations
#include "display_common.h"

typedef enum display_mod_state_t
{
  s_display_shift = ShiftMask,
  s_display_lock = LockMask,
  s_display_control = ControlMask,
  s_display_mod1 = Mod1Mask,
  s_display_mod2 = Mod2Mask,
  s_display_mod3 = Mod3Mask,
  s_display_mod4 = Mod4Mask,
  s_display_mod5 = Mod5Mask,
}
display_mod_state_t;

typedef struct display_t
{
	int isShuttingDown;
  int width;
  int height;
  char msg[40096];
  int msg_len;
  int msg_x, msg_y;
  
  // x11 stuff
	Atom wmProtocols;
	Atom wmDeleteWindow;
  Display* display;
  Window window;
  XImage* image;
  GC gc;
  uint32_t *buffer;
  int bit_depth;

  display_mod_state_t mod_state;
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
