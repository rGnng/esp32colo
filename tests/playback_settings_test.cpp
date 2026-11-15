#include "playback_settings.h"
#include "action_feedback.h"
#include "action_lighting.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace PlaybackSettings;
 Stored s;assert(valid(s)&&s.version==3);
 assert(state(true,1)==Playing&&state(true,2)==Paused&&state(true,0)==Stopped&&state(false,1)==Stopped&&state(true,255)==Stopped);
 assert(s.plan(Playing,RingSource)==Custom&&s.plan(Paused,RingSource)==Custom&&s.plan(Stopped,RingSource)==UseScene);
 for(int i=0;i<Count;++i){
  assert(field(kSpecs[i].name)==i);auto copy=s;
  copy.values[i]=kSpecs[i].minimum;assert(valid(copy));copy.values[i]=kSpecs[i].maximum;assert(valid(copy));
  copy.values[i]=kSpecs[i].minimum-1;assert(!valid(copy));copy.values[i]=kSpecs[i].maximum+1;assert(!valid(copy));
  if(kSpecs[i].type==Choice)for(int j=kSpecs[i].minimum;j<=kSpecs[i].maximum;++j){assert(set(copy,i,kSpecs[i].choices[j]));assert(copy[i]==j);}
 }
 assert(!set(s,field("ring_display"),"progress"));
 assert(!set(s,planField(Stopped,RingDisplay),"progress")&&!set(s,planField(Stopped,RingDisplay),"volume"));
 for(const char *text:{"","101","-2","1.5","nan","+1"," 1","99999999999999"})assert(!set(s,planField(Playing,RingBrightness),text));
 assert(set(s,planField(Playing,RingColor),"#aBcD12")&&s.plan(Playing,RingColor)==0xabcd12);
 for(const char *text:{"abcdef","#fffff","#fffffff","#ggffff","#12 456"})assert(!set(s,planField(Playing,RingColor),text));
 for(const char *text:{"true","2","-1","1.0"})assert(!set(s,ShowTitle,text));
 assert(set(s,ShowTitle,"0")&&s[ShowTitle]==0&&set(s,ShowTitle,"1"));
 s.setPlan(Playing,RingBrightness,12);s.setPlan(Paused,KeysSource,Custom);s.setPlan(Paused,KeysMode,2);
 assert(brightness(s,Playing,true,50)==12&&brightness(s,Paused,true,50)==50&&s.plan(Playing,KeysSource)==UseScene);
 assert(s.plan(Stopped,KeysSource)==UseScene);assert(style(s,Playing)==0);
 assert(ringColor(0xff0000,0x0000ff,1000,100)==0xff0000&&ringColor(0xff0000,0x0000ff,0,100)==0xff);
 assert(ringColor(0xff0000,0x0000ff,500,100)==0x800080&&ringColor(0xff0000,0x0000ff,0,20)==0x33);
 namespace L=PlaybackLegacy;
 L::Stored old;old.values[L::TimeDisplay]=2;old.values[L::StoppedScreen]=1;old.values[L::DisconnectedScreen]=1;
 old.values[L::RingDisplay]=1;old.values[L::RingStyle]=1;old.values[L::VolumeRingStyle]=0;old.values[L::RingColorMode]=1;old.values[L::RingColor]=0xb80000;
 old.values[L::UnfilledColorMode]=1;old.values[L::UnfilledColor]=0x123456;old.values[L::UnfilledBrightness]=50;
 old.values[L::PausedLights]=1;old.values[L::DimBrightness]=30;
 MigrationContext context;context.ringLevel=40;context.keysLevel=60;context.keysRainbow=false;context.keysColor=0xabcdef;
 Stored migrated;assert(migrate(old,migrated,context)&&valid(migrated));
 assert(migrated[TimeDisplay]==2&&migrated[StoppedScreen]==1&&migrated[DisconnectedScreen]==1);
 assert(migrated.plan(Playing,RingDisplay)==Progress&&migrated.plan(Playing,RingColor)==0xb80000&&migrated.plan(Playing,RingStyle)==1);
 assert(migrated.plan(Playing,VolumeRingStyle)==0&&migrated.plan(Playing,KeysSource)==UseScene);
 assert(migrated.plan(Paused,RingBrightness)==12&&migrated.plan(Paused,KeysBrightness)==18&&migrated.plan(Paused,KeysSource)==Custom);
 assert(migrated.plan(Paused,KeysMode)==0&&migrated.plan(Paused,KeysColor)==0xabcdef);
 assert(migrated.plan(Stopped,RingDisplay)==Lighting&&migrated.plan(Stopped,RingColor)==0x123456&&migrated.plan(Stopped,RingBrightness)==20);
 assert(migrated.plan(state(false,1),RingColor)==migrated.plan(state(true,0),RingColor));
 // Off and Scene policies override both channels independently of playing mode.
 old.values[L::PausedLights]=2;old.values[L::StoppedLights]=1;assert(migrate(old,migrated,context));
 assert(migrated.plan(Paused,RingDisplay)==Off&&migrated.plan(Paused,KeysMode)==2);
 assert(migrated.plan(Stopped,RingSource)==UseScene&&migrated.plan(Stopped,KeysSource)==UseScene);
 old.values[L::StoppedLights]=2;assert(migrate(old,migrated,context)&&migrated.plan(Stopped,RingDisplay)==Off);
 old.values[L::StoppedLights]=0;old.values[L::UnfilledBrightness]=0;assert(migrate(old,migrated,context)&&migrated.plan(Stopped,RingDisplay)==Off);
 old.values[L::RingDisplay]=2;assert(migrate(old,migrated,context)&&migrated.plan(Stopped,RingDisplay)==Lighting);
 // A malformed legacy blob never replaces a good current profile.
 const Stored saved=migrated;old.values[L::ShowTitle]=9;assert(!migrate(old,migrated,context)&&!std::memcmp(&saved,&migrated,sizeof(saved)));
 L::StoredV1 v1;L::Stored base;std::memcpy(v1.values,base.values,sizeof(v1.values));v1.values[L::ShowTitle]=0;
 assert(migrate(v1,migrated,context)&&valid(migrated)&&!migrated[ShowTitle]);
 s.values[PausedScreen]=2;s.values[StoppedScreen]=1;assert(screen(s,true,2)==2&&screen(s,true,0)==1&&screen(s,false,1)==3);
 // Per-action lighting remains restricted to the originating button and duration.
 ActionFeedback feedback;SceneSettings::Action action;action.led=SceneSettings::kFeedbackStatic;action.feedbackMs=500;action.setLedBrightness(80);action.setColorRgb(0x123456);
 feedback.apply(action,0,100,SceneSettings::kKey2);const auto cue=feedback.snapshot(200);
 assert(cue.keys[0].led==SceneSettings::kInherit&&cue.keys[1].led==SceneSettings::kFeedbackStatic&&cue.ring.led==SceneSettings::kInherit);
 const auto result=ActionLighting::resolve(cue.keys[1],0,0x123456,0,0,false,0,9,true,50);
 assert(result.applied&&result.brightness==80&&result.color==0x123456&&feedback.snapshot(600).keys[1].led==SceneSettings::kInherit);
 std::cout<<"PASS independent state/channel plans, stopped/disconnected mapping, bounded API values, migration, RGB and action priority\n";
}
