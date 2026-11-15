#include "playback_service.h"
#include "device_log.h"
#include "Preferences.h"
#include <cassert>
#include <iostream>
namespace FakeNvs {std::vector<unsigned char> saved;bool open=true,fail=false;int writes=0;}
namespace DeviceLog {void println(const char *){}}
template<class T>void seed(const T &s){const auto *p=reinterpret_cast<const unsigned char*>(&s);FakeNvs::saved.assign(p,p+sizeof(s));}
int main(int argc,char **argv){
 using namespace PlaybackSettings;const std::string scenario=argc>1?argv[1]:"save";
 Stored initial;initial.setPlan(Playing,RingColor,0x123456);
 if(scenario=="restore"||scenario=="invalid"){initial.values[ShowTitle]=scenario=="restore"?0:9;seed(initial);}
 if(scenario=="migrate"||scenario=="migrate-failure"||(scenario=="migrate-v2"||scenario=="migrate-v2-failure")){
  PlaybackLegacy::Stored old;old.values[PlaybackLegacy::ShowTitle]=0;old.values[PlaybackLegacy::TimeDisplay]=2;old.values[PlaybackLegacy::RingStyle]=0;
  if((scenario=="migrate-v2"||scenario=="migrate-v2-failure"))seed(old);
  else {PlaybackLegacy::StoredV1 v1;std::memcpy(v1.values,old.values,sizeof(v1.values));seed(v1);}
  if(scenario=="migrate-failure"||scenario=="migrate-v2-failure")FakeNvs::fail=true;
 }
 if(scenario=="unavailable")FakeNvs::open=false;
 MigrationContext context;context.ringStyle=1;PlaybackService::begin(context);const auto &s=PlaybackService::settings();
 if(scenario=="migrate"||scenario=="migrate-failure"||(scenario=="migrate-v2"||scenario=="migrate-v2-failure")){
  assert(valid(s)&&s[ShowTitle]==0&&s[TimeDisplay]==2&&s.plan(Playing,RingStyle)==0);
  if(scenario=="migrate-failure"||scenario=="migrate-v2-failure"){
   assert(FakeNvs::saved.size()==(scenario=="migrate-v2-failure"?sizeof(PlaybackLegacy::Stored):sizeof(PlaybackLegacy::StoredV1)));FakeNvs::fail=false;
   WebServer request;request.fields={{"show_title","0"}};String error;assert(PlaybackService::configure(request,error));
  }
  assert(FakeNvs::saved.size()==sizeof(Stored));Stored restored;std::memcpy(&restored,FakeNvs::saved.data(),sizeof(restored));assert(valid(restored)&&restored[TimeDisplay]==2);
 }else if(scenario=="restore")assert(s[ShowTitle]==0&&s.plan(Playing,RingColor)==0x123456&&FakeNvs::writes==0);
 else if(scenario=="invalid")assert(valid(s)&&s[ShowTitle]==1&&s.plan(Playing,RingStyle)==1&&FakeNvs::writes==1);
 else if(scenario=="unavailable"){
  assert(std::string(PlaybackService::statusJson().c_str()).find("\"stored\":false")!=std::string::npos);
  WebServer request;request.fields={{"playing_ring_display","off"}};String error;assert(!PlaybackService::configure(request,error)&&s.plan(Playing,RingDisplay)==Progress);
 }else{
  assert(valid(s)&&s.plan(Playing,RingStyle)==1&&FakeNvs::writes==1);WebServer request;String error;
  assert(!PlaybackService::configure(request,error));
  request.fields={{"show_title","0"},{"stopped_ring_display","progress"}};
  assert(!PlaybackService::configure(request,error)&&s[ShowTitle]==1&&FakeNvs::writes==1);
  request.fields={{"show_title","0"},{"ring_display","volume"}};
  assert(!PlaybackService::configure(request,error)&&s[ShowTitle]==1&&FakeNvs::writes==1);
  request.fields={{"playing_ring_source","custom"},{"playing_ring_color","#123456"},{"playing_ring_display","volume"},{"paused_keys_source","custom"},{"paused_keys_mode","off"}};
  FakeNvs::fail=true;assert(!PlaybackService::configure(request,error)&&s.plan(Playing,RingDisplay)==Progress);
  FakeNvs::fail=false;assert(PlaybackService::configure(request,error));
  assert(s.plan(Playing,RingColor)==0x123456&&s.plan(Playing,RingDisplay)==Volume&&s.plan(Paused,KeysMode)==2&&s.plan(Playing,KeysSource)==UseScene);
  const int writes=FakeNvs::writes;assert(PlaybackService::configure(request,error)&&FakeNvs::writes==writes);
  Stored restored;std::memcpy(&restored,FakeNvs::saved.data(),sizeof(restored));assert(valid(restored)&&!std::memcmp(&s,&restored,sizeof(s)));
 }
 std::cout<<"PASS state playback service "<<scenario<<"\n";
}
