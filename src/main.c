#include "eu.h"

eu_t eu;

static inline char* load_sidecar(fileinput_t *file)
{
  char filename[1024];
  (void)strncpy(filename, file->filename, 1024);
  char *t = filename + strlen(filename);
  while(t > filename && *t != '.') t--;
  if(*t == '.') sprintf(t, ".txt");

  char *text = 0;
  int sc_fd = open(filename, O_RDONLY);
  if(sc_fd != -1)
  {
    size_t sc_size = lseek(sc_fd, 0, SEEK_END);
    text = (char*)malloc(sc_size+1);
    lseek(sc_fd, 0, SEEK_SET);
    if(text)
    {
      text[sc_size] = '\0';
      while(sc_size > 0)
      {
        ssize_t res = read(sc_fd, text, sc_size);
        if(res < 0) break;
        sc_size -= res;
      }
    }
    close(sc_fd);
  }
  return text;
}

static inline void show_metadata()
{
  if(eu.gui.show_metadata)
  {
    char *text = load_sidecar(eu.file + eu.current_file);
    if(text)
    {
      display_print(eu.display, 0, 0, text);
      free(text);
    }
  }
}

static inline void toggle_metadata()
{
  if(eu.gui.show_metadata)
  {
    eu.gui.show_metadata = 0;
    display_print(eu.display, 0, 0, "");
  }
  else
  {
    eu.gui.show_metadata = 1;
    show_metadata();
  }
}

