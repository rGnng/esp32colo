#pragma once
#include <cstdint>
namespace GeneralSettings {
constexpr uint32_t kMaxSleepMinutes=1440;
struct StoredV1 {uint32_t version=1,sceneChangeSound=1;};
struct Stored {uint32_t version=2,sceneChangeSound=1,sleepTimeoutMinutes=60;};
static_assert(sizeof(StoredV1)==8&&sizeof(Stored)==12,"general settings layout changed");
inline bool valid(const Stored &settings){return settings.version==2&&settings.sceneChangeSound<=1&&settings.sleepTimeoutMinutes<=kMaxSleepMinutes;}
inline bool migrate(const StoredV1 &old,Stored &out){
  if(old.version!=1||old.sceneChangeSound>1)return false;
  Stored candidate;candidate.sceneChangeSound=old.sceneChangeSound;out=candidate;return true;
}
inline bool parseSleepMinutes(const char *text,uint32_t &out){
  if(!text||!*text)return false;
  uint32_t value=0;
  for(;*text;++text){if(*text<'0'||*text>'9')return false;value=value*10+(*text-'0');if(value>kMaxSleepMinutes)return false;}
  out=value;return true;
}
}
