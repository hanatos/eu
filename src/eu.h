#pragma once

#include "fileinput.h"
#include "display.h"

#include <stdlib.h>
#include <math.h>


// fastest to type while the right hand is on the mouse, copy/pasting the file name.
// this is true on a dvorak keyboard, where these are on the left homerow, middle + index fingers:
#define PROG_NAME "eu"


typedef struct eu_t
{
  display_t *display;
  fileinput_t file;
  fileinput_conversion_t conv;

  uint8_t *pixels;

  mouse_t pointer;
}
eu_t;

static inline void eu_init(eu_t *eu, const char *title, int wd, int ht)
{
  eu->display = display_open(title, wd, ht);

  // default input to display conversion:
  eu->conv.roi.x = 0;
  eu->conv.roi.y = 0;
  eu->conv.roi.w = fileinput_width(&eu->file);
  eu->conv.roi.h = fileinput_height(&eu->file);
  eu->conv.roi.scale = 1.0f;
  eu->conv.roi_out.x = 0;
  eu->conv.roi_out.y = 0;
  eu->conv.roi_out.w = wd;
  eu->conv.roi_out.h = ht;
  eu->conv.roi_out.scale = 0.0f; // not used, invalidate
  eu->conv.colorin = s_passthrough;
  eu->conv.colorout = s_passthrough;
  eu->conv.curve = s_none;
  eu->conv.channels = s_rgb;

  eu->pixels = (uint8_t *)aligned_alloc(16, wd*ht*3);
}

static inline void eu_cleanup(eu_t *eu)
{
  fileinput_close(&eu->file);
  display_close(eu->display);
  free(eu->pixels);
}

