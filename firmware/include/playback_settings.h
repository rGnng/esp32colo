#pragma once
#include "playback_legacy.h"
namespace PlaybackSettings {
using PlaybackLegacy::Type;using PlaybackLegacy::Spec;
using PlaybackLegacy::Choice;using PlaybackLegacy::Number;using PlaybackLegacy::Color;using PlaybackLegacy::Toggle;
enum State { Playing, Paused, Stopped, StateCount };
constexpr const char *kStates[]={"playing","paused","stopped"};
enum Field { ShowStatus,ShowTitle,ShowArtist,ShowProgress,TimeDisplay,LongText,PausedScreen,StoppedScreen,DisconnectedScreen,ScreenCount };
enum PlanField { RingSource,RingDisplay,RingStyle,VolumeRingStyle,RingColorMode,RingColor,
 UnfilledColorMode,UnfilledColor,UnfilledBrightness,RingBrightness,KeysSource,KeysMode,KeysColor,KeysBrightness,PlanCount };
enum Source { UseScene,Custom };
enum RingMode { Progress,Volume,Lighting,Off };
constexpr int Count=ScreenCount+StateCount*PlanCount;
constexpr int planField(State state,PlanField field){return ScreenCount+state*PlanCount+field;}
const char *const kSources[]={"scene","custom"};
const char *const kRingModes[]={"progress","volume","lighting","off"};
const char *const kKeysModes[]={"static","rainbow","off"};
#define PLAYBACK_PLAN(prefix,source,minimumDisplay) \
 {prefix "ring_source",Choice,0,1,source,kSources}, \
 {prefix "ring_display",Choice,minimumDisplay,3,minimumDisplay,kRingModes}, \
 {prefix "ring_style",Choice,0,1,0,PlaybackLegacy::kRingStyles}, \
 {prefix "volume_ring_style",Choice,0,1,0,PlaybackLegacy::kRingStyles}, \
 {prefix "ring_color_mode",Choice,0,2,0,PlaybackLegacy::kColors}, \
 {prefix "ring_color",Color,0,0xffffff,0xffffff,nullptr}, \
 {prefix "unfilled_color_mode",Choice,0,1,0,PlaybackLegacy::kUnfilledColors}, \
 {prefix "unfilled_color",Color,0,0xffffff,0xffffff,nullptr}, \
 {prefix "unfilled_brightness",Number,0,100,0,nullptr}, \
 {prefix "ring_brightness",Number,-1,100,-1,nullptr}, \
 {prefix "keys_source",Choice,0,1,0,kSources}, \
 {prefix "keys_mode",Choice,0,2,0,kKeysModes}, \
 {prefix "keys_color",Color,0,0xffffff,0xffffff,nullptr}, \
 {prefix "keys_brightness",Number,-1,100,-1,nullptr}
const Spec kSpecs[]={
 {"show_status",Toggle,0,1,1,nullptr},{"show_title",Toggle,0,1,1,nullptr},
 {"show_artist",Toggle,0,1,1,nullptr},{"show_progress",Toggle,0,1,1,nullptr},
 {"time_display",Choice,0,2,0,PlaybackLegacy::kTimes},
 {"long_text",Choice,0,1,0,PlaybackLegacy::kLongText},
 {"paused_screen",Choice,0,2,0,PlaybackLegacy::kScreens},
 {"stopped_screen",Choice,0,2,0,PlaybackLegacy::kScreens},
 {"disconnected_screen",Choice,0,2,0,PlaybackLegacy::kDisconnectedScreens},
 PLAYBACK_PLAN("playing_",Custom,Progress),
 PLAYBACK_PLAN("paused_",Custom,Progress),
 PLAYBACK_PLAN("stopped_",UseScene,Lighting)
};
#undef PLAYBACK_PLAN
static_assert(sizeof(kSpecs)/sizeof(kSpecs[0])==Count,"playback field schema");
struct Stored {
 uint32_t version=3;int32_t values[Count]={};
 Stored(){for(int i=0;i<Count;++i)values[i]=kSpecs[i].initial;}
 int operator[](int field)const{return values[field];}
 int plan(State state,PlanField field)const{return values[planField(state,field)];}
 void setPlan(State state,PlanField field,int value){values[planField(state,field)]=value;}
};
static_assert(sizeof(Stored)==208,"state playback NVS layout changed");
inline bool valid(const Stored &s){
 if(s.version!=3)return false;
 for(int i=0;i<Count;++i)if(s[i]<kSpecs[i].minimum||s[i]>kSpecs[i].maximum)return false;
 return true;
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
   const char *p=text;if(*p=='-'){if(spec.minimum>=0)return false;++p;}
   if(!*p||std::strlen(p)>3)return false;
   for(;*p;++p)if(*p<'0'||*p>'9')return false;
   value=std::strtol(text,nullptr,10);
 }
 if(value<spec.minimum||value>spec.maximum)return false;
 s.values[field]=value;return true;
}
inline State state(bool available,int playback){return !available||playback==0||playback<0||playback>2?Stopped:playback==2?Paused:Playing;}
inline int screen(const Stored &s,bool available,int playback){
 if(!available)return s[DisconnectedScreen]==0?3:s[DisconnectedScreen];
 return playback==0?s[StoppedScreen]:playback==2?s[PausedScreen]:0;
}
inline int brightness(const Stored &s,State state,bool ring,int scene){const int value=s.plan(state,ring?RingBrightness:KeysBrightness);return value<0?scene:value;}
inline int style(const Stored &s,State state,bool volume=false){return s.plan(state,volume?VolumeRingStyle:RingStyle);}
using PlaybackLegacy::ringColor;
struct MigrationContext {int ringStyle=0;bool sceneProgress=true;int ringLevel=70,keysLevel=70;bool keysRainbow=true;uint32_t keysColor=0xffffff;};
inline bool migrate(const PlaybackLegacy::Stored &old,Stored &current,const MigrationContext &context){
 namespace L=PlaybackLegacy;
 if(!L::valid(old))return false;
 Stored s;const int screens[]={L::ShowStatus,L::ShowTitle,L::ShowArtist,L::ShowProgress,L::TimeDisplay,L::LongText,L::PausedScreen,L::StoppedScreen,L::DisconnectedScreen};
 for(int i=0;i<ScreenCount;++i)s.values[i]=old.values[screens[i]];
 for(int i=0;i<StateCount;++i){
   const auto state=State(i);const int display=old[L::RingDisplay]==0?(context.sceneProgress?Progress:Lighting):old[L::RingDisplay]-1;
   const bool sceneRing=display==Lighting&&old[L::RingColorMode]==0&&old[L::RingBrightness]<0;
   s.setPlan(state,RingSource,sceneRing?UseScene:Custom);s.setPlan(state,RingDisplay,display);
   s.setPlan(state,RingStyle,old[L::RingStyle]);s.setPlan(state,VolumeRingStyle,old[L::VolumeRingStyle]);
   s.setPlan(state,RingColorMode,old[L::RingColorMode]);s.setPlan(state,RingColor,old[L::RingColor]);
   s.setPlan(state,UnfilledColorMode,old[L::UnfilledColorMode]);s.setPlan(state,UnfilledColor,old[L::UnfilledColor]);
   s.setPlan(state,UnfilledBrightness,old[L::UnfilledBrightness]);s.setPlan(state,RingBrightness,old[L::RingBrightness]);
   s.setPlan(state,KeysSource,old[L::KeysMode]==0&&old[L::KeysBrightness]<0?UseScene:Custom);
   s.setPlan(state,KeysMode,old[L::KeysMode]==2?2:old[L::KeysMode]==1?0:context.keysRainbow?1:0);
   s.setPlan(state,KeysColor,old[L::KeysMode]==0?context.keysColor:old[L::KeysColor]);s.setPlan(state,KeysBrightness,old[L::KeysBrightness]);
 }
 if(old[L::PausedLights]==2){s.setPlan(Paused,RingSource,Custom);s.setPlan(Paused,RingDisplay,Off);s.setPlan(Paused,KeysSource,Custom);s.setPlan(Paused,KeysMode,2);}
 else if(old[L::PausedLights]==1){
   s.setPlan(Paused,RingSource,Custom);s.setPlan(Paused,KeysSource,Custom);
   s.setPlan(Paused,RingBrightness,(old[L::RingBrightness]<0?context.ringLevel:old[L::RingBrightness])*old[L::DimBrightness]/100);
   s.setPlan(Paused,KeysBrightness,(old[L::KeysBrightness]<0?context.keysLevel:old[L::KeysBrightness])*old[L::DimBrightness]/100);
 }
 if(old[L::StoppedLights]==1){s.setPlan(Stopped,RingSource,UseScene);s.setPlan(Stopped,KeysSource,UseScene);s.setPlan(Stopped,RingDisplay,Lighting);}
 else if(old[L::StoppedLights]==2){s.setPlan(Stopped,RingSource,Custom);s.setPlan(Stopped,RingDisplay,Off);s.setPlan(Stopped,KeysSource,Custom);s.setPlan(Stopped,KeysMode,2);}
 else if(s.plan(Stopped,RingDisplay)==Progress){
   s.setPlan(Stopped,RingSource,Custom);s.setPlan(Stopped,RingDisplay,old[L::UnfilledBrightness]?Lighting:Off);
   s.setPlan(Stopped,RingColorMode,old[L::UnfilledColorMode]==1?1:old[L::RingColorMode]);
   s.setPlan(Stopped,RingColor,old[L::UnfilledColorMode]==1?old[L::UnfilledColor]:old[L::RingColor]);
   s.setPlan(Stopped,RingBrightness,(old[L::RingBrightness]<0?context.ringLevel:old[L::RingBrightness])*old[L::UnfilledBrightness]/100);
 }else if(s.plan(Stopped,RingDisplay)==Volume)s.setPlan(Stopped,RingDisplay,Lighting);
 if(!valid(s))return false;
 current=s;return true;
}
inline bool migrate(const PlaybackLegacy::StoredV1 &old,Stored &current,const MigrationContext &context){PlaybackLegacy::Stored v2;return PlaybackLegacy::migrate(old,v2)&&migrate(v2,current,context);}
}
