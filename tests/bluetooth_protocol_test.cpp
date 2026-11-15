#include "coloplay_ble_protocol.h"
#include "scene_settings.h"
#include <cassert>
#include <iostream>
int main(){
 uint8_t p[CP_BLE_BYTES];cp_header(p,CP_VOLUME,7,0x1234abcd);cp_put16(p+8,0xe9);cp_put16(p+10,3);cp_put16(p+12,64);
 assert(cp_u32(p+4)==0x1234abcd&&cp_command_valid(p,sizeof(p)));
 assert(!cp_command_valid(p,19)&&!cp_command_valid(p,21));p[0]=2;assert(!cp_command_valid(p,sizeof(p)));p[0]=1;
 cp_put16(p+12,8);assert(!cp_command_valid(p,sizeof(p)));cp_put16(p+12,16);assert(cp_command_valid(p,sizeof(p)));
 cp_put16(p+10,0);assert(!cp_command_valid(p,sizeof(p)));cp_put16(p+10,1);cp_put32(p+4,0);assert(!cp_command_valid(p,sizeof(p)));
 cp_header(p,CP_MEDIA,1,1);cp_put16(p+10,1);for(int usage:{0xb0,0xb1,0xb3,0xb4,0xb5,0xb6,0xb7,0xcd,0xe2}){cp_put16(p+8,usage);assert(cp_command_valid(p,sizeof(p)));}cp_put16(p+8,0xffff);assert(!cp_command_valid(p,sizeof(p)));
 cp_header(p,CP_STATE,7,1);p[8]=uint8_t(-1);p[9]=255;assert(cp_state_valid(p,sizeof(p)));p[8]=101;assert(!cp_state_valid(p,sizeof(p)));p[8]=67;p[9]=1;p[10]=0x81;p[11]=CP_APPLIED;assert(cp_state_valid(p,sizeof(p)));p[10]=0x83;assert(!cp_state_valid(p,sizeof(p)));
 cp_header(p,CP_VOLUME_STATE,9,1);cp_put_float(p+8,1.0f/64);p[12]=CP_APPLIED;assert(cp_state_valid(p,sizeof(p))&&cp_float(p+8)==1.0f/64);
 p[13]=64;cp_put16(p+14,9);cp_put_float(p+16,.25f);assert(cp_state_valid(p,sizeof(p)));
 cp_put32(p+16,0x7fc00000);assert(!cp_state_valid(p,sizeof(p)));cp_put_float(p+16,-1);p[13]=32;assert(!cp_state_valid(p,sizeof(p)));p[13]=64;
 cp_put32(p+8,0x7fc00000);assert(!cp_state_valid(p,sizeof(p)));cp_put_float(p+8,1.01f);assert(!cp_state_valid(p,sizeof(p)));
 cp_header(p,CP_VOLUME,9,1);cp_put16(p+8,233);cp_put16(p+10,1);cp_put16(p+12,64);cp_put_float(p+16,1.0f/64);assert(cp_command_valid(p,sizeof(p)));
 cp_put_float(p+16,1.0f/16);assert(!cp_command_valid(p,sizeof(p)));cp_put32(p+16,0x7fc00000);assert(!cp_command_valid(p,sizeof(p)));
 using namespace SceneSettings;
 auto settings=defaults();settings.version=3;auto previous=settings;for(auto &b:settings.bindings)for(auto &a:b.actions){a.setSound(false);a.setLedBrightness(0);}assert(migrateV3(settings)&&valid(settings));previous.version=7;assert(!std::memcmp(&previous,&settings,sizeof(settings)));
 settings=previous;settings.version=3;settings.bindings[0].count=4;previous=settings;assert(!migrateV3(settings)&&!std::memcmp(&previous,&settings,sizeof(settings)));
 settings=defaults();auto &a=settings.bindings[0].actions[0];a.type=kBluetoothVolume;a.led=kProportional;
 for(int value:{1,-1,16,-16,64,-64}){a.value=value;assert(valid(settings));}a.value=5;assert(!valid(settings));
 // Normalize only legacy volume direction values; every other byte survives.
 a.value=-64;auto normalized=settings;normalized.bindings[0].actions[0].value=-1;
 assert(normalizeBluetoothVolumes(settings)&&!std::memcmp(&normalized,&settings,sizeof(settings)));
 assert(!normalizeBluetoothVolumes(settings));
 a.value=16;normalized=settings;normalized.bindings[0].actions[0].value=1;
 assert(normalizeBluetoothVolumes(settings)&&!std::memcmp(&normalized,&settings,sizeof(settings)));
 auto bad=settings;bad.bindings[0].actions[0].value=5;const auto unchanged=bad;
 assert(!normalizeBluetoothVolumes(bad)&&!std::memcmp(&bad,&unchanged,sizeof(bad)));
 a.led=kInherit;a.type=kBluetoothMedia;a.value=0xcd;assert(valid(settings));a.value=0x52;assert(!valid(settings));a.type=kBluetoothKey;a.value=75;a.target=255;assert(valid(settings));a.value=256;assert(!valid(settings));
 settings=defaults();settings.version=4;previous=settings;for(auto &b:settings.bindings)for(auto &a:b.actions){a.setSound(false);a.setLedBrightness(0);}assert(migrateV4(settings)&&valid(settings));previous.version=7;assert(!std::memcmp(&previous,&settings,sizeof(settings)));
 for(int mask=1;mask<16;++mask)for(int modifier=-1;modifier<4;++modifier){const int input=comboInput(mask,modifier);assert(bool(comboMask(input))==bool((mask&(mask-1))&&(modifier<0||(mask&(1<<modifier)))));}
 settings.scenes[0].ringDisplay=kRingBluetoothProgress;assert(valid(settings));settings.scenes[0].ringDisplay=kRingDisplayCount;assert(!valid(settings));
 std::cout<<"PASS BLE packet bounds/session fields, media/volume validation, v3 preservation, Bluetooth actions and volume direction migration\n";
}
