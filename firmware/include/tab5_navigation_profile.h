#pragma once
#include "scene_settings.h"
namespace Tab5NavigationProfile {
inline bool install(SceneSettings::Stored &s,int id){
  using namespace SceneSettings;
  if(!valid(s)||id<=0||id>=kSceneCount||exists(s,id))return false;
  int free=0;for(const auto &b:s.bindings)if(!b.count)++free;
  if(free<11)return false;
  for(const auto &scene:s.scenes)if(scene.used&&!std::strcmp(scene.name,"Tab5 Navigation"))return false;
  s.scenes[id]=Scene{};auto &scene=s.scenes[id];scene.used=1;
  std::strcpy(scene.name,"Tab5 Navigation");scene.screen=kBluetoothNavigationScreen;
  scene.ringDisplay=kRingBluetoothProgress;
  auto key=[&](int input,int gesture,int usage,bool repeat=false){
    Binding b;b.scene=id;b.input=input;b.gesture=gesture;b.count=1;
    b.actions[0].type=kBluetoothKey;b.actions[0].value=usage;
    if(gesture==kHold)b.setHold(350,repeat);
    assign(s,b);
  };
  key(kKnob,kLeft,0x52);key(kKnob,kRight,0x51);key(kKnob,kSingle,0x28);
  key(kKey1,kSingle,0x29);key(kKey2,kSingle,0x2f);key(kKey3,kSingle,0x30);key(kKey4,kSingle,0x06);
  key(kKey2,kHold,0x4b,true);key(kKey3,kHold,0x4e,true);
  for(int direction=-1;direction<=1;direction+=2){
    Binding b;b.scene=id;b.input=kKnob;b.gesture=direction<0?kPressLeft:kPressRight;b.count=1;
    auto &a=b.actions[0];a.type=kBluetoothVolume;a.value=direction;a.screen=kBluetoothVolumeScreen;a.led=kProportional;assign(s,b);
  }
  return valid(s);
}
}
