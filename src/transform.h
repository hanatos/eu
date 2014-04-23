#pragma once
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "colorout_custom.h"

// NaN-safe clamping (NaN compares false, and will thus result in H)
#define CLAMP(A, L, H) ((A) > (L) ? ((A) < (H) ? (A) : (H)) : (L))
#define MAX(A, B) ((A) < (B) ? (B) : (A))
#define MIN(A, B) ((A) > (B) ? (B) : (A))


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
  s_canon,         // measured canon 5DII contrast curve
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

static inline void transform_color(float *in, const transform_color_t ci, const transform_color_t co)
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
    for(int k=0;k<3;k++) in[k] = powf(in[k], g);
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
      c = powf(a*linear+b, g)/linear;
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
      else f = powf(a*f+b, g);
      in[i] = f;
    }
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
        const float brightness = in[0] + in[1] + in[2];
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

static inline void transform_curve(const float *tmp, uint8_t *out, const transform_curve_t c)
{
  // if(c == s_canon)
  // TODO:
  // else
  for(int k=0;k<3;k++)
    out[k] = CLAMP(255.0f*tmp[k], 0, 255.0);
}

