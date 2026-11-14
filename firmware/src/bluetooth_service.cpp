#include "bluetooth_service.h"
#include "coloplay_ble_protocol.h"
#include "device_log.h"
#include "panic_capture.h"
#include "volume_grid.h"
#include "playback_clock.h"
#include <atomic>
#include <BLEDevice.h>
#include <BLEServer.h>
#include "bluetooth_hid.h"
#include <BLE2902.h>
#include <Preferences.h>
#include <esp_gap_ble_api.h>
#include <esp_system.h>
#include <freertos/queue.h>
#include <esp_attr.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <WiFi.h>

namespace BluetoothService {
namespace {
RTC_NOINIT_ATTR uint32_t gRtcMagic,gRtcStage,gRtcHeap;
uint32_t gPreviousStage=0,gPreviousHeap=0;
void stage(uint32_t value){gRtcMagic=0xb1e01690;gRtcStage=value;gRtcHeap=ESP.getFreeHeap();DeviceLog::printf("[bluetooth] stage=%u heap=%u\n",value,gRtcHeap);}
struct Settings {uint32_t version=1,enabled=0;uint8_t peer[6]={};uint8_t reserved[2]={};} gSettings;
Preferences gPrefs;bool gStored=false,gOpen=false,gStarted=false,gRecovery=false,gStartAttempted=false;
esp_err_t gStartError=ESP_OK;
BLEServer *gServer=nullptr;BLECharacteristic *gCaps=nullptr,*gCommand=nullptr,*gKeyboard=nullptr,*gConsumer=nullptr;
BLEAdvertising *gAdvertising=nullptr;
portMUX_TYPE gMux=portMUX_INITIALIZER_UNLOCKED;
Snapshot gState;PlaybackClock gPlaybackClock;uint32_t gSession=0;std::atomic<uint32_t> gPairUntil{0};uint16_t gNextRequest=0,gConn=0;
std::atomic<bool> gDisconnect{false},gPersistPeer{false},gAdvertisingOn{false};uint8_t gRemote[6]={};
struct Out {uint8_t bytes[CP_BLE_BYTES];};QueueHandle_t gQueue=nullptr;
bool gReleasing=false;uint32_t gReleaseAt=0;uint8_t gReleaseReport=0;
uint32_t gRequestAt=0;
char gText[2][64]={};
uint32_t gSent=0,gDropped=0;std::atomic<uint32_t> gInvalid{0};
uint8_t gReportMap[]={
  0x05,0x01,0x09,0x06,0xa1,0x01,0x85,0x01,
  0x05,0x07,0x19,0xe0,0x29,0xe7,0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,
  0x95,0x01,0x75,0x08,0x81,0x01,0x95,0x06,0x75,0x08,0x15,0x00,0x26,0xe7,0x00,0x19,0x00,0x29,0xe7,0x81,0x00,
  0x05,0x08,0x19,0x01,0x29,0x05,0x95,0x05,0x75,0x01,0x91,0x02,0x95,0x01,0x75,0x03,0x91,0x01,0xc0,
  0x05,0x0c,0x09,0x01,0xa1,0x01,0x85,0x02,0x15,0x00,0x26,0xff,0x03,0x19,0x00,0x2a,0xff,0x03,0x75,0x10,0x95,0x01,0x81,0x00,0xc0};
Settings readSettings(){portENTER_CRITICAL(&gMux);auto out=gSettings;portEXIT_CRITICAL(&gMux);return out;}
bool hasPeer(){uint8_t any=0;for(auto byte:gSettings.peer)any|=byte;return any!=0;}
bool pairing(){return gPairUntil&&static_cast<int32_t>(gPairUntil-millis())>0;}
String quote(const char *value){String s="\"";for(const unsigned char *p=(const unsigned char*)value;*p;++p){if(*p=='"'||*p=='\\')s+='\\';if(*p>=32)s+=char(*p);}return s+'"';}
String address(const uint8_t *p){char out[18];snprintf(out,sizeof(out),"%02X:%02X:%02X:%02X:%02X:%02X",p[0],p[1],p[2],p[3],p[4],p[5]);return out;}
void clearState(){gPlaybackClock.reset();gState.track=0;gState.elapsedMs=gState.durationMs=0;gState.connected=gState.secured=gState.extension=gState.fresh=gState.available=gState.pending=false;gState.volume=gState.preview=-1;gState.playback=255;gState.seek=0;gState.title[0]=gState.artist[0]=0;}
class ServerCallbacks:public BLEServerCallbacks {
 void onConnect(BLEServer*,esp_ble_gatts_cb_param_t *param)override {
   portENTER_CRITICAL(&gMux);clearState();gState.connected=true;gConn=param->connect.conn_id;memcpy(gRemote,param->connect.remote_bda,6);gSession=esp_random();if(!gSession)gSession=1;gNextRequest=0;portEXIT_CRITICAL(&gMux);
   const auto settings=readSettings();
   if(!settings.enabled||(!pairing()&&memcmp(gRemote,settings.peer,6))){gDisconnect=true;return;}
   gServer->updateConnParams(param->connect.remote_bda,6,12,0,500);
   esp_ble_set_encryption(param->connect.remote_bda,ESP_BLE_SEC_ENCRYPT);
 }
 void onDisconnect(BLEServer*,esp_ble_gatts_cb_param_t*)override {
   portENTER_CRITICAL(&gMux);clearState();gSession=0;portEXIT_CRITICAL(&gMux);if(gQueue)xQueueReset(gQueue);gAdvertisingOn=false;
 }
} gServerCallbacks;
class SecurityCallbacks:public BLESecurityCallbacks {
 uint32_t onPassKeyRequest()override{return 0;}
 void onPassKeyNotify(uint32_t)override{}
 bool onConfirmPIN(uint32_t)override{return pairing();}
 bool onSecurityRequest()override{const auto settings=readSettings();return settings.enabled&&(pairing()||!memcmp(gRemote,settings.peer,6));}
 void onAuthenticationComplete(esp_ble_auth_cmpl_t auth)override{
   const auto settings=readSettings();
   if(!auth.success||!settings.enabled||(!pairing()&&memcmp(auth.bd_addr,settings.peer,6))){gDisconnect=true;return;}
   portENTER_CRITICAL(&gMux);gState.secured=true;memcpy(gRemote,auth.bd_addr,6);gPersistPeer=true;portEXIT_CRITICAL(&gMux);
 }
} gSecurityCallbacks;
class CapsCallbacks:public BLECharacteristicCallbacks {
 void onRead(BLECharacteristic *c)override{uint8_t data[CP_BLE_BYTES];portENTER_CRITICAL(&gMux);cp_header(data,CP_HELLO,0,gSession);portEXIT_CRITICAL(&gMux);data[8]=CP_CAP_MEDIA|CP_CAP_VOLUME|CP_CAP_KEY|CP_CAP_STATE;c->setValue(data,sizeof(data));}
} gCapsCallbacks;
class StateCallbacks:public BLECharacteristicCallbacks {
 void onWrite(BLECharacteristic *c)override{
   const std::string bytes=c->getValue();const auto *p=(const uint8_t*)bytes.data();
   portENTER_CRITICAL(&gMux);
   if(!gState.secured||!cp_packet(p,bytes.size())||cp_u32(p+4)!=gSession){++gInvalid;portEXIT_CRITICAL(&gMux);return;}
   if(p[1]==CP_TEXT){const unsigned field=p[2]&0x7f,offset=p[3],n=p[8];if((field==1||field==2)&&offset<63&&n<=11&&offset+n<=63){char *target=gText[field-1];if(!offset)memset(target,0,64);memcpy(target+offset,p+9,n);target[offset+n]=0;if(p[2]&0x80)memcpy(field==1?gState.title:gState.artist,target,64);}else ++gInvalid;}
   else if(p[1]==CP_TRACK){gState.track=uint64_t(cp_u32(p+8))|(uint64_t(cp_u32(p+12))<<32);gPlaybackClock.track(gState.track);gState.title[0]=gState.artist[0]=0;memset(gText,0,sizeof(gText));}
   else if(cp_state_valid(p,bytes.size())){
     gState.extension=true;if(p[1]==CP_HELLO)gState.nativeSteps=p[8]==64?64:16;
     if(p[1]==CP_STATE){gState.receivedAt=millis();gState.available=(p[10]&0x80)!=0;gState.volume=(int8_t)p[8];gState.playback=p[9];gState.seek=p[10]&3;gState.elapsedMs=cp_u32(p+12);gState.durationMs=cp_u32(p+16);
       gPlaybackClock.update(gState.available,gState.playback,gState.seek,gState.elapsedMs,gState.durationMs,gState.receivedAt);
       const uint16_t request=cp_u16(p+2);if(request&&request==gState.request){gState.result=p[11];gState.pending=p[11]==CP_PENDING||p[11]==CP_ACCEPTED;if(!gState.pending)gState.preview=-1;}}
   }else ++gInvalid;
   portEXIT_CRITICAL(&gMux);
 }
} gStateCallbacks;
void start(){
  gStartAttempted=true;
  DeviceLog::println("[bluetooth] starting BLE HID and Tab5 feedback service");
  stage(1);
  // The Wi-Fi and Bluetooth controllers share the radio. Allow Wi-Fi modem
  // sleep before starting BLE so the coexistence scheduler can share airtime.
  if(!WiFi.setSleep(true)){gStartError=ESP_FAIL;return;}
  stage(11);
  esp_bt_controller_config_t cfg=BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  const auto controller=esp_bt_controller_init(&cfg);
  DeviceLog::printf("[bluetooth] controller init: %s\n",esp_err_to_name(controller));
  stage(12);
  if(controller!=ESP_OK){gStartError=controller;return;}
  const auto enabled=esp_bt_controller_enable(ESP_BT_MODE_BLE);
  DeviceLog::printf("[bluetooth] controller enable: %s\n",esp_err_to_name(enabled));
  stage(13);
  if(enabled!=ESP_OK){gStartError=enabled;return;}
  const auto host=esp_bluedroid_init();
  DeviceLog::printf("[bluetooth] host init: %s\n",esp_err_to_name(host));
  stage(14);
  if(host!=ESP_OK){gStartError=host;return;}
  const auto hostEnabled=esp_bluedroid_enable();
  DeviceLog::printf("[bluetooth] host enable: %s\n",esp_err_to_name(hostEnabled));
  stage(15);
  if(hostEnabled!=ESP_OK){gStartError=hostEnabled;return;}
  BLEDevice::init("ColoPlay");
  stage(16);
  BLEDevice::setMTU(64);BLEDevice::setSecurityCallbacks(&gSecurityCallbacks);
  stage(2);
  auto *security=new BLESecurity;security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);security->setCapability(ESP_IO_CAP_NONE);security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK|ESP_BLE_ID_KEY_MASK);security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK|ESP_BLE_ID_KEY_MASK);
  stage(3);
  gServer=BLEDevice::createServer();gServer->setCallbacks(&gServerCallbacks);
  stage(4);
  const auto reports=BluetoothHid::create(gServer,gReportMap,sizeof(gReportMap));
  gKeyboard=reports.keyboard;gConsumer=reports.consumer;
  stage(6);
  stage(7);
  auto *service=gServer->createService(CP_BLE_SERVICE);
  stage(8);
  gCaps=service->createCharacteristic(CP_BLE_CAPS,BLECharacteristic::PROPERTY_READ);gCaps->setCallbacks(&gCapsCallbacks);
  gCommand=service->createCharacteristic(CP_BLE_COMMAND,BLECharacteristic::PROPERTY_NOTIFY);auto *cccd=new BLE2902;cccd->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED|ESP_GATT_PERM_WRITE_ENCRYPTED);gCommand->addDescriptor(cccd);
  auto *state=service->createCharacteristic(CP_BLE_STATE,BLECharacteristic::PROPERTY_WRITE);state->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);state->setCallbacks(&gStateCallbacks);service->start();
  stage(9);
  gAdvertising=BLEDevice::getAdvertising();gAdvertising->addServiceUUID(CP_BLE_SERVICE);gAdvertising->addServiceUUID((uint16_t)0x1812);gAdvertising->setAppearance(0x03c1);gAdvertising->setScanResponse(true);gAdvertising->setMinPreferred(6);gAdvertising->setMaxPreferred(12);
  gQueue=xQueueCreate(32,sizeof(Out));gStarted=gQueue!=nullptr;portENTER_CRITICAL(&gMux);gState.ready=gStarted;portEXIT_CRITICAL(&gMux);
  stage(10);
}
void releaseHid(){uint8_t zero[8]={};if(gReleaseReport==1){gKeyboard->setValue(zero,8);gKeyboard->notify();}else{gConsumer->setValue(zero,2);gConsumer->notify();}gReleasing=false;}
} // namespace
void load(){if(gRtcMagic==0xb1e01690){gPreviousStage=gRtcStage;gPreviousHeap=gRtcHeap;}gOpen=gPrefs.begin("bluetooth",false);Settings saved;if(gOpen&&gPrefs.getBytesLength("settings")==sizeof(saved)&&gPrefs.getBytes("settings",&saved,sizeof(saved))==sizeof(saved)&&saved.version==1&&saved.enabled<=1){gSettings=saved;gStored=true;}gState.enabled=gSettings.enabled;}
Snapshot snapshot(){
  portENTER_CRITICAL(&gMux);
  const uint32_t now=millis();auto out=gState;
  out.positionMs=gPlaybackClock.position(now);out.positionValid=gPlaybackClock.valid(now);
  portEXIT_CRITICAL(&gMux);
  out.enabled=gSettings.enabled;out.fresh=out.connected&&out.secured&&out.receivedAt&&uint32_t(now-out.receivedAt)<PlaybackClock::kFreshMs;
  if(!out.fresh){out.available=out.positionValid=false;out.volume=-1;out.playback=255;}
  return out;
}
void tick(bool hardwareReady){
  gRecovery=!hardwareReady;if(!hardwareReady)return;
  if(gSettings.enabled&&!gStartAttempted)start();if(!gStarted)return;
  if(gDisconnect||(!gSettings.enabled&&snapshot().connected)){gDisconnect=false;gServer->disconnect(gConn);}
  if(gPersistPeer){portENTER_CRITICAL(&gMux);memcpy(gSettings.peer,gRemote,6);gPersistPeer=false;portEXIT_CRITICAL(&gMux);gStored=gOpen&&gPrefs.putBytes("settings",&gSettings,sizeof(gSettings))==sizeof(gSettings);gPairUntil=0;DeviceLog::println("[bluetooth] secure bonded peer connected");}
  const auto s=snapshot();const bool advertise=gSettings.enabled&&!s.connected&&(pairing()||hasPeer());
  if(s.pending&&uint32_t(millis()-gRequestAt)>5000){portENTER_CRITICAL(&gMux);gState.pending=false;gState.preview=-1;gState.result=CP_FAILED;portEXIT_CRITICAL(&gMux);}
  if(advertise&&!gAdvertisingOn){gAdvertising->start();gAdvertisingOn=true;}else if(!advertise&&gAdvertisingOn){gAdvertising->stop();gAdvertisingOn=false;}
  if(!s.connected||!s.secured){if(gQueue)xQueueReset(gQueue);gReleasing=false;return;}
  if(gReleasing){if(static_cast<int32_t>(millis()-gReleaseAt)>=0)releaseHid();else return;}
  Out out;if(xQueueReceive(gQueue,&out,0)!=pdTRUE)return;
  if(s.extension){gCommand->setValue(out.bytes,sizeof(out.bytes));gCommand->notify();++gSent;return;}
  const uint8_t kind=out.bytes[1];uint16_t usage=cp_u16(out.bytes+8);
  if(kind==CP_KEY){uint8_t keys[8]={uint8_t(cp_u16(out.bytes+12)),0,uint8_t(usage)};gKeyboard->setValue(keys,8);gKeyboard->notify();gReleaseReport=1;}
  else if(kind==CP_RELEASE){gReleaseReport=2;releaseHid();return;}
  else {uint8_t data[2];cp_put16(data,usage);gConsumer->setValue(data,2);gConsumer->notify();gReleaseReport=2;}
  gReleasing=!(kind==CP_MEDIA&&(usage==0xb3||usage==0xb4)&&cp_u16(out.bytes+12)==1);gReleaseAt=millis()+((usage==0xb3||usage==0xb4)?(cp_u16(out.bytes+14)<30?30:cp_u16(out.bytes+14)):20);++gSent;
}
bool send(uint8_t kind,uint16_t usage,int count,uint16_t parameter,uint16_t duration){
  auto s=snapshot();if(!gQueue||!s.connected||!s.secured||count<1||count>(kind==CP_VOLUME?32767:127)||(!s.extension&&kind==CP_VOLUME&&parameter!=1)){portENTER_CRITICAL(&gMux);gState.result=CP_UNSUPPORTED;gState.pending=false;gState.preview=-1;portEXIT_CRITICAL(&gMux);return false;}
  Out out;portENTER_CRITICAL(&gMux);if(++gNextRequest==0)++gNextRequest;cp_header(out.bytes,kind,gNextRequest,gSession);portEXIT_CRITICAL(&gMux);cp_put16(out.bytes+8,usage);cp_put16(out.bytes+10,count);cp_put16(out.bytes+12,parameter);cp_put16(out.bytes+14,duration);if(!cp_command_valid(out.bytes,sizeof(out.bytes)))return false;
  if(!s.extension&&count!=1)return false; // callers enqueue each native HID tap separately.
  if(xQueueSend(gQueue,&out,0)!=pdTRUE){++gDropped;return false;}
  gRequestAt=millis();
  portENTER_CRITICAL(&gMux);gState.request=gNextRequest;gState.usage=usage;gState.result=s.extension?CP_PENDING:CP_ACCEPTED;gState.pending=s.extension;
  if(kind==CP_VOLUME&&s.fresh&&s.volume>=0){const int steps=parameter==1?s.nativeSteps:parameter;const int base=gState.preview>=0?gState.preview:s.volume;gState.preview=volume_grid_step(base,(usage==0xe9?1:-1)*count,steps);}
  portEXIT_CRITICAL(&gMux);return true;
}
void cancel(){if(gQueue)xQueueReset(gQueue);const auto s=snapshot();if(s.connected&&s.secured)send(CP_RELEASE,0,1);}
bool configure(WebServer &server,String &error){
  const String op=server.arg("op");
  if(op=="enable"||op=="disable"){auto candidate=gSettings;candidate.enabled=op=="enable";if(!gOpen||gPrefs.putBytes("settings",&candidate,sizeof(candidate))!=sizeof(candidate)){error="could not save Bluetooth settings";return false;}portENTER_CRITICAL(&gMux);gSettings=candidate;portEXIT_CRITICAL(&gMux);gStored=true;if(!candidate.enabled){gPairUntil=0;cancel();}return true;}
  if(op=="pair"){if(!gSettings.enabled){error="enable Bluetooth first";return false;}if(gRecovery){error="hardware must be ready before pairing";return false;}gPairUntil=millis()+120000;if(snapshot().connected)gDisconnect=true;return true;}
  if(op=="stop_pairing"){gPairUntil=0;return true;}
  if(op=="forget"){
    auto candidate=gSettings;memset(candidate.peer,0,6);if(!gOpen||gPrefs.putBytes("settings",&candidate,sizeof(candidate))!=sizeof(candidate)){error="could not clear Bluetooth peer";return false;}portENTER_CRITICAL(&gMux);gSettings=candidate;portEXIT_CRITICAL(&gMux);gPairUntil=0;gStored=true;gDisconnect=true;
    if(gStarted){int n=esp_ble_get_bond_device_num();if(n>0&&n<16){auto *list=new esp_ble_bond_dev_t[n];int count=n;if(esp_ble_get_bond_device_list(&count,list)==ESP_OK)for(int i=0;i<count;++i)esp_ble_remove_bond_device(list[i].bd_addr);delete[] list;}}return true;
  }
  error="op must be enable, disable, pair, stop_pairing or forget";return false;
}
String statusJson(){const auto s=snapshot();char track[17];snprintf(track,sizeof(track),"%016llx",(unsigned long long)s.track);const int remaining=pairing()?int((gPairUntil-millis()+999)/1000):0;String out="{\"enabled\":"+String(s.enabled?"true":"false")+",\"ready\":"+String(s.ready?"true":"false")+",\"stored\":"+String(gStored?"true":"false")+",\"connected\":"+String(s.connected?"true":"false")+",\"secured\":"+String(s.secured?"true":"false")+",\"extension\":"+String(s.extension?"true":"false")+",\"pairing_seconds\":"+String(remaining)+",\"peer\":"+quote(hasPeer()?address(gSettings.peer).c_str():"");
 out+=",\"last_panic\":"+PanicCapture::json()+",\"error\":"+String(gStartError)+",\"stage\":"+String(gStartAttempted?gRtcStage:0)+",\"last_boot_stage\":"+String(gPreviousStage)+",\"last_boot_heap\":"+String(gPreviousHeap)+",\"track\":"+quote(track)+",\"advertising\":"+String(gAdvertisingOn?"true":"false")+",\"fresh\":"+String(s.fresh?"true":"false")+",\"available\":"+String(s.available?"true":"false")+",\"volume\":"+String(s.volume)+",\"preview\":"+String(s.preview)+",\"playback\":"+String(s.playback)+",\"seek\":"+String(s.seek)+",\"request\":"+String(s.request)+",\"result\":"+String(s.result)+",\"pending\":"+String(s.pending?"true":"false")+",\"title\":"+quote(s.title)+",\"artist\":"+quote(s.artist)+",\"elapsed_ms\":"+String(s.elapsedMs)+",\"position_ms\":"+String(s.positionMs)+",\"position_valid\":"+String(s.positionValid?"true":"false")+",\"duration_ms\":"+String(s.durationMs)+",\"sent\":"+String(gSent)+",\"dropped\":"+String(gDropped)+",\"invalid\":"+String(gInvalid.load())+"}";return out;}
}
