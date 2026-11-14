#include "device_log.h"
#include "hardware_demo.h"
#include "audio_demo.h"
#include "board_pins.h"
#include "demo_levels.h"
#include "led_strip.h"
#include "rotary_detents.h"
#include "clock_service.h"
#include "inter_clock_fonts.h"
#include "lighting_settings.h"
#include "scene_service.h"
#include "chord_engine.h"
#include "scene_icons.h"
#include "general_service.h"
#include "idle_timer.h"
#include "bluetooth_service.h"
#include "playback_ring.h"
#include "playback_assets.h"
#include "bitmap_font.h"

#include <Adafruit_NeoPixel.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include <driver/adc.h>
#include <driver/pcnt.h>
#include <esp_adc_cal.h>
#include <esp_heap_caps.h>
#include <cmath>
#include <esp_attr.h>

// Arduino numbers FSPI as 0; the IDF register macros number GPSPI2 as 2.
// TFT_eSPI's USE_FSPI_PORT branch bridges the two. Fail the build if a
// dependency/config change brings back the null register address.
static_assert(SPI_PORT == 2, "TFT_eSPI must use the S3 GPSPI2 register index");
static_assert(SPI_USER_REG(SPI_PORT) == DR_REG_SPI2_BASE + 0x10,
              "TFT_eSPI SPI register base does not match the selected S3 bus");

