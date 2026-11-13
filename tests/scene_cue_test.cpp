#include "scene_cue.h"
#include "general_settings.h"
#include <cassert>
#include <iostream>
int main(){
  GeneralSettings::Stored settings;assert(GeneralSettings::valid(settings)&&settings.sceneChangeSound==1);
  settings.sceneChangeSound=0;assert(GeneralSettings::valid(settings));
  settings.sceneChangeSound=2;assert(!GeneralSettings::valid(settings));
  settings={};settings.version=3;assert(!GeneralSettings::valid(settings));
  assert(SceneCue::frequency(true,0,1,1)==880);
  assert(SceneCue::frequency(true,2,1,-1)==440);
  assert(SceneCue::frequency(true,7,0,1)==880); // next wraps to Default
  assert(SceneCue::frequency(true,0,7,-1)==440); // previous wraps to last
  assert(SceneCue::frequency(true,0,7)==880&&SceneCue::frequency(true,7,0)==440);
  assert(!SceneCue::frequency(true,1,1,1)&&!SceneCue::frequency(false,0,1,1));
  assert(SceneCue::kDurationMs==90);
  std::cout<<"PASS General cue settings and direction-aware scene tones, wraparound and no-op/mute suppression\n";
}
