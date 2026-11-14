#include "device_log.h"
#include "led_strip.h"
#include "board_pins.h"
#include "factory_led_frame.h"
#include <driver/spi_master.h>
#include <esp_attr.h>

namespace LedStrip {
namespace {
spi_device_handle_t gDevice = nullptr;
uint32_t gPixels[FactoryLedFrame::kPixels] = {};
DMA_ATTR uint8_t gFrame[FactoryLedFrame::kBytes];
esp_err_t gError = ESP_OK;
uint32_t gTransfers = 0, gErrors = 0;
}  // namespace

bool begin() {
  // Factory qt.init(48) uses SPI3, independent of the LCD's SPI2 bus.
  spi_bus_config_t bus = {};
  bus.mosi_io_num = Board::kRgb;
  bus.miso_io_num = -1;
  bus.sclk_io_num = -1;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
  bus.max_transfer_sz = FactoryLedFrame::kBytes;
  gError = spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO);
  if (gError != ESP_OK) { ++gErrors; return false; }
  spi_device_interface_config_t config = {};
  config.clock_speed_hz = FactoryLedFrame::kSpiHz;
  config.mode = 0;
  config.spics_io_num = -1;
  config.queue_size = 1;
  gError = spi_bus_add_device(SPI3_HOST, &config, &gDevice);
  if (gError != ESP_OK) {
    ++gErrors;
    spi_bus_free(SPI3_HOST);
    return false;
  }
  show();
  DeviceLog::println("[leds] factory SPI3 format: GPIO48, 3.2 MHz, 14 RGB slots");
  return gError == ESP_OK;
}

bool ready() { return gDevice != nullptr && gError == ESP_OK; }

void setPixel(size_t index, uint32_t rgb) {
  if (index < FactoryLedFrame::kPixels) gPixels[index] = rgb & 0xFFFFFF;
}
uint32_t pixelColor(size_t index) { return index<FactoryLedFrame::kPixels?gPixels[index]:0; }

void show() {
  if (!gDevice) return;
  FactoryLedFrame::encode(gPixels, FactoryLedFrame::kPixels, gFrame, sizeof(gFrame));
  spi_transaction_t transaction = {};
  transaction.length = sizeof(gFrame) * 8;
  transaction.tx_buffer = gFrame;
  gError = spi_device_transmit(gDevice, &transaction);
  if (gError == ESP_OK) ++gTransfers;
  else ++gErrors;
}

int error() { return gError; }
uint32_t transfers() { return gTransfers; }
uint32_t errors() { return gErrors; }
}  // namespace LedStrip