namespace HardwareDemo {
namespace {
constexpr float kTau = 6.28318530718f;
constexpr uint16_t kBackground = 0x0843, kMuted = 0x8CB3;
constexpr uint16_t kMint = 0x4FF4, kViolet = 0xA37F, kAmber = 0xFD87;
constexpr const char *kPages[] = {"ORBIT", "INPUTS", "AUDIO", "SENSORS", "CLOCK"};
constexpr int kPageCount = sizeof(kPages) / sizeof(kPages[0]);
constexpr int kClockPage = 4;
TFT_eSPI gTft;
TFT_eSprite gCanvas(&gTft);
struct Key {
  const char *name;
  int pin;
  bool pullup;
  bool raw = false, pressed = false;
  uint32_t changed = 0, presses = 0;
  Key(const char *label, int gpio, bool up) : name(label), pin(gpio), pullup(up) {}
};
Key gKeys[] = {{"KEY1", Board::kKey1, true}, {"KEY2", Board::kKey2, true},
               {"KEY3", Board::kKey3, true}, {"KEY4", Board::kKey4, true},
               {"KNOB", Board::kKnobButton, false},
               {"OPEN_KEY", Board::kOpenButton, true},
               {"REC_KEY", Board::kRecoveryButton, true}};
bool gReady = false, gPaused = false, gDisplay = false, gEncoder = false;
bool gAwake = true, gAuto = false, gPsramTest = false, gBacklight = false;
int gPage = kClockPage;
LightingSettings::Stored gLighting;
Preferences gLightingPrefs;
bool gLightingPrefsOpen = false, gLightingStored = false;
bool gLightingDirty = false, gHaveLight = false, gLightUsable = false;
uint32_t gLightingChangedAt = 0, gLastLightAt = 0;
float gFilteredLight = 0, gAutoFactor = 1;
uint8_t gSceneShown=0;
uint32_t gSceneOverlayUntil=0;
int gScreenLevel = 70, gRingLevel = 70, gKeysLevel = 70;
int gLedTest = 0;
uint32_t gLedTestUntil = 0;
int32_t gEncoderEdges = 0, gKnobPosition = 0;
int16_t gLastEncoder = 0;
RotaryDetents gDetents;
ChordEngine gGestures;
IdleTimer gIdle;
uint32_t gSleepMinutes=60;
uint32_t gSceneEpoch = 0;
uint32_t gLastPage = 0, gLastFrame = 0, gFrames = 0, gLastSensors = 0;
uint32_t gLastMetrics = 0, gPreviousFrames = 0, gPreviousTe = 0;
uint32_t gLastLeds = 0, gLedFrames = 0, gPsramTestBytes = 0;
float gFps = 0, gTeHz = 0;
volatile uint32_t gTePulses = 0;
int gSupplyRaw = 0, gLightRaw = -1;
uint32_t gSupplyMv = 0, gLightMv = 0, gLightReadings = 0, gLightErrors = 0;
esp_err_t gLightError = ESP_OK;
esp_adc_cal_characteristics_t gSupplyCal = {}, gLightCal = {};
enum Stage : uint32_t {
  kNotStarted, kPsram, kEncoder, kSensors, kLeds, kBacklight,
  kLcdInit, kFramebuffer, kLcdTe, kAudio, kFirstFrame, kRunning, kLedPower, kStageCount
};
const char *const kStageNames[] = {"not-started", "psram-test", "encoder",
  "sensors", "leds", "backlight", "lcd-init", "framebuffer", "lcd-te",
  "audio", "first-frame", "running", "led-power"};
constexpr uint32_t kStageMagic = 0xC010D1A6;
RTC_NOINIT_ATTR uint32_t gRetainedMagic;
RTC_NOINIT_ATTR uint32_t gRetainedStage;
Stage gStage = kNotStarted, gLastBootStage = kNotStarted;

void setStage(Stage stage) {
  gStage = stage;
  gRetainedStage = stage;
  gRetainedMagic = kStageMagic;
  DeviceLog::printf("[hardware] stage=%s\n", kStageNames[stage]);
}

void IRAM_ATTR onTe() { ++gTePulses; }

bool sceneOverlayVisible(uint32_t now){return gSceneOverlayUntil&&static_cast<int32_t>(gSceneOverlayUntil-now)>0;}
bool screenVisible(uint32_t now){return gAwake&&(sceneOverlayVisible(now)||SceneService::screen()!=SceneSettings::kBlank);}

void setBacklight() {
  if (gBacklight) ledcWrite(0, screenVisible(millis()) ? gScreenLevel * 1023 / 100 : 0);
}

void updateBrightness() {
  gLightUsable = gHaveLight && uint32_t(millis() - gLastLightAt) <= 5000;
  gAutoFactor = gLightUsable ? LightingSettings::factor(
      gFilteredLight, gLighting.darkRaw, gLighting.brightRaw) : 1;
  const auto screen=SceneSettings::resolvedLevel(SceneService::brightness(0),gLighting.screen,gLighting.autoScreen);
  const auto ring=SceneSettings::resolvedLevel(SceneService::brightness(1),gLighting.ring,gLighting.autoRing);
  const auto keys=SceneSettings::resolvedLevel(SceneService::brightness(2),gLighting.keys,gLighting.autoKeys);
  gScreenLevel = LightingSettings::level(screen.percent, screen.automatic, gLightUsable, gAutoFactor);
  gRingLevel = LightingSettings::level(ring.percent, ring.automatic, gLightUsable, gAutoFactor);
  gKeysLevel = LightingSettings::level(keys.percent, keys.automatic, gLightUsable, gAutoFactor);
  setBacklight();
}

bool saveLighting(const LightingSettings::Stored &candidate) {
  return gLightingPrefsOpen && gLightingPrefs.putBytes("settings", &candidate,
      sizeof(candidate)) == sizeof(candidate);
}

void loadLighting() {
  gLightingPrefsOpen = gLightingPrefs.begin("lighting", false);
  LightingSettings::Stored stored;
  if (gLightingPrefsOpen && gLightingPrefs.getBytesLength("settings") == sizeof(stored) &&
      gLightingPrefs.getBytes("settings", &stored, sizeof(stored)) == sizeof(stored) &&
      LightingSettings::valid(stored)) {
    gLighting = stored;
    gLightingStored = true;
  } else {
    gLightingStored = saveLighting(gLighting);
  }
  updateBrightness();
}

void selectPage(int page, bool automatic) {
  gPage = constrain(page, 0, kPageCount - 1);
  gAuto = automatic;
  gLastPage = millis();
}

void readSensors() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; ++i) sum += adc1_get_raw(ADC1_CHANNEL_6);
  gSupplyRaw = sum / 16;
  // Legacy supply estimate: calibrated ADC x3. The physical divider and
  // scale are not established by the factory analysis; this is not battery SOC.
  gSupplyMv = esp_adc_cal_raw_to_voltage(gSupplyRaw, &gSupplyCal) * 3;
  int light = 0;
  gLightError = adc2_get_raw(ADC2_CHANNEL_5, ADC_WIDTH_BIT_12, &light);
  if (gLightError == ESP_OK) {
    gLightRaw = light;
    gLightMv = esp_adc_cal_raw_to_voltage(light, &gLightCal);
    gFilteredLight = gHaveLight ? gFilteredLight + 0.2f * (light - gFilteredLight) : light;
    gHaveLight = true;
    gLastLightAt = millis();
    ++gLightReadings;
  } else {
    ++gLightErrors;
    // Keep the last good measurement and publish the error explicitly.
    // WiFi can contend with ADC2; a failed read is not a dark-room value.
  }
  updateBrightness();
}

void checkPsram() {
  if (!psramFound()) return;
  constexpr size_t kBytes = 256 * 1024;
  auto *memory = static_cast<uint32_t *>(heap_caps_malloc(kBytes, MALLOC_CAP_SPIRAM));
  if (!memory) return;
  bool ok = true;
  for (uint32_t seed : {0xA55A1234u, 0x5AA5EDCBu}) {
    for (size_t i = 0; i < kBytes / sizeof(uint32_t); ++i) memory[i] = seed ^ (i * 2654435761u);
    for (size_t i = 0; i < kBytes / sizeof(uint32_t); ++i) {
      if (memory[i] != (seed ^ (i * 2654435761u))) { ok = false; break; }
    }
  }
  heap_caps_free(memory);
  gPsramTest = ok;
  gPsramTestBytes = kBytes;
}

