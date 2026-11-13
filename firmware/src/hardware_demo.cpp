#include "hardware_demo.h"
#include "audio_demo.h"
#include "board_pins.h"
#include "demo_levels.h"
#include "led_strip.h"
#include "rotary_detents.h"

#include <Adafruit_NeoPixel.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
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
constexpr const char *kPages[] = {"ORBIT", "INPUTS", "AUDIO", "SENSORS"};
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
bool gAwake = true, gAuto = true, gPsramTest = false, gBacklight = false;
int gPage = 0, gBrightness = 65, gLedBrightness = 40;
int gLedTest = 0;
uint32_t gLedTestUntil = 0;
int32_t gEncoderEdges = 0, gKnobPosition = 0;
int16_t gLastEncoder = 0;
RotaryDetents gDetents;
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
  Serial.printf("[hardware] stage=%s\n", kStageNames[stage]);
}

void IRAM_ATTR onTe() { ++gTePulses; }

void setBacklight() {
  if (gBacklight) ledcWrite(0, gAwake ? gBrightness * 1023 / 100 : 0);
}

void selectPage(int page, bool automatic) {
  gPage = constrain(page, 0, 3);
  gAuto = automatic;
  gLastPage = millis();
}

void readSensors() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; ++i) sum += adc1_get_raw(ADC1_CHANNEL_6);
  gSupplyRaw = sum / 16;
  // R37=2M / R38=1M: this measures the VDD rail, not an isolated battery.
  gSupplyMv = esp_adc_cal_raw_to_voltage(gSupplyRaw, &gSupplyCal) * 3;
  int light = 0;
  gLightError = adc2_get_raw(ADC2_CHANNEL_5, ADC_WIDTH_BIT_12, &light);
  if (gLightError == ESP_OK) {
    gLightRaw = light;
    gLightMv = esp_adc_cal_raw_to_voltage(light, &gLightCal);
    ++gLightReadings;
  } else {
    ++gLightErrors;
    // Keep the last good measurement and publish the error explicitly.
    // WiFi can contend with ADC2; a failed read is not a dark-room value.
  }
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
  for (size_t i = 0; i < sizeof(gKeys) / sizeof(gKeys[0]); ++i) {
    Key &key = gKeys[i];
    const bool pressed = digitalRead(key.pin) == LOW;
    if (pressed != key.raw) { key.raw = pressed; key.changed = now; }
    if (now - key.changed < 25 || key.pressed == key.raw) continue;
    key.pressed = key.raw;
    if (!key.pressed) continue;
    ++key.presses;
    if (i < 4) {
      selectPage(i, false);
      if (i == 2) AudioDemo::beep();
    } else if (i == 4) {
      gAuto = !gAuto;
      gLastPage = now;
    } else if (i == 5) {
      gAwake = !gAwake;
      setBacklight();
    } else {
      AudioDemo::beep(880, 180);
    }
  }
  if (!gEncoder) return;
  int16_t value = 0;
  if (pcnt_get_counter_value(PCNT_UNIT_0, &value) != ESP_OK) return;
  const int32_t edges = value - gLastEncoder;
  gEncoderEdges += edges;
  gLastEncoder = value;
  if (abs(value) > 20000) {
    pcnt_counter_pause(PCNT_UNIT_0);
    pcnt_counter_clear(PCNT_UNIT_0);
    gLastEncoder = 0;
    pcnt_counter_resume(PCNT_UNIT_0);
  }
  const int32_t delta = gDetents.consume(edges);
  if (!delta) return;
  gKnobPosition += delta;
  if (gPage == 2) {
    AudioDemo::setVolume(AudioDemo::snapshot().volume + delta * 2);
  } else {
    gBrightness = constrain(gBrightness + delta * 3, 0, 100);
    setBacklight();
  }
}

void text(const String &value, int x, int y, uint16_t color, int font = 2) {
  gCanvas.setTextColor(color, kBackground);
  gCanvas.drawString(value, x, y, font);
}

void drawOrbit(uint32_t now) {
  const float t = now * 0.0006f;
  gCanvas.drawCircle(120, 119, 71, 0x1969);
  gCanvas.drawCircle(120, 119, 88, 0x1969);
  for (int i = 0; i < 9; ++i) {
    const float phase = t + i * kTau / 9;
    const float radius = 74 + 8 * sinf(t * 1.8f + i);
    const uint16_t color = i % 2 ? kViolet : kMint;
    gCanvas.fillCircle(120 + cosf(phase) * radius,
                       119 + sinf(phase) * radius, 3 + i % 3, color);
  }
  text("COLO", 120, 85, TFT_WHITE, 4);
  text("PLAY", 120, 117, kMint, 4);
  text("HARDWARE DEMO", 120, 151, kMuted);
  text(gAuto ? "Live / auto tour" : "Live / manual", 120, 172, kMuted);
}

