#include "eu.h"

eu_t eu;

static inline void pointer_to_image(float *x, float *y)
{
  *x = (eu.pointer.x - eu.conv.roi_out.x) / eu.conv.roi.scale + eu.conv.roi.x;
  *y = (eu.pointer.y - eu.conv.roi_out.y) / eu.conv.roi.scale + eu.conv.roi.y;
}

static inline void offset_image(float x, float y)
{
  // center image roi around given x and y in image coordinates
  eu.conv.roi.x = MAX(0, x - eu.conv.roi_out.w/(eu.conv.roi.scale * 2));
  eu.conv.roi.y = MAX(0, y - eu.conv.roi_out.h/(eu.conv.roi.scale * 2));
}

void onKeyDown(keycode_t key)
{
  float x, y;
  pointer_to_image(&x, &y);
  switch(key)
  {
    case KeyOne: // toggle 1:1 and 1:2
      if(eu.conv.roi.scale == 1.0f)
        eu.conv.roi.scale = 2.0f;
      else 
        eu.conv.roi.scale = 1.0f;
      offset_image(x, y);
      break;
    case KeyTwo: // scale to fit
      eu.conv.roi.scale = fminf(eu.display->width/(float)fileinput_width(&eu.file),
          eu.display->height/(float)fileinput_height(&eu.file));
      eu.conv.roi.x = eu.conv.roi.y = 0;
      break;
    case KeyThree: // scale to fill
      eu.conv.roi.scale = fmaxf(eu.display->width/(float)fileinput_width(&eu.file),
          eu.display->height/(float)fileinput_height(&eu.file));
      eu.conv.roi.x = eu.conv.roi.y = 0;
      break;
    case KeyR: // red channel
      if(eu.conv.channels == s_red)
        eu.conv.channels = s_rgb;
      else
        eu.conv.channels = s_red;
      break;
    case KeyG: // green channel
      if(eu.conv.channels == s_green)
        eu.conv.channels = s_rgb;
      else
        eu.conv.channels = s_green;
      break;
    case KeyB: // blue channel
      if(eu.conv.channels == s_blue)
        eu.conv.channels = s_rgb;
      else
        eu.conv.channels = s_blue;
      break;
    case KeyC: // all color channels
      eu.conv.channels = s_rgb;
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
  eu_init(&eu, "viewer", wd, ht);
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

  while(1)
  {
    // get user input, wait for it if need be.
    int ret = display_wait_event(eu.display);
    if(ret < 0) break;
    if(ret)
    {
      // update buffer from out-of-core storage
      fileinput_grab(&eu.file, &eu.conv, eu.pixels);
      // show on screen
      display_update(eu.display, eu.pixels);
    }
  }

  eu_cleanup(&eu);

  exit(0);
}
