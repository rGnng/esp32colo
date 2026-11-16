#ifndef COLOPLAY_BLE_PROTOCOL_H
#define COLOPLAY_BLE_PROTOCOL_H
/* Shared with mpdtab5/firmware/main/coloplay_ble_protocol.h. Wire integers LE.
 * Every control/state packet fits the default 20-byte ATT value payload. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#define CP_BLE_SERVICE "a91c0001-c169-4acb-9f5d-121d621d17b5"
#define CP_BLE_CAPS "a91c0002-c169-4acb-9f5d-121d621d17b5"
#define CP_BLE_COMMAND "a91c0003-c169-4acb-9f5d-121d621d17b5"
#define CP_BLE_STATE "a91c0004-c169-4acb-9f5d-121d621d17b5"
#define CP_BLE_BYTES 20
enum {CP_BLE_VERSION=1,CP_MEDIA=1,CP_VOLUME=2,CP_KEY=3,CP_RELEASE=4,
      CP_HELLO=16,CP_STATE=17,CP_TEXT=18,CP_TRACK=19,CP_VOLUME_STATE=20,CP_NAVIGATION=21,CP_NAV_TEXT=22};
enum {CP_PENDING=0,CP_ACCEPTED=1,CP_APPLIED=2,CP_FAILED=3,CP_NO_CHANGE=4,CP_UNSUPPORTED=5,CP_BUSY=6};
enum {CP_CAP_MEDIA=1,CP_CAP_VOLUME=2,CP_CAP_KEY=4,CP_CAP_STATE=8,CP_CAP_VOLUME_FLOAT=16,CP_CAP_VOLUME_PREDICT=32,CP_CAP_VOLUME_SCALE=64,CP_CAP_NAVIGATION=128};
enum {CP_NAV_CONTEXT=1,CP_NAV_EDITING=2,CP_NAV_LOADING=4,CP_NAV_FOCUS=8,CP_NAV_QUEUE=16,CP_NAV_SCREEN_ON=32};
typedef struct {uint8_t view,flags;uint32_t index,count;char focus[96];} cp_navigation_t;
static inline uint16_t cp_u16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static inline uint32_t cp_u32(const uint8_t *p){return (uint32_t)cp_u16(p)|((uint32_t)cp_u16(p+2)<<16);}
static inline void cp_put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static inline void cp_put32(uint8_t *p,uint32_t v){cp_put16(p,(uint16_t)v);cp_put16(p+2,(uint16_t)(v>>16));}
/* IEEE-754 binary32, little endian, normalized 0..1. -1 means unavailable. */
static inline float cp_float(const uint8_t *p){uint32_t bits=cp_u32(p);float value;memcpy(&value,&bits,4);return value;}
static inline void cp_put_float(uint8_t *p,float value){uint32_t bits;memcpy(&bits,&value,4);cp_put32(p,bits);}
static inline bool cp_media_usage(uint16_t u){return u==0xb0||u==0xb1||u==0xb3||u==0xb4||u==0xb5||u==0xb6||u==0xb7||u==0xcd||u==0xe2;}
static inline bool cp_packet(const uint8_t *p,size_t n){return p&&n==CP_BLE_BYTES&&p[0]==CP_BLE_VERSION;}
static inline bool cp_command_valid(const uint8_t *p,size_t n){
  if(!cp_packet(p,n)||!cp_u16(p+2)||!cp_u32(p+4))return false;
  const int count=(int16_t)cp_u16(p+10);const unsigned arg=cp_u16(p+12),usage=cp_u16(p+8);
  if(p[1]==CP_MEDIA)return cp_media_usage(usage)&&count>=1&&count<=((usage==0xb5||usage==0xb6)?127:1)&&arg<=1&&(!arg||usage==0xb3||usage==0xb4)&&cp_u16(p+14)<=10000;
  if(p[1]==CP_VOLUME){
    const float step=cp_float(p+16);
    return (usage==0xe9||usage==0xea)&&count>=1&&count<=32767&&(arg==1||arg==16||arg==64)&&
      (step==0.0f||(step==1.0f/16&&(arg==1||arg==16))||(step==1.0f/64&&(arg==1||arg==64)));
  }
  if(p[1]==CP_KEY)return usage>0&&usage<=0xe7&&arg<=255&&count>=1&&count<=127;
  return p[1]==CP_RELEASE;
}
static inline void cp_header(uint8_t *p,uint8_t kind,uint16_t id,uint32_t session){memset(p,0,CP_BLE_BYTES);p[0]=CP_BLE_VERSION;p[1]=kind;cp_put16(p+2,id);cp_put32(p+4,session);}
/* State: header; signed volume, playback (0/1/2 or 255 unknown),
 * seek direction in flags bits 0..1, backend connected in bit 7, result;
 * elapsed/duration milliseconds. id 0 is an unsolicited state update. */
static inline bool cp_state_valid(const uint8_t *p,size_t n){
  /* Negotiated companion frame keeps legacy state and its full 32-bit times
   * intact at the default ATT MTU. Byte 12 carries the request result. */
  if(cp_packet(p,n)&&p[1]==CP_VOLUME_STATE){
    const float v=cp_float(p+8),target=cp_float(p+16);
    return (v==-1.0f||(v>=0.0f&&v<=1.0f))&&p[12]<=CP_BUSY&&
      (!p[13]||((p[13]==16||p[13]==64||p[13]==2||p[13]==5)&&(target==-1.0f||(target>=0&&target<=1))));
  }
  if(!cp_packet(p,n)||(p[1]!=CP_STATE&&p[1]!=CP_HELLO))return false;
  const int volume=(int8_t)p[8];return volume>=-1&&volume<=100&&(p[9]<=2||p[9]==255)&&(p[10]&3)<=2&&!(p[10]&0x7c)&&p[11]<=CP_BUSY;
}
#endif
