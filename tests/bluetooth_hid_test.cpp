#include "bluetooth_hid.h"
#include <iostream>
int main(){
 BLEServer server;uint8_t map[]={5,1,9,6};
 auto reports=BluetoothHid::create(&server,map,sizeof(map));
 server.confirm(); // Regression: the unstarted implicit battery service crashed here.
 assert(server.services.size()==2);
 for(auto *s:server.services)assert(s->uuid!=0x180f&&s->started);
 assert(reports.keyboard->value.size()==8&&reports.consumer->value.size()==2);
 assert(reports.keyboard->permissions==ESP_GATT_PERM_READ_ENCRYPTED);
 assert(reports.consumer->permissions==ESP_GATT_PERM_READ_ENCRYPTED);
 for(auto *c:{reports.keyboard,reports.consumer}){
  unsigned refs=0,cccd=0;
  for(auto *d:c->descriptors){
   if(d->uuid==0x2908){++refs;assert(d->value.size()==2&&d->value[1]==1);assert(d->value[0]==(c==reports.keyboard?1:2));}
   if(d->uuid==0x2902){++cccd;assert(d->permissions==(ESP_GATT_PERM_READ_ENCRYPTED|ESP_GATT_PERM_WRITE_ENCRYPTED));}
  }
  assert(refs==1&&cccd==1);
 }
 std::cout<<"PASS powered HID services initialized before confirmations; no implicit battery, encrypted keyboard/consumer reports and references\n";
}
