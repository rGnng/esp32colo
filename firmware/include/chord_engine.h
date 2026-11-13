#pragma once
#include "gesture_engine.h"

// Front-button combinations are assembled before individual gestures dispatch.
// A single group owns its member buttons until every member is released.
class ChordEngine {
 public:
  static constexpr uint32_t kWindowMs=80;
  void seed(const bool *raw,uint32_t now) {
    singles_.seed(raw,now);previous_=0;
    for(int i=0;i<4;++i){front_[i].raw=front_[i].down=raw[i];front_[i].at=now;if(raw[i])previous_|=1u<<i;}
    cancel();
  }
  void configure(uint16_t combinations) { combinations_=combinations;cancel(); }
  void cancel() {
    singles_.cancel();window_=false;active_=pendingCount_=0;
    for(auto &click:clicks_)click=Clicks{};
    armed_=true;for(const auto &key:front_)if(key.raw||key.down)armed_=false;
  }
  bool down(int i) const {return singles_.down(i);}
  template<typename Emit> void tick(const bool *raw,int32_t detents,uint32_t now,Emit emit) {
    using namespace SceneSettings;
    uint8_t mask=0,rawMask=0;
    for(int i=0;i<4;++i) {
      auto &key=front_[i];if(raw[i]!=key.raw){key.raw=raw[i];key.at=now;}
      if(key.raw!=key.down&&uint32_t(now-key.at)>=GestureEngine::kDebounceMs)key.down=key.raw;
      if(key.down)mask|=1u<<i;
      if(raw[i])rawMask|=1u<<i;
    }
    if(!armed_&&!mask&&!rawMask)armed_=true;
    if(active_) {
      singles_.suppress(active_);
      advanceActive(mask,now,emit);
    }
    if(!window_&&!active_&&armed_&&previous_==0&&mask&&possible(mask)) {
      window_=true;windowAt_=now;union_=0;fullSeen_=broken_=false;
    }
    if(window_) {
      // Newly observed buttons after the deadline belong to ordinary inputs.
      if(uint32_t(now-windowAt_)<=kWindowMs) {
        const uint8_t combined=union_|mask;
        if(combined!=union_){union_=combined;fullSeen_=broken_=false;}
        if(mask==union_&&!fullSeen_){fullSeen_=true;joinedAt_=now;}
        if(fullSeen_&&mask!=union_&&!broken_){broken_=true;brokenAt_=now;}
      }
      if(!mask||uint32_t(now-windowAt_)>=kWindowMs) {
        const uint8_t claim=fullSeen_&&(combinations_&(1u<<union_))?union_:0;
        window_=false;
        if(claim) {
          active_=claim;held_=false;singles_.suppress(claim);
          auto &click=clicks_[claim];
          if(click.count&&uint32_t(windowAt_-click.upAt)>=GestureEngine::kClickMs){emit(kComboFlag|claim,click.count-1,1);click.count=0;}
        }
        flush(claim,emit);
        if(active_)advanceActive(mask,now,emit);
      }
    }
    singles_.tick(raw,detents,now,[&](int input,int gesture,int repeat) {
      if(active_&&input<4&&(active_&(1u<<input)))return;
      if(window_&&input<4) {
        if(pendingCount_<8){pending_[pendingCount_++]={input,gesture,repeat};return;}
        window_=false;flush(0,emit); // Bounded fallback preserves ordinary events.
      }
      emit(input,gesture,repeat);
    });
    for(int group=3;group<16;++group) {
      auto &click=clicks_[group];
      const bool joining=window_&&(union_&group)==union_&&uint32_t(windowAt_-click.upAt)<GestureEngine::kClickMs;
      if(click.count&&active_!=group&&!joining&&uint32_t(now-click.upAt)>=GestureEngine::kClickMs) {
        emit(kComboFlag|group,click.count-1,1);click.count=0;
      }
    }
    previous_=mask;
  }
 private:
  struct Key {bool raw=false,down=false;uint32_t at=0;} front_[4];
  struct Clicks {uint8_t count=0;uint32_t upAt=0;} clicks_[16];
  struct Pending {int input,gesture,repeat;} pending_[8];
  GestureEngine singles_;
  uint16_t combinations_=0;
  uint8_t previous_=0,union_=0,active_=0,pendingCount_=0;
  bool armed_=true,window_=false,fullSeen_=false,broken_=false,held_=false;
  uint32_t windowAt_=0,joinedAt_=0,brokenAt_=0;
  bool possible(uint8_t mask) const {
    for(int group=3;group<16;++group)if((combinations_&(1u<<group))&&(group&mask)==mask)return true;
    return false;
  }
  template<typename Emit> void flush(uint8_t consumed,Emit emit) {
    for(int i=0;i<pendingCount_;++i)if(!(consumed&(1u<<pending_[i].input)))emit(pending_[i].input,pending_[i].gesture,pending_[i].repeat);
    pendingCount_=0;
  }
  template<typename Emit> void advanceActive(uint8_t mask,uint32_t now,Emit emit) {
    using namespace SceneSettings;
    if((mask&active_)!=active_&&!broken_){broken_=true;brokenAt_=now;}
    auto &click=clicks_[active_];
    const uint32_t duration=(broken_?brokenAt_:now)-joinedAt_;
    if(!held_&&duration>=GestureEngine::kHoldMs){held_=true;click.count=0;emit(kComboFlag|active_,kHold,1);}
    if(!(mask&active_)) {
      if(held_)emit(kComboFlag|active_,kHoldRelease,1);
      else {click.upAt=now;++click.count;if(click.count==3){emit(kComboFlag|active_,kTriple,1);click.count=0;}}
      active_=0;
    }
  }
};
