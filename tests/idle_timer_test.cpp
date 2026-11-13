#include "idle_timer.h"
#include "general_settings.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main(){
  using namespace GeneralSettings;
  Stored settings;assert(valid(settings)&&settings.sleepTimeoutMinutes==60);
  StoredV1 old;old.sceneChangeSound=0;
  assert(migrate(old,settings)&&settings.sceneChangeSound==0&&settings.sleepTimeoutMinutes==60);
  auto kept=settings;old.version=2;assert(!migrate(old,settings)&&!std::memcmp(&kept,&settings,sizeof(settings)));
  old.version=1;old.sceneChangeSound=2;assert(!migrate(old,settings));
  settings.sleepTimeoutMinutes=0;assert(valid(settings));
  settings.sleepTimeoutMinutes=kMaxSleepMinutes;assert(valid(settings));
  settings.sleepTimeoutMinutes=kMaxSleepMinutes+1;assert(!valid(settings));
  uint32_t number=99;
  for(const auto *text:{"","-1","+1","1.5","60min"," 60","1441","4294967296","999999999999999"}){
    assert(!parseSleepMinutes(text,number)&&number==99);
  }
  assert(parseSleepMinutes("0",number)&&number==0);
  assert(parseSleepMinutes("60",number)&&number==60);
  assert(parseSleepMinutes("1440",number)&&number==1440);
  IdleTimer timer;assert(!timer.due(3599999)&&timer.due(3600000));
  timer.configure(1,1000);assert(!timer.due(60999)&&timer.due(61000));
  timer.activity(61000);assert(!timer.due(120999)&&timer.due(121000));
  // Each sample of a held input keeps the timeout from expiring.
  for(uint32_t now=100000;now<500000;now+=10000){timer.activity(now);assert(!timer.due(now));}
  timer.configure(0,500000);assert(!timer.due(0xFFFFFFFE));
  timer.configure(60,0xFFFF0000u);assert(!timer.due(uint32_t(0xFFFF0000u+3599999u)));assert(timer.due(uint32_t(0xFFFF0000u+3600000u)));
  timer.configure(1440,0);assert(!timer.due(86399999)&&timer.due(86400000));
  // Changing the setting starts a fresh interval without an immediate timeout.
  timer.configure(1,90000000);assert(!timer.due(90000000)&&timer.elapsed(90001000)==1000);
  std::cout<<"PASS sleep defaults/migration, strict timeout validation, inactivity reset, disable, one-hour boundary and timer wrap\n";
}
