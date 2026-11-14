#pragma once
#include "gesture_engine.h"

// Front-button combinations are assembled before individual gestures dispatch.
// Releasing any member rearms the group; held members can anchor the next one.
class ChordEngine {
 public:
  static constexpr uint32_t kWindowMs=80;
  void seed(const bool *raw,uint32_t now) {
    singles_.seed(raw,now);previous_=0;
    for(int i=0;i<4;++i){front_[i].raw=front_[i].down=raw[i];front_[i].at=front_[i].pressedAt=now;if(raw[i])previous_|=1u<<i;}
    armed_=previous_==0;cancel();
  }
  void configure(uint16_t combinations,const GestureBindings &bindings=GestureBindings(GesturePolicy::kAll)) {
    combinations_=combinations;bindings_=bindings;
    uint8_t members=0;for(int group=3;group<16;++group){if(bindings_.policy(group)&&(combinations_&(1u<<group)))members|=group;for(int modifier=0;modifier<4;++modifier)if(bindings_.ordered[modifier][group])members|=1u<<modifier;}
    singles_.setBindings(bindings_.physical,members);cancel();
  }
  void cancel() {
    singles_.cancel();window_=false;active_=pendingCount_=0;
    for(auto &click:clicks_)click=Clicks{};
    previous_=suppressedEdges_=0;for(int i=0;i<4;++i){if(front_[i].down)previous_|=1u<<i;if(front_[i].raw||front_[i].down)suppressedEdges_|=1u<<i;}
  }
  bool down(int i) const {return singles_.down(i);}
  template<typename Emit> void tick(const bool *raw,int32_t detents,uint32_t now,Emit emit) {
    using namespace SceneSettings;
    uint8_t mask=0,rawMask=0;
    for(int i=0;i<4;++i) {
      auto &key=front_[i];if(raw[i]!=key.raw){key.raw=raw[i];key.at=now;}
      if(key.raw!=key.down&&uint32_t(now-key.at)>=GestureEngine::kDebounceMs){key.down=key.raw;if(key.down)key.pressedAt=key.at;}
      if(key.down)mask|=1u<<i;
      if(raw[i])rawMask|=1u<<i;
    }
    if(!armed_&&!mask&&!rawMask)armed_=true;
    suppressedEdges_&=mask|rawMask;
    const uint8_t pressed=(mask&~previous_)&~suppressedEdges_;
    if(active_) {
      singles_.suppress(active_&15);
      advanceActive(mask,now,emit);
    }
    for(int attempt=0;attempt<2;++attempt) {
    if(!window_&&!active_&&armed_&&pressed&&possible(mask)) {
      window_=true;windowAt_=now;union_=0;fullSeen_=broken_=false;
    }
    if(window_) {
      // Late edges may start a new candidate with an already-held member.
      if(uint32_t(now-windowAt_)<=kWindowMs) {
        const uint8_t combined=union_|mask;
        if(combined!=union_){union_=combined;fullSeen_=broken_=false;}
        if(mask==union_&&!fullSeen_){fullSeen_=true;joinedAt_=now;}
        if(fullSeen_&&mask!=union_&&!broken_){broken_=true;brokenAt_=now;}
      }
      if(!mask||uint32_t(now-windowAt_)>=kWindowMs||(selectGroup(mask)&&!superset(mask))) {
        const uint8_t claim=fullSeen_?selectGroup(union_):0;
        window_=false;
        if(claim) {
          active_=claim;held_=fired_=false;singles_.suppress(claim&15);
          auto &click=clicks_[claim];
          if(click.count&&uint32_t(windowAt_-click.upAt)>=GestureEngine::kClickMs){send(emit,claim,click.count-1);click.count=0;}
        }
        flush(claim&15,emit);
        if(active_)advanceActive(mask,now,emit);
      }
    }
    }
    singles_.tick(raw,detents,now,[&](int input,int gesture,int repeat) {
      if(active_&&input<4&&((active_&15)&(1u<<input)))return;
      if(window_&&input<4) {
        if(pendingCount_<8){pending_[pendingCount_++]={input,gesture,repeat};return;}
        window_=false;flush(0,emit); // Bounded fallback preserves ordinary events.
      }
      emit(input,gesture,repeat);
    });
    for(int group=3;group<80;++group) {
      auto &click=clicks_[group];
      const bool joining=window_&&(union_&(group&15))==union_&&uint32_t(windowAt_-click.upAt)<GestureEngine::kClickMs;
      if(click.count&&active_!=group&&!joining&&uint32_t(now-click.upAt)>=GestureEngine::kClickMs) {
        send(emit,group,click.count-1);click.count=0;
      }
    }
    previous_=mask;
  }
 private:
  struct Key {bool raw=false,down=false;uint32_t at=0,pressedAt=0;} front_[4];
  struct Clicks {uint8_t count=0;uint32_t upAt=0;} clicks_[80];
  struct Pending {int input,gesture,repeat;} pending_[8];
  GestureEngine singles_;
  uint16_t combinations_=0;
  GestureBindings bindings_{GesturePolicy::kAll};
  uint8_t previous_=0,union_=0,active_=0,pendingCount_=0,suppressedEdges_=0;
  bool armed_=true,window_=false,fullSeen_=false,broken_=false,held_=false,fired_=false;
  uint32_t windowAt_=0,joinedAt_=0,brokenAt_=0;
  bool first(int modifier,uint8_t mask)const {
    if(!(mask&(1u<<modifier)))return false;
    for(int key=0;key<4;++key)if(key!=modifier&&(mask&(1u<<key))&&
       int32_t(front_[key].pressedAt-front_[modifier].pressedAt)<=0)return false;
    return true;
  }
  bool canJoin(uint8_t group,uint8_t mask)const {
    if(bindings_.combos[group])return true;
    for(int modifier=0;modifier<4;++modifier)if(bindings_.ordered[modifier][group]&&first(modifier,mask))return true;
    return false;
  }
  uint8_t selectGroup(uint8_t mask)const {
    if(!mask||!(combinations_&(1u<<mask)))return 0;
    for(int modifier=0;modifier<4;++modifier)if(bindings_.ordered[modifier][mask]&&first(modifier,mask))return ((modifier+1)<<4)|mask;
    return bindings_.combos[mask]?mask:0;
  }
  bool possible(uint8_t mask) const {
    for(int group=3;group<16;++group)if((combinations_&(1u<<group))&&(group&mask)==mask&&canJoin(group,mask))return true;
    return false;
  }
  template<typename Emit> void flush(uint8_t consumed,Emit emit) {
    for(int i=0;i<pendingCount_;++i)if(!(consumed&(1u<<pending_[i].input)))emit(pending_[i].input,pending_[i].gesture,pending_[i].repeat);
    pendingCount_=0;
  }
  bool superset(uint8_t mask) const {
    for(int group=3;group<16;++group)if(group!=mask&&(combinations_&(1u<<group))&&(group&mask)==mask&&canJoin(group,mask))return true;
    return false;
  }
  template<typename Emit> void send(Emit &emit,int group,int gesture){if(GesturePolicy::has(bindings_.policy(group),gesture))emit(SceneSettings::kComboFlag|group,gesture,1);}
  template<typename Emit> void advanceActive(uint8_t mask,uint32_t now,Emit emit) {
    using namespace SceneSettings;
    if((mask&(active_&15))!=(active_&15)&&!broken_){broken_=true;brokenAt_=now;}
    auto &click=clicks_[active_];const auto policy=bindings_.policy(active_);
    if(!fired_&&GesturePolicy::press(policy)){send(emit,active_,kSingle);fired_=true;}
    const uint32_t duration=(broken_?brokenAt_:now)-joinedAt_;
    if(!held_&&!fired_&&GesturePolicy::hold(policy)&&duration>=GestureEngine::kHoldMs){held_=true;click.count=0;send(emit,active_,kHold);}
    if(broken_) {
      if(held_)send(emit,active_,kHoldRelease);
      else if(!fired_){click.upAt=now;++click.count;if(click.count==GesturePolicy::clicks(policy)){send(emit,active_,click.count-1);click.count=0;}}
      active_=0;
    }
  }
};
