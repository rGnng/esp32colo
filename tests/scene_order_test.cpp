#include "scene_order.h"
#include "action_sound.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace SceneSettings;
static void add(Stored &s,int id){s.scenes[id].used=1;s.scenes[id].name[0]='A'+id;s.scenes[id].name[1]=0;}
static void expect(const SceneOrder::Stored &order,const Stored &s,std::initializer_list<int> expected){
  uint8_t live[kSceneCount];assert(SceneOrder::valid(order));assert(SceneOrder::list(order,s,live)==int(expected.size()));
  int at=0;for(int id:expected){assert(live[at]==id&&SceneOrder::position(order,s,id)==at);++at;}
}
int main(){
  SceneOrder::Stored order;auto s=defaults();add(s,3);add(s,6);expect(order,s,{0,3,6});
  assert(SceneOrder::move(order,s,6,0));expect(order,s,{6,0,3});
  assert(SceneOrder::neighbor(order,s,3,1)==6&&SceneOrder::neighbor(order,s,6,-1)==3);
  assert(SceneOrder::neighbor(order,s,0,1)==3&&SceneOrder::neighbor(order,s,0,-1)==6);
  assert(SceneOrder::neighbor(order,s,0,0)==0&&SceneOrder::neighbor(order,s,2,1)==-1);
  auto unchanged=order;assert(!SceneOrder::move(order,s,2,0)&&!SceneOrder::move(order,s,0,-1)&&!SceneOrder::move(order,s,0,3));
  assert(!std::memcmp(&unchanged,&order,sizeof(order)));
  // A low-numbered unused slot becomes the last visible scene. Persisting the
  // append first cannot reorder existing scenes if creating the scene fails.
  assert(SceneOrder::append(order,1));expect(order,s,{6,0,3});add(s,1);expect(order,s,{6,0,3,1});
  assert(remove(s,3));expect(order,s,{6,0,1});assert(SceneOrder::append(order,3));add(s,3);expect(order,s,{6,0,1,3});
  assert(SceneOrder::move(order,s,0,3));expect(order,s,{6,1,3,0});assert(!remove(s,0));
  SceneOrder::Stored loaded;std::memcpy(&loaded,&order,sizeof(order));expect(loaded,s,{6,1,3,0});
  assert(sizeof(loaded)==12&&sizeof(s)==3304&&valid(s));
  auto corrupt=order;corrupt.ids[1]=corrupt.ids[0];assert(!SceneOrder::valid(corrupt));
  corrupt=order;corrupt.ids[0]=8;assert(!SceneOrder::valid(corrupt));corrupt=order;corrupt.version=2;assert(!SceneOrder::valid(corrupt));
  unchanged=order;assert(!SceneOrder::append(order,-1)&&!SceneOrder::append(order,8));assert(!std::memcmp(&unchanged,&order,sizeof(order)));
  // Direct switches use displayed positions, while explicit next/previous cues
  // keep their direction when wrapping across the ends of the reordered list.
  Action a;a.type=kSwitchScene;a.setSound(true);
  assert(ActionSound::frequency(a,SceneOrder::position(order,s,6),SceneOrder::position(order,s,0))==880);
  a.type=kNextScene;assert(ActionSound::frequency(a,3,0)==880);a.type=kPreviousScene;assert(ActionSound::frequency(a,0,3)==440);
  auto full=defaults();for(int id=1;id<kSceneCount;++id)add(full,id);
  const auto settingsBefore=full;
  for(int source=0;source<8;++source)for(int destination=0;destination<8;++destination){
    SceneOrder::Stored moved;assert(SceneOrder::move(moved,full,source,destination));
    uint8_t live[kSceneCount];assert(SceneOrder::list(moved,full,live)==8&&live[destination]==source&&SceneOrder::valid(moved));
    for(int i=0;i<8;++i){assert(SceneOrder::neighbor(moved,full,live[i],1)==live[(i+1)%8]);assert(SceneOrder::neighbor(moved,full,live[i],-1)==live[(i+7)%8]);}
  }
  assert(!std::memcmp(&full,&settingsBefore,sizeof(full)));
  auto single=defaults();assert(SceneOrder::neighbor(order,single,0,1)==0&&SceneOrder::neighbor(order,single,0,-1)==0);
  std::cout<<"PASS scene moves, stable IDs, next/previous wrap, sparse append/reuse, protected Default, persistence, invalid order and unchanged scene/action storage\n";
}
