#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>

namespace PlaybackLegacy {
enum Field { RingDisplay, RingStyle, RingColorMode, RingColor, UnfilledBrightness,
  KeysMode, KeysColor, RingBrightness, KeysBrightness, PausedLights, DimBrightness,
  StoppedLights, DisconnectedLights, ShowStatus, ShowTitle, ShowArtist, ShowProgress,
  TimeDisplay, LongText, PausedScreen, StoppedScreen, DisconnectedScreen, VolumeRingStyle, UnfilledColorMode, UnfilledColor, Count };
const char *const kRingDisplays[]={"scene","progress","volume","lighting","off"};
const char *const kRingStyles[]={"clockwise","bottom_to_top"};
const char *const kColors[]={"scene","static","rainbow"};
const char *const kUnfilledColors[]={"filled","static"};
const char *const kKeys[]={"scene","static","off"};
const char *const kPausedLights[]={"keep","dim","off"};
const char *const kInactiveLights[]={"keep","scene","off"};
const char *const kTimes[]={"elapsed_total","remaining","off"};
const char *const kLongText[]={"ellipsis","scroll"};
const char *const kScreens[]={"playback","clock","blank"};
const char *const kDisconnectedScreens[]={"connecting","clock","blank"};
enum Type { Choice, Number, Color, Toggle };
struct Spec {const char *name;Type type;int minimum,maximum,initial;const char *const *choices;};
const Spec kSpecs[]={
  {"ring_display",Choice,0,4,0,kRingDisplays},
  {"ring_style",Choice,0,1,0,kRingStyles},
  {"ring_color_mode",Choice,0,2,0,kColors},
  {"ring_color",Color,0,0xffffff,0xffffff,nullptr},
  {"unfilled_brightness",Number,0,100,0,nullptr},
  {"keys_mode",Choice,0,2,0,kKeys},
  {"keys_color",Color,0,0xffffff,0xffffff,nullptr},
  {"ring_brightness",Number,-1,100,-1,nullptr},
  {"keys_brightness",Number,-1,100,-1,nullptr},
  {"paused_lights",Choice,0,2,0,kPausedLights},
  {"dim_brightness",Number,0,100,30,nullptr},
  {"stopped_lights",Choice,0,2,0,kInactiveLights},
  {"disconnected_lights",Choice,0,2,0,kInactiveLights},
  {"show_status",Toggle,0,1,1,nullptr},
  {"show_title",Toggle,0,1,1,nullptr},
  {"show_artist",Toggle,0,1,1,nullptr},
  {"show_progress",Toggle,0,1,1,nullptr},
  {"time_display",Choice,0,2,0,kTimes},
  {"long_text",Choice,0,1,0,kLongText},
  {"paused_screen",Choice,0,2,0,kScreens},
  {"stopped_screen",Choice,0,2,0,kScreens},
  {"disconnected_screen",Choice,0,2,0,kDisconnectedScreens},
  {"volume_ring_style",Choice,0,1,0,kRingStyles},
  {"unfilled_color_mode",Choice,0,1,0,kUnfilledColors},
  {"unfilled_color",Color,0,0xffffff,0xffffff,nullptr}
};
static_assert(sizeof(kSpecs)/sizeof(kSpecs[0])==Count,"playback field schema");
struct Stored {
  uint32_t version=2;
  int32_t values[Count]={};
  Stored(){for(int i=0;i<Count;++i)values[i]=kSpecs[i].initial;}
  int operator[](Field field)const{return values[field];}
};
struct StoredV1 {uint32_t version=1;int32_t values[23]={};};
static_assert(sizeof(StoredV1)==96,"legacy playback NVS layout changed");
static_assert(sizeof(Stored)==104,"playback NVS layout changed");
inline bool valid(const Stored &s){
  if(s.version!=2)return false;
  for(int i=0;i<Count;++i)if(s.values[i]<kSpecs[i].minimum||s.values[i]>kSpecs[i].maximum)return false;
  return true;
}
inline bool migrate(const StoredV1 &old,Stored &current){
  if(old.version!=1)return false;
  for(int i=0;i<23;++i)if(old.values[i]<kSpecs[i].minimum||old.values[i]>kSpecs[i].maximum)return false;
  Stored candidate;std::memcpy(candidate.values,old.values,sizeof(old.values));current=candidate;return true;
}
// Gamma-corrected RGB crossfade, preserving the sub-pixel progress coverage.
inline uint32_t ringColor(uint32_t filled,uint32_t unfilled,int coverage,int unfilledBrightness){
  if(coverage<0)coverage=0;
  if(coverage>1000)coverage=1000;
  uint32_t out=0;
  for(int shift=0;shift<=16;shift+=8){
    const uint32_t a=(filled>>shift)&255,b=(unfilled>>shift)&255;
    const uint32_t value=(a*coverage*100+b*(1000-coverage)*unfilledBrightness+50000)/100000;
    out|=value<<shift;
  }
  return out;
}
inline int field(const char *name){for(int i=0;i<Count;++i)if(!std::strcmp(name,kSpecs[i].name))return i;return -1;}
inline bool set(Stored &s,int field,const char *text){
  if(field<0||field>=Count||!text||!*text)return false;
  const auto &spec=kSpecs[field];int value=0;
  if(spec.type==Choice){
    for(;value<=spec.maximum;++value)if(!std::strcmp(text,spec.choices[value]))break;
    if(value>spec.maximum)return false;
  }else if(spec.type==Color){
    if(std::strlen(text)!=7||*text!='#')return false;
    for(int i=1;i<7;++i){const char c=text[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;}
    value=std::strtol(text+1,nullptr,16);
  }else{
    // Bound before conversion; reject whitespace, fractions and overflow.
    const char *p=text;if(*p=='-'){if(spec.minimum>=0)return false;++p;}
    if(!*p||std::strlen(p)>3)return false;
    for(;*p;++p)if(*p<'0'||*p>'9')return false;
    value=std::strtol(text,nullptr,10);
  }
  if(value<spec.minimum||value>spec.maximum)return false;
  s.values[field]=value;return true;
}
// 0=keep playback appearance, 1=dim (paused) or use scene (inactive), 2=off.
inline int lightBehavior(const Stored &s,bool available,int playback){
  return !available?s[DisconnectedLights]:playback==0?s[StoppedLights]:playback==2?s[PausedLights]:0;
}
// Screen result: 0=playback, 1=clock, 2=blank, 3=connecting icon.
inline int screen(const Stored &s,bool available,int playback){
  if(!available)return s[DisconnectedScreen]==0?3:s[DisconnectedScreen];
  return playback==0?s[StoppedScreen]:playback==2?s[PausedScreen]:0;
}
// Ring result follows kRingDisplays; the scene's progress selection is resolved here.
inline int ring(const Stored &s,bool sceneProgress,int behavior,bool inactive){
  if(behavior==2)return 4;
  if(inactive&&behavior==1)return 3;
  return s[RingDisplay]==0?(sceneProgress?1:3):s[RingDisplay];
}
inline int style(const Stored &s,bool volume=false){return s[volume?VolumeRingStyle:RingStyle];}
inline int brightness(const Stored &s,bool ring,int sceneLevel){const int level=s[ring?RingBrightness:KeysBrightness];return level<0?sceneLevel:level;}
inline int coverage(int filled,int unfilled){return unfilled*10+(100-unfilled)*filled/100;}
}
