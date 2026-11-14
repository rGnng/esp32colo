#pragma once
#include "scene_settings.h"

// Screen and LED overlays are independent. Inherit/unchanged preserves the
// other overlay, including its original expiration time.
class ActionFeedback {
 public:
  struct State {
    bool active=false,remote=false;
    uint8_t led=SceneSettings::kInherit, screen=SceneSettings::kUnchanged;
    int percent=0;
    uint32_t color=0;
  };
  void clear() { state_=State{}; }
  void apply(const SceneSettings::Action &action,int percent,uint32_t now) {
    if (action.ledEffect()!=SceneSettings::kInherit) {
      state_.led=action.ledEffect(); state_.color=action.color;
      state_.remote=action.type==SceneSettings::kBluetoothVolume&&action.ledEffect()==SceneSettings::kProportional;
      state_.percent=percent<0?0:percent; ledUntil_=now+action.feedbackMs;
    }
    if (action.screen!=SceneSettings::kUnchanged) {
      state_.screen=action.screen; screenUntil_=now+action.feedbackMs;
    }
  }
  State snapshot(uint32_t now) {
    if (state_.led!=SceneSettings::kInherit&&static_cast<int32_t>(now-ledUntil_)>=0) {
      state_.led=SceneSettings::kInherit;state_.percent=0;state_.color=0;
    }
    if (state_.screen!=SceneSettings::kUnchanged&&static_cast<int32_t>(now-screenUntil_)>=0) state_.screen=SceneSettings::kUnchanged;
    state_.active=state_.led!=SceneSettings::kInherit||state_.screen!=SceneSettings::kUnchanged;
    return state_;
  }
 private:
  State state_;
  uint32_t ledUntil_=0,screenUntil_=0;
};
