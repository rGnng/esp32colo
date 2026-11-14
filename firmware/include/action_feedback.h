#pragma once
#include "scene_settings.h"

// Each physical light has an independent overlay and deadline. The screen is
// independent too. Inherit/unchanged never extends an existing overlay.
class ActionFeedback {
 public:
  struct Light {
    uint8_t led=SceneSettings::kInherit;
    bool remote=false;
    int percent=0,brightness=-1;
    uint32_t color=0,remainingMs=0;
  };
  struct State {
    bool active=false;
    uint8_t screen=SceneSettings::kUnchanged;
    Light ring,keys[4];
  };
  static uint8_t targets(int input){
    if(input>=SceneSettings::kKey1&&input<=SceneSettings::kKey4)return 1u<<input;
    if(input==SceneSettings::kKnob)return 16;
    return SceneSettings::comboMask(input);
  }
  void clear() { state_=State{};for(auto &until:lightUntil_)until=0;screenUntil_=0; }
  void apply(const SceneSettings::Action &action,int percent,uint32_t now,int input){
    const uint8_t mask=targets(input);
    if(action.ledEffect()!=SceneSettings::kInherit)for(int i=0;i<5;++i)if(mask&(1u<<i)){
      auto &light=i==4?state_.ring:state_.keys[i];
      light.led=action.ledEffect();light.color=action.colorRgb();light.brightness=action.ledBrightness();
      light.remote=action.type==SceneSettings::kBluetoothVolume&&light.led==SceneSettings::kProportional;
      light.percent=percent<0?0:percent;light.remainingMs=action.feedbackMs;lightUntil_[i]=now+action.feedbackMs;
    }
    if(action.screen!=SceneSettings::kUnchanged){state_.screen=action.screen;screenUntil_=now+action.feedbackMs;}
  }
  State snapshot(uint32_t now){
    state_.active=false;
    for(int i=0;i<5;++i){
      auto &light=i==4?state_.ring:state_.keys[i];
      if(light.led==SceneSettings::kInherit)continue;
      if(static_cast<int32_t>(now-lightUntil_[i])>=0)light=Light{};
      else{light.remainingMs=lightUntil_[i]-now;state_.active=true;}
    }
    if(state_.screen!=SceneSettings::kUnchanged&&static_cast<int32_t>(now-screenUntil_)>=0)state_.screen=SceneSettings::kUnchanged;
    state_.active|=state_.screen!=SceneSettings::kUnchanged;
    return state_;
  }
 private:
  State state_;
  uint32_t lightUntil_[5]={},screenUntil_=0;
};
