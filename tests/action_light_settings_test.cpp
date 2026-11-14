#include "scene_settings.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace SceneSettings;
int main(){
 auto s=defaults();assert(sizeof(s)==3304&&sizeof(Action)==12);auto &a=s.bindings[0].actions[0];
 assert(s.scenes[0].ringStyle==RingFill::kClockwise);
 s.scenes[0].ringStyle=RingFill::kBottomToTop;assert(valid(s));
 auto persisted=s;assert(persisted.scenes[0].ringStyle==RingFill::kBottomToTop);
 s.scenes[0].ringStyle=RingFill::kStyleCount;assert(!valid(s));s.scenes[0].ringStyle=RingFill::kClockwise;
 a.type=kNone;a.led=kOn;a.setColorRgb(0x123456);a.setSound(true);
 for(int brightness:{-1,0,1,70,100}){a.setLedBrightness(brightness);assert(valid(s)&&a.ledBrightness()==brightness&&a.colorRgb()==0x123456&&a.soundEnabled());auto copy=s;assert(copy.bindings[0].actions[0].ledBrightness()==brightness);}
 a.color=0x66123456;assert(!valid(s));a.color=0xFF123456;assert(!valid(s));
 auto legacy=defaults();legacy.version=6;
 for(auto &b:legacy.bindings)for(auto &action:b.actions)action.setLedBrightness(0);
 auto &old=legacy.bindings[0].actions[0];old.led=kFeedbackStatic;old.setSound(true);old.color=0xABCDEF;old.feedbackMs=6000;
 auto previous=legacy;assert(migrateV6(legacy)&&valid(legacy)&&legacy.version==7);
 assert(legacy.bindings[0].actions[0].ledBrightness()==-1&&legacy.bindings[0].actions[0].colorRgb()==0xABCDEF&&legacy.bindings[0].actions[0].soundEnabled());
 auto normalized=legacy;normalized.version=6;for(auto &b:normalized.bindings)for(auto &action:b.actions)action.setLedBrightness(0);
 assert(!std::memcmp(&previous,&normalized,sizeof(previous)));
 for(int corrupt=0;corrupt<4;++corrupt){
  auto invalid=previous;
  if(corrupt==0)invalid.bindings[0].actions[0].led=kOn;
  if(corrupt==1)invalid.bindings[0].actions[0].color=0x01000000;
  if(corrupt==2)invalid.bindings[0].count=4;
  if(corrupt==3)invalid.active=7;
  const auto before=invalid;assert(!migrateV6(invalid)&&!std::memcmp(&before,&invalid,sizeof(before)));
 }
 std::cout<<"PASS action brightness/RGB/sound independence, 12-byte storage, v6 preservation and atomic invalid-record rejection\n";
}
