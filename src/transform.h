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

static inline void transform_curve(const float *tmp, uint8_t *out, const transform_curve_t c)
{
  // if(c == s_canon)
    // TODO:
  // else
    for(int k=0;k<3;k++)
      out[k] = CLAMP(255.0f*tmp[k], 0, 255.0);
}

