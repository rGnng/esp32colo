#pragma once
#include "Arduino.h"
namespace FakeUsb {
extern bool connected,reboot,allocationFailure,began;
extern uint32_t timeout;
extern size_t space;
extern std::deque<uint8_t> rx;
extern std::vector<uint8_t> tx;
}
class USBCDC {
 public:
 explicit USBCDC(uint8_t){}
 void enableReboot(bool enabled){FakeUsb::reboot=enabled;}
 void setTxTimeoutMs(uint32_t timeout){FakeUsb::timeout=timeout;}
 size_t setRxBufferSize(size_t size){return FakeUsb::allocationFailure?0:size;}
 void begin(int){FakeUsb::began=true;}
 int available(){return FakeUsb::rx.size();}
 int read(){if(FakeUsb::rx.empty())return -1;const int c=FakeUsb::rx.front();FakeUsb::rx.pop_front();return c;}
 int peek(){return FakeUsb::rx.empty()?-1:FakeUsb::rx.front();}
};
