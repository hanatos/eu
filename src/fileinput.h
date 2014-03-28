#pragma once
#include "transform.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


typedef struct fileinput_roi_t
{
  float scale;
  int x, y, w, h;
}
fileinput_roi_t;

/* descriptor of all necessary conversions to convert to screen output. */
typedef struct fileinput_conversion_t
{
  float exposure;                  // stop the image up or down
  transform_channels_t channels;   // display only those channels
  transform_color_t    colorin;    // input color space description
  transform_color_t    colorout;   // desired output color space description
  transform_curve_t    curve;      // extra contrast curve for cinematic look or hdr compression.
  transform_gamut_t    gamutmap;   // gamut mapping
  fileinput_roi_t      roi;        // region of interest. first scale input, then crop to int bounds
  fileinput_roi_t      roi_out;    // output buffer description
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

/* wrappers to get dimensions, for future format extension. */
static inline int fileinput_width(fileinput_t *in)
{
  return in->pfm.width;
}

static inline int fileinput_height(fileinput_t *in)
{
  return in->pfm.height;
}

/* unmap the file. */
static inline void fileinput_close(fileinput_t *in)
{
  if(in->data) munmap(in->data, in->data_size);
  if(in->fd > 2) close(in->fd);
  in->fd = -1;
}

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
  while(endptr < (char *)in->data + 100 && *endptr != '\n') endptr++;
  in->pfm.pixel = (float*)(++endptr); // remove second newline

  // TODO: check alignment of float pointer!
  // TODO: while writing make sure the pixel data is 16-byte aligned for sse.
  // TODO: achieve this by padding up the idiotic scale factor line in the header.

  // sanity check:
  if(in->pfm.width*in->pfm.height*3*sizeof(float) > in->data_size - (endptr - (char *)in->data))
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
  // skip dead frames
  if(in->fd < 0) return 1;
  const float scalex = 1.0f/c->roi.scale;
  const float scaley = 1.0f/c->roi.scale;
  int32_t ix2 = MAX(c->roi.x, 0);
  int32_t iy2 = MAX(c->roi.y, 0);
  int32_t ibw = in->pfm.width, ibh = in->pfm.height;
  int32_t obw = c->roi_out.w, obh = c->roi_out.h;
  int32_t ow = c->roi_out.w, oh = c->roi_out.h;
  int32_t ox2 = MAX(0, (c->roi_out.w-in->pfm.width*c->roi.scale)*.5f);
  int32_t oy2 = MAX(0, (c->roi_out.h-in->pfm.height*c->roi.scale)*.5f);
  int32_t oh2 = MIN(MIN(oh, (ibh - iy2)/scaley), obh - oy2);
  int32_t ow2 = MIN(MIN(ow, (ibw - ix2)/scalex), obw - ox2);
  assert((int)(ix2 + ow2*scalex) <= ibw);
  assert((int)(iy2 + oh2*scaley) <= ibh);
  assert(ox2 + ow2 <= obw);
  assert(oy2 + oh2 <= obh);
  assert(ix2 >= 0 && iy2 >= 0 && ox2 >= 0 && oy2 >= 0);
  float x = ix2, y = iy2;

  const float f = powf(2.0f, c->exposure);

  // TODO parallel
  // fill top/bottom borders:
  for(int j=0;j<oy2;j++) memset(buf + 3*j*obw, 0, 3*obw);
  for(int j=oy2+oh2;j<obh;j++) memset(buf + 3*j*obw, 0, 3*obw);
  for(int s=0; s<oh2; s++)
  {
    // fill left/right borders
    for(int t=0;t<3*ox2;t++) buf[3*obw*(oy2+s)+t] = 0.0f;
    for(int t=3*(ow2+ox2);t<3*obw;t++) buf[3*obw*(oy2+s)+t] = 0.0f;

    int idx = ox2 + obw*(oy2+s);
    for(int t=0; t<ow2; t++)
    {
      // TODO: clamp x y
      // for(int k=0; k<3; k++) buf[3*idx + k] = //255.0f*in->pfm.pixel[3*(ibw*(int)y + (int)x) + k];
      //  CLAMP((in->pfm.pixel[3*(ibw*(int32_t) y +            (int32_t) (x + .5f*scalex)) + k] +
      //         in->pfm.pixel[3*(ibw*(int32_t)(y+.5f*scaley) +(int32_t) (x + .5f*scalex)) + k] +
      //         in->pfm.pixel[3*(ibw*(int32_t)(y+.5f*scaley) +(int32_t) (x             )) + k] +
      //         in->pfm.pixel[3*(ibw*(int32_t) y +            (int32_t) (x             )) + k])*64.0f, 0, 255);
      // TODO: get input as float triple, then:
      float tmp[3];
      for(int k=0; k<3; k++) tmp[k] = //255.0f*in->pfm.pixel[3*(ibw*(int)y + (int)x) + k];
      (in->pfm.pixel[3*(ibw*(int32_t) y +            (int32_t) (x + .5f*scalex)) + k] +
       in->pfm.pixel[3*(ibw*(int32_t)(y+.5f*scaley) +(int32_t) (x + .5f*scalex)) + k] +
       in->pfm.pixel[3*(ibw*(int32_t)(y+.5f*scaley) +(int32_t) (x             )) + k] +
       in->pfm.pixel[3*(ibw*(int32_t) y +            (int32_t) (x             )) + k])*.25f;

      // float exposure; adjust exposure
      transform_exposure(tmp, f);

      // color conversion
      transform_color(tmp, c->colorin, c->colorout);

      // gamut mapping 
      transform_gamutmap(tmp, c->gamutmap);

      // apply curve
      transform_curve(tmp, buf+3*idx, c->curve);

      // zero out channels
      transform_channels(buf+3*idx, c->channels);

      x += scalex;
      idx++;
    }
    y += scaley;
    x = ix2;
  }

#if 0
  // XXX this is rubbish, not feature complete, and needs to be optimized a lot!
  for(int j=0;j<c->roi.h;j++)
  {
    for(int i=0;i<c->roi.w;i++)
    {
      for(int k=0;k<3;k++)
        buf[3*(j*c->roi.w + i) + k] = 255.0 * in->pfm.pixel[3*(j*in->pfm.width + i) + k];
    }
  }
#endif
  return 0;
}

/* prefetches the input buffer by instructing the kernel that we'll soon need it. */
static inline void fileinput_prefetch(fileinput_t *in)
{
  madvise(in->data, in->data_size, MADV_WILLNEED);
}

/* instructs the kernel that we're done for now. */
static inline void fileinput_dontneed(fileinput_t *in)
{
  madvise(in->data, in->data_size, MADV_DONTNEED);
}

