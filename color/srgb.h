#ifndef CORONA_COLOROUT_H
#define CORONA_COLOROUT_H

static inline void colorout_xyz_to_rgb(const float *const xyz, float *rgb)
{
  // created for /usr/share/color/argyll/ref/sRGB.icm
  const float XYZtoRGB[] =
  {
    3.24098245796182477888, -1.53746119787722604863, -.49863933040938950320,
    -.96928282108131036624, 1.87599094572176246173, .04156124552878093778,
    .05566083510567107827, -.20399485317594359118, 1.05736353854618246226,
  };

  rgb[0] = rgb[1] = rgb[2] = 0.0f;
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++) rgb[k] += xyz[i]*XYZtoRGB[i+3*k];
  
  // apply tonecurve
  rgb[0] = powf(rgb[0], 1.f/2.2f);
  rgb[1] = powf(rgb[1], 1.f/2.2f);
  rgb[2] = powf(rgb[2], 1.f/2.2f);
}

#endif