void setupEncoder() {
  pinMode(Board::kEncoderA, INPUT_PULLUP);
  pinMode(Board::kEncoderB, INPUT_PULLUP);
  pcnt_config_t channel = {};
  channel.unit = PCNT_UNIT_0;
  channel.channel = PCNT_CHANNEL_0;
  channel.pulse_gpio_num = Board::kEncoderA;
  channel.ctrl_gpio_num = Board::kEncoderB;
  channel.pos_mode = PCNT_COUNT_INC;
  channel.neg_mode = PCNT_COUNT_DEC;
  channel.lctrl_mode = PCNT_MODE_REVERSE;
  channel.hctrl_mode = PCNT_MODE_KEEP;
  channel.counter_h_lim = 32767;
  channel.counter_l_lim = -32768;
  if (pcnt_unit_config(&channel) != ESP_OK) return;
  channel.channel = PCNT_CHANNEL_1;
  channel.pulse_gpio_num = Board::kEncoderB;
  channel.ctrl_gpio_num = Board::kEncoderA;
  channel.lctrl_mode = PCNT_MODE_KEEP;
  channel.hctrl_mode = PCNT_MODE_REVERSE;
  if (pcnt_unit_config(&channel) != ESP_OK) return;
  // Reject pulses shorter than 12.5 us; complete quadrature transitions
  // cancel contact bounce before the UI consumes a full detent.
  pcnt_set_filter_value(PCNT_UNIT_0, 1000);
  pcnt_filter_enable(PCNT_UNIT_0);
  pcnt_counter_clear(PCNT_UNIT_0);
  gEncoder = pcnt_counter_resume(PCNT_UNIT_0) == ESP_OK;
}

void pollInputs(uint32_t now) {
  if (gSceneEpoch != SceneService::epoch()) {
    gSceneEpoch = SceneService::epoch(); gGestures.configure(SceneService::combinations(),SceneService::gestureBindings());
    gAwake = true; gIdle.activity(now); updateBrightness();
    if(gSceneShown!=SceneService::activeScene()){gSceneShown=SceneService::activeScene();gSceneOverlayUntil=now+1600;gLastFrame=now-200;}
  }
  int32_t delta = 0;
  if (gEncoder) {
    int16_t value = 0;
    if (pcnt_get_counter_value(PCNT_UNIT_0, &value) == ESP_OK) {
      const int32_t edges = value - gLastEncoder;
      gEncoderEdges += edges; gLastEncoder = value;
      if (abs(value) > 20000) {
        pcnt_counter_pause(PCNT_UNIT_0); pcnt_counter_clear(PCNT_UNIT_0);
        gLastEncoder = 0; pcnt_counter_resume(PCNT_UNIT_0);
      }
      delta = gDetents.consume(edges); gKnobPosition += delta;
    }
  }
  bool raw[SceneSettings::kInputCount];
  for (int i=0;i<SceneSettings::kInputCount;++i) raw[i]=digitalRead(gKeys[i].pin)==LOW;
  struct Event { int input, gesture, repeat; } events[32];
  int count=0;
  gGestures.tick(raw,delta,now,[&](int input,int gesture,int repeat) {
    if (count<32) events[count++]={input,gesture,repeat};
  });
  bool activity=delta!=0,held=false;
  for (int i=0;i<SceneSettings::kInputCount;++i) {
    const bool down=gGestures.down(i);
    if (down&&!gKeys[i].pressed) {++gKeys[i].presses;activity=true;}
    held|=down;
    gKeys[i].pressed=down;
  }
  if(held||activity)gIdle.activity(now);
  const bool wakeByInput=activity&&!gAwake;
  for (int i=0;i<count;++i) {
    SceneService::event(events[i].input,events[i].gesture,events[i].repeat);
    if (gSceneEpoch!=SceneService::epoch()) {
      gSceneEpoch=SceneService::epoch(); gGestures.configure(SceneService::combinations(),SceneService::gestureBindings());
      gAwake=true;gIdle.activity(now);updateBrightness();
      if(gSceneShown!=SceneService::activeScene()){gSceneShown=SceneService::activeScene();gSceneOverlayUntil=now+1600;gLastFrame=now-200;}
      break;
    }
  }
  if(wakeByInput){gAwake=true;gLastFrame=now-200;updateBrightness();DeviceLog::println("[sleep] wake by input");}
}

void drawClock() {
  const auto clock = ClockService::snapshot();
  gCanvas.loadFont(kInterClock64);
  gCanvas.setTextColor(TFT_WHITE, kBackground);
  gCanvas.setTextDatum(MC_DATUM);
  // Fixed positions keep the digits still when the sync colon appears.
  gCanvas.drawString(String(clock.hhmm).substring(0, 2), 72, 120);
  gCanvas.drawString(String(clock.hhmm).substring(3, 5), 168, 120);
  if (clock.valid && clock.synced) gCanvas.drawString(":", 120, 120);
  gCanvas.unloadFont();
}

