
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

  mouse_t pointer;
}
eu_t;

// global state:
eu_t eu;

void onKeyDown(keycode_t key)
{
  switch(key)
  {
    case KeyOne:
      if(eu.conv.roi.scale == 1.0f)
        eu.conv.roi.scale = 2.0f;
      else 
        eu.conv.roi.scale = 1.0f;
      break;
    case KeyTwo:
      eu.conv.roi.scale = fminf(eu.display->width/(float)fileinput_width(&eu.file),
          eu.display->height/(float)fileinput_height(&eu.file));
      break;
    case KeyThree:
      eu.conv.roi.scale = fmaxf(eu.display->width/(float)fileinput_width(&eu.file),
          eu.display->height/(float)fileinput_height(&eu.file));
      break;
    case KeyR:
      if(eu.conv.channels == s_red)
        eu.conv.channels = s_rgb;
      else
        eu.conv.channels = s_red;
      break;
    case KeyG:
      if(eu.conv.channels == s_green)
        eu.conv.channels = s_rgb;
      else
        eu.conv.channels = s_green;
      break;
    case KeyB:
      if(eu.conv.channels == s_blue)
        eu.conv.channels = s_rgb;
      else
        eu.conv.channels = s_blue;
      break;
    default:
      break;
  }
}

void onMouseMove(mouse_t *mouse)
{
  eu.pointer = *mouse;
}
/*
  void (*onKeyPressed)(keycode_t);
  void (*onKeyUp)(keycode_t);
  void (*onMouseButtonUp)(mouse_t);
  void (*onMouseButtonDown)(mouse_t);
  void (*onClose)();
  */


int main(int argc, char *arg[])
{
  const int wd = 1024, ht = 576;
  eu.display = display_open("viewer", wd, ht);
  eu.display->onKeyDown = onKeyDown;
  eu.display->onMouseMove = onMouseMove;

  if(argc < 2)
  {
    fprintf(stderr, "["PROG_NAME"] no input files, stop.\n");
    exit(1);
  }
  if(fileinput_open(&eu.file, arg[1]))
  {
    fprintf(stderr, "["PROG_NAME"] could not open file `%s'\n", arg[1]);
    exit(2);
  }

  eu.conv.roi.x = 0;
  eu.conv.roi.y = 0;
  eu.conv.roi.w = fileinput_width(&eu.file);
  eu.conv.roi.h = fileinput_height(&eu.file);
  eu.conv.roi.scale = 1.0f;
  eu.conv.roi_out.x = 0;
  eu.conv.roi_out.y = 0;
  eu.conv.roi_out.w = wd;
  eu.conv.roi_out.h = ht;
  eu.conv.roi_out.scale = 0.0f; // not used, invalidate
  eu.conv.colorin = s_passthrough;
  eu.conv.colorout = s_passthrough;
  eu.conv.curve = s_none;
  eu.conv.channels = s_rgb;

  uint8_t *pixels = (uint8_t *)aligned_alloc(16, wd*ht*3);

  while(1)
  {
    // get user input, wait for it if need be.
    int ret = display_wait_event(eu.display);
    if(ret < 0) break;
    if(ret)
    {
      // update buffer from out-of-core storage
      fileinput_grab(&eu.file, &eu.conv, pixels);
      // show on screen
      display_update(eu.display, pixels);
    }
  }

  fileinput_close(&eu.file);
  display_close(eu.display);
  free(pixels);

  exit(0);
}
