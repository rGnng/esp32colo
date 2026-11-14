#pragma once
#include <cstdint>
#include "ring_fill.h"
namespace PlaybackRing {
// Consume the shared display clock; never restart interpolation per packet.
inline int progress(bool available,int playback,uint32_t position,uint32_t duration){
  if(!available||playback<0||playback>2||!duration)return -1;
  if(playback==0)return 0;
  if(position>duration)position=duration;
  return uint64_t(position)*1000/duration;
}
using RingFill::coverage;
}