void drawInputs() {
  for (int i = 0; i < 4; ++i) {
    const int x = 52 + (i % 2) * 136, y = 80 + (i / 2) * 64;
    const uint16_t color = gKeys[i].pressed ? kMint : 0x3189;
    gCanvas.fillCircle(x, y, 18, color);
    text(String(i + 1), x, y - 8, gKeys[i].pressed ? kBackground : TFT_WHITE);
    text(String(gKeys[i].presses), x, y + 20, kMuted);
  }
  text(String(gKnobPosition), 120, 91, kMint, 4);
  text("KNOB", 120, 120, kMuted);
  gCanvas.drawCircle(120, 117, 44, gKeys[4].pressed ? kAmber : 0x3189);
  text("Light " + String(gBrightness) + "%", 120, 177, TFT_WHITE);
  text("Press knob: auto tour", 120, 199, kMuted);
}

void drawAudio() {
  const AudioDemo::Snapshot s = AudioDemo::snapshot();
  text(s.codec ? "MICROPHONE" : "CODEC NOT READY", 120, 64,
       s.codec ? kMint : kAmber);
  for (int i = 1; i < 64; ++i) {
    // Auto-scale for visibility, while the numerical meter stays absolute.
    const float scale = fmaxf(400, s.peak);
    gCanvas.drawLine(28 + (i - 1) * 184 / 63,
                     115 - s.waveform[i - 1] * 24 / scale,
                     28 + i * 184 / 63, 115 - s.waveform[i] * 24 / scale, kMint);
  }
  const int level = constrain(int((s.dbfs + 72) / 72 * 160), 0, 160);
  gCanvas.fillRoundRect(40, 151, 160, 9, 4, 0x3189);
  if (level) gCanvas.fillRoundRect(40, 151, level, 9, 4, kMint);
  text(String(s.dbfs, 1) + " dBFS", 120, 168, TFT_WHITE);
  text("Volume " + String(s.volume) + "%", 120, 190, kMuted);
  text("Key 3 / REC: test tone", 120, 210, kMuted);
}

void drawSensors() {
  text("SUPPLY RAIL", 120, 66, kMuted);
  text(String(gSupplyMv / 1000.0f, 2) + " V", 120, 88, kMint, 4);
  text("Light " + String(gLightRaw) +
       (gLightError == ESP_OK ? " ADC" : " (retry)"), 120, 124, TFT_WHITE);
  text(digitalRead(Board::kCharge) == LOW ? "Charge pin LOW" : "Charge pin HIGH",
       120, 146, kMuted);
  text("WiFi " + String(WiFi.RSSI()) + " dBm", 120, 168, TFT_WHITE);
  text("RAM " + String(ESP.getPsramSize() / 1048576.0f, 1) + " MiB / " +
       (gPsramTest ? "PASS" : "unverified"), 120, 190, gPsramTest ? kMint : kAmber);
}

void render(uint32_t now) {
  if (!gDisplay || !gAwake) return;
  gCanvas.fillSprite(kBackground);
  gCanvas.drawCircle(120, 120, 117, 0x2969);
  // A slow moving marker makes a stalled render loop immediately visible.
  const float phase = now * 0.001f;
  gCanvas.fillCircle(120 + cosf(phase) * 116, 120 + sinf(phase) * 116, 3, kMint);
  gCanvas.setTextDatum(TC_DATUM);
  text(kPages[gPage], 120, 24, kMuted);
  switch (gPage) {
    case 0: drawOrbit(now); break;
    case 1: drawInputs(); break;
    case 2: drawAudio(); break;
    case 3: drawSensors(); break;
  }
  for (int i = 0; i < 4; ++i) {
    gCanvas.fillCircle(99 + i * 14, 229, 3, i == gPage ? kMint : 0x3189);
  }
  gCanvas.pushSprite(0, 0);
  ++gFrames;
}

void animateLeds(uint32_t now) {
  const int brightness = gAwake ? gLedBrightness : 0;
  if (gLedTest && static_cast<int32_t>(now - gLedTestUntil) >= 0) gLedTest = 0;
  constexpr uint32_t kTestColors[] = {0, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF};
  for (int i = 0; i < Board::kPixelCount; ++i) {
    const uint16_t hue = now * 12 + i * 65536 / Board::kPixelCount;
    uint32_t color = gLedTest ? kTestColors[gLedTest]
                             : Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue, 220, 255));
    if (!gLedTest && i >= Board::kKeyPixelStart &&
        i < Board::kKeyPixelStart + Board::kKeyPixels &&
        gKeys[i - Board::kKeyPixelStart].pressed) color = 0xFFFFFF;
    if (!gLedTest && i >= Board::kKeyPixelStart + Board::kKeyPixels) color = 0;
    LedStrip::setPixel(i, DemoLevels::scaleRgb(color, brightness));
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

void captureBootStage() {
  if (esp_reset_reason() != ESP_RST_POWERON && gRetainedMagic == kStageMagic &&
      gRetainedStage < kStageCount) {
    gLastBootStage = static_cast<Stage>(gRetainedStage);
  }
}

