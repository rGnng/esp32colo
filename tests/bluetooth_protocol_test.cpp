#include "coloplay_ble_protocol.h"
#include "scene_settings.h"
#include "volume_grid.h"
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
 using namespace SceneSettings;
 auto settings=defaults();settings.version=3;auto previous=settings;for(auto &b:settings.bindings)for(auto &a:b.actions)a.setSound(false);assert(migrateV3(settings)&&valid(settings));previous.version=6;assert(!std::memcmp(&previous,&settings,sizeof(settings)));
 settings=previous;settings.version=3;settings.bindings[0].count=4;previous=settings;assert(!migrateV3(settings)&&!std::memcmp(&previous,&settings,sizeof(settings)));
 settings=defaults();auto &a=settings.bindings[0].actions[0];a.type=kBluetoothVolume;a.led=kProportional;
 for(int value:{1,-1,16,-16,64,-64}){a.value=value;assert(valid(settings));}a.value=5;assert(!valid(settings));
 a.led=kInherit;a.type=kBluetoothMedia;a.value=0xcd;assert(valid(settings));a.value=0x52;assert(!valid(settings));a.type=kBluetoothKey;a.value=75;a.target=255;assert(valid(settings));a.value=256;assert(!valid(settings));
 // Shared Headunit rounding: no second, competing Bluetooth volume algorithm.
 assert(volume_grid_step(67,1,64)==69&&volume_grid_step(67,-1,64)==66&&volume_grid_step(67,1,16)==75);
 for(int grid:{16,64})for(int value=0;value<=100;++value){assert(volume_grid_step(value,200,grid)==100);assert(volume_grid_step(value,-200,grid)==0);}
 settings=defaults();settings.version=4;previous=settings;for(auto &b:settings.bindings)for(auto &a:b.actions)a.setSound(false);assert(migrateV4(settings)&&valid(settings));previous.version=6;assert(!std::memcmp(&previous,&settings,sizeof(settings)));
 for(int mask=1;mask<16;++mask)for(int modifier=-1;modifier<4;++modifier){const int input=comboInput(mask,modifier);assert(bool(comboMask(input))==bool((mask&(mask-1))&&(modifier<0||(mask&(1<<modifier)))));}
 settings.scenes[0].ringDisplay=kRingBluetoothProgress;assert(valid(settings));settings.scenes[0].ringDisplay=kRingDisplayCount;assert(!valid(settings));
 std::cout<<"PASS BLE packet bounds/session fields, media/volume validation, v3 preservation, Bluetooth actions and shared volume grids\n";
}
