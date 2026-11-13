#pragma once
#include "scene_settings.h"
struct GestureBindings {
  uint16_t physical[SceneSettings::kInputCount],combos[16];
  explicit GestureBindings(uint16_t value=0){for(auto &m:physical)m=value;for(auto &m:combos)m=value;}
};
namespace GesturePolicy {
constexpr uint16_t kAll=(1u<<SceneSettings::kGestureCount)-1;
inline bool has(uint16_t mask,int gesture){return (mask&(1u<<gesture))!=0;}
inline bool hold(uint16_t mask){return has(mask,SceneSettings::kHold)||has(mask,SceneSettings::kHoldRelease);}
inline int clicks(uint16_t mask){return has(mask,SceneSettings::kTriple)?3:has(mask,SceneSettings::kDouble)?2:1;}
inline bool press(uint16_t mask){return has(mask,SceneSettings::kSingle)&&clicks(mask)==1&&!hold(mask);}
}
