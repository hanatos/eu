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
  int num_files;
  int current_file;
  fileinput_t *file;
  fileinput_conversion_t conv;

  uint8_t *pixels;

  mouse_t pointer;
  mouse_t pointer_button;
  float gui_x_button, gui_y_button;
  int gui_state;
}
eu_t;

static inline void eu_init(eu_t *eu, int wd, int ht, int argc, char *arg[])
{
  eu->display = display_open(PROG_NAME, wd, ht);

  eu->gui_state = 0;

  eu->conv.verbosity = s_silent;

  // default input to display conversion:
  eu->conv.roi.x = 0;
  eu->conv.roi.y = 0;
  eu->conv.roi.scale = 1.0f;
  eu->conv.roi_out.x = 0;
  eu->conv.roi_out.y = 0;
  eu->conv.roi_out.w = wd;
  eu->conv.roi_out.h = ht;
  eu->conv.roi_out.scale = 0.0f; // not used, invalidate
  eu->conv.colorin = s_xyz;
  eu->conv.colorout = s_srgb;
  eu->conv.gamutmap = s_gamut_clamp;
  eu->conv.curve = s_none;
  eu->conv.channels = s_rgb;

  eu->num_files = argc-1;
  eu->file = (fileinput_t *)aligned_alloc(16, (argc-1)*sizeof(fileinput_t));
  for(int k=1;k<argc;k++)
  {
    if(fileinput_open(eu->file+k-1, arg[k]))
    {
      // just go on with empty frames.
      fprintf(stderr, "[eu_init] could not open file `%s'\n", arg[k]);
    }
  }

  // use dimensions of first file
  eu->conv.roi.w = fileinput_width(eu->file);
  eu->conv.roi.h = fileinput_height(eu->file);
  eu->current_file = 0;

  eu->pixels = (uint8_t *)aligned_alloc(16, wd*ht*3);
}

static inline void eu_cleanup(eu_t *eu)
{
  for(int k=0;k<eu->num_files;k++)
    fileinput_close(eu->file+k);
  display_close(eu->display);
  free(eu->pixels);
  free(eu->file);
}

