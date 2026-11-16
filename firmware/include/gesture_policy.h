#pragma once
#include "scene_settings.h"
struct GestureBindings {
  struct Hold {uint16_t ms=600;bool repeat=false;};
  uint16_t physical[SceneSettings::kInputCount],combos[16],ordered[4][16]={};
  Hold physicalHold[SceneSettings::kInputCount],comboHold[16],orderedHold[4][16];
  explicit GestureBindings(uint16_t value=0){for(auto &m:physical)m=value;for(auto &m:combos)m=value;}
  uint16_t policy(int group)const{return group<16?combos[group]:ordered[(group>>4)-1][group&15];}
  Hold hold(int group)const{return group<16?comboHold[group]:orderedHold[(group>>4)-1][group&15];}
  void setHold(int input,uint16_t ms,bool repeat){
    Hold *out=SceneSettings::comboMask(input)?&((input&0x70)?orderedHold[((input>>4)&7)-1][input&15]:comboHold[input&15]):&physicalHold[input];
    out->ms=ms;out->repeat=repeat;
  }
};
namespace GesturePolicy {
constexpr uint16_t kAll=(1u<<SceneSettings::kGestureCount)-1;
inline bool has(uint16_t mask,int gesture){return (mask&(1u<<gesture))!=0;}
inline bool hold(uint16_t mask){return has(mask,SceneSettings::kHold)||has(mask,SceneSettings::kHoldRelease);}
inline int clicks(uint16_t mask){return has(mask,SceneSettings::kTriple)?3:has(mask,SceneSettings::kDouble)?2:1;}
inline bool press(uint16_t mask){return has(mask,SceneSettings::kSingle)&&clicks(mask)==1&&!hold(mask);}
}
