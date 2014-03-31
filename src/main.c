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

int onKeyPressed(keycode_t key)
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

    case KeyE:
      eu.gui_state = 2;
      display_print(eu.display, 0, 0, "exposure %f", eu.conv.exposure);
      return 1;

    case KeyH:
      if(eu.display->msg_len > 0)
        display_print(eu.display, 0, 0, "");
      else
        display_print(eu.display, 0, 0, "[e]xpose [123] zoom [rgbc] channels [arrows] next/prev [h]elp [esc/q]uit [m] gamut map [p]rofile ");
      return 1;


    case KeyZero:
      if(eu.gui_state == 2)
      {
        eu.conv.exposure = 0.0f;
        eu.gui_state = 0;
        display_print(eu.display, 0, 0, "exposure %f", eu.conv.exposure);
        return 1;
      }
      return 0;

    case KeyM:
      if(eu.conv.gamutmap == s_gamut_clamp)
      {
        eu.conv.gamutmap = s_gamut_project;
        display_print(eu.display, 0, 0, "gamut mapping: project");
      }
      else
      {
        eu.conv.gamutmap = s_gamut_clamp;
        display_print(eu.display, 0, 0, "gamut mapping: clip");
      }
      return 1;

    case KeyP:
      if(eu.conv.colorout == s_adobergb)
      {
        eu.conv.colorout = s_srgb;
        display_print(eu.display, 0, 0, "color profile: srgb");
      }
      else if(eu.conv.colorout == s_srgb)
      {
        eu.conv.colorout = s_custom;
        display_print(eu.display, 0, 0, "color profile: custom");
      }
      else
      {
        eu.conv.colorout = s_adobergb;
        display_print(eu.display, 0, 0, "color profile: adobergb");
      }
      return 1;
    case KeyQ:
      return -1;

    default:
      return 0;
  }
}

int onMouseButtonDown(mouse_t *mouse)
{
  eu.pointer = eu.pointer_button = *mouse;
  eu.gui_x_button = eu.conv.roi.x;
  eu.gui_y_button = eu.conv.roi.y;
  if(eu.gui_state == 0)
    eu.gui_state = 1;
  return 0;
}

int onMouseButtonUp(mouse_t *mouse)
{
  if(eu.gui_state == 1)
  {
    // release drag
    eu.gui_state = 0;
    return 1;
  }
  if(eu.gui_state == 2)
  {
    // exposure correction, one screen width is 2 stops
    eu.conv.exposure += 2.0f*(mouse->x - eu.pointer_button.x)/(float)eu.display->width;
    eu.gui_state = 0;
    display_print(eu.display, 0, 0, "exposure %f", eu.conv.exposure);
    return 1;
  }
  return 0;
}

int onMouseMove(mouse_t *mouse)
{
  eu.pointer = *mouse;
  if(eu.gui_state == 1)
  {
    // if drag move image
    float diff_x = (eu.pointer.x - eu.pointer_button.x - eu.conv.roi_out.x)/eu.conv.roi.scale;
    float diff_y = (eu.pointer.y - eu.pointer_button.y - eu.conv.roi_out.y)/eu.conv.roi.scale;
    eu.conv.roi.x = MAX(0, eu.gui_x_button - diff_x);
    eu.conv.roi.y = MAX(0, eu.gui_y_button + diff_y);
    return 1;
  }
  return 0;
}

/*
  int (*onKeyUp)(keycode_t);
  int (*onKeyDown)(keycode_t);
  int (*onClose)();
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
  eu.display->onKeyPressed = onKeyPressed;
  eu.display->onMouseMove = onMouseMove;
  eu.display->onMouseButtonDown = onMouseButtonDown;
  eu.display->onMouseButtonUp = onMouseButtonUp;

  // display help:
  onKeyPressed(KeyH);

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
    ret = display_wait_event(eu.display);
    if(ret < 0) break;
  }

  eu_cleanup(&eu);

  exit(0);
}
