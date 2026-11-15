#pragma once

namespace VolumeDisplay {
inline bool valid(float level){return level>=0.0f&&level<=1.0f;}

// Preview provides immediate feedback; a terminal reply restores the host value.
// Unavailable/stale feedback must never look like an actual zero volume.
inline float value(bool fresh,bool available,bool pending,float confirmed,float preview){
  if(!fresh||!available)return -1.0f;
  if(pending&&valid(preview))return preview;
  return valid(confirmed)?confirmed:-1.0f;
}
inline int pixels(float level,int width){
  return valid(level)&&width>0?int(level*width+0.5f):0;
}
}
