#include "usb_console.h"
#include "USBCDC.h"
#include <cassert>
#include <iostream>
FakeSerial Serial;
namespace FakeUsb {
bool connected=true,reboot=true,allocationFailure=false,began=false;
uint32_t timeout=250;size_t space=11;
std::deque<uint8_t> rx;std::vector<uint8_t> tx;
}
int main(){
 auto &port=UsbConsole::stream();std::vector<uint8_t> payload(7000);for(size_t i=0;i<payload.size();++i)payload[i]=i%256;
 assert(port.write(payload.data(),payload.size())==payload.size());
 for(int i=0;i<1000&&UsbConsole::queued();++i)UsbConsole::tick();
 assert(!UsbConsole::queued()&&Serial.tx==payload&&FakeUsb::tx.empty());
 Serial.rx={1,2};assert(port.available()==2&&port.peek()==1&&port.read()==1&&port.read()==2&&port.read()==-1);
 FakeUsb::allocationFailure=true;assert(!UsbConsole::prepareNetwork()&&!FakeUsb::began);
 FakeUsb::allocationFailure=false;assert(UsbConsole::prepareNetwork()&&FakeUsb::began&&!FakeUsb::reboot&&FakeUsb::timeout==0);
 UsbConsole::useNetwork(true);FakeUsb::rx={3,4};assert(port.available()==2&&port.peek()==3&&port.read()==3&&port.read()==4);
 assert(port.write(payload.data(),payload.size())==payload.size());
 for(int i=0;i<1000&&UsbConsole::queued();++i)UsbConsole::tick();
 assert(!UsbConsole::queued()&&FakeUsb::tx==payload);
 FakeUsb::space=0;
 for(int i=0;i<100;++i){port.write(payload.data(),payload.size());UsbConsole::tick();}
 assert(UsbConsole::queued()==8192&&UsbConsole::dropped()>0);
 FakeUsb::connected=false;UsbConsole::tick();assert(!UsbConsole::queued()&&!UsbConsole::connected()&&port.write(uint8_t(5))==0);
 UsbConsole::useNetwork(false);assert(UsbConsole::connected());Serial.space=0;assert(port.write(uint8_t(6))==1);UsbConsole::tick();assert(UsbConsole::queued()==1);
 Serial.space=1;UsbConsole::tick();assert(!UsbConsole::queued()&&Serial.tx.back()==6);
 std::cout<<"PASS actual USB console routes, RX, long status output, host backpressure, disconnect and allocation failure\n";
}
