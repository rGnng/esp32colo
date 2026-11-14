#include "action_feedback.h"
#include "action_lighting.h"
#include <cassert>
#include <iostream>
using namespace SceneSettings;
int main(){
 ActionFeedback f;Action a;a.led=kFeedbackStatic;a.setColorRgb(0xFF0000);a.setLedBrightness(80);a.feedbackMs=1000;
 f.apply(a,-1,100,kKey1);auto s=f.snapshot(101);
 assert(s.keys[0].led==kFeedbackStatic&&s.keys[0].color==0xFF0000&&s.keys[0].brightness==80&&s.keys[0].remainingMs==999);
 for(int i=1;i<4;++i){assert(s.keys[i].led==kInherit);}assert(s.ring.led==kInherit);
 a.led=kOff;a.feedbackMs=2000;f.apply(a,-1,200,kKey2);s=f.snapshot(1100);
 assert(s.keys[0].led==kInherit&&s.keys[1].led==kOff&&s.ring.led==kInherit);assert(!f.snapshot(2200).active);
 // Each key and the ring preserve their own deadlines and brightness.
 for(int i=0;i<4;++i){a.led=kOn;a.feedbackMs=1000+i*250;a.setLedBrightness(i*25);f.apply(a,0,3000,i);}
 a.led=kProportional;a.type=kBluetoothVolume;a.feedbackMs=3000;a.setSound(true);f.apply(a,50,3000,kKnob);
 s=f.snapshot(4000);assert(s.keys[0].led==kInherit&&s.keys[1].led==kOn&&s.keys[1].brightness==25&&s.ring.remote);
 Action keep;f.apply(keep,-1,4100,kKey2);assert(f.snapshot(4250).keys[1].led==kInherit);
 assert(!f.snapshot(6000).active);
 // Combo feedback affects its member keys, without touching the ring or others.
 a=Action{};a.led=kFeedbackRainbow;a.feedbackMs=500;f.apply(a,0,7000,comboInput(10,3));s=f.snapshot(7001);
 assert(s.keys[0].led==kInherit&&s.keys[1].led==kFeedbackRainbow&&s.keys[2].led==kInherit&&s.keys[3].led==kFeedbackRainbow&&s.ring.led==kInherit);
 f.clear();a.screen=kClock;f.apply(a,0,8000,kOpen);s=f.snapshot(8001);assert(s.active&&s.screen==kClock);for(const auto &key:s.keys){assert(key.led==kInherit);}assert(s.ring.led==kInherit);
 a=Action{};a.screen=kClock;a.feedbackMs=2000;f.apply(a,0,9000,kRec);
 a.screen=kUnchanged;a.led=kFeedbackStatic;a.feedbackMs=250;f.apply(a,0,9100,kKey1);s=f.snapshot(9400);assert(s.screen==kClock&&s.keys[0].led==kInherit);assert(!f.snapshot(11000).active);
 a.feedbackMs=500;f.apply(a,0,0xFFFFFFF0u,kKey3);assert(f.snapshot(0xFFFFFFF0u+499).keys[2].led==kFeedbackStatic);assert(!f.snapshot(0xFFFFFFF0u+500).active);
 f.clear();assert(!f.snapshot(1).active);
 // Exercise the production color/brightness resolver with independent targets.
 a=Action{};a.led=kFeedbackStatic;a.setColorRgb(0xFF0000);a.setLedBrightness(80);f.apply(a,0,0,kKey1);s=f.snapshot(1);
 auto resolve=[&](const ActionFeedback::Light &cue,bool ring=false,int pixel=0,bool fresh=true,int remote=50){return ActionLighting::resolve(cue,0x112233,0xFF0000,0x00FF00,21,ring,pixel,9,fresh,remote);};
 auto r=resolve(s.keys[0]);assert(r.applied&&r.color==0xFF0000&&r.brightness==80);
 for(int i=1;i<4;++i){r=resolve(s.keys[i]);assert(!r.applied&&r.color==0x112233&&r.brightness==21);}r=resolve(s.ring,true);assert(!r.applied&&r.brightness==21);
 ActionFeedback::Light cue;cue.led=kOn;cue.brightness=0;r=resolve(cue);assert(r.applied&&r.color==0x112233&&r.brightness==0);
 cue.brightness=-1;r=resolve(cue);assert(r.brightness==21);cue.led=kOff;assert(resolve(cue).color==0);
 cue.led=kFeedbackRainbow;assert(resolve(cue).color==0x00FF00);
 cue.led=kProportional;cue.percent=50;r=resolve(cue);assert(r.color==DemoLevels::scaleRgb(0xFF0000,LightingSettings::pwm(50)));
 assert(resolve(cue,true,0).color==0xFF0000&&resolve(cue,true,8).color==0);cue.remote=true;assert(!resolve(cue,true,0,false).applied);assert(!resolve(cue,false,0,true,-1).applied);
 // Bluetooth volume uses the same symmetric coverage as playback, with live
 // remote/preview percentages and unchanged per-button percentage brightness.
 for(int p:{0,10,50,90,100})for(int i=0;i<9;++i){
  const auto symmetric=ActionLighting::resolve(cue,0x112233,0xFF0000,0,21,true,i,9,true,p,RingFill::kBottomToTop);
  const int expected=RingFill::coverage(p*10,i,9,RingFill::kBottomToTop);
  assert(symmetric.applied&&symmetric.color==DemoLevels::scaleRgb(0xFF0000,(expected*255+500)/1000));
 }
 assert(ActionLighting::resolve(cue,0,0xFF0000,0,21,true,0,9,true,50,RingFill::kBottomToTop).color==0);
 assert(ActionLighting::resolve(cue,0,0xFF0000,0,21,true,4,9,true,50,RingFill::kBottomToTop).color==0xFF0000);
 assert(ActionLighting::resolve(cue,0,0xFF0000,0,21,false,0,9,true,50,RingFill::kBottomToTop).color==resolve(cue).color);
 std::cout<<"PASS independent key/ring targets, brightness, deadlines, inheritance, combinations, screen, rollover and rendered colors\n";
}
