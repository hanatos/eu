#pragma once
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


typedef enum fileinput_roi_t
{
  float scale;
  int x, y, w, h;
}
fileinput_roi_t;

typedef enum fileinput_channels_t
{
  s_red = 1,
  s_green = 2,
  s_blue = 4,
  s_rgb = s_red | s_green | s_blue,
}
fileinput_channels_t;

typedef enum fileinput_color_t
{
  s_passthrough,   // don't do any colorspace conversion
  s_xyz,           // linear xyz illum E (not d50, straight 1931 cmf)
  s_rec709,        // rec709 rgb primaries, linear
  s_srgb,          // rec709 D65 + linear toe slope gamma table
  s_adobergb,      // adobergb primaries + gamma
  s_custom,        // convert using custom code (put your display profile matrix there)
}
fileinput_colorout_t;

typedef enum fileinput_curve_t
{
  s_none,          // straight output of colorout space
  s_canon,         // measured canon 5DII contrast curve
}
fileinput_curve_t;

/* descriptor of all necessary conversions to convert to screen output. */
typedef struct fileinput_conversion_t
{
  float exposure;                  // stop the image up or down
  fileinput_channels_t channels;   // display only those channels
  fileinput_color_t    colorin;    // input color space description
  fileinput_color_t    colorout;   // desired output color space description
  fileinput_curve_t    curve;      // extra contrast curve for cinematic look or hdr compression.
  fileinput_roi_t      roi;        // region of interest. first scale input, then crop to int bounds
}
fileinput_conversion_t;

/* struct encapsulating all pfm specific stuff */
typedef struct fileinput_pfm_t
{
  int width, height;   // dimensions of the image
  float *pixel;        // pointer to start of pixel data
}
fileinput_pfm_t;

/* input file buffer type. */
typedef struct fileinput_t
{
  int fd;              // file descriptor
  void *data;          // mapped buffer
  size_t data_size;    // size of file

  fileinput_pfm_t pfm; // only supported format so far
}
fileinput_t;


/* open input file via mmap, to not consume any memory if we don't need it. */
static inline int fileinput_open(fileinput_t *in, const char *filename)
{
  in->data = 0;
  in->fd = open(filename, O_RDONLY);
  if(in->fd == -1) return 1;
  in->data_size = lseek(in->fd, 0, SEEK_END);
  if(in->data_size < 100)
  {
    fileinput_close(in);
    return 2;
  }
  lseek(in->fd, 0, SEEK_SET);
  // this will cause segfaults in case anybody else is writing it while we have it mapped
  in->data = mmap(0, in->data_size, PROT_READ, MAP_SHARED, in->fd, 0);

  // get pfm header for faster grabbing later on.
  // no error handling is done, no comments supported.
  char *endptr;
  in->pfm.width = strtol(in->data+3, &endptr, 10);
  in->pfm.height = strtol(endptr, &endptr, 10);
  endptr++; // remove newline
  while(endptr < in->data + 100 && *endptr != '\n') endptr++;
  in->pfm.pixels = (float*)(++endptr); // remove second newline

  // TODO: check alignment of float pointer!
  // TODO: while writing make sure the pixel data is 16-byte aligned for sse.
  // TODO: achieve this by padding up the idiotic scale factor line in the header.

  // sanity check:
  if(in->pfm.width*in->pfm.height*3*sizeof(float) > in->data_size - (endptr - in->data))
  {
    fileinput_close(in);
    return 3;
  }
  return 0;
}

/* grab a framebuffer from the mmapped file, only use the memory allocated for the framebuffer.
 * this needs to be extremely efficient to allow for video playback. */
static inline int fileinput_grab(fileinput_t *in, const fileinput_conversion_t *c, uint8_t *buf)
{
  // XXX this is rubbish, not feature complete, and needs to be optimized a lot!
  for(int j=0;j<c->roi.h;j++)
  {
    for(int i=0;i<c->roi.w;i++)
    {
      for(int k=0;k<3;k++)
        buf[4*(j*c->roi.w + i) + k] = 255.0 * in->pfm.pixel[3*(j*c->pfm.width + i) + k]
    }
  }
}

/* prefetches the input buffer by instructing the kernel that we'll soon need it. */
static inline void fileinput_prefetch(fileinput_t *in)
{
  return madvise(in->data, in->data_size, MADV_WILLNEED);
}

/* instructs the kernel that we're done for now. */
static inline void fileinput_dontneed(fileinput_t *in)
{
  return madvise(in->data, in->data_size, MADV_DONTNEED);
}

/* unmap the file. */
static inline void fileinput_close(fileinput_t *in)
{
  if(in->data) munmap(in->data, in->data_size);
  if(in->fd > 2) close(in->fd);
}

