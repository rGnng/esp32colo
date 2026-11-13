#include "chord_engine.h"
#include <cassert>
#include <vector>
#include <iostream>
using namespace SceneSettings;
struct Event {int input,gesture,repeat;};
struct Sim {
  ChordEngine engine;bool raw[kInputCount]={};uint32_t now=1000;std::vector<Event> events;
  Sim(uint16_t flags=1u<<3){engine.seed(raw,now);engine.configure(flags);}
  void step(uint32_t ms=0,int turns=0){now+=ms;engine.tick(raw,turns,now,[&](int i,int g,int n){events.push_back({i,g,n});});}
  void set(int key,bool value){raw[key]=value;step();}
  void press(int mask){for(int i=0;i<4;++i)if(mask&(1<<i))raw[i]=true;step();step(25);}
  void release(int mask){for(int i=0;i<4;++i)if(mask&(1<<i))raw[i]=false;step();step(25);}
  void tap(int mask){press(mask);step(40);release(mask);step(20);}
  void expect(int count,int input,int gesture){assert(int(events.size())==count);assert(events.back().input==input&&events.back().gesture==gesture);}
};
int main(){
  for(int count=1;count<=3;++count){Sim s;for(int i=0;i<count;++i)s.tap(3);s.step(300);s.expect(1,kComboFlag|3,count-1);}
  {Sim s;s.press(1);s.step(35);s.press(2);s.step(80);s.release(1);s.step(100);assert(s.events.empty());s.release(2);s.step(300);s.expect(1,kComboFlag|3,kSingle);}
  {Sim s;s.press(3);s.step(80);s.step(520);s.expect(1,kComboFlag|3,kHold);s.release(1);s.step(800);s.expect(1,kComboFlag|3,kHold);s.release(2);s.expect(2,kComboFlag|3,kHoldRelease);s.step(500);assert(s.events.size()==2);}
  {Sim s;s.press(3);s.step(80);s.release(1);s.step(1000);assert(s.events.empty());s.release(2);s.step(300);s.expect(1,kComboFlag|3,kSingle);}
  {Sim s;s.press(1);s.step(81);s.press(2);s.step(30);s.release(3);s.step(300);assert(s.events.size()==2);for(auto e:s.events)assert(e.input<4&&e.gesture==kSingle);}
  {Sim s;s.tap(1);s.tap(2);s.step(300);assert(s.events.size()==2);for(auto e:s.events)assert(e.input<4);}
  {Sim s((1u<<3)|(1u<<7));s.press(3);s.step(20);s.press(4);s.step(80);s.release(7);s.step(300);s.expect(1,kComboFlag|7,kSingle);}
  {Sim s;s.tap(7);s.step(300);assert(s.events.size()==3);for(auto e:s.events)assert(e.input<4);}
  {Sim s;s.press(1);s.step(20);s.release(1);s.press(2);s.step(80);s.release(2);s.step(300);assert(s.events.size()==2);for(auto e:s.events)assert(e.input<4);}
  {Sim s;s.press(3);s.step(80);s.engine.cancel();s.step(1000);s.release(3);s.step(500);assert(s.events.empty());s.tap(3);s.step(300);s.expect(1,kComboFlag|3,kSingle);}
  {Sim s;s.press(3);s.step(20);s.engine.cancel();s.step(1000);s.release(3);s.step(300);assert(s.events.empty());}
  {Sim s;s.raw[0]=s.raw[1]=true;s.engine.seed(s.raw,s.now);s.step(1000);s.release(3);s.step(300);assert(s.events.empty());}
  {Sim s;s.now=0xFFFFFF80;s.engine.seed(s.raw,s.now);s.tap(3);s.step(300);s.expect(1,kComboFlag|3,kSingle);}
  {Sim s;s.press(3);s.step(80);s.set(kKnob,true);s.step(10,-2);s.release(3);s.step(300);assert(s.events[0].input==kKnob&&s.events[0].gesture==kPressLeft&&s.events[0].repeat==2);assert(s.events.back().input==(kComboFlag|3));}
  {Sim s;s.press(3);s.step(80);s.press(4);s.release(4);s.release(3);s.step(300);assert(s.events.size()==2);}
  // A second chord starting inside the click window may finish joining after it.
  {Sim s;s.tap(3);s.step(240);s.press(1);s.step(20);s.press(2);s.step(80);s.release(3);s.step(300);s.expect(1,kComboFlag|3,kDouble);}
  {Sim s(0);s.tap(3);s.step(300);assert(s.events.size()==2);for(auto e:s.events)assert(e.input<4);}
  std::cout<<"PASS combo clicks/holds, staggered/exact groups, member suppression, fallback, cancellation, boot-held, rollover and knob independence\n";
}
