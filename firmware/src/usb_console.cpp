#include "usb_console.h"
#include "serial_tx_buffer.h"
#include <atomic>
#ifndef COLOPLAY_USB_CONCURRENCY_TEST
#include <USBCDC.h>
#include <tusb.h>
#endif
namespace UsbConsole {
namespace {
std::atomic<bool> gNetwork{false};
#ifndef COLOPLAY_USB_CONCURRENCY_TEST
// Constructor registers ACM before USB.begin(); Serial/JTAG builds never start OTG.
USBCDC gAcm(0);
SerialTxBuffer<8192> gTx;
portMUX_TYPE gTxLock=portMUX_INITIALIZER_UNLOCKED;
#endif
class Console:public Stream {
 public:
 int available()override{
#ifndef COLOPLAY_USB_CONCURRENCY_TEST
   if(gNetwork.load())return gAcm.available();
#endif
   return Serial.available();
 }
 int read()override{
#ifndef COLOPLAY_USB_CONCURRENCY_TEST
   if(gNetwork.load())return gAcm.read();
#endif
   return Serial.read();
 }
 int peek()override{
#ifndef COLOPLAY_USB_CONCURRENCY_TEST
   if(gNetwork.load())return gAcm.peek();
#endif
   return Serial.peek();
 }
 void flush()override{/* Console output must never wait for a host. */}
 size_t write(uint8_t byte)override{return write(&byte,1);}
 size_t write(const uint8_t *data,size_t size)override{
#ifndef COLOPLAY_USB_CONCURRENCY_TEST
   if(!connected())return 0;
   portENTER_CRITICAL(&gTxLock);const size_t written=gTx.append(data,size);portEXIT_CRITICAL(&gTxLock);return written;
#else
   return Serial.write(data,size);
#endif
 }
} gConsole;
}
Stream &stream(){return gConsole;}
bool prepareNetwork(){
#ifndef COLOPLAY_USB_CONCURRENCY_TEST
  gAcm.enableReboot(false);gAcm.setTxTimeoutMs(0);
  if(gAcm.setRxBufferSize(512)!=512)return false;
  gAcm.begin(115200);
#endif
  return true;
}
void useNetwork(bool enabled){gNetwork.store(enabled);}
void tick(){
#ifndef COLOPLAY_USB_CONCURRENCY_TEST
  if(!connected()){portENTER_CRITICAL(&gTxLock);gTx.clear();portEXIT_CRITICAL(&gTxLock);return;}
  // The pinned Arduino USBCDC::write spins when the host stops reading.
  // Feed TinyUSB's nonblocking FIFO directly from the sole consumer instead.
  uint8_t bytes[256];portENTER_CRITICAL(&gTxLock);const size_t count=gTx.peek(bytes,sizeof(bytes));portEXIT_CRITICAL(&gTxLock);
  if(count){const size_t sent=gNetwork.load()?tud_cdc_n_write(0,bytes,count):Serial.write(bytes,std::min(count,size_t(Serial.availableForWrite())));portENTER_CRITICAL(&gTxLock);gTx.consume(sent);portEXIT_CRITICAL(&gTxLock);if(gNetwork.load())tud_cdc_n_write_flush(0);}
#endif
}
bool connected(){
#ifndef COLOPLAY_USB_CONCURRENCY_TEST
  if(gNetwork.load())return tud_cdc_n_connected(0);
#endif
  return bool(Serial);
}
uint32_t dropped(){
#ifndef COLOPLAY_USB_CONCURRENCY_TEST
  portENTER_CRITICAL(&gTxLock);const uint32_t count=gTx.dropped();portEXIT_CRITICAL(&gTxLock);return count;
#else
  return 0;
#endif
}
size_t queued(){
#ifndef COLOPLAY_USB_CONCURRENCY_TEST
  portENTER_CRITICAL(&gTxLock);const size_t count=gTx.size();portEXIT_CRITICAL(&gTxLock);return count;
#else
  return 0;
#endif
}
}
