#include "navigation_feedback.h"
#include "tab5_navigation_profile.h"
#include "chord_engine.h"
#include <cassert>
#include <algorithm>
#include <iostream>
#include <string>
static void header(uint8_t *p,uint16_t revision,const std::string &text,uint8_t flags=CP_NAV_FOCUS){
 cp_header(p,CP_NAVIGATION,revision,123);p[8]=1;p[9]=flags;cp_put32(p+10,7);cp_put32(p+14,100);cp_put16(p+18,text.size());
}
static void text(NavigationFeedback &nav,uint16_t revision,const std::string &label,uint32_t now){
 for(unsigned offset=0;offset<label.size();offset+=10){uint8_t p[20];cp_header(p,CP_NAV_TEXT,revision,123);p[8]=offset;p[9]=std::min(size_t(10),label.size()-offset);std::memcpy(p+10,label.data()+offset,p[9]);assert(nav.accept(p,sizeof(p),now));}
}
int main(){
 NavigationFeedback nav;uint8_t p[20];const std::string label=u8"日本語のアルバム 한글 노래";
 header(p,1,label);assert(nav.accept(p,20,100));assert(!nav.snapshot(100,true).valid);
 text(nav,1,label,110);auto s=nav.snapshot(120,true);assert(s.fresh&&s.ui.index==7&&s.ui.count==100&&label==s.ui.focus);
 assert(!nav.snapshot(5110,true).fresh&&!nav.snapshot(120,false).fresh);
 header(p,1,label);assert(nav.accept(p,20,5200)&&nav.snapshot(5200,true).fresh);
 header(p,2,"Next");assert(nav.accept(p,20,5300));assert(std::string(nav.snapshot(5300,true).ui.focus)==label);
 header(p,1,label);assert(!nav.accept(p,20,5300));
 cp_header(p,CP_NAV_TEXT,1,123);p[9]=1;p[10]='x';assert(!nav.accept(p,20,5300));
 cp_header(p,CP_NAV_TEXT,2,123);p[8]=1;p[9]=1;assert(!nav.accept(p,20,5300));
 text(nav,2,"Next",5310);assert(std::string(nav.snapshot(5310,true).ui.focus)=="Next");
 header(p,3,"",CP_NAV_LOADING);assert(nav.accept(p,20,5400));assert(!nav.snapshot(5400,true).ui.focus[0]);
 header(p,4,std::string(96,'x'));assert(!nav.accept(p,20,5500));
 header(p,4,"x");p[8]=7;assert(!nav.accept(p,20,5500));p[8]=1;p[9]=128;assert(!nav.accept(p,20,5500));
 header(p,4,"x");cp_put32(p+10,100);assert(!nav.accept(p,20,5500));
 nav.reset();header(p,65535,"Last");assert(nav.accept(p,20,0));text(nav,65535,"Last",0);
 header(p,1,"Wrapped");assert(nav.accept(p,20,1));text(nav,1,"Wrapped",1);assert(nav.snapshot(2,true).revision==1);
 header(p,65535,"Stale");assert(!nav.accept(p,20,3));
 nav.reset();assert(!nav.snapshot(1,true).valid);
 using namespace SceneSettings;
 auto settings=defaults(),before=settings;settings.version=7;assert(migrateV7(settings)&&!std::memcmp(&settings,&before,sizeof(settings)));
 assert(Tab5NavigationProfile::install(settings,3)&&valid(settings));
 assert(settings.scenes[3].screen==kBluetoothNavigationScreen&&settings.active==before.active);
 for(int key:{kKey2,kKey3}){
  const auto *hold=effectiveBinding(settings,3,key,kHold);assert(hold&&hold->repeatHold()&&hold->holdMs()==350);
  assert(effectiveBinding(settings,3,key,kSingle));
 }
 assert(!Tab5NavigationProfile::install(settings,3)&&!Tab5NavigationProfile::install(settings,4));
 for(int i=0;i<kBindingCount;i++)if(before.bindings[i].count)assert(!std::memcmp(&before.bindings[i],&settings.bindings[i],sizeof(Binding)));
 auto invalid=before;invalid.version=7;invalid.scenes[0].screen=kBluetoothNavigationScreen;const auto copy=invalid;assert(!migrateV7(invalid)&&!std::memcmp(&invalid,&copy,sizeof(copy)));
 // The paging key retains immediate release behavior even beside global scene combinations.
 {
  ChordEngine engine;GestureBindings bindings;bool raw[kInputCount]={};int singles=0,holds=0;
  bindings.physical[kKey2]=(1u<<kSingle)|(1u<<kHold);bindings.setHold(kKey2,350,true);
  bindings.combos[7]=bindings.combos[11]=1u<<kSingle;
  engine.seed(raw,1000);engine.configure((1u<<7)|(1u<<11),bindings);
  auto tick=[&](uint32_t now){engine.tick(raw,0,now,[&](int input,int gesture,int){assert(input==kKey2);if(gesture==kSingle)++singles;else if(gesture==kHold)++holds;});};
  raw[kKey2]=true;tick(1000);tick(1025);tick(1105);tick(1374);assert(!holds&&!singles);
  tick(1375);tick(1575);assert(holds==2&&!singles);
  raw[kKey2]=false;tick(1775);tick(1800);assert(holds==2&&!singles);
  raw[kKey2]=true;tick(1900);tick(1925);raw[kKey2]=false;tick(1940);tick(1965);assert(singles==1&&holds==2);
  raw[kKey2]=true;tick(2100);tick(2125);tick(2475);assert(holds==3);
  engine.configure((1u<<7)|(1u<<11),bindings);tick(3000);assert(holds==3);
 }
 std::cout<<"PASS coherent navigation fragments, UTF-8, stale/partial frames, expiry, revision wrap, v7 migration and additive Tab5 profile\n";
}
