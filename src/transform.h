#pragma once
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "colorout_custom.h"

// NaN-safe clamping (NaN compares false, and will thus result in H)
#define CLAMP(A, L, H) (((A) > (L)) ? (((A) < (H)) ? (A) : (H)) : (L))
#define MAX(A, B) (((A) < (B)) ? (B) : (A))
#define MIN(A, B) (((A) > (B)) ? (B) : (A))


typedef enum transform_channels_t
{
  s_red = 0,
  s_green = 1,
  s_blue = 2,
  s_rgb = 3
}
transform_channels_t;

typedef enum transform_color_t
{
  s_passthrough,   // don't do any colorspace conversion
  s_xyz,           // linear xyz illum E (not d50, straight 1931 cmf)
  s_rec709,        // rec709 rgb primaries, linear
  s_srgb,          // rec709 D65 + linear toe slope gamma table
  s_adobergb,      // adobergb primaries + gamma
  s_custom,        // convert using custom code (put your display profile matrix there)
}
transform_color_t;

typedef enum transform_gamut_t
{
  s_gamut_clamp,   // just clamp to 0, 1 (absolute colorimetric)
  s_gamut_project, // project to 1/3 wp  (relative colorimetric)
  s_gamut_mark,    // mark out of gamut spots in weird colors
}
transform_gamut_t;

typedef enum transform_curve_t
{
  s_none,          // straight output of colorout space
  s_contrast,      // contrast s curve
  s_tonemap,       // L = L/(L+1) tonemapping
  s_isolines,      // highlight a couple of isoline steps
  s_viridis,       // colour map the [0 1] range
}
transform_curve_t;

static inline void transform_exposure(float *in, float f)
{
  for(int k=0;k<3;k++)
    in[k] *= f;
}

static inline void transform_channels(uint8_t *in, const int c)
{
  if(c >= 3) return;
  for(int k=0;k<3;k++) in[k] = in[c];
}


static inline float
fastpow2(float p)
{
  float clipp = (p < -126) ? -126.0f : p;
  union { uint32_t i; float f; } v = { (uint32_t) ( (1 << 23) * (clipp + 126.94269504f) ) };
  return v.f;
}

static inline float
fastlog2(float x)
{
  union { float f; uint32_t i; } vx = { x };
  float y = vx.i;
  y *= 1.1920928955078125e-7f;
  return y - 126.94269504f;
}

static inline float fast_powf(float a, float b)
{
  return fastpow2(b * fastlog2(a));
}

static inline void transform_color(float *in, const transform_color_t ci, const transform_color_t co, const int fast)
{
  if(ci == s_passthrough) return;
  assert(ci == s_xyz);
  if(co == s_custom)
  {
    const float xyz[3] = {in[0], in[1], in[2]};
    colorout_xyz_to_rgb(xyz, in);
  }
  else if(co == s_adobergb)
  {
    const float XYZtoRGB[] =
    {
      // adobe rgb 1998
      2.0413690, -0.5649464, -0.3446944,
      -0.9692660,  1.8760108,  0.0415560,
      0.0134474, -0.1183897,  1.0154096,
    };

    float xyz[3] = {in[0], in[1], in[2]};
    in[0] = in[1] = in[2] = 0.0f;
    for(int k=0;k<3;k++)
      for(int i=0;i<3;i++) in[k] += xyz[i]*XYZtoRGB[i+3*k];

    // apply tonecurve. adobe rgb does not have a linear toe slope, but gamma of:
    const float g = 1.f/2.19921875f;
    if(fast)
      for(int k=0;k<3;k++) in[k] = copysignf(fast_powf(fabsf(in[k]), g), in[k]);
    else
      for(int k=0;k<3;k++) in[k] = copysignf(powf(fabsf(in[k]), g), in[k]);
  }
  else if(co == s_srgb)
  {
    // see http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    const float XYZtoRGB[] =
    {
      // sRGB D65
      3.2404542, -1.5371385, -0.4985314,
      -0.9692660,  1.8760108,  0.0415560,
      0.0556434, -0.2040259,  1.0572252,
    };

    float xyz[3] = {in[0], in[1], in[2]};
    in[0] = in[1] = in[2] = 0.0f;
    for(int k=0;k<3;k++)
      for(int i=0;i<3;i++) in[k] += xyz[i]*XYZtoRGB[i+3*k];

    // add srgb gamma with linear toe slope:
    float a, b, c, g;
    const float linear = 0.1f, gamma = 0.4f;
    if(linear<1.0)
    {
      g = gamma*(1.0-linear)/(1.0-gamma*linear);
      a = 1.0/(1.0+linear*(g-1));
      b = linear*(g-1)*a;
      c = fast_powf(a*linear+b, g)/linear;
    }
    else
    {
      a = b = g = 0.0;
      c = 1.0;
    }
    for(int i=0;i<3;i++)
    {
      float f = in[i];
      if(f < linear) f = c*f;
      else f = fast_powf(a*f+b, g);
      in[i] = f;
    }
  }
  else if(co == s_rec709)
  {
    // see http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    const float XYZtoRGB[] =
    {
      // sRGB D65
      3.2404542, -1.5371385, -0.4985314,
      -0.9692660,  1.8760108,  0.0415560,
      0.0556434, -0.2040259,  1.0572252,
    };

    float xyz[3] = {in[0], in[1], in[2]};
    in[0] = in[1] = in[2] = 0.0f;
    for(int k=0;k<3;k++)
      for(int i=0;i<3;i++) in[k] += xyz[i]*XYZtoRGB[i+3*k];
  }
}

