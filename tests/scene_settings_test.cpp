#include "scene_settings.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace SceneSettings;
int main() {
  auto s=defaults(); assert(valid(s)); assert(!remove(s,0));
  s.scenes[0].ring=Light{kSceneStatic,0xFF4400};
  s.scenes[0].keys=Light{kSceneRainbow,0x22FF66};
  assert(valid(s));
  auto restored=s;
  assert(restored.scenes[0].ring.color==0xFF4400 && restored.scenes[0].keys.mode==kSceneRainbow);
  restored.scenes[0].keys.mode=kLightingModeCount; assert(!valid(restored));
  restored=s;restored.scenes[0].ring.color=0x1000000;assert(!valid(restored));
  auto general=resolveLighting(Light{},true,0x112233);
  assert(general.mode==kSceneStatic && general.color==0x112233);
  assert(resolveLighting(Light{},false,0x112233).mode==kSceneRainbow);
  assert(resolveLighting(s.scenes[0].ring,false,0).color==0xFF4400);
  assert(resolveLighting(s.scenes[0].keys,true,0).mode==kSceneRainbow);

  assert(s.active==0 && !std::strcmp(s.scenes[0].name,"Default"));
  assert(binding(s,0,kKnob,kLeft)->actions[0].led==kProportional);
  s.scenes[1].used=1; std::strcpy(s.scenes[1].name,"Work"); s.active=1;
  Binding b; b.count=3; b.scene=1; b.input=kKnob; b.gesture=kPressRight;
  b.actions[0].type=kScreenChange; b.actions[0].value=3;
  b.actions[1].type=kVolumeSet; b.actions[1].value=50; b.actions[1].led=kProportional;
  b.actions[2].type=kSwitchScene; b.actions[2].target=0;
  assert(assign(s,b)&&valid(s));
  auto copy=s; assert(valid(copy)); assert(binding(copy,1,kKnob,kPressRight)->count==3);
  b.actions[0].value=101; assert(assign(s,b)&&!valid(s)); s=copy;
  b=Binding{}; b.scene=0; b.input=kKey3; b.count=1; b.actions[0].type=kSwitchScene; b.actions[0].target=1;
  assert(assign(s,b)&&valid(s)); assert(remove(s,1)&&s.active==0&&valid(s));
  assert(binding(s,0,kKey3,kSingle)->actions[0].target==0);
  assert(!binding(s,1,kKnob,kPressRight));
  auto corrupt=s; std::memset(corrupt.scenes[0].name,'x',25); assert(!valid(corrupt));
  corrupt=s; corrupt.scenes[0].used=0; assert(!valid(corrupt));
  corrupt=s; corrupt.bindings[0].input=kKey1; corrupt.bindings[0].gesture=kPressLeft; assert(!valid(corrupt));
  corrupt=s; corrupt.bindings[0].actions[0].led=kProportional; assert(!valid(corrupt));
  corrupt=s; corrupt.bindings[0].count=4; assert(!valid(corrupt));
  corrupt=s; corrupt.bindings[20]=corrupt.bindings[0]; assert(!valid(corrupt));
  corrupt=s; corrupt.scenes[2].used=1; std::strcpy(corrupt.scenes[2].name,"Default"); assert(!valid(corrupt));
  Stored full=defaults(); for (auto &slot:full.bindings) slot=Binding{};
  for (int i=0;i<8;++i) { full.scenes[i].used=1; if(i) {full.scenes[i].name[0]='A'+i;full.scenes[i].name[1]=0;} }
  for (int i=0;i<kBindingCount;++i) {
    Binding row; row.scene=i/8; row.input=(i%8)/5; row.gesture=(i%8)%5; row.count=1;
    assert(assign(full,row));
  }
  Binding extra; extra.scene=7; extra.input=kKnob; extra.gesture=kRight; extra.count=1;
  assert(!assign(full,extra)); assert(valid(full));
  auto *row=binding(full,0,kKey1,kSingle); assert(row); row->count=0; assert(assign(full,extra)&&valid(full));
  auto editing=defaults();
  Binding moved=*binding(editing,0,kKnob,kLeft); moved.input=kKey1; moved.gesture=kDouble;
  assert(editBinding(editing,moved,kKnob,kLeft)==kBindingSaved);
  assert(!binding(editing,0,kKnob,kLeft) && binding(editing,0,kKey1,kDouble));
  auto unchanged=editing;
  moved.gesture=kSingle;
  assert(editBinding(editing,moved,kKey1,kDouble)==kBindingConflict);
  assert(!std::memcmp(&editing,&unchanged,sizeof(editing)));
  assert(editBinding(editing,moved,-1,-1,true)==kBindingConflict);
  moved.gesture=kTriple;
  assert(editBinding(editing,moved,-1,-1,true)==kBindingSaved);
  assert(binding(editing,0,kKey1,kDouble) && binding(editing,0,kKey1,kTriple));
  moved.gesture=kHold;
  assert(editBinding(editing,moved,kKnob,kLeft)==kBindingMissing);
  Binding deletion=moved; deletion.count=0; deletion.gesture=kSingle;
  assert(editBinding(editing,deletion,kKey1,kDouble)==kBindingSaved);
  assert(!binding(editing,0,kKey1,kDouble) && binding(editing,0,kKey1,kSingle));
  assert(valid(editing));
  // Moving an existing row succeeds even when all storage slots are used.
  auto fullMove=full;
  Binding replacement=*binding(fullMove,0,kKey1,kDouble);
  replacement.input=kKnob; replacement.gesture=kLeft;
  assert(editBinding(fullMove,replacement,kKey1,kDouble)==kBindingSaved && valid(fullMove));
  // V1 migration preserves the selected scene and every ordered binding.
  StoredV1 legacy;
  legacy.active=7;
  for (int i=0;i<kSceneCount;++i) legacy.scenes[i]=full.scenes[i];
  std::memcpy(legacy.bindings,full.bindings,sizeof(legacy.bindings));
  Stored migrated;
  assert(migrate(legacy,migrated) && valid(migrated));
  assert(migrated.active==7 && !std::memcmp(migrated.bindings,legacy.bindings,sizeof(legacy.bindings)));
  for (int i=0;i<kSceneCount;++i) {
    assert(!std::strcmp(migrated.scenes[i].name,legacy.scenes[i].name));
    assert(migrated.scenes[i].ring.mode==kGeneralLighting && migrated.scenes[i].keys.mode==kGeneralLighting);
  }
  auto kept=migrated;
  legacy.bindings[0].count=4;
  assert(!migrate(legacy,migrated) && !std::memcmp(&kept,&migrated,sizeof(kept)));
  legacy.version=3;assert(!migrate(legacy,migrated));
  migrated.scenes[7].ring=Light{kSceneStatic,0xABCDEF};
  assert(remove(migrated,7) && migrated.scenes[7].ring.mode==kGeneralLighting);
  std::cout<<"PASS scene defaults, protected Default, ordered actions, deletion/retargeting, corrupt records, bounded capacity, independent scene lighting and v1 migration\n";
}
