#pragma once
#include <cstdint>
namespace GeneralSettings {
constexpr uint32_t kMaxSleepMinutes=1440;
struct StoredV1 {uint32_t version=1,sceneChangeSound=1;};
struct StoredV2 {uint32_t version=2,sceneChangeSound=1,sleepTimeoutMinutes=60;};
struct Stored {uint32_t version=3,sceneChangeSound=1,sleepTimeoutMinutes=60,soundVolume=70;};
static_assert(sizeof(StoredV1)==8&&sizeof(StoredV2)==12&&sizeof(Stored)==16,"general settings layout changed");
inline bool valid(const Stored &settings){return settings.version==3&&settings.sceneChangeSound<=1&&settings.sleepTimeoutMinutes<=kMaxSleepMinutes&&settings.soundVolume<=100;}
inline bool migrate(const StoredV1 &old,Stored &out){
  if(old.version!=1||old.sceneChangeSound>1)return false;
  Stored candidate;candidate.sceneChangeSound=old.sceneChangeSound;out=candidate;return true;
}
inline bool migrateV2(const StoredV2 &old,Stored &out){
  if(old.version!=2||old.sceneChangeSound>1||old.sleepTimeoutMinutes>kMaxSleepMinutes)return false;
  Stored candidate;candidate.sceneChangeSound=old.sceneChangeSound;candidate.sleepTimeoutMinutes=old.sleepTimeoutMinutes;out=candidate;return true;
}
inline bool parseSleepMinutes(const char *text,uint32_t &out){
  if(!text||!*text)return false;
  uint32_t value=0;
  for(;*text;++text){if(*text<'0'||*text>'9')return false;value=value*10+(*text-'0');if(value>kMaxSleepMinutes)return false;}
  out=value;return true;
}
}
