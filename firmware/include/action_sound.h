#pragma once
#include "scene_settings.h"
#include "scene_cue.h"

namespace ActionSound {
inline bool sceneChange(const SceneSettings::Action &a) {
  return a.type>=SceneSettings::kNextScene&&a.type<=SceneSettings::kSwitchScene;
}
inline unsigned frequency(const SceneSettings::Action &a,int previous,int next) {
  if(!a.soundEnabled())return 0;
  if(a.type==SceneSettings::kTone)return a.value;
  if(sceneChange(a))return SceneCue::frequency(true,previous,next,
    a.type==SceneSettings::kNextScene?1:a.type==SceneSettings::kPreviousScene?-1:0);
  return 660;
}
inline unsigned duration(const SceneSettings::Action &a) {
  return a.type==SceneSettings::kTone?200:sceneChange(a)?SceneCue::kDurationMs:50;
}
}
