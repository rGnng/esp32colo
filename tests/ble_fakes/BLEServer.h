#pragma once
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
constexpr unsigned ESP_GATT_PERM_READ_ENCRYPTED=1,ESP_GATT_PERM_WRITE_ENCRYPTED=2;
struct BLEUUID {uint16_t value;explicit BLEUUID(uint16_t v):value(v){}};
struct BLEDescriptor {
 uint16_t uuid;unsigned permissions=0;std::vector<uint8_t> value;
 explicit BLEDescriptor(BLEUUID id):uuid(id.value){}
 void setValue(uint8_t *p,size_t n){value.assign(p,p+n);}
 void setAccessPermissions(unsigned p){permissions=p;}
};
struct BLEService;
struct BLECharacteristic {
 enum {PROPERTY_READ=1,PROPERTY_NOTIFY=2,PROPERTY_WRITE=4,PROPERTY_WRITE_NR=8};
 uint16_t uuid;unsigned properties,permissions=0;BLEService *service=nullptr;
 std::vector<uint8_t> value;std::vector<BLEDescriptor*> descriptors;
 BLECharacteristic(uint16_t id,unsigned props):uuid(id),properties(props){}
 void setValue(uint8_t *p,size_t n){value.assign(p,p+n);}
 void setValue(std::string s){value.assign(s.begin(),s.end());}
 void setAccessPermissions(unsigned p){permissions=p;}
 void addDescriptor(BLEDescriptor *d){descriptors.push_back(d);}
};
struct BLEService {
 uint16_t uuid;bool started=false;std::vector<BLECharacteristic*> characteristics;
 explicit BLEService(uint16_t id):uuid(id){}
 BLECharacteristic *createCharacteristic(uint16_t id,unsigned props){auto *c=new BLECharacteristic(id,props);characteristics.push_back(c);return c;}
 void start(){started=true;for(auto *c:characteristics)c->service=this;}
};
struct BLEServer {
 std::vector<BLEService*> services;
 BLEService *createService(BLEUUID id,unsigned=15){auto *s=new BLEService(id.value);services.push_back(s);return s;}
 // Arduino dispatches CONF to every characteristic, regardless of its handle.
 void confirm(){for(auto *s:services)for(auto *c:s->characteristics){assert(c->service);assert(c->service->started);}}
 ~BLEServer(){for(auto *s:services){for(auto *c:s->characteristics){for(auto *d:c->descriptors)delete d;delete c;}delete s;}}
};
