#include "action_feedback.h"
#include <cassert>
#include <iostream>
using namespace SceneSettings;
int main() {
 ActionFeedback f;
 Action led;led.type=kVolumeSet;led.value=50;led.led=kProportional;led.feedbackMs=3000;
 Action screen;screen.screen=kClock;screen.feedbackMs=250;
 f.apply(led,50,1000);f.apply(screen,-1,1100);
 auto state=f.snapshot(1400);assert(state.active&&state.led==kProportional&&state.percent==50&&state.screen==kUnchanged);
 assert(!f.snapshot(4000).active);
 screen.feedbackMs=5000;f.apply(screen,-1,5000);led.feedbackMs=250;f.apply(led,0,5100);
 state=f.snapshot(5400);assert(state.active&&state.led==kInherit&&state.screen==kClock);
 Action unchanged;f.apply(unchanged,-1,6000);assert(f.snapshot(9999).screen==kClock);assert(!f.snapshot(10000).active);
 led.feedbackMs=500;f.apply(led,75,0xFFFFFFF0u);assert(f.snapshot(0xFFFFFFF0u+499).percent==75);assert(!f.snapshot(0xFFFFFFF0u+500).active);
 f.apply(screen,-1,100);f.clear();assert(!f.snapshot(101).active);
 f.apply(led,40,100);led.led=kFeedbackStatic;f.apply(led,-1,200);assert(f.snapshot(201).led==kFeedbackStatic);
 // Audio flags must never appear as LED effects or break remote progress.
 for(int effect=0;effect<kLedCount;++effect){
   f.clear();led.type=kBluetoothVolume;led.led=effect;led.setSound(true);
   f.apply(led,42,100);state=f.snapshot(101);
   assert(state.led==effect&&state.remote==(effect==kProportional));
 }
 std::cout<<"PASS independent screen/LED overlays, original deadlines, inheritance, replacement, zero, clear and rollover\n";
}
