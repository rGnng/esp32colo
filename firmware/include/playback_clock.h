#pragma once
#include <cstdint>

// Position samples arrive slower than render frames and may be repeated by
// heartbeats/command replies. Keep arrival freshness separate from the display
// anchor; ordinary updates change clock speed slightly, never its position.
class PlaybackClock {
 public:
  static constexpr uint32_t kFreshMs=5000,kStallMs=3000,kSeekJumpMs=2000;
  void reset(){*this=PlaybackClock();}
  void track(uint64_t key){
    if(trackKnown_&&key!=track_)initialized_=false;
    track_=key;trackKnown_=true;
  }
  bool valid(uint32_t now)const{return initialized_&&uint32_t(now-packetAt_)<kFreshMs;}
  uint32_t position(uint32_t now)const {
    if(!initialized_||playback_==0)return 0;
    int64_t value=anchorMs_;
    if(playback_==1&&!seek_){
      uint32_t dt=now-anchorAt_;if(dt>kStallMs)dt=kStallMs;
      const int64_t budget=dt/8; // Bounded 0.875x..1.125x correction rate.
      const int64_t correction=correctionMs_>budget?budget:correctionMs_<-budget?-budget:correctionMs_;
      value+=dt+correction;
    }
    return value<0?0:uint64_t(value)>duration_?duration_:uint32_t(value);
  }
  void update(bool available,int playback,int seek,uint32_t elapsed,uint32_t duration,uint32_t now){
    if(!available||playback<0||playback>2||seek<0||seek>2||!duration){initialized_=false;return;}
    if(elapsed>duration)elapsed=duration;
    const bool discontinuity=!initialized_||uint32_t(now-packetAt_)>=kFreshMs||
      playback!=playback_||seek!=seek_;
    const uint32_t displayed=position(now);
    packetAt_=now;duration_=duration;
    if(discontinuity||playback!=1||seek){anchor(elapsed,playback,seek,now);return;}
    // Duplicate samples only refresh connection freshness, including ACKs.
    if(elapsed==lastElapsed_)return;
    const int64_t error=int64_t(elapsed)-displayed;
    if(uint32_t(now-anchorAt_)>=kStallMs||error>=kSeekJumpMs||error<=-int64_t(kSeekJumpMs)){
      anchor(elapsed,playback,seek,now);return;
    }
    // A slightly older cached sample is not an instruction to seek backward.
    if(elapsed<lastElapsed_)return;
    anchorMs_=displayed;anchorAt_=now;lastElapsed_=elapsed;
    correctionMs_=int32_t(error);
  }
 private:
  void anchor(uint32_t elapsed,int playback,int seek,uint32_t now){
    initialized_=true;anchorMs_=lastElapsed_=elapsed;anchorAt_=now;
    playback_=playback;seek_=seek;correctionMs_=0;
  }
  uint32_t anchorMs_=0,anchorAt_=0,lastElapsed_=0,packetAt_=0,duration_=0;
  int32_t correctionMs_=0;
  int playback_=255,seek_=0;
  bool initialized_=false,trackKnown_=false;
  uint64_t track_=0;
};
