#pragma once
#include <cstdint>
#include "gesture_policy.h"
namespace InputWorker {
struct Event {uint32_t generation=0,atMs=0;int16_t input=0;uint8_t gesture=0,reserved=0;int32_t repeat=1;};
struct State {
  bool ready=false;uint8_t down=0;uint16_t reserved=0;
  uint32_t presses[SceneSettings::kInputCount]={};
  uint32_t activity=0,samples=0,maxGapUs=0,dropped=0,generation=0;
  int32_t edges=0,position=0;
};
uint32_t configure(uint16_t combinations,const GestureBindings &bindings,uint32_t epoch);
bool begin(bool encoderReady);
State snapshot();
bool next(Event &event);
void discard();
void resetTiming();
}