const BitmapFont::Font &playbackFont(){static const BitmapFont::Font font(PlaybackAssets::kJapanese16,sizeof(PlaybackAssets::kJapanese16));return font;}
void playbackText(const char *text,int baseline,uint16_t color,int maxWidth=208){
  char fitted[64];const auto &font=playbackFont();font.fit(text,maxWidth,fitted,sizeof(fitted));
  uint16_t palette[16];for(int a=0;a<16;++a)palette[a]=gCanvas.alphaBlend(a*17,color,kBackground);
  font.draw(fitted,120-font.width(fitted)/2,baseline,[&](int x,int y,uint8_t alpha){gCanvas.drawPixel(x,y,palette[alpha]);});
}
void bluetoothConnecting(){
  uint16_t palette[16];for(int a=0;a<16;++a)palette[a]=gCanvas.alphaBlend(a*17,kMuted,kBackground);
  for(int i=0;i<32*32;++i){const uint8_t packed=PlaybackAssets::kBluetoothConnecting32[i/2],alpha=i&1?packed&15:packed>>4;if(alpha)gCanvas.drawPixel(104+i%32,104+i/32,palette[alpha]);}
}

void render(uint32_t now) {
  setBacklight();
  if (!gDisplay || !screenVisible(now)) return;
  gCanvas.fillSprite(kBackground);
  if(sceneOverlayVisible(now)) {
    gCanvas.drawXBitmap(88,60,SceneIcons::kBitmaps[SceneService::icon()],64,64,TFT_WHITE);
    const String name=SceneService::name();
    const int font=gCanvas.textWidth(name,4)>200?2:4;
    gCanvas.setTextColor(TFT_WHITE,kBackground);gCanvas.setTextDatum(MC_DATUM);
    gCanvas.drawString(name,120,157,font);
  } else switch (SceneService::screen()) {
    case SceneSettings::kClock: drawClock(); break;
    case SceneSettings::kBluetoothVolumeScreen: {
      const auto s=BluetoothService::snapshot();
      gCanvas.setTextDatum(MC_DATUM);gCanvas.setTextColor(TFT_WHITE,kBackground);
      if(!s.fresh||!s.available){bluetoothConnecting();break;}
      gCanvas.drawString("Bluetooth volume",120,54,2);
      const int value=s.pending&&s.preview>=0?s.preview:s.volume;
      gCanvas.setTextSize(2);gCanvas.drawString(s.available&&value>=0?String(value)+"%":"--",120,115,4);gCanvas.setTextSize(1);
      gCanvas.drawString(s.volume<0?"Mixer unavailable":s.pending?"Pending":s.result==CP_UNSUPPORTED?"Unsupported":s.result==CP_BUSY?"Busy":s.result==CP_FAILED?"Failed":"Confirmed",120,168,2);
      break;
    }
    case SceneSettings::kBluetoothPlaybackScreen: {
      const auto s=BluetoothService::snapshot();
      if(!s.fresh||!s.available||s.playback>2){bluetoothConnecting();break;}
      // Icon-only playback status at the top; metadata surrounds screen center.
      const int left=110,cy=54;
      if(s.seek){const int direction=s.seek==1?1:-1;for(int n=0;n<2;++n){const int x=left+(direction>0?n*8:18-n*8);gCanvas.fillTriangle(x,cy-7,x,cy+7,x+direction*7,cy,TFT_WHITE);}}
      else if(s.playback==1)gCanvas.fillTriangle(left+2,cy-8,left+2,cy+8,left+16,cy,TFT_WHITE);
      else if(s.playback==2){gCanvas.fillRect(left+2,cy-7,5,14,TFT_WHITE);gCanvas.fillRect(left+11,cy-7,5,14,TFT_WHITE);}
      else gCanvas.fillRect(left+3,cy-6,12,12,TFT_WHITE);
      playbackText(s.title,115,TFT_WHITE);playbackText(s.artist,139,kMuted);
      const uint32_t position=s.positionValid?s.positionMs:0;
      gCanvas.drawRoundRect(46,171,148,4,2,kMuted);if(s.durationMs)gCanvas.fillRoundRect(46,171,int(uint64_t(position)*148/s.durationMs),4,2,TFT_WHITE);
      char time[32];snprintf(time,sizeof(time),"%lu:%02lu / %lu:%02lu",(unsigned long)(position/60000),(unsigned long)(position/1000%60),(unsigned long)(s.durationMs/60000),(unsigned long)(s.durationMs/1000%60));playbackText(time,196,kMuted,160);
      break;
    }
    default: break;
  }
  gCanvas.pushSprite(0, 0);
  ++gFrames;
}

