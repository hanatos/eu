#include "eu.h"

eu_t eu;

static inline void pointer_to_image(float *x, float *y)
{
  *x = CLAMP((eu.pointer.x - eu.conv.roi_out.x) / eu.conv.roi.scale + eu.conv.roi.x, 0, eu.conv.roi.w-1);
  *y = CLAMP(((eu.display->height - eu.pointer.y - 1) - eu.conv.roi_out.y) / eu.conv.roi.scale + eu.conv.roi.y, 0, eu.conv.roi.h-1);
}

static inline void offset_image(float x, float y)
{
  // center image roi around given x and y in image coordinates
  eu.conv.roi.x = MAX(0, x - eu.conv.roi_out.w/(eu.conv.roi.scale * 2));
  eu.conv.roi.y = MAX(0, y - eu.conv.roi_out.h/(eu.conv.roi.scale * 2));
}

int onKeyDown(keycode_t key)
{
  char title[256];
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
      return 1;
    case KeyTwo: // scale to fit
      eu.conv.roi.scale = fminf(eu.display->width/(float)fileinput_width(eu.file+eu.current_file),
          eu.display->height/(float)fileinput_height(eu.file+eu.current_file));
      eu.conv.roi.x = eu.conv.roi.y = 0;
      return 1;
    case KeyThree: // scale to fill
      eu.conv.roi.scale = fmaxf(eu.display->width/(float)fileinput_width(eu.file+eu.current_file),
          eu.display->height/(float)fileinput_height(eu.file+eu.current_file));
      eu.conv.roi.x = eu.conv.roi.y = 0;
      return 1;
    case KeyR: // red channel
      if(eu.conv.channels == s_red)
        eu.conv.channels = s_rgb;
      else
        eu.conv.channels = s_red;
      return 1;
    case KeyG: // green channel
      if(eu.conv.channels == s_green)
        eu.conv.channels = s_rgb;
      else
        eu.conv.channels = s_green;
      return 1;
    case KeyB: // blue channel
      if(eu.conv.channels == s_blue)
        eu.conv.channels = s_rgb;
      else
        eu.conv.channels = s_blue;
      return 1;
    case KeyC: // all color channels
      eu.conv.channels = s_rgb;
      return 1;
    case KeyDown:
    case KeyRight:
      if(eu.current_file < eu.num_files-1)
      {
        eu.current_file++;
        snprintf(title, 256, "frame %04d/%04d", eu.current_file+1, eu.num_files);
        display_title(eu.display, title);
        return 1;
      }
      return 0;
    case KeyLeft:
    case KeyUp:
      if(eu.current_file > 0)
      {
        eu.current_file--;
        snprintf(title, 256, "frame %04d/%04d", eu.current_file+1, eu.num_files);
        display_title(eu.display, title);
        return 1;
      }
      return 0;
    default:
      return 0;
  }
}

int onMouseMove(mouse_t *mouse)
{
  eu.pointer = *mouse;
  return 0;
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
  if(argc < 2)
  {
    fprintf(stderr, "["PROG_NAME"] no input files, stop.\n");
    exit(1);
  }

  const int wd = 1024, ht = 576;
  eu_init(&eu, wd, ht, argc, arg);
  eu.display->onKeyDown = onKeyDown;
  eu.display->onMouseMove = onMouseMove;

  int ret = 1;
  while(1)
  {
    if(ret)
    {
      // update buffer from out-of-core storage
      fileinput_grab(eu.file+eu.current_file, &eu.conv, eu.pixels);
      // show on screen
      display_update(eu.display, eu.pixels);
    }
    // get user input, wait for it if need be.
    int ret = display_wait_event(eu.display);
    if(ret < 0) break;
  }

  eu_cleanup(&eu);

  exit(0);
}