static inline void transform_gamutmap(float *in, const transform_gamut_t c)
{
  static int mark = 0;
  if(c == s_gamut_clamp)
  {
    for(int k=0;k<3;k++) in[k] = MAX(in[k], 0.0f);
  }
  else if(c == s_gamut_project)
  {
    for(int k=0;k<3;k++)
    {
      if(in[k] < 0.0f)
      {
        // const float wp = 1./3.f;
        int s = k+1 > 2 ? 0 : k+1;
        int t = k-1 < 0 ? 2 : k-1;
        // scale white point to same brightness as rgb.
        // as we're using wp = 1/3 for gamut mapping, brightness is r+g+b.
        // const float brightness = in[0] + in[1] + in[2];
        const float white = 1./3.;//fmaxf(0.0f, brightness); // this will need to be white[3] for more complex wp
        const float a = white/(white-in[k]);
        assert(a <= 1.0f);
        if(a < 0.0) fprintf(stderr, "a = %f wp %f in[%d] = %f (%f %f %f)\n", a, white, k, in[k], in[0], in[1], in[2]);
        assert(a >= 0.0);
        in[s] = white + a * (in[s]-white);
        in[t] = white + a * (in[t]-white);
        in[k] = 0.0f;
      }
    }
  }
  else if(c == s_gamut_mark)
  {
    if(in[0] < 0.0 || in[1] < 0.0 || in[2] < 0.0)
    {
      in[0] = (mark&4) ? 1.0f : 0.0f;
      in[1] = in[2] = (mark&4) ? 0.0f : 1.0f;
      mark++;
    }
  }
}

static inline void transform_curve(const float *tmp, uint8_t *out, const transform_curve_t c, int cc)
{
  if(c == s_contrast)
  {
    // for(int k=0;k<3;k++)
      // out[k] = CLAMP(255.0f*canon_curve(tmp[k]), 0, 255.0);
    for(int k=0;k<3;k++)
    {
      const float a = 0.5f;
      const float f = (1.0f-a)*tmp[k] + a*(.5f - cosf(CLAMP(tmp[k], 0.0f, 1.0f) * M_PI) * .5f);
      out[k] = CLAMP(255.0f*f, 0, 255.0);
    }
  }
  else if(c == s_tonemap)
  {
    // tonemap in yuv: compress y and adjust uv saturation accordingly
    const float M[] = {
      0.299f, 0.587f, 0.114f,
     -0.14713f, -0.28886f, 0.436f,
      0.615f, -0.51499f, -0.10001f};
    const float Mi[] = {
      1.0f, 0.0f, 1.13983f,
      1.0f, -0.39465f, -0.58060f,
      1.0f, 2.03211f,  0.0f};
    float yuv[3] = {0.0f};
    for(int i=0;i<3;i++)
    for(int j=0;j<3;j++)
      yuv[i] += M[3*i+j]*tmp[j];
    // const float new_y = 2.0/3.0*yuv[0]/(yuv[0]+1.0f);
    // log leads to more natural compression of highlights (still allows some clipping)
    const float new_y = logf(yuv[0]+1.0f)/logf(10.0f);
    for(int k=1;k<3;k++) yuv[k] *= CLAMP(new_y/yuv[0], 1e-5, 1e5);
    yuv[0] = new_y;
    float rgb[3] = {0.0f};
    for(int i=0;i<3;i++)
    for(int j=0;j<3;j++)
      rgb[i] += Mi[3*i+j]*yuv[j];

    for(int k=0;k<3;k++)
      out[k] = CLAMP(255.0f*rgb[k], 0, 255.0);
  }
  else if(c == s_isolines)
  {
    float md = 1e30;
    float f = tmp[1];
    int num = 10;
    for(int k=0;k<num;k++)
      md = fminf(md, fabsf(f - k/(num-1.0f)));
    float wd = 0.01f;
    const float col = fmaxf(0.0f, (wd - md)/wd);
    for(int k=0;k<3;k++)
      out[k] = CLAMP(255.0f*col, 0, 255.0);
  }
  else if(c == s_viridis)
  {
    float x = tmp[cc % 3];
    x = CLAMP(x, 0.0, 1.0);
    float x2 = x*x, x3 = x2*x, x4 = x2*x2, x5 = x3*x2;
    const float col[] = {
      +0.280268003 -0.143510503*x +2.2257938770*x2  -14.815088879*x3 + +25.212752309*x4 -11.772589584*x5,
      -0.002117546 +1.617109353*x -1.9093050700*x2  +2.701152864 *x3 + -1.685288385 *x4  +0.178738871*x5,
      +0.300805501 +2.614650302*x -12.019139090*x2 +28.933559110 *x3 + -33.491294770*x4 +13.762053843*x5,
    };
    for(int k=0;k<3;k++)
      out[k] = CLAMP(255.0f*col[k], 0, 255.0);
  }
  else
  {
    for(int k=0;k<3;k++)
      out[k] = CLAMP(255.0f*tmp[k], 0, 255.0);
  }
}