void animateLeds(uint32_t now) {
  if (gLedTest && static_cast<int32_t>(now - gLedTestUntil) >= 0) gLedTest = 0;
  const auto feedback = SceneService::feedback();
  const auto remote=BluetoothService::snapshot();
  const int remotePercent=remote.pending&&remote.preview>=0?remote.preview:remote.volume;
  const int progress=PlaybackRing::progress(remote.fresh&&remote.available&&remote.positionValid,remote.playback,remote.positionMs,remote.durationMs);
  const auto ring = SceneSettings::resolveLighting(SceneService::lighting(true),gLighting.mode==LightingSettings::kStatic,gLighting.color);
  const auto keys = SceneSettings::resolveLighting(SceneService::lighting(false),gLighting.mode==LightingSettings::kStatic,gLighting.color);
  constexpr uint32_t kTestColors[] = {0, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF};
  for (int i = 0; i < Board::kPixelCount; ++i) {
    const uint16_t hue = now * 12 + i * 65536 / Board::kPixelCount;
    const auto &light=i<Board::kRingPixels?ring:keys;
    uint32_t color = gLedTest ? kTestColors[gLedTest] : Adafruit_NeoPixel::gamma32(
        light.mode == SceneSettings::kSceneStatic ? light.color
        : Adafruit_NeoPixel::ColorHSV(hue, 220, 255));
    if(!gLedTest&&i<Board::kRingPixels&&SceneService::ringDisplay()==SceneSettings::kRingBluetoothProgress){
      const int coverage=progress<0?0:PlaybackRing::coverage(progress,i,Board::kRingPixels);
      color=DemoLevels::scaleRgb(color,(coverage*255+500)/1000);
    }
    if (!gLedTest && feedback.active) {
      using namespace SceneSettings;
      if (feedback.led==kOff) color=0;
      else if (feedback.led==kFeedbackStatic) color=Adafruit_NeoPixel::gamma32(feedback.color);
      else if (feedback.led==kFeedbackRainbow) color=Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue,220,255));
      else if (feedback.led==kProportional && i<Board::kRingPixels&&(!feedback.remote||(remote.fresh&&remotePercent>=0))) {
        const int percent=feedback.remote?remotePercent:feedback.percent;
        const int coverage=constrain(percent*Board::kRingPixels-i*100,0,100);
        color=DemoLevels::scaleRgb(Adafruit_NeoPixel::gamma32(feedback.color),LightingSettings::pwm(coverage));
      }
    }
    if (i >= Board::kKeyPixelStart + Board::kKeyPixels) color = 0;
    const int level = !gAwake ? 0 : i < Board::kKeyPixelStart ? gRingLevel : gKeysLevel;
    LedStrip::setPixel(i, DemoLevels::scaleRgb(color, LightingSettings::pwm(level)));
  }
  LedStrip::show();
  ++gLedFrames;
}

bool argument(WebServer &server, const char *name, int low, int high,
              int &value, String &error) {
  if (!server.hasArg(name)) return true;
  const String raw = server.arg(name);
  if (!raw.length()) { error = String(name) + " requires an integer"; return false; }
  for (size_t i = 0; i < raw.length(); ++i) {
    if (raw[i] < '0' || raw[i] > '9') {
      error = String(name) + " requires an integer"; return false;
    }
  }
  if (raw.length() > 6 || raw.toInt() < low || raw.toInt() > high) {
    error = String(name) + " out of range"; return false;
  }
  value = raw.toInt();
  return true;
}
}  // namespace

int applyAction(const SceneSettings::Action &a, int repetitions) {
  using namespace SceneSettings;
  int result=-1; bool lightingChanged=false;
  auto level = [&](uint32_t &field, bool change,int channel) {
    const int local=SceneService::adjustBrightness(channel,change?int(a.value)*repetitions:a.value,change);
    if(local>=0){result=local;return;}
    field=constrain(change?int(field)+int(a.value)*repetitions:int(a.value),0,100);
    result=field; lightingChanged=true;
  };
  switch (a.type) {
    case kScreenChange: case kScreenSet: level(gLighting.screen,a.type==kScreenChange,0); break;
    case kRingChange: case kRingSet: level(gLighting.ring,a.type==kRingChange,1); break;
    case kKeysChange: case kKeysSet: level(gLighting.keys,a.type==kKeysChange,2); break;
    case kVolumeChange: case kVolumeSet:
      AudioDemo::setVolume(a.type==kVolumeChange?AudioDemo::snapshot().volume+a.value*repetitions:a.value);
      result=AudioDemo::snapshot().volume; break;
    case kRainbow: gLighting.mode=LightingSettings::kRainbow; lightingChanged=true; break;
    case kStatic: gLighting.mode=LightingSettings::kStatic; gLighting.color=a.color; lightingChanged=true; break;
    case kToggleAwake: gAwake=!gAwake;gIdle.activity(millis());break;
    case kSync: ClockService::requestSync(); break;
    case kTone: AudioDemo::beep(a.value,200); break;
    case kBluetoothVolume: {
      const auto s=BluetoothService::snapshot();const int denominator=abs(a.value);const int usage=a.value>0?0xe9:0xea;
      if(s.extension)BluetoothService::send(CP_VOLUME,usage,repetitions,denominator);
      else for(int n=0;n<repetitions;++n)if(!BluetoothService::send(CP_VOLUME,usage,1,denominator))break;
      const auto after=BluetoothService::snapshot();result=after.preview>=0?after.preview:after.volume;break;
    }
    case kBluetoothMedia:
      if(a.target==2)BluetoothService::send(CP_RELEASE,a.value,1);
      else {const auto s=BluetoothService::snapshot();if(s.extension&&(a.value==0xb5||a.value==0xb6))BluetoothService::send(CP_MEDIA,a.value,repetitions,a.target,a.target?0:200);else for(int n=0;n<repetitions;++n)if(!BluetoothService::send(CP_MEDIA,a.value,1,a.target,a.target?0:200))break;}
      break;
    case kBluetoothKey: {const auto s=BluetoothService::snapshot();if(s.extension)BluetoothService::send(CP_KEY,a.value,repetitions,a.target);else for(int n=0;n<repetitions;++n)if(!BluetoothService::send(CP_KEY,a.value,1,a.target))break;break;}
    default: break;
  }
  if (lightingChanged) { gLightingDirty=true; gLightingChangedAt=millis(); }
  if(a.type>=kBluetoothVolume)gLastFrame=millis()-200;
  updateBrightness();
  return result;
}

