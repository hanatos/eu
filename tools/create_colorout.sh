#!/bin/bash

# iccdump outputs the transpose of the rgb -> xyz matrix, so read the transpose of that:
read -r w0 w1 w2 a00 a10 a20 a01 a11 a21 a02 a12 a22 <<< $(iccdump -v3 -t wtpt -t rXYZ -t gXYZ -t bXYZ $1 | grep 0: | awk '{print $2 $3 $4}' | tr ',' ' ' | tr '\n' ' ')
echo "wp $w0 $w1 $w2"
echo "rgb to xyz"
echo $a00 $a01 $a02
echo $a10 $a11 $a12
echo $a20 $a21 $a22
# bradford adaptation Ma
ma00=0.8951000;  ma01=0.2664000;  ma02=-0.1614000
ma10=-0.7502000; ma11=1.7135000;  ma12=0.0367000
ma20=0.0389000;  ma21=-0.0685000; ma22=1.0296000
# bradford adaptation Ma_inv
mai00=0.9869929;  mai01=-0.1470543; mai02=0.1599627
mai10=0.4323053;  mai11=0.5183603;  mai12=0.0492912
mai20=-0.0085287; mai21=0.0400428;  mai22=0.9684867

# create matrix m = mai s ma a:

# b = ma * a
b00=$(echo $ma00*$a00 + $ma01*$a10 + $ma02*$a20 | bc -l)
b01=$(echo $ma00*$a01 + $ma01*$a11 + $ma02*$a21 | bc -l)
b02=$(echo $ma00*$a02 + $ma01*$a12 + $ma02*$a22 | bc -l)
b10=$(echo $ma10*$a00 + $ma11*$a10 + $ma12*$a20 | bc -l)
b11=$(echo $ma10*$a01 + $ma11*$a11 + $ma12*$a21 | bc -l)
b12=$(echo $ma10*$a02 + $ma11*$a12 + $ma12*$a22 | bc -l)
b20=$(echo $ma20*$a00 + $ma21*$a10 + $ma22*$a20 | bc -l)
b21=$(echo $ma20*$a01 + $ma21*$a11 + $ma22*$a21 | bc -l)
b22=$(echo $ma20*$a02 + $ma21*$a12 + $ma22*$a22 | bc -l)

#  D50   0.96422   1.00000   0.82521
#  D55   0.95682   1.00000   0.92149
#  D65   0.95047   1.00000   1.08883
x=0.96422; y=1.00000; z=0.82521
d50l=$(echo $ma00*$x + $ma01*$y + $ma02*$z | bc -l)
d50m=$(echo $ma10*$x + $ma11*$y + $ma12*$z | bc -l)
d50s=$(echo $ma20*$x + $ma21*$y + $ma22*$z | bc -l)
wl=$(echo $ma00*$w0 + $ma01*$w1 + $ma02*$w2 | bc -l)
wm=$(echo $ma10*$w0 + $ma11*$w1 + $ma12*$w2 | bc -l)
ws=$(echo $ma20*$w0 + $ma21*$w1 + $ma22*$w2 | bc -l)
s0=$(echo $wl/$d50l | bc -l)
s1=$(echo $wm/$d50m | bc -l)
s2=$(echo $ws/$d50s | bc -l)
# c = s * b, where s is the scale matrix with the white point
c00=$(echo $s0*$b00 | bc -l)
c01=$(echo $s0*$b01 | bc -l)
c02=$(echo $s0*$b02 | bc -l)
c10=$(echo $s1*$b10 | bc -l)
c11=$(echo $s1*$b11 | bc -l)
c12=$(echo $s1*$b12 | bc -l)
c20=$(echo $s2*$b20 | bc -l)
c21=$(echo $s2*$b21 | bc -l)
c22=$(echo $s2*$b22 | bc -l)
# 
# m = mai * c
m00=$(echo $mai00*$c00 + $mai01*$c10 + $mai02*$c20 | bc -l)
m01=$(echo $mai00*$c01 + $mai01*$c11 + $mai02*$c21 | bc -l)
m02=$(echo $mai00*$c02 + $mai01*$c12 + $mai02*$c22 | bc -l)
m10=$(echo $mai10*$c00 + $mai11*$c10 + $mai12*$c20 | bc -l)
m11=$(echo $mai10*$c01 + $mai11*$c11 + $mai12*$c21 | bc -l)
m12=$(echo $mai10*$c02 + $mai11*$c12 + $mai12*$c22 | bc -l)
m20=$(echo $mai20*$c00 + $mai21*$c10 + $mai22*$c20 | bc -l)
m21=$(echo $mai20*$c01 + $mai21*$c11 + $mai22*$c21 | bc -l)
m22=$(echo $mai20*$c02 + $mai21*$c12 + $mai22*$c22 | bc -l)

#debug: with a=identity, this results in the same matrix as bruce lindbloom has as d50->d65 chromatic adaptation (bradford)
echo "bradford adapted rgb to xyz"
echo $m00 $m01 $m02
echo $m10 $m11 $m12
echo $m20 $m21 $m22

# now invert the matrix to get xyz -> rgb as needed in the code
invdet=$(echo 1/\($m00*\($m22*$m11 - $m21*$m12\) - $m10*\($m22*$m01 - $m21*$m02\) + $m20*\($m12*$m01 - $m11*$m02\) \)| bc -l);
i00=$(echo  $invdet*\($m22*$m11 - $m21*$m12\) | bc -l)
i01=$(echo -$invdet*\($m22*$m01 - $m21*$m02\) | bc -l)
i02=$(echo  $invdet*\($m12*$m01 - $m11*$m02\) | bc -l)
i10=$(echo -$invdet*\($m22*$m10 - $m20*$m12\) | bc -l)
i11=$(echo  $invdet*\($m22*$m00 - $m20*$m02\) | bc -l)
i12=$(echo -$invdet*\($m12*$m00 - $m10*$m02\) | bc -l)
i20=$(echo  $invdet*\($m21*$m10 - $m20*$m11\) | bc -l)
i21=$(echo -$invdet*\($m21*$m00 - $m20*$m01\) | bc -l)
i22=$(echo  $invdet*\($m11*$m00 - $m10*$m01\) | bc -l)

echo "xyz to rgb"
echo $i00 $i01 $i02
echo $i10 $i11 $i12
echo $i20 $i21 $i22

# this outputs gamma values for rgb:
read -r gr gg gb <<< $(iccdump -v3 -t rTRC -t gTRC -t bTRC $1 | grep gamma | awk '{print $NF}')
echo "gamma red, green, blue:"
echo $gr $gg $gb


cat > color/colorout_custom.h << EOF
#ifndef CORONA_COLOROUT_H
#define CORONA_COLOROUT_H

static inline void colorout_xyz_to_rgb(const float *const xyz, float *rgb)
{
  // created for $1
  const float XYZtoRGB[] =
  {
    $i00, $i01, $i02,
    $i10, $i11, $i12,
    $i20, $i21, $i22,
  };

  memset(rgb, 0, sizeof(float)*3);
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++) rgb[k] += xyz[i]*XYZtoRGB[i+3*k];
  
  // apply tonecurve
  rgb[0] = powf(rgb[0], 1.f/$gr);
  rgb[1] = powf(rgb[1], 1.f/$gg);
  rgb[2] = powf(rgb[2], 1.f/$gb);
}

#endif
EOF
