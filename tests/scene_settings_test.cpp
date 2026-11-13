#include "scene_settings.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace SceneSettings;
int main() {
  auto s=defaults(); assert(valid(s)); assert(!remove(s,0));
  auto blank=s;blank.scenes[0].screen=kBlank;assert(valid(blank));
  blank.bindings[0].actions[0].screen=kBlank;assert(valid(blank));
  blank.scenes[0].screen=kUnchanged;assert(!valid(blank));
  blank.scenes[0].screen=kScreenCount;assert(!valid(blank));
  blank=s;blank.bindings[0].actions[0].screen=kScreenCount;assert(!valid(blank));
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
  for(int i=0;i<kBindingCount;++i)legacy.bindings[i]=full.bindings[i];
  Stored migrated;
  assert(migrate(legacy,migrated) && valid(migrated));
  assert(migrated.active==7);
  for(int i=0;i<kBindingCount;++i)assert(!std::memcmp(static_cast<const BindingV1 *>(&migrated.bindings[i]),&legacy.bindings[i],sizeof(BindingV1)));
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
  Stored modern=defaults();modern.scenes[0].ring=Light{kSceneStatic,0x123456};
  StoredV2 old;old.active=modern.active;
  for(int i=0;i<kSceneCount;++i)old.scenes[i]=modern.scenes[i];
  for(int i=0;i<kBindingCount;++i)old.bindings[i]=modern.bindings[i];
  assert(migrateV2(old,migrated)&&valid(migrated)&&migrated.version==3);
  assert(!std::memcmp(&modern,&migrated,sizeof(migrated)));
  Binding combo;combo.input=kComboFlag|3;combo.gesture=kHold;combo.count=1;
  assert(assign(migrated,combo)&&valid(migrated)&&binding(migrated,0,kComboFlag|3,kHold));
  for(int invalid : {kComboFlag,kComboFlag|1,kComboFlag|16,255})assert(!validInput(invalid));
  assert(!validGesture(kComboFlag|3,kLeft)&&validGesture(kComboFlag|15,kHoldRelease));
  auto saved=migrated;old.bindings[0].input=kComboFlag|3;
  assert(!migrateV2(old,migrated)&&!std::memcmp(&saved,&migrated,sizeof(saved)));
  auto scoped=defaults();scoped.scenes[1].used=1;std::strcpy(scoped.scenes[1].name,"Local");
  Binding global;global.input=kKey3;global.count=1;global.global=1;global.actions[0].type=kTone;global.actions[0].value=660;
  assert(assign(scoped,global)&&valid(scoped));
  assert(effectiveBinding(scoped,1,kKey3,kSingle)->scene==0);
  auto local=global;local.scene=1;local.global=0;local.actions[0].value=880;
  assert(assign(scoped,local)&&valid(scoped));assert(effectiveBinding(scoped,1,kKey3,kSingle)->actions[0].value==880);
  local.global=1;assert(assign(scoped,local)&&!valid(scoped));
  scoped=defaults();scoped.scenes[0].levels[0].overrideGeneral=1;scoped.scenes[0].levels[0].percent=25;scoped.scenes[0].levels[0].automatic=1;
  assert(valid(scoped));assert(resolvedLevel(scoped.scenes[0].levels[0],70,false).percent==25);
  assert(resolvedLevel(Level{},42,true).percent==42&&resolvedLevel(Level{},42,true).automatic);
  scoped.scenes[0].levels[2].percent=101;assert(!valid(scoped));
  scoped=defaults();scoped.scenes[0].icon=32;assert(!valid(scoped));
  std::cout<<"PASS scene defaults, protected Default, ordered actions, deletion/retargeting, corrupt records, bounded capacity, independent scene lighting and v1 migration\n";
}