void captureBootStage() {
  if (esp_reset_reason() != ESP_RST_POWERON && gRetainedMagic == kStageMagic &&
      gRetainedStage < kStageCount) {
    gLastBootStage = static_cast<Stage>(gRetainedStage);
  }
}

void begin() {
  DeviceLog::println("[hardware] initialise ColoPLAY V1 peripherals");
  loadLighting();
  for (Key &key : gKeys) {
    pinMode(key.pin, key.pullup ? INPUT_PULLUP : INPUT);
    key.raw = key.pressed = digitalRead(key.pin) == LOW;
  }
  bool initial[SceneSettings::kInputCount];
  for (int i=0;i<SceneSettings::kInputCount;++i) initial[i]=gKeys[i].pressed;
  gSceneShown=SceneService::activeScene();gSceneOverlayUntil=0;
  gGestures.seed(initial,millis()); gGestures.configure(SceneService::combinations(),SceneService::gestureBindings()); gSceneEpoch=SceneService::epoch();
  pinMode(Board::kCharge, INPUT_PULLUP);
  setStage(kLedPower);
  pinMode(Board::kLedPowerEnable, OUTPUT);
  digitalWrite(Board::kLedPowerEnable, HIGH);
  delay(2); // retain the working output-settle delay before LED traffic
  setStage(kPsram);
  checkPsram();
  DeviceLog::printf("[psram] bytes=%u checked=%u passed=%d\n",
                ESP.getPsramSize(), gPsramTestBytes, gPsramTest);
  setStage(kEncoder);
  setupEncoder();
  setStage(kSensors);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_12);
  adc2_config_channel_atten(ADC2_CHANNEL_5, ADC_ATTEN_DB_12);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &gSupplyCal);
  esp_adc_cal_characterize(ADC_UNIT_2, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &gLightCal);
  readSensors();

  setStage(kLeds);
  LedStrip::begin();
  // LEDC channel 0 / timer 0 matches the factory backlight configuration.
  setStage(kBacklight);
  gBacklight = ledcSetup(0, 5000, 10) > 0;
  ledcAttachPin(Board::kBacklight, 0);
  ledcWrite(0, 0);
  setStage(kLcdInit);
  gTft.init();
  gTft.setRotation(0);
  gTft.fillScreen(kBackground);
  setStage(kFramebuffer);
  gCanvas.setColorDepth(16);
  gCanvas.setAttribute(PSRAM_ENABLE, true);
  gDisplay = gCanvas.createSprite(240, 240) != nullptr;
  // GC9A01 TE output provides physical panel scan feedback on GPIO17.
  // This does not establish that the pixels or backlight look correct.
  setStage(kLcdTe);
  pinMode(Board::kLcdTe, INPUT);
  attachInterrupt(digitalPinToInterrupt(Board::kLcdTe), onTe, RISING);
  gTft.startWrite();
  gTft.writecommand(0x35);  // TEON: vertical blanking only
  gTft.writedata(0);
  gTft.endWrite();
  setBacklight();
  setStage(kAudio);
  AudioDemo::begin();
  gReady = true;
  gSleepMinutes=GeneralService::sleepTimeoutMinutes();gIdle.configure(gSleepMinutes,millis());
  gLastPage = gLastMetrics = millis();
  setStage(kFirstFrame);
  render(millis());
  setStage(kRunning);
  DeviceLog::println("[hardware] desktop gadget ready; scene bindings control inputs");
}

bool ready() { return gReady; }

void tick() {
  if (!gReady || gPaused) return;
  const uint32_t now = millis();
  if(gSleepMinutes!=GeneralService::sleepTimeoutMinutes()){
    gSleepMinutes=GeneralService::sleepTimeoutMinutes();gIdle.configure(gSleepMinutes,now);
  }
  pollInputs(now);
  if(gAwake&&gIdle.due(now)){
    gAwake=false;gSceneOverlayUntil=0;setBacklight();animateLeds(now);
    DeviceLog::println("[sleep] inactivity timeout; screen and LEDs off");
  }
  // Debounce knob writes so continuous turning does not wear NVS.
  if (gLightingDirty && now - gLightingChangedAt >= 1500) {
    gLightingStored = saveLighting(gLighting);
    gLightingDirty = !gLightingStored;
    gLightingChangedAt = now;
  }

  if (now - gLastSensors >= 500) { gLastSensors = now; readSensors(); }
  if (now - gLastLeds >= 33) { gLastLeds = now; animateLeds(now); }
  if (now - gLastFrame >= (SceneService::screen()>=SceneSettings::kBluetoothVolumeScreen ? 50u : 200u)) {
    gLastFrame = now; render(now);
  }
  if (now - gLastMetrics >= 1000) {
    const uint32_t elapsed = now - gLastMetrics, pulses = gTePulses;
    gFps = (gFrames - gPreviousFrames) * 1000.0f / elapsed;
    gTeHz = (pulses - gPreviousTe) * 1000.0f / elapsed;
    gPreviousFrames = gFrames;
    gPreviousTe = pulses;
    gLastMetrics = now;
  }
}

