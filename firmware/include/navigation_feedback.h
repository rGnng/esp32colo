#pragma once
#include "coloplay_ble_protocol.h"
#include <cstring>

// Navigation is a separate, coherent stream; it never changes media/volume ACKs.
class NavigationFeedback {
 public:
  struct State {cp_navigation_t ui={};bool valid=false,fresh=false;uint16_t revision=0;uint32_t at=0;};
  void reset(){state_=State{};pending_=cp_navigation_t{};revision_=0;used_=length_=0;}
  State snapshot(uint32_t now,bool connected)const {auto out=state_;out.fresh=connected&&out.valid&&uint32_t(now-out.at)<5000;return out;}
  bool accept(const uint8_t *p,size_t n,uint32_t now){
    if(!cp_packet(p,n)||!cp_u16(p+2))return false;
    const uint16_t revision=cp_u16(p+2);
    if(p[1]==CP_NAVIGATION){
      const unsigned length=cp_u16(p+18);const uint32_t index=cp_u32(p+10),count=cp_u32(p+14);
      if(p[8]>6||(p[9]&~63)||length>95||(!(p[9]&CP_NAV_FOCUS)&&length)||
         ((p[9]&CP_NAV_FOCUS)&&count&&index>=count))return false;
      if(state_.valid&&revision==state_.revision&&revision==revision_){state_.at=now;return true;}
      if(revision_&&int16_t(revision-revision_)<0)return false;
      pending_=cp_navigation_t{};pending_.view=p[8];pending_.flags=p[9];pending_.index=index;pending_.count=count;
      revision_=revision;length_=length;used_=0;
      if(!length_)commit(now);
      return true;
    }
    if(p[1]!=CP_NAV_TEXT||revision!=revision_||p[8]!=used_||!p[9]||p[9]>10||used_+p[9]>length_)return false;
    std::memcpy(pending_.focus+used_,p+10,p[9]);used_+=p[9];
    if(used_==length_)commit(now);
    return true;
  }
  static const char *viewName(const cp_navigation_t &ui){
    if(ui.flags&CP_NAV_CONTEXT)return "Context menu";
    if(ui.flags&CP_NAV_QUEUE)return "Queue";
    static const char *names[]={"Now Playing","Albums","Artists","Songs","Playlists","Bluetooth","Settings"};
    return ui.view<7?names[ui.view]:"";
  }
 private:
  State state_;cp_navigation_t pending_={};uint16_t revision_=0;unsigned used_=0,length_=0;
  void commit(uint32_t now){state_.ui=pending_;state_.revision=revision_;state_.valid=true;state_.at=now;}
};
