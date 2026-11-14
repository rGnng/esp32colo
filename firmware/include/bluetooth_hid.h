#pragma once
#include <BLEServer.h>
#include <BLE2902.h>

namespace BluetoothHid {
struct Reports {BLECharacteristic *keyboard,*consumer;};
// Build only services that we actually publish. BLEHIDDevice also creates a
// battery characteristic; leaving it unstarted makes Arduino 2.0.17 dispatch
// CONF events through its uninitialized service pointer (LoadProhibited).
inline Reports create(BLEServer *server,uint8_t *map,size_t length){
  auto *info=server->createService(BLEUUID(uint16_t(0x180a)));
  info->createCharacteristic(uint16_t(0x2a29),BLECharacteristic::PROPERTY_READ)->setValue(std::string("ColoPlay"));
  uint8_t pnp[]={1,0xff,0xff,0x69,0xc1,1,0};
  info->createCharacteristic(uint16_t(0x2a50),BLECharacteristic::PROPERTY_READ)->setValue(pnp,sizeof(pnp));
  auto *hid=server->createService(BLEUUID(uint16_t(0x1812)),40);
  uint8_t version[]={0x11,0x01,0,2},protocol[]={1};
  hid->createCharacteristic(uint16_t(0x2a4a),BLECharacteristic::PROPERTY_READ)->setValue(version,sizeof(version));
  hid->createCharacteristic(uint16_t(0x2a4b),BLECharacteristic::PROPERTY_READ)->setValue(map,length);
  hid->createCharacteristic(uint16_t(0x2a4c),BLECharacteristic::PROPERTY_WRITE_NR);
  hid->createCharacteristic(uint16_t(0x2a4e),BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE_NR)->setValue(protocol,sizeof(protocol));
  auto report=[&](uint8_t id,bool input,size_t size){
    const uint32_t properties=BLECharacteristic::PROPERTY_READ|(input?BLECharacteristic::PROPERTY_NOTIFY:BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_WRITE_NR);
    auto *c=hid->createCharacteristic(uint16_t(0x2a4d),properties);
    c->setAccessPermissions(input?ESP_GATT_PERM_READ_ENCRYPTED:ESP_GATT_PERM_READ_ENCRYPTED|ESP_GATT_PERM_WRITE_ENCRYPTED);
    uint8_t zero[8]={};c->setValue(zero,size);
    auto *reference=new BLEDescriptor(BLEUUID(uint16_t(0x2908)));
    uint8_t value[]={id,uint8_t(input?1:2)};reference->setValue(value,sizeof(value));
    reference->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);c->addDescriptor(reference);
    if(input){auto *cccd=new BLE2902;cccd->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED|ESP_GATT_PERM_WRITE_ENCRYPTED);c->addDescriptor(cccd);}
    return c;
  };
  Reports out={report(1,true,8),report(2,true,2)};report(1,false,1);
  info->start();hid->start();
  return out;
}
}
