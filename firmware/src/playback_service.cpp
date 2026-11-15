#include "playback_service.h"
#include "device_log.h"
#include <Preferences.h>
namespace PlaybackService {
namespace {Preferences gPrefs;PlaybackSettings::Stored gSettings;bool gOpen=false,gStored=false;}
void begin(const PlaybackSettings::MigrationContext &context){
  for(int i=0;i<PlaybackSettings::StateCount;++i){gSettings.setPlan(PlaybackSettings::State(i),PlaybackSettings::RingStyle,context.ringStyle);gSettings.setPlan(PlaybackSettings::State(i),PlaybackSettings::VolumeRingStyle,context.ringStyle);}
  gOpen=gPrefs.begin("bt-playback",false);
  PlaybackSettings::Stored saved;
  if(gOpen&&gPrefs.getBytesLength("settings")==sizeof(saved)&&gPrefs.getBytes("settings",&saved,sizeof(saved))==sizeof(saved)&&PlaybackSettings::valid(saved)){
    gSettings=saved;gStored=true;
  }else {
    PlaybackLegacy::Stored old;
    PlaybackLegacy::StoredV1 v1;
    if(gOpen&&gPrefs.getBytesLength("settings")==sizeof(old)&&gPrefs.getBytes("settings",&old,sizeof(old))==sizeof(old))PlaybackSettings::migrate(old,gSettings,context);
    else if(gOpen&&gPrefs.getBytesLength("settings")==sizeof(v1)&&gPrefs.getBytes("settings",&v1,sizeof(v1))==sizeof(v1))PlaybackSettings::migrate(v1,gSettings,context);
    gStored=gOpen&&gPrefs.putBytes("settings",&gSettings,sizeof(gSettings))==sizeof(gSettings);
  }
}
const PlaybackSettings::Stored &settings(){return gSettings;}
String statusJson(){
  String out="{\"schema\":3,\"stored\":"+String(gStored?"true":"false");
  for(int i=0;i<PlaybackSettings::Count;++i){
    const auto &spec=PlaybackSettings::kSpecs[i];const int value=gSettings.values[i];
    out+=",\""+String(spec.name)+"\":";
    if(spec.type==PlaybackSettings::Choice)out+="\""+String(spec.choices[value])+"\"";
    else if(spec.type==PlaybackSettings::Color){char color[10];snprintf(color,sizeof(color),"\"#%06x\"",unsigned(value));out+=color;}
    else if(spec.type==PlaybackSettings::Toggle)out+=value?"true":"false";
    else out+=String(value);
  }
  return out+"}";
}
bool configure(WebServer &server,String &error){
  if(!server.args()){error="a playback setting is required";return false;}
  auto candidate=gSettings;
  for(int i=0;i<server.args();++i){
    const String name=server.argName(i);const int field=PlaybackSettings::field(name.c_str());
    if(field<0&&PlaybackLegacy::field(name.c_str())>=0){error="Playback lighting changed; refresh the page";return false;}
    if(!PlaybackSettings::set(candidate,field,server.arg(i).c_str())){error="invalid playback setting: "+name;return false;}
  }
  if(!gStored||std::memcmp(&candidate,&gSettings,sizeof(candidate))){
    if(!gOpen||gPrefs.putBytes("settings",&candidate,sizeof(candidate))!=sizeof(candidate)){error="could not save playback settings";return false;}
    gSettings=candidate;gStored=true;
  }
  DeviceLog::println("[playback] appearance settings saved");return true;
}
}