void begin() {
  Serial.println("[hardware] initialise ColoPLAY V1 peripherals");
  for (Key &key : gKeys) {
    pinMode(key.pin, key.pullup ? INPUT_PULLUP : INPUT);
    key.raw = key.pressed = digitalRead(key.pin) == LOW;
  }
  pinMode(Board::kCharge, INPUT_PULLUP);
  setStage(kLedPower);
  pinMode(Board::kLedPowerEnable, OUTPUT);
  digitalWrite(Board::kLedPowerEnable, HIGH);
  delay(2); // let the Q7/Q6-switched VCCL rail settle before LED traffic
  setStage(kPsram);
  checkPsram();
  Serial.printf("[psram] bytes=%u checked=%u passed=%d\n",
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
  gLastPage = gLastMetrics = millis();
  setStage(kFirstFrame);
  render(millis());
  setStage(kRunning);
  AudioDemo::beep(660, 220);
  Serial.println("[hardware] demo ready; keys 1..4 select pages, knob adjusts light / volume");
}

bool ready() { return gReady; }

void tick() {
  if (!gReady || gPaused) return;
  const uint32_t now = millis();
  pollInputs(now);
  if (gAuto && now - gLastPage >= 10000) selectPage((gPage + 1) % 4, true);
  if (now - gLastSensors >= 500) { gLastSensors = now; readSensors(); }
  if (now - gLastLeds >= 33) { gLastLeds = now; animateLeds(now); }
  if (now - gLastFrame >= 50) { gLastFrame = now; render(now); }
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
  json += ",\"paused\":" + String(gPaused ? "true" : "false");
  json += ",\"display\":{\"driver\":\"GC9A01\",\"width\":240,\"height\":240";
  json += ",\"framebuffer_ready\":" + String(gDisplay ? "true" : "false");
  json += ",\"frames\":" + String(gFrames) + ",\"fps\":" + String(gFps, 1);
  json += ",\"te_pulses\":" + String(gTePulses) + ",\"te_hz\":" + String(gTeHz, 1);
  json += ",\"brightness\":" + String(gBrightness) + ",\"spi_hz\":40000000}";
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
  json += ",\"brightness\":" + String(gLedBrightness) + ",\"test_mode\":" + String(gLedTest);
  json += ",\"frames\":" + String(gLedFrames) + "}";
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
  int page = gPage, automatic = gAuto, light = gBrightness;
  int leds = gLedBrightness, volume = AudioDemo::snapshot().volume, ledTest = 0;
  int tone = 0, duration = 250, awake = gAwake;
  if (!argument(server, "page", 0, 3, page, error) ||
      !argument(server, "auto", 0, 1, automatic, error) ||
      !argument(server, "brightness", 0, 100, light, error) ||
      !argument(server, "leds", 0, 64, leds, error) ||
      !argument(server, "volume", 0, 100, volume, error) ||
      !argument(server, "led_test", 0, 4, ledTest, error) ||
      !argument(server, "tone", 100, 4000, tone, error) ||
      !argument(server, "duration", 30, 1500, duration, error) ||
      !argument(server, "awake", 0, 1, awake, error)) return false;
  if (server.hasArg("page") && !server.hasArg("auto")) automatic = false;
  selectPage(page, automatic);
  gBrightness = light;
  gLedBrightness = leds;
  if (server.hasArg("led_test")) {
    gLedTest = ledTest;
    gLedTestUntil = millis() + 5000;
  }
  gAwake = awake;
  setBacklight();
  if (server.hasArg("volume")) AudioDemo::setVolume(volume);
  if (tone) AudioDemo::beep(tone, duration);
  return true;
}

void sendScreenshot(WebServer &server) {
  if (!gDisplay) { server.send(503, "text/plain", "framebuffer unavailable\n"); return; }
  // Standard 24-bit BMP, bottom-up. Stream rows to avoid a second 173 KB
  // allocation. The image is the render buffer, not a panel readback.
  uint8_t header[54] = {'B', 'M'};
  auto put32 = [&](size_t offset, uint32_t value) {
    for (int i = 0; i < 4; ++i) header[offset + i] = value >> (8 * i);
  };
  constexpr uint32_t bytes = 54 + 240 * 240 * 3;
  put32(2, bytes); put32(10, 54); put32(14, 40);
  put32(18, 240); put32(22, 240); header[26] = 1; header[28] = 24;
  put32(34, 240 * 240 * 3);
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(bytes);
  server.send(200, "image/bmp", "");
  WiFiClient client = server.client();
  client.write(header, sizeof(header));
  uint8_t row[240 * 3];
  for (int y = 239; y >= 0 && client.connected(); --y) {
    for (int x = 0; x < 240; ++x) {
      const uint16_t color = gCanvas.readPixel(x, y);
      row[x * 3] = ((color & 31) * 255) / 31;
      row[x * 3 + 1] = (((color >> 5) & 63) * 255) / 63;
      row[x * 3 + 2] = (((color >> 11) & 31) * 255) / 31;
    }
    if (client.write(row, sizeof(row)) != sizeof(row)) break;
    delay(0);
  }
}
}  // namespace HardwareDemo
