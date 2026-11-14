#include "scene_cue.h"
#include "general_settings.h"
#include <cassert>
#include <iostream>
int main(){
  GeneralSettings::Stored settings;assert(GeneralSettings::valid(settings)&&settings.sceneChangeSound==1);
  settings.sceneChangeSound=0;assert(GeneralSettings::valid(settings));
  settings.sceneChangeSound=2;assert(!GeneralSettings::valid(settings));
  settings={};settings.version=4;assert(!GeneralSettings::valid(settings));
  settings={};assert(settings.soundVolume==70);settings.soundVolume=101;assert(!GeneralSettings::valid(settings));
  GeneralSettings::StoredV2 old;old.sceneChangeSound=0;old.sleepTimeoutMinutes=20;
  assert(GeneralSettings::migrateV2(old,settings)&&settings.soundVolume==70&&settings.sleepTimeoutMinutes==20&&!settings.sceneChangeSound);
  assert(SceneCue::frequency(true,0,1,1)==880);
  assert(SceneCue::frequency(true,2,1,-1)==440);
  assert(SceneCue::frequency(true,7,0,1)==880); // next wraps to Default
  assert(SceneCue::frequency(true,0,7,-1)==440); // previous wraps to last
  assert(SceneCue::frequency(true,0,7)==880&&SceneCue::frequency(true,7,0)==440);
  assert(!SceneCue::frequency(true,1,1,1)&&!SceneCue::frequency(false,0,1,1));
  assert(SceneCue::kDurationMs==90);
  std::cout<<"PASS General cue settings and direction-aware scene tones, wraparound and no-op/mute suppression\n";
}
