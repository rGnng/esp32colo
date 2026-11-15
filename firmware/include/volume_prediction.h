#pragma once
#include <cstdint>
#include "volume_grid.h"
#include "coloplay_ble_protocol.h"

// No allocations or I/O. Callers serialize mutations with their BLE state lock.
class VolumePrediction {
 public:
  static constexpr unsigned kCapacity=64,kQuietMs=150,kBlendMs=100,kTimeoutMs=5000;
  void reset(){*this=VolumePrediction();}
  void configure(bool enabled,int steps){
    if(!enabled){reset();return;}
    enabled_=true;
    if(volume_scale_valid(steps)){if(steps_!=steps)ready_=false;steps_=steps;}
    else ready_=false;
  }
  void invalidate(){count_=0;ready_=false;blocked_=false;hasInput_=false;}
  bool canQueue()const{return !enabled_||count_<kCapacity;}
  bool queued(uint16_t request,int delta,uint32_t now){
    if(!enabled_)return true;
    if(!request||request==watermark_||!delta||count_==kCapacity)return false;
    for(auto &id:retired_)if(id==request)id=0;
    pending_[count_++]={request,int16_t(delta),now};
    lastInput_=now;hasInput_=true;project();return true;
  }
  // A watermark describes the snapshot, independently of the enclosing ACK ID.
  // Thus late replies cannot erase turns already based on a newer snapshot.
  bool observe(float actual,float target,int steps,uint16_t completed,
               uint16_t request,int result,uint32_t now){
    if(!enabled_||!volume_scale_valid(steps))return false;
    const bool failed=request&&(result==CP_FAILED||result==CP_UNSUPPORTED||result==CP_BUSY);
    if(failed)for(unsigned i=0;i<count_;++i)if(pending_[i].request==request){
      erase(i);blocked_=true;ready_=false;hasInput_=false;break;
    }
    int matched=-1;
    for(unsigned i=0;i<count_;++i)if(pending_[i].request==completed)matched=int(i);
    if(watermark_!=completed){
      for(auto id:retired_)if(completed&&id==completed)return false;
      if(matched<0&&!newer(completed,watermark_))return false;
      if(watermark_)retired_[retiredAt_++%kCapacity]=watermark_;
    }
    const unsigned before=count_;
    watermark_=completed;steps_=steps;
    unsigned keep=0;
    for(unsigned i=0;i<count_;++i)
      if(matched>=0?int(i)>matched:!completed||newer(pending_[i].request,completed))pending_[keep++]=pending_[i];
    count_=keep;
    if(!valid(actual)){invalidate();return true;}
    base_=valid(target)?target:actual;
    if(!count_)blocked_=false;
    ready_=!blocked_;
    if(before&&!count_)settledAt_=now;
    project();return true;
  }
  bool expire(uint32_t now){
    if(count_&&uint32_t(now-pending_[0].at)>kTimeoutMs){invalidate();return true;}
    return false;
  }
  float display(uint32_t now,float fallback)const{
    if(!ready_||!hasInput_)return volume_grid_display(fallback,steps_);
    const uint32_t idle=now-lastInput_;
    if(count_||idle<kQuietMs)return volume_grid_display(projected_,steps_);
    const uint32_t confirmed=now-settledAt_;
    const uint32_t elapsed=idle-kQuietMs<confirmed?idle-kQuietMs:confirmed;
    if(elapsed>=kBlendMs)return volume_grid_display(fallback,steps_);
    const float from=volume_grid_display(projected_,steps_),to=volume_grid_display(fallback,steps_);
    return from+(to-from)*(float(elapsed)/kBlendMs);
  }
  bool active(uint32_t now)const{
    if(!ready_||!hasInput_)return false;
    const uint32_t idle=now-lastInput_;
    return count_||idle<kQuietMs+kBlendMs||uint32_t(now-settledAt_)<kBlendMs;
  }
  float predicted()const{return ready_&&hasInput_?projected_:-1.0f;}
  bool ready()const{return ready_;}
  unsigned pending()const{return count_;}
  int steps()const{return steps_;}
  uint16_t completed()const{return watermark_;}
 private:
  struct Pending{uint16_t request;int16_t delta;uint32_t at;};
  Pending pending_[kCapacity]={};
  uint16_t retired_[kCapacity]={};
  unsigned retiredAt_=0;
  unsigned count_=0;
  uint32_t lastInput_=0,settledAt_=0;
  uint16_t watermark_=0;
  int steps_=0;
  bool enabled_=false,ready_=false,blocked_=false,hasInput_=false;
  float base_=-1,projected_=-1;
  static bool valid(float value){return value>=0&&value<=1;}
  static bool newer(uint16_t a,uint16_t b){return a&&(b==0||int16_t(a-b)>0);}
  void erase(unsigned i){for(unsigned j=i+1;j<count_;++j)pending_[j-1]=pending_[j];--count_;}
  void project(){
    if(!ready_)return;
    projected_=base_;
    for(unsigned i=0;i<count_;++i)projected_=volume_grid_step(projected_,pending_[i].delta,steps_);
  }
};
