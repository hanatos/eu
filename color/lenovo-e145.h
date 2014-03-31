#ifndef CORONA_COLOROUT_H
#define CORONA_COLOROUT_H

static inline void colorout_xyz_to_rgb(const float *const xyz, float *rgb)
{
  // created for /home/jo/.color/icc/lenovo-e145.icc
  const float XYZtoRGB[] =
  {
    3.92570580970774253533, -2.14424872528078874059, -.55298660652991082859,
    -1.34552888542930462065, 2.31075905805145458751, -.02110074986212282093,
    -.27813323213902656569, -.03421114233800218515, 1.17519843914529421413,
  };

  rgb[0] = rgb[1] = rgb[2] = 0.0f;
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++) rgb[k] += xyz[i]*XYZtoRGB[i+3*k];
  
  // apply tonecurve
  rgb[0] = powf(rgb[0], 1.f/1.980469);
  rgb[1] = powf(rgb[1], 1.f/1.980469);
  rgb[2] = powf(rgb[2], 1.f/1.980469);
}

#endif