void pause(bool paused) {
  gPaused = paused;
  if (gReady) AudioDemo::pause(paused);
}

String buttonsJson() {
  String json = "[";
  for (size_t i = 0; i < sizeof(gKeys) / sizeof(gKeys[0]); ++i) {
    if (i) json += ',';
    const Key &key = gKeys[i];
    json += "{\"name\":\"" + String(key.name) + "\",\"pin\":" + String(key.pin);
    json += ",\"pressed\":" + String(key.pressed ? "true" : "false");
    json += ",\"presses\":" + String(key.presses) + "}";
  }
  return json + "]";
}

String statusJson() {
  String json;
  json.reserve(1500);
  json += "{\"ready\":" + String(gReady ? "true" : "false");
  json += ",\"stage\":\"" + String(kStageNames[gStage]) + "\"";
  json += ",\"last_boot_stage\":\"" + String(kStageNames[gLastBootStage]) + "\"";
  json += ",\"page\":" + String(gPage) + ",\"page_name\":\"" + kPages[gPage] + "\"";
  json += ",\"auto\":" + String(gAuto ? "true" : "false");
  json += ",\"awake\":" + String(gAwake ? "true" : "false");
  json += ",\"idle_ms\":" + String(gIdle.elapsed(millis()));
  json += ",\"paused\":" + String(gPaused ? "true" : "false");
  json += ",\"display\":{\"driver\":\"GC9A01\",\"width\":240,\"height\":240";
  json += ",\"scene_overlay\":"+String(gSceneOverlayUntil&&static_cast<int32_t>(gSceneOverlayUntil-millis())>0?"true":"false");
  json += ",\"playback_font\":\"Noto Sans JP 16\",\"playback_font_glyphs\":"+String(PlaybackAssets::kGlyphCount)+",\"playback_font_bytes\":"+String(PlaybackAssets::kFontBytes);
  json += ",\"framebuffer_ready\":" + String(gDisplay ? "true" : "false");
  json += ",\"frames\":" + String(gFrames) + ",\"fps\":" + String(gFps, 1);
  json += ",\"te_pulses\":" + String(gTePulses) + ",\"te_hz\":" + String(gTeHz, 1);
  json += ",\"brightness\":" + String(gLighting.screen);
  json += ",\"auto_brightness\":" + String(gLighting.autoScreen ? "true" : "false");
  json += ",\"effective_brightness\":" + String(screenVisible(millis()) ? gScreenLevel : 0) + ",\"spi_hz\":40000000}";
  json += ",\"encoder\":{\"ready\":" + String(gEncoder ? "true" : "false");
  json += ",\"edges\":" + String(gEncoderEdges) + ",\"position\":" + String(gKnobPosition);
  json += ",\"edges_per_detent\":" + String(RotaryDetents::kEdgesPerDetent);
  json += ",\"a\":" + String(digitalRead(Board::kEncoderA));
  json += ",\"b\":" + String(digitalRead(Board::kEncoderB)) + "}";
  json += ",\"leds\":{\"pin\":48,\"frame_pixels\":14,\"ring_pixels\":9,\"key_pixels\":4";
  json += ",\"key_start\":9,\"power_pin\":8,\"protocol\":\"factory-SPI3-GRB\",\"spi_hz\":3200000";
  json += ",\"ready\":" + String(LedStrip::ready() ? "true" : "false");
  json += ",\"transfers\":" + String(LedStrip::transfers());
  json += ",\"error\":" + String(LedStrip::error()) + ",\"errors\":" + String(LedStrip::errors());
  // Retain the legacy byte-scale ring brightness for older API clients.
  json += ",\"brightness\":" + String(LightingSettings::pwm(gLighting.ring));
  json += ",\"ring_brightness\":" + String(gLighting.ring);
  json += ",\"keys_brightness\":" + String(gLighting.keys);
  json += ",\"auto_ring\":" + String(gLighting.autoRing ? "true" : "false");
  json += ",\"auto_keys\":" + String(gLighting.autoKeys ? "true" : "false");
  const auto remote=BluetoothService::snapshot();
  json += ",\"progress_permille\":"+String(PlaybackRing::progress(remote.fresh&&remote.available&&remote.positionValid,remote.playback,remote.positionMs,remote.durationMs));
  json += ",\"ring_effective\":" + String(gAwake ? gRingLevel : 0);
  json += ",\"keys_effective\":" + String(gAwake ? gKeysLevel : 0);
  json += ",\"mode\":\"" + String(gLighting.mode == LightingSettings::kStatic ? "static" : "rainbow") + "\"";
  char color[8];
  snprintf(color, sizeof(color), "#%06lx", static_cast<unsigned long>(gLighting.color));
  json += ",\"color\":\"" + String(color) + "\",\"test_mode\":" + String(gLedTest);
  json += ",\"frames\":" + String(gLedFrames) + "}";
  json += ",\"lighting\":{\"stored\":" + String(gLightingStored && !gLightingDirty ? "true" : "false");
  json += ",\"auto_dark_raw\":" + String(gLighting.darkRaw);
  json += ",\"auto_bright_raw\":" + String(gLighting.brightRaw);
  json += ",\"sensor_usable\":" + String(gLightUsable ? "true" : "false") + "}";
  json += ",\"sensors\":{\"vdd_raw\":" + String(gSupplyRaw);
  json += ",\"vdd_mv_approx\":" + String(gSupplyMv);
  json += ",\"light_raw\":" + String(gLightRaw) + ",\"light_mv\":" + String(gLightMv);
  json += ",\"light_error\":" + String(gLightError);
  json += ",\"light_readings\":" + String(gLightReadings) + ",\"light_errors\":" + String(gLightErrors);
  json += ",\"charge_pin_low\":" + String(digitalRead(Board::kCharge) == LOW ? "true" : "false") + "}";
  json += ",\"psram\":{\"test_passed\":" + String(gPsramTest ? "true" : "false");
  json += ",\"tested_bytes\":" + String(gPsramTestBytes);
  json += ",\"free_bytes\":" + String(ESP.getFreePsram()) + "}";
  json += ",\"audio\":" + AudioDemo::statusJson() + "}";
  return json;
}