static inline void pointer_to_image(float *x, float *y)
{
  *x = CLAMP((eu.gui.pointer.x - eu.conv.roi_out.x) / eu.conv.roi.scale + eu.conv.roi.x, 0, eu.conv.roi.w-1);
  *y = CLAMP(((eu.display->height - eu.gui.pointer.y - 1) - eu.conv.roi_out.y) / eu.conv.roi.scale + eu.conv.roi.y, 0, eu.conv.roi.h-1);
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
  const int shift = eu.display->mod_state & s_display_shift;
  if(eu.gui.dragging == 2)
  { // exposure typing mode
    eu.gui.input_string[eu.gui.input_string_len+1] = 0;
    if(eu.gui.input_string_len + 1 >= 256 || key == KeyEnter)
    {
      eu.conv.exposure = atof(eu.gui.input_string);
      display_print(eu.display, 0, 0, "exposure %f", eu.conv.exposure);
      eu.gui.dragging = 0;
      return 1;
    }
    else if(key >= KeyZero && key <= KeyNine)
      eu.gui.input_string[eu.gui.input_string_len++] = '0' + key-KeyZero;
    else if(key == KeyPeriod)
      eu.gui.input_string[eu.gui.input_string_len++] = '.';
    else if(key == KeyMinus)
      eu.gui.input_string[eu.gui.input_string_len++] = '-';

    display_print(eu.display, 0, 0, "exposure %s", eu.gui.input_string);
    return 1; // redraw string
  }
  else switch(key)
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

    case KeyF: // flag/unflag for comparison
      eu.file[eu.current_file].flag ^= 1;
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

    case KeyI: // input color space
      if(eu.conv.colorin == s_passthrough)
      {
        display_print(eu.display, 0, 0, "input color: xyz");
        eu.conv.colorin = s_xyz;
      }
      else
      {
        display_print(eu.display, 0, 0, "input color: passthrough");
        eu.conv.colorin = s_passthrough;
      }
      return 1;

    case KeyD: // dump buffer to file
      {
        FILE *f = fopen("dump.ppm", "wb");
        if(f)
        {
          fprintf(f, "P6\n%d %d\n255\n", eu.display->width, eu.display->height);
          size_t w = fwrite(eu.pixels, 3*sizeof(uint8_t), eu.display->width*eu.display->height, f);
          fclose(f);
          if(w != eu.display->width*eu.display->height)
            display_print(eu.display, 0, 0, "failed to write dump.ppm");
          else
            display_print(eu.display, 0, 0, "dumped screen buffer to dump.ppm");
        }
      }

    case KeyDown:
    case KeyRight:
      do
      {
        if(eu.current_file >= eu.num_files-1) break;
        eu.current_file++;
      }
      while(shift && !eu.file[eu.current_file].flag);
      snprintf(title, 256, "frame %04d/%04d -- %s", eu.current_file+1, eu.num_files, eu.file[eu.current_file].filename);
      display_title(eu.display, title);
      show_metadata();
      return 1;
    case KeyLeft:
    case KeyUp:
      do
      {
        if(eu.current_file == 0) break;
        eu.current_file--;
      }
      while(shift && !eu.file[eu.current_file].flag);
      snprintf(title, 256, "frame %04d/%04d -- %s", eu.current_file+1, eu.num_files, eu.file[eu.current_file].filename);
      display_title(eu.display, title);
      show_metadata();
      return 1;

    case KeySpace: // toggle play mode
      eu.gui.play ^= 1;
      display_print(eu.display, 0, 0, eu.gui.play ? "playing all frames" : "stopped");
      return 1;

    case KeyE:
      eu.gui.dragging = 2;
      eu.gui.input_string_len = 0;
      eu.gui.input_string[0] = 0;
      display_print(eu.display, 0, 0, "exposure ..");
      return 1;

    case KeyH:
      if(eu.display->msg_len > 0)
        display_print(eu.display, 0, 0, "");
      else
        display_print(eu.display, 0, 0, "[e]xpose\n[123] zoom\n[rgbc] channels\n[arrows] next/prev\n[h]elp\n[esc/q]uit\n[m] gamut map\n[p]rofile\n[s]idecar\n[t]onecurve");
      return 1;

    case KeyS:
      toggle_metadata();
      return 1;

    case KeyT:
      if(eu.conv.curve == s_none)
      {
        eu.conv.curve = s_contrast;
        display_print(eu.display, 0, 0, "curve: contrast s");
        return 1;
      }
      else if(eu.conv.curve == s_contrast)
      {
        eu.conv.curve = s_tonemap;
        display_print(eu.display, 0, 0, "curve: tonemap");
        return 1;
      }
      else if(eu.conv.curve == s_tonemap)
      {
        eu.conv.curve = s_isolines;
        display_print(eu.display, 0, 0, "curve: isolines");
        return 1;
      }
      else
      {
        eu.conv.curve = s_none;
        display_print(eu.display, 0, 0, "curve: linear");
        return 1;
      }

    case KeyM:
      if(eu.conv.gamutmap == s_gamut_clamp)
      {
        eu.conv.gamutmap = s_gamut_project;
        display_print(eu.display, 0, 0, "gamut mapping: project");
      }
      else if(eu.conv.gamutmap == s_gamut_project)
      {
        eu.conv.gamutmap = s_gamut_mark;
        display_print(eu.display, 0, 0, "gamut mapping: mark");
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
      else if(eu.conv.colorout == s_custom)
      {
        eu.conv.colorout = s_rec709;
        display_print(eu.display, 0, 0, "color profile: rec709");
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
  eu.gui.pointer = eu.gui.pointer_button = *mouse;
  eu.gui.button_x = eu.conv.roi.x;
  eu.gui.button_y = eu.conv.roi.y;
  eu.gui.start_exposure = eu.conv.exposure;
  if(eu.gui.dragging == 0)
    eu.gui.dragging = 1;
  return 0;
}

int onMouseButtonUp(mouse_t *mouse)
{
  if(eu.gui.dragging == 1)
  {
    // release drag
    eu.gui.dragging = 0;
    return 1;
  }
  if(eu.gui.dragging == 2)
  {
    // exposure correction, one screen width is 2 stops
    eu.conv.exposure = eu.gui.start_exposure + 2.0f*(mouse->x - eu.gui.pointer_button.x)/(float)eu.display->width;
    eu.gui.dragging = 0;
    display_print(eu.display, 0, 0, "exposure %f", eu.conv.exposure);
    return 1;
  }
  return 0;
}

int onMouseMove(mouse_t *mouse)
{
  eu.gui.pointer = *mouse;
  if(eu.gui.dragging == 1)
  {
    // if drag move image, difference in image space (not screen space)
    float diff_x = (eu.gui.pointer.x - eu.gui.pointer_button.x - eu.conv.roi_out.x)/eu.conv.roi.scale;
    float diff_y = (eu.gui.pointer.y - eu.gui.pointer_button.y - eu.conv.roi_out.y)/eu.conv.roi.scale;
    eu.conv.roi.x = eu.gui.button_x - diff_x;
    eu.conv.roi.y = eu.gui.button_y - diff_y;
    return 1;
  }
  if(eu.gui.dragging == 2)
  {
    // exposure correction, one screen width is 2 stops
    eu.conv.exposure = eu.gui.start_exposure + 2.0f*(mouse->x - eu.gui.pointer_button.x)/(float)eu.display->width;
    display_print(eu.display, 0, 0, "exposure %f", eu.conv.exposure);
    return 1;
  }
  // all dragging should refresh
  if(eu.gui.dragging) return 1;
  return 0;
}

/*
  int (*onKeyUp)(keycode_t);
  int (*onKeyPressed)(keycode_t);
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
  if(eu_init(&eu, wd, ht, argc, arg)) goto exit;
  eu.display->onKeyDown = onKeyDown;
  eu.display->onMouseMove = onMouseMove;
  eu.display->onMouseButtonDown = onMouseButtonDown;
  eu.display->onMouseButtonUp = onMouseButtonUp;

  onKeyDown(KeyTwo); // scale to fit on startup

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
    
    if(eu.gui.play)
    {
      ret = display_pump_events(eu.display);
      if(ret < 0) break;
      if(eu.current_file >= eu.num_files-1) eu.current_file = 0;
      else eu.current_file++;
      ret = 1; // trigger redraw
      onKeyDown(KeyTwo); // scale to fit
    }
    else
      ret = display_wait_event(eu.display);

    if(ret < 0) break;
  }

exit:
  eu_cleanup(&eu);
  exit(0);
}
