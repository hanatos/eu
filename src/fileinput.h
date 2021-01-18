#pragma once
#include "transform.h"
#include "framebuffer.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef enum fileinput_verbosity_t
{
  s_silent = 0,
  s_timing = 1,
  s_warning = 2,
}
fileinput_verbosity_t;

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

  fileinput_verbosity_t verbosity; // control log output to stderr
}
fileinput_conversion_t;

typedef enum fileinput_type_t
{
  s_pfm = 0,
  s_fb  = 1,
}
fileinput_type_t;

/* struct encapsulating all pfm specific stuff */
typedef struct fileinput_pfm_t
{
  int width, height;     // dimensions of the image
  float *scale;          // optional scale read from end of file
  float *pixel;          // pointer to start of pixel data
}
fileinput_pfm_t;

/* input file buffer type. */
typedef struct fileinput_t
{
  int fd;              // file descriptor
  void *data;          // mapped buffer
  size_t data_size;    // size of file
  char filename[1024]; // buffer file name

  int flag;            // flagged for comparison?

  fileinput_type_t format;

  fileinput_pfm_t pfm; // pfm file
  framebuffer_t fb;    // framebuffer file
}
fileinput_t;

/* wrappers to get dimensions, for future format extension. */
static inline int fileinput_width(fileinput_t *in)
{
  if(in->format == s_pfm)
    return in->pfm.width;
  return in->fb.header->width;
}

static inline int fileinput_height(fileinput_t *in)
{
  if(in->format == s_pfm)
    return in->pfm.height;
  return in->fb.header->height;
}

/* unmap the file. */
static inline void fileinput_close(fileinput_t *in)
{
  if(in->format == s_fb) fb_cleanup(&in->fb);
  if(in->data) munmap(in->data, in->data_size);
  if(in->fd > 2) close(in->fd);
  in->fd = -1;
}

/* open input file via mmap, to not consume any memory if we don't need it. */
static inline int fileinput_open(fileinput_t *in, const char *filename)
{
  in->flag = 0;
  in->data = 0;

  if(!fb_map(&in->fb, filename))
  { // first try to map as fb
    in->format = s_fb;
    return 0;
  }

  in->format = s_pfm;
  in->fd = open(filename, O_RDONLY);
  (void)strncpy(in->filename, filename, 1024);
  if(in->fd == -1) return 1;
  in->data_size = lseek(in->fd, 0, SEEK_END);
  if(in->data_size < 100)
  {
    fileinput_close(in);
    return 2;
  }
  lseek(in->fd, 0, SEEK_SET);
  // this will cause segfaults in case anybody else is writing it while we have it mapped
  in->data = mmap(0, in->data_size, PROT_READ, MAP_SHARED | MAP_NORESERVE, in->fd, 0);

  // get pfm header for faster grabbing later on.
  // no error handling is done, no comments supported.
  char *endptr;
  in->pfm.width = strtol(in->data+3, &endptr, 10);
  in->pfm.height = strtol(endptr, &endptr, 10);
  endptr++; // remove newline
  while(endptr < (char *)in->data + 100 && *endptr != '\n') endptr++;
  in->pfm.pixel = (float*)(++endptr); // remove second newline

  // got extra data at the end?
  in->pfm.scale = 0;
  if(in->data_size - sizeof(float)*(in->pfm.pixel - (float *)in->data) > sizeof(float)*3*in->pfm.width*in->pfm.height)
    in->pfm.scale = in->pfm.pixel + in->pfm.width * in->pfm.height * 3;

  // while writing make sure the pixel data is 16-byte aligned for sse.
  // achieve this by padding up the idiotic scale factor line in the header with additional 0s
  if((size_t)in->pfm.pixel & 0xf)
    fprintf(stderr, "[fileinput_open] `%s' pixel buffer not SSE aligned!\n", filename);
  if((size_t)in->pfm.pixel & 0x3)
    fprintf(stderr, "[fileinput_open] `%s' pixel buffer not float aligned!\n", filename);

  // sanity check:
  if(in->pfm.width*in->pfm.height*3*sizeof(float) > in->data_size - (endptr - (char *)in->data))
  {
    fileinput_close(in);
    return 3;
  }
  return 0;
}

static inline double _time_wallclock()
{
  struct timeval time;
  gettimeofday(&time, NULL);
  return time.tv_sec - 1290608000 + (1.0/1000000.0)*time.tv_usec;
}

