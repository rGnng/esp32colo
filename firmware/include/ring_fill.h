#pragma once
#include <cstdint>

namespace RingFill {
enum Style : uint8_t { kClockwise, kBottomToTop, kStyleCount };
constexpr const char *kStyles[]={"clockwise","bottom_to_top"};

// Coverage is in permille. Symmetric pairs share each increment, including
// partial pixels; a pixel on the vertical axis occupies one slot instead of two.
// topPixel identifies the physical top, independently of chain direction.
inline int coverage(int value,int pixel,int count,uint8_t style=kClockwise,int topPixel=0){
  if(count<=0||pixel<0||pixel>=count||value<=0)return 0;
  if(value>=1000)return 1000;
  int filled=value*count;
  if(style==kBottomToTop){
    const int around=2*count;
    const int bottom=(2*(topPixel%count)+count)%around;
    int distance=2*pixel-bottom;
    if(distance<0)distance=-distance;
    if(distance>count)distance=around-distance;
    const int before=distance>0?distance-1:0;
    const int members=distance==0||distance==count?1:2;
    filled=(filled-before*1000)/members;
  }else filled-=pixel*1000;
  return filled<0?0:filled>1000?1000:filled;
}
}