bool configure(WebServer &server, String &error) {
  if (!gReady) { error = "hardware not started (or recovery mode)"; return false; }
  int page = gPage, automatic = gAuto;
  int volume = AudioDemo::snapshot().volume, ledTest = 0;
  int tone = 0, duration = 250, awake = gAwake;
  if (!argument(server, "page", 0, kPageCount - 1, page, error) ||
      !argument(server, "auto", 0, 1, automatic, error) ||
      !argument(server, "volume", 0, 100, volume, error) ||
      !argument(server, "led_test", 0, 4, ledTest, error) ||
      !argument(server, "tone", 100, 4000, tone, error) ||
      !argument(server, "duration", 30, 1500, duration, error) ||
      !argument(server, "awake", 0, 1, awake, error)) return false;
  if (page != kClockPage || automatic) { error = "scene mode currently supports the Clock screen only"; return false; }
  LightingSettings::Stored candidate = gLighting;
  if (server.hasArg("leds")) {
    int legacy = 0;
    if (!argument(server, "leds", 0, 64, legacy, error)) return false;
    candidate.ring = candidate.keys = (legacy * 100 + 127) / 255;
  }
  auto setting = [&](const char *name, uint32_t &value, int maximum) {
    int number = value;
    if (!argument(server, name, 0, maximum, number, error)) return false;
    value = number;
    return true;
  };
  if (!setting("brightness", candidate.screen, 100) ||
      !setting("ring_brightness", candidate.ring, 100) ||
      !setting("keys_brightness", candidate.keys, 100) ||
      !setting("auto_screen", candidate.autoScreen, 1) ||
      !setting("auto_ring", candidate.autoRing, 1) ||
      !setting("auto_keys", candidate.autoKeys, 1) ||
      !setting("auto_dark_raw", candidate.darkRaw, 4095) ||
      !setting("auto_bright_raw", candidate.brightRaw, 4095)) return false;
  if (server.hasArg("led_mode")) {
    const String mode = server.arg("led_mode");
    if (mode != "rainbow" && mode != "static") {
      error = "led_mode must be rainbow or static"; return false;
    }
    candidate.mode = mode == "static" ? LightingSettings::kStatic : LightingSettings::kRainbow;
  }
  if (server.hasArg("led_color")) {
    const String color = server.arg("led_color");
    if (color.length() != 7 || color[0] != '#') {
      error = "led_color must be #RRGGBB"; return false;
    }
    uint32_t rgb = 0;
    for (int i = 1; i < 7; ++i) {
      const char c = color[i];
      const int digit = c >= '0' && c <= '9' ? c - '0' :
        c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
      if (digit < 0) { error = "led_color must be #RRGGBB"; return false; }
      rgb = (rgb << 4) | digit;
    }
    candidate.color = rgb;
  }
  if (!LightingSettings::valid(candidate)) {
    error = "dark and bright calibration readings must differ"; return false;
  }
  if (memcmp(&candidate, &gLighting, sizeof(candidate)) != 0) {
    if (!saveLighting(candidate)) { error = "could not save lighting settings"; return false; }
    gLighting = candidate;
    gLightingStored = true;
    gLightingDirty = false;
  }
  if (server.hasArg("page") && !server.hasArg("auto")) automatic = false;
  selectPage(page, automatic);
  if (server.hasArg("led_test")) {
    gLedTest = ledTest;
    gLedTestUntil = millis() + 5000;
  }
  gAwake = awake;
  if(server.hasArg("awake"))gIdle.activity(millis());
  updateBrightness();
  if (server.hasArg("volume")) AudioDemo::setVolume(volume);
  if (tone) AudioDemo::beep(tone, duration);
  return true;
}

}  // namespace HardwareDemo
