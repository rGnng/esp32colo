#include "action_sound.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace SceneSettings;
int main(){
  Action a;
  for(int type=0;type<kTypeCount;++type){
    a.type=type;a.value=777;a.setSound(false);
    assert(!ActionSound::frequency(a,1,2));
    a.setSound(true);
    const unsigned expected=type==kTone?777:type==kPreviousScene?440:
      ActionSound::sceneChange(a)?880:660;
    assert(ActionSound::frequency(a,1,2)==expected);
    if(ActionSound::sceneChange(a))assert(!ActionSound::frequency(a,1,1));
  }
  a.type=kNextScene;assert(ActionSound::frequency(a,7,0)==880);
  a.type=kPreviousScene;assert(ActionSound::frequency(a,0,7)==440);
  a.type=kSwitchScene;assert(ActionSound::frequency(a,7,0)==440);
  assert(ActionSound::duration(a)==90);a.type=kNone;assert(ActionSound::duration(a)==50);
  a.type=kTone;assert(ActionSound::duration(a)==200);
  // Every saved LED mode can independently enable and disable audio.
  auto current=defaults();
  for(int led=0;led<kLedCount;++led){
    a=Action{};a.type=kVolumeSet;a.value=50;a.led=led;
    a.setSound(true);assert(a.soundEnabled()&&a.ledEffect()==led&&validAction(current,a));
    a.setSound(false);assert(!a.soundEnabled()&&a.ledEffect()==led&&validAction(current,a));
  }
  a.led=0x40;assert(!validAction(current,a));a.setSound(true);assert(!validAction(current,a));
  // A real v5 record has no audio flags. Preserve all other bytes, including
  // ordered actions, local/global bindings, lighting and active scene.
  auto legacy=defaults();legacy.version=5;
  for(auto &b:legacy.bindings)for(auto &action:b.actions){action.setSound(false);action.setLedBrightness(0);}
  legacy.bindings[0].global=1;legacy.scenes[0].ringDisplay=kRingBluetoothProgress;
  legacy.scenes[0].levels[1].percent=31;
  for(bool sceneSound:{false,true}){
    auto migrated=legacy;assert(migrateV5(migrated,sceneSound)&&valid(migrated));
    for(const auto &b:migrated.bindings)for(int i=0;i<b.count;++i){
      const auto &action=b.actions[i];assert(action.soundEnabled()==
        (action.type==kTone||(sceneSound&&ActionSound::sceneChange(action))));
    }
    migrated.version=5;
    for(auto &b:migrated.bindings)for(auto &action:b.actions){action.setSound(false);action.setLedBrightness(0);}
    assert(!std::memcmp(&legacy,&migrated,sizeof(legacy)));
  }
  auto corrupt=legacy;corrupt.bindings[0].actions[0].setSound(true);
  auto before=corrupt;assert(!migrateV5(corrupt)&&!std::memcmp(&before,&corrupt,sizeof(before)));
  corrupt=legacy;corrupt.bindings[0].count=4;before=corrupt;
  assert(!migrateV5(corrupt)&&!std::memcmp(&before,&corrupt,sizeof(before)));
  corrupt=legacy;corrupt.active=7;before=corrupt;
  assert(!migrateV5(corrupt)&&!std::memcmp(&before,&corrupt,sizeof(before)));
  std::cout<<"PASS per-action sound, scene direction, tone gating, LED isolation and v5 migration\n";
}
