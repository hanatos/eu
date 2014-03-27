
#include "fileinput.h"
#include "display.h"

#include <stdlib.h>


// fastest to type while the right hand is on the mouse, copy/pasting the file name.
// this is true on a dvorak keyboard, where these are on the left homerow, middle + index fingers:
#define PROG_NAME "eu"


typedef struct eu_t
{
  fileinput_conversion_t conv;
}
eu_t;

// global state:
eu_t eu;

/*
  void (*onKeyDown)(keycode_t);
  void (*onKeyPressed)(keycode_t);
  void (*onKeyUp)(keycode_t);
  void (*onMouseButtonUp)(mouse_t);
  void (*onMouseButtonDown)(mouse_t);
  void (*onMouseMove)(mouse_t);
  void (*onClose)();
  */


int main(int argc, char *arg[])
{
  const int wd = 1024, ht = 576;
  display_t *display = display_open("viewer", wd, ht);

  fileinput_t file;
  if(argc < 2)
  {
    fprintf(stderr, "["PROG_NAME"] no input files, stop.\n");
    exit(1);
  }
  if(fileinput_open(&file, arg[1]))
  {
    fprintf(stderr, "["PROG_NAME"] could not open file `%s'\n", arg[1]);
    exit(2);
  }

  eu.conv.roi.x = 0;
  eu.conv.roi.y = 0;
  eu.conv.roi.w = wd;
  eu.conv.roi.h = ht;
  eu.conv.roi.scale = 1.0f;
  eu.conv.colorin = s_passthrough;
  eu.conv.colorout = s_passthrough;

  uint8_t *pixels = (uint8_t *)aligned_alloc(16, wd*ht*3);

  while(1)
  {
    // get user input, wait for it if need be.
    if(display_wait_event(display)) break;
    // update buffer from out-of-core storage
    fileinput_grab(&file, &eu.conv, pixels);
    // show on screen
    display_update(display, pixels);
  }

  fileinput_close(&file);
  display_close(display);
  free(pixels);

  exit(0);
}
