#ifndef CORONA_COLOROUT_H
#define CORONA_COLOROUT_H

static inline void colorout_xyz_to_rgb(const float *const xyz, float *rgb)
{
  // rec 2020
  const float XYZtoRGB[] =
  {
     1.7166511880, -0.3556707838, -0.2533662814,
    -0.6666843518,  1.6164812366,  0.0157685458,
     0.0176398574, -0.0427706133,  0.9421031212
  };

  rgb[0] = rgb[1] = rgb[2] = 0.0f;
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++) rgb[k] += xyz[i]*XYZtoRGB[i+3*k];
  
  // apply gamma
  rgb[0] = powf(rgb[0], 1.f/2.2f);
  rgb[1] = powf(rgb[1], 1.f/2.2f);
  rgb[2] = powf(rgb[2], 1.f/2.2f);
}

#endif
