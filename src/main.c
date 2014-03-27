
#include "fileinput.h"
#include "display.h"

#include <stdlib.h>


#define PROG_NAME "eu"


int main(int argc, char *arg[])
{
  const int wd = 512, ht = 512;
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

  fileinput_conversion_t c;
  c.roi.x = 0;
  c.roi.y = 0;
  c.roi.w = wd;
  c.roi.h = ht;
  c.roi.scale = 1.0f;
  c.colorin = s_passthrough;
  c.colorout = s_passthrough;

  uint8_t *pixels = (uint8_t *)aligned_alloc(16, wd*ht*3);
  fileinput_grab(&file, &c, pixels);

  while(1)
  {
    if(display_wait_event(display)) break;
    display_update(display, pixels);
  }

  fileinput_close(&file);
  display_close(display);
  free(pixels);

  exit(0);
}
