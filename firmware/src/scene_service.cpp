#include "scene_service.h"
#include <Preferences.h>
#include <cstdlib>
#include <new>
#include "scene_icons.h"
#include "scene_cue.h"
#include "action_sound.h"
#include "scene_order.h"
#include "general_service.h"
#include "audio_demo.h"
#include "bluetooth_service.h"

namespace SceneService {
namespace {
using namespace SceneSettings;
Stored gSettings, gCandidate;
SceneOrder::Stored gOrder;
bool gOrderStored=false;
Preferences gPrefs;
bool gOpen=false, gStored=false,gLevelDirty=false;
uint32_t gLevelChangedAt=0,gRevision=1;
Handler gHandler=nullptr;
uint32_t gEpoch=1, gSequence=0, gAt=0;
int gSource=-1;bool gWasGlobal=false;
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
String lightFeedback(const ActionFeedback::Light &light){
  return "{\"active\":"+String(light.led!=kInherit?"true":"false")+",\"led\":"+quote(kLeds[light.led])+
    ",\"brightness\":"+String(light.brightness)+",\"color\":"+color(light.color)+",\"percent\":"+String(light.percent)+
    ",\"remote\":"+String(light.remote?"true":"false")+",\"remaining_ms\":"+String(light.remainingMs)+"}";
}
String sceneLighting(const Scene &scene) {
  String out=",\"ring_mode\":"+quote(kLightingModes[scene.ring.mode])+",\"ring_color\":"+color(scene.ring.color)+
    ",\"keys_mode\":"+quote(kLightingModes[scene.keys.mode])+",\"keys_color\":"+color(scene.keys.color)+",\"icon\":"+quote(SceneIcons::kNames[scene.icon])+",\"ring_display\":"+quote(kRingDisplays[scene.ringDisplay])+",\"ring_style\":"+quote(RingFill::kStyles[scene.ringStyle]);
  const char *names[]={"screen","ring","keys"};
  for(int i=0;i<3;++i) {
    const String channel=names[i];const auto &level=scene.levels[i];
    out+=",\""+channel+"_brightness_mode\":"+quote(level.overrideGeneral?"override":"general");
    out+=",\""+channel+"_brightness\":"+String(level.percent);
  }
  return out;
}
int index(const String &value, const char *const *list, int count) {
  for (int i=0;i<count;++i) if (value==list[i]) return i;
  return -1;
}
String inputName(int input) {
  const int mask=comboMask(input);
  if(!mask)return String(kInputs[input]);
  const int modifier=comboModifier(input);
  String out=modifier>=0?String(kInputs[modifier])+">":"";
  for(int i=0;i<4;++i)if((mask&(1u<<i))&&i!=modifier){if(out.length()&&out[out.length()-1]!='>')out+='+';out+=kInputs[i];}
  return out;
}
int readInput(const String &value) {
  const int physical=index(value,kInputs,kInputCount);
  if(physical>=0)return physical;
  int mask=0,at=0,modifier=-1;
  const int arrow=value.indexOf('>');
  if(arrow>=0){modifier=index(value.substring(0,arrow),kInputs,4);if(modifier<0||arrow==int(value.length())-1)return -1;mask=1u<<modifier;at=arrow+1;}
  while(at<int(value.length())) {
    int end=value.indexOf('+',at);if(end<0)end=value.length();
    const int key=index(value.substring(at,end),kInputs,4);
    if(key<0||(mask&(1u<<key)))return -1;
    mask|=1u<<key;at=end+1;
    if(end==int(value.length())-1)return -1;
  }
  return comboMask(comboInput(mask,modifier))?comboInput(mask,modifier):-1;
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
bool saveOrder(const SceneOrder::Stored &order,String &error){
  if(!SceneOrder::valid(order)){error="invalid scene order";return false;}
  if((!gOrderStored||std::memcmp(&gOrder,&order,sizeof(order)))&&
     (!gOpen||gPrefs.putBytes("order",&order,sizeof(order))!=sizeof(order))){error="could not save scene order to NVS";return false;}
  gOrderStored=true;return true;
}
bool commit(String &error,int direction=0,bool sceneSound=true) {
  if (!valid(gCandidate)) { error="invalid scene, action or effect configuration"; return false; }
  if (!std::memcmp(&gSettings,&gCandidate,sizeof(gSettings))&&!gLevelDirty) return true;
  if (!gOpen||gPrefs.putBytes("settings",&gCandidate,sizeof(gCandidate))!=sizeof(gCandidate)) {
    error="could not save scenes to NVS"; return false;
  }
  const int previous=gSettings.active;
  const int previousPosition=SceneOrder::position(gOrder,gSettings,previous);
  BluetoothService::cancel();
  gSettings=gCandidate; gStored=true;gLevelDirty=false;++gRevision; ++gEpoch; gFeedback.clear();
  const unsigned cue=SceneCue::frequency(previous!=gSettings.active&&sceneSound&&GeneralService::sceneChangeSound(),
    previousPosition,SceneOrder::position(gOrder,gSettings,gSettings.active),direction);
  if(cue)AudioDemo::beep(cue,SceneCue::kDurationMs);
  return true;
}
bool activate(int id,int direction=0,bool sceneSound=true) {
  if (!exists(gSettings,id)) return false;
  gCandidate=gSettings; gCandidate.active=id;
  return commit(gError,direction,sceneSound);
}
String list(const char *const *names,int count) {
  String out="["; for (int i=0;i<count;++i) { if (i) out+=','; out+=quote(names[i]); } return out+"]";
}
}  // namespace

void begin(Handler handler) {
  gHandler=handler; gOpen=gPrefs.begin("scenes",false);
  SceneOrder::Stored order;
  if(gOpen&&gPrefs.getBytesLength("order")==sizeof(order)&&gPrefs.getBytes("order",&order,sizeof(order))==sizeof(order)&&SceneOrder::valid(order)){
    gOrder=order;gOrderStored=true;
  }
  const size_t bytes=gOpen?gPrefs.getBytesLength("settings"):0;
  if(bytes==sizeof(gSettings)&&gPrefs.getBytes("settings",&gSettings,sizeof(gSettings))==sizeof(gSettings)){
    if(valid(gSettings)){gStored=true;return;}
    const bool sound=GeneralService::sceneChangeSound();
    if(migrateV6(gSettings)||migrateV5(gSettings,sound)||migrateV4(gSettings,sound)||migrateV3(gSettings,sound)){gStored=gPrefs.putBytes("settings",&gSettings,sizeof(gSettings))==sizeof(gSettings);if(!gStored)gError="could not persist migrated scenes";return;}
  }
  // The two legacy layouts share one scratch area, avoiding permanent duplication.
  union Legacy { StoredV1 v1;StoredV2 v2;Legacy():v2(){} };
  static Legacy legacy;
  bool migrated=false;
  if(bytes==sizeof(StoredV2)) {
    new (&legacy.v2) StoredV2;
    migrated=gPrefs.getBytes("settings",&legacy.v2,sizeof(legacy.v2))==sizeof(legacy.v2)&&migrateV2(legacy.v2,gSettings,GeneralService::sceneChangeSound());
  } else if(bytes==sizeof(StoredV1)) {
    new (&legacy.v1) StoredV1;
    migrated=gPrefs.getBytes("settings",&legacy.v1,sizeof(legacy.v1))==sizeof(legacy.v1)&&migrate(legacy.v1,gSettings,GeneralService::sceneChangeSound());
  }
  if(migrated){gStored=gPrefs.putBytes("settings",&gSettings,sizeof(gSettings))==sizeof(gSettings);if(!gStored)gError="could not persist migrated scenes";return;}

  gSettings=defaults();
  gStored=gOpen&&gPrefs.putBytes("settings",&gSettings,sizeof(gSettings))==sizeof(gSettings);
}
uint32_t epoch() { return gEpoch; }
uint8_t activeScene(){return gSettings.active;}
uint8_t icon(){return gSettings.scenes[gSettings.active].icon;}
const char *name(){return gSettings.scenes[gSettings.active].name;}
SceneSettings::Level brightness(int channel){return gSettings.scenes[gSettings.active].levels[channel];}
int adjustBrightness(int channel,int value,bool relative) {
  auto &level=gSettings.scenes[gSettings.active].levels[channel];
  if(!level.overrideGeneral)return -1;
  int next=relative?int(level.percent)+value:value;if(next<0)next=0;if(next>100)next=100;
  if(next!=level.percent){level.percent=next;gLevelDirty=true;gStored=false;gLevelChangedAt=millis();++gRevision;}
  return next;
}
void tick(){if(gLevelDirty&&uint32_t(millis()-gLevelChangedAt)>=1500){gStored=gOpen&&gPrefs.putBytes("settings",&gSettings,sizeof(gSettings))==sizeof(gSettings);gLevelDirty=!gStored;gLevelChangedAt=millis();if(!gStored)gError="could not save scene brightness";}}
uint16_t combinations() {
  uint16_t flags=0;
  for(const auto &binding:gSettings.bindings)if(binding.count&&(binding.scene==gSettings.active||(binding.scene==0&&binding.global))) {
    const int mask=comboMask(binding.input);if(mask)flags|=1u<<mask;
  }
  return flags;
}
GestureBindings gestureBindings(){
  GestureBindings out;
  for(const auto &b:gSettings.bindings)if(b.count&&(b.scene==gSettings.active||(b.scene==0&&b.global))){
    const int group=comboMask(b.input);
    if(group){const int modifier=comboModifier(b.input);if(modifier<0)out.combos[group]|=1u<<b.gesture;else out.ordered[modifier][group]|=1u<<b.gesture;}else out.physical[b.input]|=1u<<b.gesture;
  }
  return out;
}
Feedback feedback() { return gFeedback.snapshot(millis()); }
SceneSettings::Light lighting(bool ring) {
  const auto &scene=gSettings.scenes[gSettings.active];
  return ring?scene.ring:scene.keys;
}
uint8_t ringDisplay(){return gSettings.scenes[gSettings.active].ringDisplay;}
uint8_t ringStyle(){return gSettings.scenes[gSettings.active].ringStyle;}
uint8_t screen() {
  const auto f=feedback();
  return f.active&&f.screen!=SceneSettings::kUnchanged?f.screen:gSettings.scenes[gSettings.active].screen;
}
String statusJson() {
  const auto f=feedback();
  String out="{\"active\":"+String(gSettings.active)+",\"name\":"+quote(gSettings.scenes[gSettings.active].name);
  out+=",\"standby_screen\":"+quote(kScreens[gSettings.scenes[gSettings.active].screen])+",\"screen\":"+quote(kScreens[screen()])+",\"stored\":"+String(gStored?"true":"false");
  out+=sceneLighting(gSettings.scenes[gSettings.active]);
  out+=",\"order_stored\":"+String(gOrderStored?"true":"false");
  out+=",\"revision\":"+String(gRevision);
  out+=",\"epoch\":"+String(gEpoch)+",\"last_event\":{\"sequence\":"+String(gSequence)+",\"at_ms\":"+String(gAt);
  out+=",\"input\":"+(gInput<0?String("null"):quote(inputName(gInput).c_str()));
  out+=",\"gesture\":"+(gGesture<0?String("null"):quote(kGestures[gGesture]));
  out+=",\"source_scene\":"+(gSource<0?String("null"):String(gSource))+",\"global\":"+String(gWasGlobal?"true":"false");
  out+=",\"action\":"+quote(kTypes[gLastType])+",\"percent\":"+String(gPercent)+"}";
  // Preserve the scalar summary for older clients; explicit channels are authoritative.
  const ActionFeedback::Light *summary=&f.ring;
  for(const auto &key:f.keys)if(summary->led==kInherit&&key.led!=kInherit)summary=&key;
  out+=",\"feedback\":{\"active\":"+String(f.active?"true":"false")+",\"led\":"+quote(kLeds[summary->led]);
  out+=",\"percent\":"+String(summary->percent)+",\"color\":"+color(summary->color)+",\"ring\":"+lightFeedback(f.ring)+",\"keys\":[";
  for(int i=0;i<4;++i){if(i)out+=',';out+=lightFeedback(f.keys[i]);}
  out+="]},\"error\":"+quote(gError.c_str())+"}";
  return out;
}
String configJson() {
  String out; out.reserve(8000);
  out="{\"state\":"+statusJson()+",\"limits\":{\"scenes\":8,\"bindings\":64,\"actions_per_gesture\":3}";
  out+=",\"timing\":{\"debounce_ms\":25,\"click_window_ms\":300,\"hold_ms\":600,\"combo_window_ms\":80}";
  out+=",\"catalog\":{\"inputs\":"+list(kInputs,kInputCount)+",\"combo_inputs\":"+list(kInputs,4)+",\"gestures\":"+list(kGestures,kGestureCount);
  out+=",\"actions\":"+list(kTypes,kTypeCount)+",\"led_effects\":"+list(kLeds,kLedCount)+",\"screen_effects\":"+list(kScreens,kScreenCount)+",\"lighting_modes\":"+list(kLightingModes,kLightingModeCount)+",\"icons\":[";
  for(int i=0;i<SceneIcons::kCount;++i){if(i)out+=',';out+="{\"id\":"+quote(SceneIcons::kNames[i])+",\"label\":"+quote(SceneIcons::kLabels[i])+"}";}
  out+="]},\"scenes\":[";
  bool first=true;
  uint8_t ordered[kSceneCount];const int sceneCount=SceneOrder::list(gOrder,gSettings,ordered);
  for (int n=0;n<sceneCount;++n) {
    const int i=ordered[n];
    if (!first) out+=','; first=false;
    out+="{\"id\":"+String(i)+",\"name\":"+quote(gSettings.scenes[i].name)+",\"protected\":"+String(i==0?"true":"false")+",\"standby_screen\":"+quote(kScreens[gSettings.scenes[i].screen])+sceneLighting(gSettings.scenes[i])+",\"bindings\":[";
    bool firstBinding=true;
    for (const auto &b:gSettings.bindings) {
      if (!b.count||b.scene!=i) continue;
      if (!firstBinding) out+=','; firstBinding=false;
      out+="{\"input\":"+quote(inputName(b.input).c_str())+",\"gesture\":"+quote(kGestures[b.gesture])+",\"global\":"+String(b.global?"true":"false")+",\"actions\":[";
      for (int n=0;n<b.count;++n) {
        if (n) out+=','; const auto &a=b.actions[n];
        out+="{\"type\":"+quote(kTypes[a.type])+",\"value\":"+String(a.value)+",\"target\":"+String(a.target);
        out+=",\"led\":"+quote(kLeds[a.ledEffect()])+",\"led_brightness\":"+String(a.ledBrightness())+",\"sound\":"+String(a.soundEnabled()?"true":"false")+",\"screen\":"+quote(kScreens[a.screen])+",\"color\":"+color(a.colorRgb())+",\"feedback_ms\":"+String(a.feedbackMs)+"}";
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
    gCandidate.scenes[id]=Scene{};gCandidate.scenes[id].used=1;
  } else if (!number(server,"scene_id",0,kSceneCount-1,id,error,true)||!exists(gCandidate,id)) {
    if (error.isEmpty()) error="scene not found"; return false;
  }
  if(op=="reorder"){
    int position=0;SceneOrder::Stored order=gOrder;
    if(!number(server,"position",0,kSceneCount-1,position,error,true)||!SceneOrder::move(order,gSettings,id,position)){
      if(error.isEmpty())error="invalid scene position";return false;
    }
    if(!saveOrder(order,error))return false;
    if(std::memcmp(&gOrder,&order,sizeof(order))){
      gOrder=order;++gRevision;++gEpoch;gFeedback.clear();BluetoothService::cancel();
    }
    return true;
  }
  if (op=="create"||op=="update") {
    String name=server.arg("name"); name.trim();
    if (!name.length()||name.length()>24) { error="scene name must be 1-24 UTF-8 bytes"; return false; }
    if (id==0&&name!="Default") { error="Default scene name is protected"; return false; }
    for (int i=0;i<kSceneCount;++i) if (i!=id && exists(gCandidate,i) && name==gCandidate.scenes[i].name) {
      error="scene names must be distinct"; return false;
    }
    name.toCharArray(gCandidate.scenes[id].name,25);
    if(server.hasArg("standby_screen")){
      const int screen=index(server.arg("standby_screen"),kScreens,kScreenCount);
      if(screen<kClock){error="invalid idle screen";return false;}
      gCandidate.scenes[id].screen=screen;
    }
    if(server.hasArg("ring_display")){const int mode=index(server.arg("ring_display"),kRingDisplays,kRingDisplayCount);if(mode<0){error="unknown ring display";return false;}gCandidate.scenes[id].ringDisplay=mode;}
    if(server.hasArg("ring_style")){const int style=index(server.arg("ring_style"),RingFill::kStyles,RingFill::kStyleCount);if(style<0){error="unknown ring fill style";return false;}gCandidate.scenes[id].ringStyle=style;}
    if(server.hasArg("icon")) {
      const int icon=index(server.arg("icon"),SceneIcons::kNames,SceneIcons::kCount);
      if(icon<0){error="unknown scene icon";return false;}gCandidate.scenes[id].icon=icon;
    }
    const char *channels[]={"screen","ring","keys"};
    for(int channel=0;channel<3;++channel) {
      const String name=channels[channel];auto &level=gCandidate.scenes[id].levels[channel];
      if(server.hasArg(name+"_brightness_mode")) {
        const String mode=server.arg(name+"_brightness_mode");
        if(mode!="general"&&mode!="override"){error="brightness mode must be general or override";return false;}
        level.overrideGeneral=mode=="override";
      }
      if(server.hasArg("auto_"+name)){error="automatic brightness is no longer supported";return false;}
      int percent=level.percent;
      if(!number(server,(name+"_brightness").c_str(),0,100,percent,error))return false;
      level.percent=percent;
    }
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
    const int input=readInput(server.arg("input")), gesture=index(server.arg("gesture"),kGestures,kGestureCount);
    if (id!=gSettings.active||!validGesture(input,gesture)) { error="run a valid gesture in the active scene"; return false; }
    event(input,gesture); return true;
  } else if (op=="binding") {
    Binding b; b.scene=id;
    const int input=readInput(server.arg("input")), gesture=index(server.arg("gesture"),kGestures,kGestureCount);
    int count=0;
    if (!validGesture(input,gesture)) { error="invalid input or gesture"; return false; }
    if (!number(server,"count",0,kActionCount,count,error,true)) return false;
    b.input=input; b.gesture=gesture; b.count=count;
    int global=0;if(!number(server,"global",0,1,global,error))return false;
    if(global&&id!=0){error="only Default bindings can be Global";return false;}b.global=global;
    for (int i=0;i<count;++i) {
      const String prefix="a"+String(i)+"_";
      auto &a=b.actions[i];
      int type=index(server.arg(prefix+"type"),kTypes,kTypeCount), led=kInherit, screen=kUnchanged;
      if (server.hasArg(prefix+"led")) led=index(server.arg(prefix+"led"),kLeds,kLedCount);
      if (server.hasArg(prefix+"screen")) screen=index(server.arg(prefix+"screen"),kScreens,kScreenCount);
      if (type<0||led<0||screen<0) { error="invalid action or effect"; return false; }
      a.type=type; a.led=led; a.screen=screen;
      int value=0,target=0,duration=1500,sound=0,brightness=-1;
      if (!number(server,(prefix+"value").c_str(),-100,4000,value,error)||
          !number(server,(prefix+"target").c_str(),0,type==kBluetoothKey?255:kSceneCount-1,target,error)||
          !number(server,(prefix+"feedback_ms").c_str(),250,10000,duration,error)||
          !number(server,(prefix+"sound").c_str(),0,1,sound,error)||
          !number(server,(prefix+"led_brightness").c_str(),-1,100,brightness,error)) return false;
      a.value=value; a.target=target; a.feedbackMs=duration;
      a.setSound(sound);a.setLedBrightness(brightness);
      uint32_t rgb=a.colorRgb();
      if (server.hasArg(prefix+"color")&&!readColor(server.arg(prefix+"color"),rgb)) { error="color must be #RRGGBB"; return false; }
      a.setColorRgb(rgb);
      if (!validAction(gCandidate,a)) { error="invalid action value, scene target or proportional effect"; return false; }
    }
    int sourceInput=-1,sourceGesture=-1,createOnly=0;
    if (server.hasArg("source_input") || server.hasArg("source_gesture")) {
      sourceInput=readInput(server.arg("source_input"));
      sourceGesture=index(server.arg("source_gesture"),kGestures,kGestureCount);
      if (!validGesture(sourceInput,sourceGesture)) {
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
  if(op=="create"){
    if(!valid(gCandidate)){error="invalid scene, action or effect configuration";return false;}
    SceneOrder::Stored order=gOrder;SceneOrder::append(order,id);
    if(!saveOrder(order,error))return false;
    gOrder=order;
  }
  return commit(error);
}
void event(int input,int gesture,int repetitions) {
  if (!validGesture(input,gesture)) return;
  gInput=input; gGesture=gesture; gAt=millis(); ++gSequence; gLastType=kNone; gPercent=-1; gError="";gSource=-1;gWasGlobal=false;
  const auto *found=effectiveBinding(gSettings,gSettings.active,input,gesture);
  if (!found) return;
  const Binding b=*found;gSource=b.scene;gWasGlobal=b.global;
  const auto previousEpoch=gEpoch;
  for (int i=0;i<b.count;++i) {
    const Action a=b.actions[i]; gLastType=a.type;
    const int previousScene=gSettings.active;
    const int previousPosition=SceneOrder::position(gOrder,gSettings,previousScene);
    if (a.type==kSwitchScene) activate(a.target,0,false);
    else if (a.type==kNextScene||a.type==kPreviousScene) {
      const int direction=a.type==kNextScene?1:-1;
      const int id=SceneOrder::neighbor(gOrder,gSettings,gSettings.active,direction);
      if(id>=0)activate(id,direction,false);
    } else if (gHandler) gPercent=gHandler(a,repetitions);
    const unsigned cue=gError.isEmpty()?ActionSound::frequency(a,previousPosition,SceneOrder::position(gOrder,gSettings,gSettings.active)):0;
    if(cue)AudioDemo::beep(cue,ActionSound::duration(a));
    gFeedback.apply(a,gPercent,millis(),input);
    if (a.type==kSwitchScene||a.type==kNextScene||a.type==kPreviousScene||gEpoch!=previousEpoch||!gError.isEmpty()) break;
  }
}
}  // namespace SceneService
