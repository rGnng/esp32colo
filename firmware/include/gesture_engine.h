#pragma once
#include "gesture_policy.h"
#include <cstdint>

// Independent physical input recognizers. Unused gestures consume no timers.
class GestureEngine {
 public:
  static constexpr uint32_t kDebounceMs=25,kClickMs=300,kHoldMs=600;
  struct State {
    bool raw=false,down=false,blocked=false,held=false,turned=false,fired=false;
    uint8_t clicks=0;
    uint32_t rawAt=0,downAt=0,upAt=0;
  };
  void setBindings(const uint16_t *masks,uint8_t deferPress=0){for(int i=0;i<SceneSettings::kInputCount;++i)bindings_[i]=masks[i];deferPress_=deferPress;}
  void seed(const bool *raw,uint32_t now){
    for(int i=0;i<SceneSettings::kInputCount;++i){states_[i]=State{};states_[i].raw=states_[i].down=raw[i];states_[i].blocked=raw[i];states_[i].rawAt=now;}
  }
  void cancel(){for(auto &s:states_){s.clicks=0;s.blocked=s.raw||s.down;s.held=s.turned=s.fired=false;}}
  void suppress(uint8_t mask){for(int i=0;i<4;++i)if(mask&(1u<<i)){auto &s=states_[i];s.clicks=0;s.blocked=s.raw||s.down;s.held=s.turned=s.fired=false;}}
  bool down(int i) const{return states_[i].down;}
  template<typename Emit> void tick(const bool *raw,int32_t detents,uint32_t now,Emit emit){
    using namespace SceneSettings;
    for(int i=0;i<kInputCount;++i){
      auto &s=states_[i];
      if(raw[i]!=s.raw){s.raw=raw[i];s.rawAt=now;}
      if(s.raw==s.down||uint32_t(now-s.rawAt)<kDebounceMs)continue;
      s.down=s.raw;
      if(s.down){
        if(s.clicks&&uint32_t(now-s.upAt)>=kClickMs){send(emit,i,s.clicks-1);s.clicks=0;}
        s.downAt=now;s.held=s.fired=false;if(i!=kKnob)s.turned=false;
        const bool modifiedTurn=i==kKnob&&(GesturePolicy::has(bindings_[i],kPressLeft)||GesturePolicy::has(bindings_[i],kPressRight));
        if(!s.blocked&&!s.turned&&!(deferPress_&(1u<<i))&&!modifiedTurn&&GesturePolicy::press(bindings_[i])){send(emit,i,kSingle);s.fired=true;}
      }else{
        if(!s.blocked&&!s.turned&&!s.fired){
          if(!s.held&&GesturePolicy::hold(bindings_[i])&&uint32_t(now-s.downAt)>=kHoldMs){s.held=true;s.clicks=0;send(emit,i,kHold);}
          if(s.held)send(emit,i,kHoldRelease);
          else{
            ++s.clicks;s.upAt=now;
            if(s.clicks==GesturePolicy::clicks(bindings_[i])){send(emit,i,s.clicks-1);s.clicks=0;}
          }
        }
        s.blocked=false;s.held=s.turned=s.fired=false;
      }
    }
    auto &knob=states_[kKnob];
    if(detents&&!knob.blocked){
      const bool modified=knob.down||raw[kKnob];
      if(modified){knob.turned=true;knob.clicks=0;}
      send(emit,kKnob,modified?(detents<0?kPressLeft:kPressRight):(detents<0?kLeft:kRight),detents<0?-detents:detents);
    }
    for(int i=0;i<kInputCount;++i){
      auto &s=states_[i];if(s.blocked)continue;
      if(s.down&&!s.held&&!s.turned&&!s.fired&&GesturePolicy::hold(bindings_[i])&&uint32_t(now-s.downAt)>=kHoldMs){s.clicks=0;s.held=true;send(emit,i,kHold);}
      if(!s.down&&s.clicks&&uint32_t(now-s.upAt)>=kClickMs){send(emit,i,s.clicks-1);s.clicks=0;}
    }
  }
 private:
  State states_[SceneSettings::kInputCount];
  uint16_t bindings_[SceneSettings::kInputCount]={GesturePolicy::kAll,GesturePolicy::kAll,GesturePolicy::kAll,GesturePolicy::kAll,GesturePolicy::kAll,GesturePolicy::kAll,GesturePolicy::kAll};
  uint8_t deferPress_=0;
  template<typename Emit> void send(Emit &emit,int input,int gesture,int repeat=1){if(GesturePolicy::has(bindings_[input],gesture))emit(input,gesture,repeat);}
};
