#include "device_log.h"
#include "audio_demo.h"
#include "board_pins.h"
#include "demo_levels.h"

#include <cmath>
#include <driver/i2c.h>
#include <driver/i2s.h>
#include <es8311.h>

namespace AudioDemo {
namespace {
constexpr i2s_port_t kI2s = I2S_NUM_0;
constexpr i2c_port_t kI2c = I2C_NUM_0;
constexpr uint32_t kSampleRate = 16000;
constexpr size_t kFrames = 256;
constexpr float kTau = 6.28318530718f;
portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
Snapshot gState;
es8311_handle_t gCodec = nullptr;
bool gPaused = false;
uint32_t gRequest = 0, gDuration = 250;
unsigned gFrequency = 660;

esp_err_t readRegister(uint8_t address, uint8_t reg, uint8_t &value) {
  return i2c_master_write_read_device(kI2c, address, &reg, 1, &value, 1,
                                      pdMS_TO_TICKS(100));
}

esp_err_t configureVolume(int percent) {
  const uint8_t data[] = {0x32, DemoLevels::codecVolume(percent)};
  esp_err_t result = i2c_master_write_to_device(kI2c, gState.address, data,
                                               sizeof(data), pdMS_TO_TICKS(100));
  if (result == ESP_OK) result = es8311_voice_mute(gCodec, percent == 0);
  return result;
}

void run(void *) {
  int16_t tx[kFrames * 2] = {}, rx[kFrames * 2] = {};
  uint32_t seenRequest = 0, remaining = 0;
  float phase = 0, increment = 0;
  while (true) {
    portENTER_CRITICAL(&gMux);
    const bool paused = gPaused;
    const uint32_t request = gRequest;
    const unsigned frequency = gFrequency;
    const uint32_t duration = gDuration;
    portEXIT_CRITICAL(&gMux);
    bool started = false;
    if (request != seenRequest) {
      seenRequest = request;
      remaining = duration * kSampleRate / 1000;
      increment = kTau * frequency / kSampleRate;
      phase = 0;
      started = !paused;
    }
    if (paused) remaining = 0;
    for (size_t i = 0; i < kFrames; ++i) {
      // A modest -18 dBFS test signal with a 5 ms envelope to avoid clicks.
      float envelope = 0;
      if (remaining) {
        const uint32_t elapsed = duration * kSampleRate / 1000 - remaining;
        envelope = fminf(1, fminf(elapsed / 80.0f, remaining / 80.0f));
      }
      const int16_t sample = remaining ? sinf(phase) * 4096 * envelope : 0;
      tx[i * 2] = tx[i * 2 + 1] = sample;
      if (remaining) --remaining;
      phase += increment;
      if (phase >= kTau) phase -= kTau;
    }
    size_t written = 0, received = 0;
    const esp_err_t txResult = i2s_write(kI2s, tx, sizeof(tx), &written,
                                       pdMS_TO_TICKS(100));
    const esp_err_t rxResult = i2s_read(kI2s, rx, sizeof(rx), &received,
                                      pdMS_TO_TICKS(100));
    const size_t frames = received / (2 * sizeof(int16_t));
    // The mono codec can occupy either I2S slot. Measure both and select
    // the slot carrying the signal; subtract DC before computing RMS.
    double sum[2] = {}, square[2] = {};
    for (size_t i = 0; i < frames; ++i) {
      for (size_t channel = 0; channel < 2; ++channel) {
        const int32_t v = rx[i * 2 + channel];
        sum[channel] += v;
        square[channel] += double(v) * v;
      }
    }
    double variance[2] = {}, mean[2] = {};
    for (size_t channel = 0; channel < 2; ++channel) {
      if (frames) {
        mean[channel] = sum[channel] / frames;
        variance[channel] = fmax(0, square[channel] / frames -
                                    mean[channel] * mean[channel]);
      }
    }
    const size_t channel = variance[1] > variance[0] ? 1 : 0;
    float peak = 0;
    int16_t wave[64] = {};
    for (size_t i = 0; i < frames; ++i) {
      peak = fmaxf(peak, fabsf(rx[i * 2 + channel] - mean[channel]));
    }
    for (size_t i = 0; i < 64 && frames; ++i) {
      const int32_t v = rx[(i * frames / 64) * 2 + channel] - mean[channel];
      wave[i] = constrain(v, -32768, 32767);
    }
    const float rms = sqrt(variance[channel]);
    portENTER_CRITICAL(&gMux);
    gState.txFrames += written / (2 * sizeof(int16_t));
    gState.rxFrames += frames;
    if (txResult != ESP_OK || rxResult != ESP_OK ||
        written != sizeof(tx) || received != sizeof(rx)) ++gState.ioErrors;
    if (started && written) ++gState.tonesPlayed;
    gState.rms = rms;
    gState.peak = peak;
    gState.dbfs = rms > 0 ? fmaxf(-96, 20 * log10f(rms / 32768)) : -96;
    gState.tone = remaining > 0;
    memcpy(gState.waveform, wave, sizeof(wave));
    portEXIT_CRITICAL(&gMux);
    // Driver errors must not turn this task into a busy loop.
    if (!written || !received) vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void fail(const char *step, esp_err_t error) {
  gState.error = error;
  DeviceLog::printf("[audio] %s failed: %s\n", step, esp_err_to_name(error));
  digitalWrite(Board::kAmpEnable, LOW);
}
}  // namespace

void begin() {
  pinMode(Board::kAmpEnable, OUTPUT);
  digitalWrite(Board::kAmpEnable, LOW);

  i2c_config_t bus = {};
  bus.mode = I2C_MODE_MASTER;
  bus.sda_io_num = Board::kSda;
  bus.scl_io_num = Board::kScl;
  bus.sda_pullup_en = GPIO_PULLUP_ENABLE;
  bus.scl_pullup_en = GPIO_PULLUP_ENABLE;
  bus.master.clk_speed = 100000;
  esp_err_t result = i2c_param_config(kI2c, &bus);
  if (result == ESP_OK) result = i2c_driver_install(kI2c, bus.mode, 0, 0, 0);
  if (result != ESP_OK) return fail("I2C", result);
  gState.i2c = true;
  // CE may be strapped either way. An ID read proves a responding codec,
  // rather than treating a successful bus installation as detection.
  for (const uint8_t address : {uint8_t(0x18), uint8_t(0x19)}) {
    uint8_t high = 0, low = 0, version = 0;
    if (readRegister(address, 0xFD, high) == ESP_OK &&
        readRegister(address, 0xFE, low) == ESP_OK &&
        readRegister(address, 0xFF, version) == ESP_OK) {
      gState.address = address;
      gState.idHigh = high;
      gState.idLow = low;
      gState.version = version;
      break;
    }
  }
  if (!gState.address) return fail("codec detection", ESP_ERR_NOT_FOUND);
  DeviceLog::printf("[audio] codec I2C=0x%02x ID=%02x%02x rev=%02x\n",
                gState.address, gState.idHigh, gState.idLow, gState.version);
  if (gState.idHigh != 0x83 || gState.idLow != 0x11) {
    return fail("unexpected codec ID", ESP_ERR_NOT_SUPPORTED);
  }

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  config.sample_rate = kSampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = kFrames;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = kSampleRate * 256;
  config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  config.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;
  result = i2s_driver_install(kI2s, &config, 0, nullptr);
  if (result != ESP_OK) return fail("I2S install", result);
  i2s_pin_config_t pins = {};
  pins.mck_io_num = Board::kMclk;
  pins.bck_io_num = Board::kBclk;
  pins.ws_io_num = Board::kLrck;
  pins.data_out_num = Board::kAudioOut;
  pins.data_in_num = Board::kAudioIn;
  result = i2s_set_pin(kI2s, &pins);
  if (result != ESP_OK) {
    i2s_driver_uninstall(kI2s);
    return fail("I2S pins", result);
  }
  gState.i2s = true;
  i2s_zero_dma_buffer(kI2s);

  gCodec = es8311_create(kI2c, gState.address);
  if (!gCodec) return fail("codec allocation", ESP_ERR_NO_MEM);
  es8311_clock_config_t clocks = {};
  clocks.mclk_from_mclk_pin = true;
  clocks.mclk_frequency = kSampleRate * 256;
  clocks.sample_frequency = kSampleRate;
  result = es8311_init(gCodec, &clocks, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
  if (result == ESP_OK) result = es8311_microphone_config(gCodec, false);
  if (result == ESP_OK) result = es8311_microphone_gain_set(gCodec, ES8311_MIC_GAIN_24DB);
  if (result == ESP_OK) result = configureVolume(gState.volume);
  if (result != ESP_OK) return fail("codec configuration", result);
  gState.codec = true;
  gState.dacVolumeRegister = DemoLevels::codecVolume(gState.volume);
  const BaseType_t created = xTaskCreatePinnedToCore(run, "audio-demo", 6144,
                                                   nullptr, 2, nullptr, 1);
  gState.task = created == pdPASS;
  if (!gState.task) return fail("audio task", ESP_ERR_NO_MEM);
  // CS8120 shutdown is low. Codec and a zero-filled DMA ring are ready
  // before enabling the speaker; pause() also disables the amplifier.
  digitalWrite(Board::kAmpEnable, HIGH);
  DeviceLog::println("[audio] 16 kHz stereo I2S clocks, mono ADC/DAC ready");
}

Snapshot snapshot() {
  portENTER_CRITICAL(&gMux);
  const Snapshot result = gState;
  portEXIT_CRITICAL(&gMux);
  return result;
}

void beep(unsigned frequency, unsigned durationMs) {
  if (!gState.task) return;
  portENTER_CRITICAL(&gMux);
  gFrequency = constrain(frequency, 100u, 4000u);
  gDuration = constrain(durationMs, 30u, 1500u);
  ++gRequest;
  portEXIT_CRITICAL(&gMux);
}

void setVolume(int percent) {
  const int value = constrain(percent, 0, 100);
  if (gCodec && gState.codec && configureVolume(value) == ESP_OK) {
    portENTER_CRITICAL(&gMux);
    gState.volume = value;
    gState.dacVolumeRegister = DemoLevels::codecVolume(value);
    portEXIT_CRITICAL(&gMux);
  }
}

void pause(bool paused) {
  portENTER_CRITICAL(&gMux);
  gPaused = paused;
  portEXIT_CRITICAL(&gMux);
  digitalWrite(Board::kAmpEnable, !paused && gState.codec ? HIGH : LOW);
}

String statusJson() {
  const Snapshot s = snapshot();
  String json;
  json.reserve(500);
  json += "{\"i2c_ready\":" + String(s.i2c ? "true" : "false");
  json += ",\"address\":" + String(s.address);
  json += ",\"id\":" + String((s.idHigh << 8) | s.idLow);
  json += ",\"revision\":" + String(s.version);
  json += ",\"codec_ready\":" + String(s.codec ? "true" : "false");
  json += ",\"i2s_ready\":" + String(s.i2s ? "true" : "false");
  json += ",\"task_ready\":" + String(s.task ? "true" : "false");
  json += ",\"sample_rate\":16000,\"volume\":" + String(s.volume);
  json += ",\"dac_volume_register\":" + String(s.dacVolumeRegister);
  json += ",\"dac_gain_db\":" + String((int(s.dacVolumeRegister) - 191) * 0.5f, 1);
  json += ",\"error\":" + String(s.error);
  json += ",\"rx_frames\":" + String(s.rxFrames);
  json += ",\"tx_frames\":" + String(s.txFrames);
  json += ",\"io_errors\":" + String(s.ioErrors);
  json += ",\"tones_played\":" + String(s.tonesPlayed);
  json += ",\"tone_active\":" + String(s.tone ? "true" : "false");
  json += ",\"mic_rms\":" + String(s.rms, 2);
  json += ",\"mic_peak\":" + String(s.peak, 2);
  json += ",\"mic_dbfs\":" + String(s.dbfs, 1) + "}";
  return json;
}
}  // namespace AudioDemo
