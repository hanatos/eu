
#include "fileinput.h"
#include "display.h"

#include <stdlib.h>



int main(int argc, char *arg[])
{
  fileinput_t file;

  fileinput_open(&file, arg[1]);

  fileinput_close(&file);
  exit(0);
}
