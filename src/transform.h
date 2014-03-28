#pragma once
#include <stdint.h>

// NaN-safe clamping (NaN compares false, and will thus result in H)
#define CLAMP(A, L, H) ((A) > (L) ? ((A) < (H) ? (A) : (H)) : (L))

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
  if(co == s_adobergb)
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
}

static inline void transform_gamut_map(float *in, const transform_gamut_t c)
{
  if(c == s_clamp)
    for(int k=0;k<3;k++) in[k] = MAX(in[k], 0.0f);
  else if(c == s_project)
    for(int k=0;k<3;k++)
    {
      if(in[k] < 0.0f)
      {
        int s = k+1 > 2 ? 0 : k+1;
        int t = k+2 > 2 ? 0 : k+2;
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

