#pragma once

namespace VolumeDisplay {
inline bool valid(float level){return level>=0.0f&&level<=1.0f;}
inline int percent(float level){return valid(level)?int(level*100.0f+0.5f):-1;}

// Render the synchronized display level; prediction and settling are upstream.
inline float value(bool fresh,bool available,float confirmed){
  if(!fresh||!available)return -1.0f;
  return valid(confirmed)?confirmed:-1.0f;
}
inline int pixels(float level,int width){
  return valid(level)&&width>0?int(level*width+0.5f):0;
}
}
