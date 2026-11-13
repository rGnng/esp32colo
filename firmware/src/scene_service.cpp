#include "scene_service.h"
#include <Preferences.h>
#include <cstdlib>

namespace SceneService {
namespace {
using namespace SceneSettings;
Stored gSettings, gCandidate;
Preferences gPrefs;
bool gOpen=false, gStored=false;
Handler gHandler=nullptr;
uint32_t gEpoch=1, gSequence=0, gAt=0;
int gInput=-1, gGesture=-1, gLastType=0, gPercent=-1;
String gError;
ActionFeedback gFeedback;
String quote(const char *value) {
  String out="\"";
  for (const unsigned char *p=reinterpret_cast<const unsigned char *>(value);*p;++p) {
    if (*p=='"' || *p=='\\') out+='\\';
    if (*p<32) { out+=' '; } else out+=char(*p);
  }
  return out+'"';
}
String color(uint32_t rgb) { char value[8]; snprintf(value,sizeof(value),"#%06lx",static_cast<unsigned long>(rgb)); return quote(value); }
String sceneLighting(const Scene &scene) {
  return ",\"ring_mode\":"+quote(kLightingModes[scene.ring.mode])+",\"ring_color\":"+color(scene.ring.color)+
    ",\"keys_mode\":"+quote(kLightingModes[scene.keys.mode])+",\"keys_color\":"+color(scene.keys.color);
}
int index(const String &value, const char *const *list, int count) {
  for (int i=0;i<count;++i) if (value==list[i]) return i;
  return -1;
}
bool number(WebServer &server,const char *name,int low,int high,int &out,String &error,bool required=false) {
  if (!server.hasArg(name)) { if (!required) return true; error=String(name)+" is required"; return false; }
  const String raw=server.arg(name);
  if (!raw.length() || raw.length()>7) { error=String(name)+" requires an integer"; return false; }
  for (size_t i=0;i<raw.length();++i) if (!(raw[i]>='0'&&raw[i]<='9') && !(i==0&&raw[i]=='-'&&raw.length()>1)) {
    error=String(name)+" requires an integer"; return false;
  }
  const long value=std::strtol(raw.c_str(),nullptr,10);
  if (value<low||value>high) { error=String(name)+" out of range"; return false; }
  out=value; return true;
}
bool readColor(const String &value,uint32_t &out) {
  if (value.length()!=7||value[0]!='#') return false;
  uint32_t rgb=0;
  for (int i=1;i<7;++i) {
    char c=value[i]; int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
    if (digit<0) return false; rgb=(rgb<<4)|digit;
  }
  out=rgb; return true;
}
bool commit(String &error) {
  if (!valid(gCandidate)) { error="invalid scene, action or effect configuration"; return false; }
  if (!std::memcmp(&gSettings,&gCandidate,sizeof(gSettings))) return true;
  if (!gOpen||gPrefs.putBytes("settings",&gCandidate,sizeof(gCandidate))!=sizeof(gCandidate)) {
    error="could not save scenes to NVS"; return false;
  }
  gSettings=gCandidate; gStored=true; ++gEpoch; gFeedback.clear(); return true;
}
bool activate(int id) {
  if (!exists(gSettings,id)) return false;
  gCandidate=gSettings; gCandidate.active=id;
  return commit(gError);
}
String list(const char *const *names,int count) {
  String out="["; for (int i=0;i<count;++i) { if (i) out+=','; out+=quote(names[i]); } return out+"]";
}
}  // namespace

void begin(Handler handler) {
  gHandler=handler; gOpen=gPrefs.begin("scenes",false);
  if (gOpen && gPrefs.getBytesLength("settings")==sizeof(gSettings) &&
      gPrefs.getBytes("settings",&gSettings,sizeof(gSettings))==sizeof(gSettings) && valid(gSettings)) {
    gStored=true; return;
  }
  // Static scratch avoids putting both old and new scene records on the
  // setup task stack during migration. Preserve all scene IDs and bindings.
  static StoredV1 legacy;
  if (gOpen && gPrefs.getBytesLength("settings")==sizeof(legacy) &&
      gPrefs.getBytes("settings",&legacy,sizeof(legacy))==sizeof(legacy) && migrate(legacy,gSettings)) {
    gStored=gPrefs.putBytes("settings",&gSettings,sizeof(gSettings))==sizeof(gSettings);
    if (!gStored) gError="could not persist migrated scenes";
    return;
  }
  gSettings=defaults();
  gStored=gOpen&&gPrefs.putBytes("settings",&gSettings,sizeof(gSettings))==sizeof(gSettings);
}
uint32_t epoch() { return gEpoch; }
Feedback feedback() { return gFeedback.snapshot(millis()); }
SceneSettings::Light lighting(bool ring) {
  const auto &scene=gSettings.scenes[gSettings.active];
  return ring?scene.ring:scene.keys;
}
uint8_t screen() {
  const auto f=feedback();
  return f.active&&f.screen!=SceneSettings::kUnchanged?f.screen:gSettings.scenes[gSettings.active].screen;
}
String statusJson() {
  const auto f=feedback();
  String out="{\"active\":"+String(gSettings.active)+",\"name\":"+quote(gSettings.scenes[gSettings.active].name);
  out+=",\"standby_screen\":\"clock\",\"screen\":\"clock\",\"stored\":"+String(gStored?"true":"false");
  out+=sceneLighting(gSettings.scenes[gSettings.active]);
  out+=",\"epoch\":"+String(gEpoch)+",\"last_event\":{\"sequence\":"+String(gSequence)+",\"at_ms\":"+String(gAt);
  out+=",\"input\":"+(gInput<0?String("null"):quote(kInputs[gInput]));
  out+=",\"gesture\":"+(gGesture<0?String("null"):quote(kGestures[gGesture]));
  out+=",\"action\":"+quote(kTypes[gLastType])+",\"percent\":"+String(gPercent)+"}";
  out+=",\"feedback\":{\"active\":"+String(f.active?"true":"false")+",\"led\":"+quote(kLeds[f.led]);
  out+=",\"percent\":"+String(f.percent)+",\"color\":"+color(f.color)+"},\"error\":"+quote(gError.c_str())+"}";
  return out;
}
String configJson() {
  String out; out.reserve(8000);
  out="{\"state\":"+statusJson()+",\"limits\":{\"scenes\":8,\"bindings\":64,\"actions_per_gesture\":3}";
  out+=",\"timing\":{\"debounce_ms\":25,\"click_window_ms\":300,\"hold_ms\":600}";
  out+=",\"catalog\":{\"inputs\":"+list(kInputs,kInputCount)+",\"gestures\":"+list(kGestures,kGestureCount);
  out+=",\"actions\":"+list(kTypes,kTypeCount)+",\"led_effects\":"+list(kLeds,kLedCount)+",\"screen_effects\":"+list(kScreens,kScreenCount)+",\"lighting_modes\":"+list(kLightingModes,kLightingModeCount)+"},\"scenes\":[";
  bool first=true;
  for (int i=0;i<kSceneCount;++i) {
    if (!exists(gSettings,i)) continue;
    if (!first) out+=','; first=false;
    out+="{\"id\":"+String(i)+",\"name\":"+quote(gSettings.scenes[i].name)+",\"protected\":"+String(i==0?"true":"false")+",\"standby_screen\":\"clock\""+sceneLighting(gSettings.scenes[i])+",\"bindings\":[";
    bool firstBinding=true;
    for (const auto &b:gSettings.bindings) {
      if (!b.count||b.scene!=i) continue;
      if (!firstBinding) out+=','; firstBinding=false;
      out+="{\"input\":"+quote(kInputs[b.input])+",\"gesture\":"+quote(kGestures[b.gesture])+",\"actions\":[";
      for (int n=0;n<b.count;++n) {
        if (n) out+=','; const auto &a=b.actions[n];
        out+="{\"type\":"+quote(kTypes[a.type])+",\"value\":"+String(a.value)+",\"target\":"+String(a.target);
        out+=",\"led\":"+quote(kLeds[a.led])+",\"screen\":"+quote(kScreens[a.screen])+",\"color\":"+color(a.color)+",\"feedback_ms\":"+String(a.feedbackMs)+"}";
      }
      out+="]}";
    }
    out+="]}";
  }
  return out+"]}";
}
bool configure(WebServer &server,String &error) {
  gCandidate=gSettings;
  const String op=server.arg("op");
  int id=0;
  if (op=="create") {
    id=-1; for (int i=1;i<kSceneCount;++i) if (!exists(gCandidate,i)) { id=i; break; }
    if (id<0) { error="scene capacity reached (8 scenes)"; return false; }
    gCandidate.scenes[id].used=1;
  } else if (!number(server,"scene_id",0,kSceneCount-1,id,error,true)||!exists(gCandidate,id)) {
    if (error.isEmpty()) error="scene not found"; return false;
  }
  if (op=="create"||op=="update") {
    String name=server.arg("name"); name.trim();
    if (!name.length()||name.length()>24) { error="scene name must be 1-24 UTF-8 bytes"; return false; }
    if (id==0&&name!="Default") { error="Default scene name is protected"; return false; }
    for (int i=0;i<kSceneCount;++i) if (i!=id && exists(gCandidate,i) && name==gCandidate.scenes[i].name) {
      error="scene names must be distinct"; return false;
    }
    name.toCharArray(gCandidate.scenes[id].name,25);
    if (server.hasArg("standby_screen")&&server.arg("standby_screen")!="clock") { error="Clock is the available screen behavior"; return false; }
    for (int channel=0;channel<2;++channel) {
      auto &light=channel==0?gCandidate.scenes[id].ring:gCandidate.scenes[id].keys;
      const String prefix=channel==0?"ring_":"keys_";
      if (server.hasArg(prefix+"mode")) {
        const int mode=index(server.arg(prefix+"mode"),kLightingModes,kLightingModeCount);
        if (mode<0) { error=prefix+"mode must be general, rainbow or static"; return false; }
        light.mode=mode;
      }
      if (server.hasArg(prefix+"color")&&!readColor(server.arg(prefix+"color"),light.color)) {
        error=prefix+"color must be #RRGGBB"; return false;
      }
    }
  } else if (op=="delete") {
    if (!remove(gCandidate,id)) { error="Default scene cannot be deleted"; return false; }
  } else if (op=="activate") {
    gCandidate.active=id;
  } else if (op=="run") {
    const int input=index(server.arg("input"),kInputs,kInputCount), gesture=index(server.arg("gesture"),kGestures,kGestureCount);
    if (id!=gSettings.active||input<0||gesture<0||(input!=kKnob&&gesture>=kLeft)) { error="run a valid gesture in the active scene"; return false; }
    event(input,gesture); return true;
  } else if (op=="binding") {
    Binding b; b.scene=id;
    const int input=index(server.arg("input"),kInputs,kInputCount), gesture=index(server.arg("gesture"),kGestures,kGestureCount);
    int count=0;
    if (input<0||gesture<0||(input!=kKnob&&gesture>=kLeft)) { error="invalid input or gesture"; return false; }
    if (!number(server,"count",0,kActionCount,count,error,true)) return false;
    b.input=input; b.gesture=gesture; b.count=count;
    for (int i=0;i<count;++i) {
      const String prefix="a"+String(i)+"_";
      auto &a=b.actions[i];
      int type=index(server.arg(prefix+"type"),kTypes,kTypeCount), led=kInherit, screen=kUnchanged;
      if (server.hasArg(prefix+"led")) led=index(server.arg(prefix+"led"),kLeds,kLedCount);
      if (server.hasArg(prefix+"screen")) screen=index(server.arg(prefix+"screen"),kScreens,kScreenCount);
      if (type<0||led<0||screen<0) { error="invalid action or effect"; return false; }
      a.type=type; a.led=led; a.screen=screen;
      int value=0,target=0,duration=1500;
      if (!number(server,(prefix+"value").c_str(),-100,4000,value,error)||
          !number(server,(prefix+"target").c_str(),0,kSceneCount-1,target,error)||
          !number(server,(prefix+"feedback_ms").c_str(),250,10000,duration,error)) return false;
      a.value=value; a.target=target; a.feedbackMs=duration;
      if (server.hasArg(prefix+"color")&&!readColor(server.arg(prefix+"color"),a.color)) { error="color must be #RRGGBB"; return false; }
      if (!validAction(gCandidate,a)) { error="invalid action value, scene target or proportional effect"; return false; }
    }
    int sourceInput=-1,sourceGesture=-1,createOnly=0;
    if (server.hasArg("source_input") || server.hasArg("source_gesture")) {
      sourceInput=index(server.arg("source_input"),kInputs,kInputCount);
      sourceGesture=index(server.arg("source_gesture"),kGestures,kGestureCount);
      if (sourceInput<0||sourceGesture<0||(sourceInput!=kKnob&&sourceGesture>=kLeft)) {
        error="invalid source binding"; return false;
      }
    }
    if (!number(server,"create_only",0,1,createOnly,error)) return false;
    const auto result=editBinding(gCandidate,b,sourceInput,sourceGesture,createOnly);
    if (result!=kBindingSaved) {
      error=result==kBindingMissing?"source binding no longer exists; reload scenes":
        result==kBindingConflict?"this key/event already has a binding":
        "binding capacity reached (64 configured gestures)"; return false;
    }
  } else { error="unknown scene operation"; return false; }
  return commit(error);
}
void event(int input,int gesture,int repetitions) {
  if (input<0||input>=kInputCount||gesture<0||gesture>=kGestureCount) return;
  gInput=input; gGesture=gesture; gAt=millis(); ++gSequence; gLastType=kNone; gPercent=-1; gError="";
  auto *found=binding(gSettings,gSettings.active,input,gesture);
  if (!found) return;
  const Binding b=*found;
  const auto previousEpoch=gEpoch;
  for (int i=0;i<b.count;++i) {
    const Action a=b.actions[i]; gLastType=a.type;
    if (a.type==kSwitchScene) activate(a.target);
    else if (a.type==kNextScene||a.type==kPreviousScene) {
      const int direction=a.type==kNextScene?1:-1;
      for (int n=1;n<=kSceneCount;++n) {
        const int id=(int(gSettings.active)+direction*n+kSceneCount)%kSceneCount;
        if (exists(gSettings,id)) { activate(id); break; }
      }
    } else if (gHandler) gPercent=gHandler(a,repetitions);
    gFeedback.apply(a,gPercent,millis());
    if (a.type==kSwitchScene||a.type==kNextScene||a.type==kPreviousScene||gEpoch!=previousEpoch||!gError.isEmpty()) break;
  }
}
}  // namespace SceneService
