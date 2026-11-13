#include "general_service.h"
#include "general_settings.h"
#include "device_log.h"
#include <Preferences.h>
#include <cstring>
namespace GeneralService {
namespace {GeneralSettings::Stored gSettings;Preferences gPrefs;bool gOpen=false,gStored=false;}
void begin(){
  gOpen=gPrefs.begin("general",false);
  GeneralSettings::Stored saved;
  if(gOpen&&gPrefs.getBytesLength("settings")==sizeof(saved)&&gPrefs.getBytes("settings",&saved,sizeof(saved))==sizeof(saved)&&GeneralSettings::valid(saved)){gSettings=saved;gStored=true;}
  else {
    GeneralSettings::StoredV1 old;
    if(gOpen&&gPrefs.getBytesLength("settings")==sizeof(old)&&gPrefs.getBytes("settings",&old,sizeof(old))==sizeof(old))GeneralSettings::migrate(old,gSettings);
    gStored=gOpen&&gPrefs.putBytes("settings",&gSettings,sizeof(gSettings))==sizeof(gSettings);
  }
}
bool sceneChangeSound(){return gSettings.sceneChangeSound;}
uint32_t sleepTimeoutMinutes(){return gSettings.sleepTimeoutMinutes;}
String statusJson(){return "{\"scene_change_sound\":"+String(gSettings.sceneChangeSound?"true":"false")+",\"sleep_timeout_minutes\":"+String(gSettings.sleepTimeoutMinutes)+",\"stored\":"+String(gStored?"true":"false")+"}";}
bool configure(WebServer &server,String &error){
  if(!server.hasArg("scene_change_sound")&&!server.hasArg("sleep_timeout_minutes")){error="a General setting is required";return false;}
  auto candidate=gSettings;
  if(server.hasArg("scene_change_sound")){
    const String value=server.arg("scene_change_sound");
    if(value!="0"&&value!="1"){error="scene_change_sound must be 0 or 1";return false;}
    candidate.sceneChangeSound=value=="1";
  }
  if(server.hasArg("sleep_timeout_minutes")&&!GeneralSettings::parseSleepMinutes(server.arg("sleep_timeout_minutes").c_str(),candidate.sleepTimeoutMinutes)){
    error="sleep_timeout_minutes must be an integer from 0 to 1440";return false;
  }
  if(!gStored||std::memcmp(&candidate,&gSettings,sizeof(candidate))){
    if(!gOpen||gPrefs.putBytes("settings",&candidate,sizeof(candidate))!=sizeof(candidate)){error="could not save General settings";return false;}
    gSettings=candidate;gStored=true;
  }
  DeviceLog::printf("[general] scene change sound %s, sleep after %u min\n",gSettings.sceneChangeSound?"on":"off",gSettings.sleepTimeoutMinutes);
  return true;
}
}
