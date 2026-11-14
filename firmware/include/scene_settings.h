#pragma once
#include <cstdint>
#include <cstring>
#include "coloplay_ble_protocol.h"

namespace SceneSettings {
constexpr int kSceneCount = 8, kBindingCount = 64, kActionCount = 3, kInputCount = 7;
enum Input : uint8_t { kKey1, kKey2, kKey3, kKey4, kKnob, kOpen, kRec };
enum Gesture : uint8_t { kSingle, kDouble, kTriple, kHold, kHoldRelease,
  kLeft, kRight, kPressLeft, kPressRight, kGestureCount };
enum Type : uint8_t { kNone, kNextScene, kPreviousScene, kSwitchScene,
  kScreenChange, kScreenSet, kRingChange, kRingSet, kKeysChange, kKeysSet,
  kVolumeChange, kVolumeSet, kRainbow, kStatic, kToggleAwake, kSync, kTone,
  kBluetoothVolume,kBluetoothMedia,kBluetoothKey,kTypeCount };
enum Led : uint8_t { kInherit, kProportional, kFeedbackRainbow, kFeedbackStatic, kOff, kLedCount };
enum Screen : uint8_t { kUnchanged, kClock, kBlank, kBluetoothVolumeScreen, kBluetoothPlaybackScreen, kScreenCount };
constexpr const char *kInputs[] = {"key1","key2","key3","key4","knob","open","rec"};
constexpr const char *kGestures[] = {"single","double","triple","hold","hold_release","left","right","press_left","press_right"};
constexpr const char *kTypes[] = {"none","next_scene","previous_scene","switch_scene",
  "screen_change","screen_set","ring_change","ring_set","keys_change","keys_set",
  "volume_change","volume_set","rainbow","static","toggle_awake","sync_time","tone",
  "bluetooth_volume","bluetooth_media","bluetooth_key"};
constexpr const char *kLeds[] = {"inherit","proportional","rainbow","static","off"};
constexpr const char *kScreens[] = {"unchanged","clock","blank","bluetooth_volume","bluetooth_playback"};
struct Action {
  // Keep the on-flash action at 12 bytes: the high LED byte bit is audio feedback.
  static constexpr uint8_t kSound = 0x80;
  uint8_t type = kNone, target = 0, led = kInherit, screen = kUnchanged;
  int16_t value = 0;
  uint16_t feedbackMs = 1500;
  uint32_t color = 0x40FFAA;
  uint8_t ledEffect() const { return led & ~kSound; }
  bool soundEnabled() const { return led & kSound; }
  void setSound(bool enabled) { led = ledEffect() | (enabled ? kSound : 0); }
};
struct BindingV1 {
  uint8_t scene = 0, input = 0, gesture = 0, count = 0;
  Action actions[kActionCount];
};
struct Binding : BindingV1 { uint8_t global=0,reserved[3]={}; };
enum LightingMode : uint32_t { kGeneralLighting, kSceneRainbow, kSceneStatic, kLightingModeCount };
constexpr const char *kLightingModes[] = {"general","rainbow","static"};
struct Light {
  uint32_t mode, color;
  constexpr Light(uint32_t modeValue=kGeneralLighting, uint32_t colorValue=0xFFFFFF) : mode(modeValue), color(colorValue) {}
};
struct SceneV1 { uint8_t used = 0, screen = kClock; char name[25] = {}; uint8_t reserved = 0; };
struct StoredV1 {
  uint32_t version = 1;
  uint8_t active = 0, reserved[3] = {};
  SceneV1 scenes[kSceneCount];
  BindingV1 bindings[kBindingCount];
};
struct SceneV2 : SceneV1 { Light ring, keys; };
struct StoredV2 {
  uint32_t version=2;
  uint8_t active=0,reserved[3]={};
  SceneV2 scenes[kSceneCount];
  BindingV1 bindings[kBindingCount];
};
struct Level {
  uint8_t overrideGeneral=0,percent=70,automatic=0,reserved=0;
};
enum RingDisplay : uint8_t { kRingLighting, kRingBluetoothProgress, kRingDisplayCount };
constexpr const char *kRingDisplays[]={"lighting","bluetooth_progress"};
struct Scene : SceneV2 { Level levels[3]; uint8_t icon=0,ringDisplay=kRingLighting,reserved2[2]={}; };
struct Stored {
  uint32_t version = 6;
  uint8_t active = 0, reserved[3] = {};
  Scene scenes[kSceneCount];
  Binding bindings[kBindingCount];
};
static_assert(sizeof(Action) == 12 && sizeof(StoredV1) == 2792 && sizeof(StoredV2)==2920 && sizeof(Stored)==3304, "scene storage layout changed");
constexpr int kComboFlag=0x80;
inline int comboMask(int input) {
  const int mask=input&15;
  const int modifier=((input>>4)&7)-1;
  return input>=kComboFlag&&input<=255&&modifier<4&&mask&&(mask&(mask-1))&&
    (modifier<0||(mask&(1u<<modifier))) ? mask : 0;
}
inline int comboModifier(int input){return comboMask(input)?((input>>4)&7)-1:-1;}
inline int comboInput(int mask,int modifier=-1){return kComboFlag|((modifier+1)<<4)|mask;}
inline bool validInput(int input) { return (input>=0&&input<kInputCount)||comboMask(input); }
inline bool validGesture(int input,int gesture) {
  return validInput(input)&&gesture>=0&&gesture<kGestureCount&&(input==kKnob||gesture<kLeft);
}
inline bool exists(const Stored &s, int id) { return id >= 0 && id < kSceneCount && s.scenes[id].used == 1; }
inline bool percentType(int type) { return (type >= kScreenChange && type <= kVolumeSet)||type==kBluetoothVolume; }
inline bool changeType(int type) { return type == kScreenChange || type == kRingChange || type == kKeysChange || type == kVolumeChange; }
inline bool validName(const char *name) {
  const auto *end = static_cast<const char *>(std::memchr(name, 0, 25));
  if (!end || end == name) return false;
  bool visible = false;
  for (const char *p = name; p < end; ++p) {
    if (static_cast<unsigned char>(*p) < 32 || *p == 127) return false;
    if (*p != ' ') visible = true;
  }
  return visible;
}
inline bool validAction(const Stored &s, const Action &a) {
  if (a.type >= kTypeCount || a.ledEffect() >= kLedCount || a.screen >= kScreenCount ||
      a.color > 0xFFFFFF || a.feedbackMs < 250 || a.feedbackMs > 10000) return false;
  if (a.type == kSwitchScene && !exists(s, a.target)) return false;
  if (a.ledEffect() == kProportional && !percentType(a.type)) return false;
  if (a.type>=kScreenChange&&a.type<=kVolumeSet && (a.value > 100 || a.value < (changeType(a.type) ? -100 : 0))) return false;
  if(a.type==kBluetoothVolume&&a.value!=1&&a.value!=-1&&a.value!=16&&a.value!=-16&&a.value!=64&&a.value!=-64)return false;
  if(a.type==kBluetoothMedia&&(!cp_media_usage(a.value)||a.target>2||(a.target&&a.value!=0xb3&&a.value!=0xb4)))return false;
  if(a.type==kBluetoothKey&&(a.value<1||a.value>0xe7))return false;
  if (a.type == kTone && (a.value < 100 || a.value > 4000)) return false;
  return true;
}
inline bool validLevel(const Level &level){return level.overrideGeneral<=1&&level.percent<=100&&level.automatic<=1;}
inline Level resolvedLevel(const Level &scene,uint32_t general,bool automatic) {
  if(scene.overrideGeneral)return scene;
  Level out;out.percent=general;out.automatic=automatic;return out;
}
inline bool validLight(const Light &light) { return light.mode < kLightingModeCount && light.color <= 0xFFFFFF; }
// General mode is resolved at render time, so later General changes still apply.
inline Light resolveLighting(const Light &scene, bool generalStatic, uint32_t generalColor) {
  return scene.mode == kGeneralLighting ? Light{generalStatic ? kSceneStatic : kSceneRainbow, generalColor} : scene;
}
inline bool valid(const Stored &s) {
  if (s.version != 6 || !exists(s, 0) || !exists(s, s.active) || !validName(s.scenes[0].name) || std::strcmp(s.scenes[0].name, "Default")) return false;
  for (int i=0;i<kSceneCount;++i) {
    const auto &scene=s.scenes[i];
    if (scene.used > 1 || (scene.used && (!validName(scene.name) || scene.screen < kClock || scene.screen >= kScreenCount || !validLight(scene.ring) || !validLight(scene.keys)))) return false;
    if(scene.used){if(scene.icon>=32||scene.ringDisplay>=kRingDisplayCount)return false;for(const auto &level:scene.levels)if(!validLevel(level))return false;}
    if (scene.used) for (int j=0;j<i;++j) if (s.scenes[j].used&&!std::strcmp(scene.name,s.scenes[j].name)) return false;
  }
  for (int i = 0; i < kBindingCount; ++i) {
    const auto &b = s.bindings[i];
    if (!b.count) continue;
    if (!exists(s, b.scene) || b.global>1 || (b.global&&b.scene!=0) || !validGesture(b.input,b.gesture) || b.count > kActionCount) return false;
    for (int j = 0; j < b.count; ++j) if (!validAction(s, b.actions[j])) return false;
    for (int j = 0; j < i; ++j) {
      const auto &p = s.bindings[j];
      if (p.count && p.scene == b.scene && p.input == b.input && p.gesture == b.gesture) return false;
    }
  }
  return true;
}
// Validate the complete legacy record before replacing the caller's settings.
inline bool migrateV5(Stored &settings,bool sceneSound=true){
  if(settings.version!=5)return false;
  for(const auto &b:settings.bindings)if(b.count){
    if(b.count>kActionCount)return false;
    for(int i=0;i<b.count;++i)if(b.actions[i].led>=kLedCount)return false;
  }
  settings.version=6;
  if(!valid(settings)){settings.version=5;return false;}
  for(auto &b:settings.bindings)for(int i=0;i<b.count;++i){
    auto &a=b.actions[i];
    a.setSound(a.type==kTone||(sceneSound&&a.type>=kNextScene&&a.type<=kSwitchScene));
  }
  return true;
}
inline bool migrateV4(Stored &settings,bool sceneSound=true){
  if(settings.version!=4)return false;
  for(const auto &scene:settings.scenes)if(scene.used&&scene.ringDisplay)return false;
  for(const auto &b:settings.bindings)if(b.count&&b.input>=kComboFlag&&b.input!=(kComboFlag|(b.input&15)))return false;
  settings.version=5;if(migrateV5(settings,sceneSound))return true;settings.version=4;return false;
}
inline bool migrateV3(Stored &settings,bool sceneSound=true){
  if(settings.version!=3)return false;
  for(const auto &scene:settings.scenes)if(scene.used&&scene.screen>kBlank)return false;
  for(const auto &binding:settings.bindings)if(binding.count){if(binding.count>kActionCount)return false;for(int i=0;i<binding.count;++i)if(binding.actions[i].type>kTone||binding.actions[i].screen>kBlank)return false;}
  settings.version=4;if(migrateV4(settings,sceneSound))return true;settings.version=3;return false;
}
inline bool migrate(const StoredV1 &old, Stored &out,bool sceneSound=true) {
  if (old.version != 1) return false;
  Stored candidate;
  candidate.version=5;
  candidate.active=old.active;
  for (int i=0;i<kSceneCount;++i) static_cast<SceneV1 &>(candidate.scenes[i])=old.scenes[i];
  for(int i=0;i<kBindingCount;++i){if(old.bindings[i].count&&old.bindings[i].input>=kInputCount)return false;static_cast<BindingV1 &>(candidate.bindings[i])=old.bindings[i];}
  if (!migrateV5(candidate,sceneSound)) return false;
  out=candidate;
  return true;
}
// Upgrade the original lighting scene record without changing its saved fields.
inline bool migrateV2(const StoredV2 &old,Stored &out,bool sceneSound=true) {
  if(old.version!=2)return false;
  Stored candidate;candidate.active=old.active;
  candidate.version=5;
  for(int i=0;i<kSceneCount;++i)static_cast<SceneV2 &>(candidate.scenes[i])=old.scenes[i];
  for(int i=0;i<kBindingCount;++i) {
    if(old.bindings[i].count&&old.bindings[i].input>=kInputCount)return false;
    static_cast<BindingV1 &>(candidate.bindings[i])=old.bindings[i];
  }
  if(!migrateV5(candidate,sceneSound))return false;
  out=candidate;return true;
}
inline Binding *binding(Stored &s, int scene, int input, int gesture) {
  for (auto &b : s.bindings) if (b.count && b.scene == scene && b.input == input && b.gesture == gesture) return &b;
  return nullptr;
}
inline const Binding *effectiveBinding(const Stored &s,int scene,int input,int gesture) {
  const Binding *global=nullptr;
  for(const auto &b:s.bindings)if(b.count&&b.input==input&&b.gesture==gesture) {
    if(b.scene==scene)return &b;
    if(b.scene==0&&b.global)global=&b;
  }
  return global;
}
inline bool assign(Stored &s, const Binding &value) {
  Binding *b = binding(s, value.scene, value.input, value.gesture);
  if (!b && !value.count) return true;
  if (!b) for (auto &slot : s.bindings) if (!slot.count) { b = &slot; break; }
  if (!b) return false;
  *b = value;
  return true;
}
enum BindingEdit { kBindingSaved, kBindingMissing, kBindingConflict, kBindingFull };
// Move/replace one row in the candidate before its single NVS commit. New or
// duplicated rows use createOnly to avoid overwriting an existing gesture.
inline BindingEdit editBinding(Stored &s, const Binding &value,
                               int sourceInput=-1, int sourceGesture=-1, bool createOnly=false) {
  Binding *source=nullptr;
  if (sourceInput>=0 || sourceGesture>=0) {
    if (sourceInput<0 || sourceGesture<0) return kBindingMissing;
    source=binding(s,value.scene,sourceInput,sourceGesture);
    if (!source) return kBindingMissing;
  }
  Binding *destination=binding(s,value.scene,value.input,value.gesture);
  if (destination && (createOnly || (source && source!=destination && value.count))) return kBindingConflict;
  if (source && source!=destination) {
    *source=Binding{};
    if (!value.count) return kBindingSaved;
  }
  return assign(s,value)?kBindingSaved:kBindingFull;
}
inline bool remove(Stored &s, int id) {
  if (!id || !exists(s, id)) return false;
  s.scenes[id] = Scene{};
  if (s.active == id) s.active = 0;
  for (auto &b : s.bindings) {
    if (b.scene == id) b = Binding{};
    else for (int i = 0; i < b.count; ++i) if (b.actions[i].type == kSwitchScene && b.actions[i].target == id) b.actions[i].target = 0;
  }
  return true;
}
inline Stored defaults() {
  Stored s;
  s.scenes[0].used = 1;
  std::strcpy(s.scenes[0].name, "Default");
  auto add = [&](int input, int gesture, int type, int value, int led = kInherit) {
    Binding b; b.input = input; b.gesture = gesture; b.count = 1;
    b.actions[0].type = type; b.actions[0].value = value; b.actions[0].led = led;
    b.actions[0].setSound(type==kTone||(type>=kNextScene&&type<=kSwitchScene)); assign(s, b);
  };
  add(kKey1,kSingle,kNextScene,0); add(kKey2,kSingle,kPreviousScene,0);
  add(kKey4,kHold,kSwitchScene,0); add(kOpen,kSingle,kToggleAwake,0);
  add(kRec,kSingle,kTone,660);
  add(kKnob,kLeft,kVolumeChange,-2,kProportional); add(kKnob,kRight,kVolumeChange,2,kProportional);
  add(kKnob,kPressLeft,kScreenChange,-3); add(kKnob,kPressRight,kScreenChange,3);
  return s;
}
}  // namespace SceneSettings