static inline int fileinput_process(fileinput_t *in, const fileinput_conversion_t *c, const char *filename)
{
  if(in->format != s_pfm) return 1; // TODO: use fb input, too
  fprintf(stderr, "[process] rendering `%s'\n", filename);
  FILE *out = fopen(filename, "wb");
  if(!out) return 1;

  double start = _time_wallclock();
  // skip dead frames
  if(in->fd < 0) return 1;

  const float f = (in->pfm.scale ? in->pfm.scale[0] : 1.0f) * powf(2.0f, c->exposure);

  char header[1024];
  snprintf(header, 1024, "PF\n%d %d\n-1.0", in->pfm.width, in->pfm.height);
  size_t len = strlen(header);
  fprintf(out, "PF\n%d %d\n-1.0", in->pfm.width, in->pfm.height);
  ssize_t off = 0;
  while((len + 1 + off) & 0xf) off++;
  while(off-- > 0) fprintf(out, "0");
  fprintf(out, "\n");

  for(int j=0; j<in->pfm.height; j++)
  {
    for(int i=0; i<in->pfm.width; i++)
    {
      float tmp[3];
      for(int k=0; k<3; k++) tmp[k] = in->pfm.pixel[3*(in->pfm.width*j + i) + k];

      // float exposure; adjust exposure
      transform_exposure(tmp, f);

      // color conversion
      transform_color(tmp, c->colorin, c->colorout, 0);

      // gamut mapping 
      if(c->colorin != s_passthrough)
        transform_gamutmap(tmp, c->gamutmap);

      // not applying curve or channel zeroing, outputting linear only.

      fwrite(tmp, sizeof(float), 3, out);
    }
  }
  fclose(out);
  if(c->verbosity & s_timing)
  {
    double end = _time_wallclock();
    fprintf(stderr, "[process] frame rendered in %.04f sec\n", end-start);
  }
  return 0;
}

/* grab a framebuffer from the mmapped file, only use the memory allocated for the framebuffer.
 * this needs to be extremely efficient to allow for video playback. */
static inline int fileinput_grab(fileinput_t *in, const fileinput_conversion_t *c, uint8_t *buf)
{
  double start = _time_wallclock();
  const uint64_t wd = in->format == s_pfm ? in->pfm.width  : in->fb.header->width;
  const uint64_t ht = in->format == s_pfm ? in->pfm.height : in->fb.header->height;
  const int32_t roix = CLAMP(c->roi.x, 0, MAX(0, wd - c->roi_out.w/c->roi.scale - 1));
  const int32_t roiy = CLAMP(c->roi.y, 0, MAX(0, ht - c->roi_out.h/c->roi.scale - 1));
  // skip dead frames
  if(in->fd < 0) return 1;
  const float scalex = 1.0f/c->roi.scale;
  const float scaley = 1.0f/c->roi.scale;
  int32_t ix2 = roix;
  int32_t iy2 = roiy;
  int32_t ibw = wd, ibh = ht;
  int32_t obw = c->roi_out.w, obh = c->roi_out.h;
  int32_t ow = c->roi_out.w, oh = c->roi_out.h;
  int32_t ox2 = MAX(0, (c->roi_out.w-wd*c->roi.scale)*.5f);
  int32_t oy2 = MAX(0, (c->roi_out.h-ht*c->roi.scale)*.5f);
  int32_t oh2 = MIN(MIN(oh, MAX(0, (ibh - iy2)/scaley)), MAX(0, obh - oy2));
  int32_t ow2 = MIN(MIN(ow, MAX(0, (ibw - ix2)/scalex)), MAX(0, obw - ox2));
  assert((int)(ix2 + ow2*scalex) <= ibw);
  assert((int)(iy2 + oh2*scaley) <= ibh);
  assert(ox2 + ow2 <= obw);
  assert(oy2 + oh2 <= obh);
  assert(ix2 >= 0 && iy2 >= 0 && ox2 >= 0 && oy2 >= 0);
  float x = ix2, y = iy2;

  float sc = 1.0f;
  if(in->format == s_pfm && in->pfm.scale) sc = in->pfm.scale[0];
  if(in->format == s_fb) sc = in->fb.header->gain;
  const float f = sc * powf(2.0f, c->exposure);

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
      float tmp[3];
      // TODO: fb channel offset selection
      const int nc = in->format == s_pfm ? 3 : in->fb.header->channels;
      const float *const inb = in->format == s_pfm ? in->pfm.pixel : in->fb.fb;
      for(int k=0; k<3; k++) tmp[k] = //255.0f*in->pfm.pixel[3*(ibw*(int)y + (int)x) + k];
      (inb[nc*(ibw*(int32_t) y +            (int32_t) (x + .5f*scalex)) + k] +
       inb[nc*(ibw*(int32_t)(y+.5f*scaley) +(int32_t) (x + .5f*scalex)) + k] +
       inb[nc*(ibw*(int32_t)(y+.5f*scaley) +(int32_t) (x             )) + k] +
       inb[nc*(ibw*(int32_t) y +            (int32_t) (x             )) + k])*.25f;

      // float exposure; adjust exposure
      transform_exposure(tmp, f);
      for(int k=0;k<3;k++) assert(tmp[k] == tmp[k]);

      // color conversion
      transform_color(tmp, c->colorin, c->colorout, 1);
      for(int k=0;k<3;k++) assert(tmp[k] == tmp[k]);

      // gamut mapping 
      if(c->colorin != s_passthrough)
        transform_gamutmap(tmp, c->gamutmap);
      for(int k=0;k<3;k++) assert(tmp[k] == tmp[k]);

      // apply curve
      transform_curve(tmp, buf+3*idx, c->curve, c->channels);

      // zero out channels
      if(c->curve != s_viridis)
        transform_channels(buf+3*idx, c->channels);

      x += scalex;
      idx++;
    }
    y += scaley;
    x = ix2;
  }
  if(c->verbosity & s_timing)
  {
    double end = _time_wallclock();
    fprintf(stderr, "[grab] frame rendered in %.04f sec\n", end-start);
  }
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

