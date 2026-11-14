#pragma once
#include "scene_settings.h"

namespace SceneOrder {
// Include unused IDs so deletion needs no second NVS write. Creation moves its
// unused ID to the tail before saving the scene: existing scenes keep their
// relative order even if the scene write fails or power is lost between writes.
struct Stored {uint32_t version=1;uint8_t ids[SceneSettings::kSceneCount]={0,1,2,3,4,5,6,7};};
static_assert(sizeof(Stored)==12,"scene order storage layout changed");
inline bool valid(const Stored &order){
  if(order.version!=1)return false;
  unsigned seen=0;
  for(auto id:order.ids){if(id>=SceneSettings::kSceneCount||(seen&(1u<<id)))return false;seen|=1u<<id;}
  return true;
}
inline int list(const Stored &order,const SceneSettings::Stored &settings,uint8_t *out){
  int count=0;for(auto id:order.ids)if(SceneSettings::exists(settings,id))out[count++]=id;return count;
}
inline int position(const Stored &order,const SceneSettings::Stored &settings,int id){
  uint8_t live[SceneSettings::kSceneCount];const int count=list(order,settings,live);
  for(int i=0;i<count;++i){if(live[i]==id)return i;}
  return -1;
}
inline int neighbor(const Stored &order,const SceneSettings::Stored &settings,int id,int direction){
  uint8_t live[SceneSettings::kSceneCount];const int count=list(order,settings,live);
  for(int i=0;i<count;++i)if(live[i]==id)return live[(i+(direction>0?1:direction<0?-1:0)+count)%count];
  return -1;
}
inline bool append(Stored &order,int id){
  if(id<0||id>=SceneSettings::kSceneCount)return false;
  for(int i=0;i<SceneSettings::kSceneCount;++i)if(order.ids[i]==id){
    for(int j=i+1;j<SceneSettings::kSceneCount;++j)order.ids[j-1]=order.ids[j];
    order.ids[SceneSettings::kSceneCount-1]=id;return true;
  }
  return false;
}
inline bool move(Stored &order,const SceneSettings::Stored &settings,int id,int destination){
  uint8_t live[SceneSettings::kSceneCount];const int count=list(order,settings,live);
  const int source=position(order,settings,id);
  if(source<0||destination<0||destination>=count)return false;
  if(source<destination)for(int i=source;i<destination;++i)live[i]=live[i+1];
  else for(int i=source;i>destination;--i)live[i]=live[i-1];
  live[destination]=id;
  int next=count;for(auto unused:order.ids)if(!SceneSettings::exists(settings,unused))live[next++]=unused;
  for(int i=0;i<SceneSettings::kSceneCount;++i)order.ids[i]=live[i];
  return true;
}
}
