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
  {Sim s;s.press(3);s.step(80);s.step(520);s.expect(1,kComboFlag|3,kHold);s.release(1);s.step(800);s.expect(2,kComboFlag|3,kHoldRelease);s.release(2);s.expect(2,kComboFlag|3,kHoldRelease);s.step(500);assert(s.events.size()==2);}
  {Sim s;s.press(3);s.step(80);s.release(1);s.step(1000);s.expect(1,kComboFlag|3,kSingle);s.release(2);s.step(300);s.expect(1,kComboFlag|3,kSingle);}
  {Sim s;s.press(1);s.step(81);s.press(2);s.step(30);s.release(3);s.step(300);s.expect(1,kComboFlag|3,kSingle);}
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
  {Sim s;s.tap(3);s.step(210);s.press(1);s.step(20);s.press(2);s.step(80);s.release(3);s.step(300);s.expect(1,kComboFlag|3,kDouble);}
  {Sim s(0);s.tap(3);s.step(300);assert(s.events.size()==2);for(auto e:s.events)assert(e.input<4);}
  // No multi-click or hold binding: trigger immediately after debounce.
  {Sim s;GestureBindings bindings;bindings.combos[3]=1u<<kSingle;s.engine.configure(1u<<3,bindings);s.press(3);s.expect(1,kComboFlag|3,kSingle);s.step(1000);s.release(3);s.step(300);assert(s.events.size()==1);}
  // Key 4 can stay held while Key 2/3 are pressed repeatedly, including across reconfiguration.
  {Sim s;GestureBindings bindings;bindings.physical[kKey4]=1u<<kSingle;bindings.combos[10]=bindings.combos[12]=1u<<kSingle;const uint16_t flags=(1u<<10)|(1u<<12);s.engine.configure(flags,bindings);
   s.press(8);s.step(1200);assert(s.events.empty());s.press(2);s.expect(1,kComboFlag|10,kSingle);
   s.engine.configure(flags,bindings);s.release(2);s.press(4);s.expect(2,kComboFlag|12,kSingle);
   s.release(4);s.press(2);s.expect(3,kComboFlag|10,kSingle);s.release(2);s.release(8);s.step(1000);assert(s.events.size()==3);}
  // Unused potential modifiers keep an optional single-tap action on release.
  {Sim s;GestureBindings b;b.physical[kKey4]=1u<<kSingle;b.combos[10]=1u<<kSingle;s.engine.configure(1u<<10,b);s.press(8);s.step(1000);assert(s.events.empty());s.release(8);s.expect(1,kKey4,kSingle);}
  // Combo Hold is tracked only when bound; a short click has no 300 ms delay.
  {Sim s;GestureBindings b;b.combos[3]=(1u<<kSingle)|(1u<<kHold);s.engine.configure(1u<<3,b);s.press(3);s.step(40);assert(s.events.empty());s.release(1);s.expect(1,kComboFlag|3,kSingle);s.release(2);}
  {Sim s;GestureBindings b;b.combos[3]=(1u<<kSingle)|(1u<<kHold)|(1u<<kHoldRelease);s.engine.configure(1u<<3,b);s.press(3);s.step(600);s.expect(1,kComboFlag|3,kHold);s.release(1);s.expect(2,kComboFlag|3,kHoldRelease);s.release(2);assert(s.events.size()==2);}
  // With Double but no Triple, the second release resolves immediately.
  {Sim s;GestureBindings b;b.combos[3]=(1u<<kSingle)|(1u<<kDouble);s.engine.configure(1u<<3,b);s.tap(3);assert(s.events.empty());s.tap(3);s.expect(1,kComboFlag|3,kDouble);}
  // Hold-release alone tracks the threshold without emitting Hold or short clicks.
  {Sim s;GestureBindings b;b.combos[3]=1u<<kHoldRelease;s.engine.configure(1u<<3,b);s.tap(3);s.step(300);assert(s.events.empty());s.press(3);s.step(600);assert(s.events.empty());s.release(1);s.expect(1,kComboFlag|3,kHoldRelease);s.release(2);assert(s.events.size()==1);}
  // Preserve the assembly window only when a larger configured group could win.
  {Sim s;GestureBindings b;b.combos[3]=b.combos[7]=1u<<kSingle;s.engine.configure((1u<<3)|(1u<<7),b);s.press(3);s.step(20);assert(s.events.empty());s.press(4);s.expect(1,kComboFlag|7,kSingle);s.release(7);assert(s.events.size()==1);}
  {Sim s;GestureBindings b;b.combos[3]=b.combos[7]=1u<<kSingle;s.engine.configure((1u<<3)|(1u<<7),b);s.press(3);s.step(79);assert(s.events.empty());s.step(1);s.expect(1,kComboFlag|3,kSingle);s.release(3);assert(s.events.size()==1);}
  // Strict modifier-first pairs share a mask but have independent identities.
  {Sim s;GestureBindings b;b.ordered[kKey4][10]=b.ordered[kKey2][10]=1u<<kSingle;s.engine.configure(1u<<10,b);
   s.press(8);s.step(1000);s.press(2);s.expect(1,comboInput(10,kKey4),kSingle);
   s.release(2);s.press(2);s.expect(2,comboInput(10,kKey4),kSingle);s.release(10);
   s.press(2);s.step(1000);s.press(8);s.expect(3,comboInput(10,kKey2),kSingle);s.release(10);s.step(1000);assert(s.events.size()==3);}
  // Same-tick presses are not ordered. Wrong-order presses cannot claim a binding.
  {Sim s;GestureBindings b;b.ordered[kKey4][10]=1u<<kSingle;s.engine.configure(1u<<10,b);
   s.tap(10);s.step(1000);assert(s.events.empty());s.press(2);s.press(8);s.step(100);s.release(10);s.step(1000);assert(s.events.empty());}
  // Ordered pairs take priority over an unordered fallback; a true tie uses fallback.
  {Sim s;GestureBindings b;b.combos[10]=b.ordered[kKey4][10]=1u<<kSingle;s.engine.configure(1u<<10,b);
   s.press(8);s.press(2);s.expect(1,comboInput(10,kKey4),kSingle);s.release(10);s.press(10);s.expect(2,comboInput(10),kSingle);s.release(10);}
  // Reuse modifier 4 across targets, then use still-held 2 as the opposite modifier.
  {Sim s;GestureBindings b;b.physical[kKey1]=1u<<kSingle;b.ordered[kKey4][10]=b.ordered[kKey4][12]=b.ordered[kKey2][3]=b.ordered[kKey2][10]=1u<<kSingle;
   s.engine.configure((1u<<10)|(1u<<12)|(1u<<3),b);s.press(8);s.press(2);s.expect(1,comboInput(10,kKey4),kSingle);
   s.release(2);s.press(4);s.expect(2,comboInput(12,kKey4),kSingle);s.release(4);s.press(2);s.expect(3,comboInput(10,kKey4),kSingle);
   s.release(8);s.press(8);s.expect(4,comboInput(10,kKey2),kSingle);s.release(8);s.press(1);s.expect(5,comboInput(3,kKey2),kSingle);s.release(3);s.step(1000);assert(s.events.size()==5);}
  {Sim s;GestureBindings b;b.ordered[kKey4][10]=(1u<<kHold)|(1u<<kHoldRelease);s.engine.configure(1u<<10,b);
   s.press(8);s.step(2000);s.press(2);s.step(600);s.expect(1,comboInput(10,kKey4),kHold);s.release(2);s.expect(2,comboInput(10,kKey4),kHoldRelease);s.release(8);}
  {Sim s;GestureBindings b;b.ordered[kKey4][10]=1u<<kSingle;s.now=0xfffffff0;s.engine.seed(s.raw,s.now);s.engine.configure(1u<<10,b);s.press(8);s.press(2);s.expect(1,comboInput(10,kKey4),kSingle);}
  // Bounce on an already-held modifier must not change its press order.
  {Sim s;GestureBindings b;b.ordered[kKey4][10]=1u<<kSingle;s.engine.configure(1u<<10,b);
   s.press(8);s.raw[1]=true;s.step();s.step(1);s.raw[3]=false;s.step();s.step(2);s.raw[3]=true;s.step();s.step(25);s.expect(1,comboInput(10,kKey4),kSingle);}
  // Raw edges one millisecond apart remain ordered even when both debounce together.
  {Sim s;GestureBindings b;b.ordered[kKey4][10]=1u<<kSingle;s.engine.configure(1u<<10,b);
   s.raw[3]=true;s.step();s.step(1);s.raw[1]=true;s.step();s.step(25);s.expect(1,comboInput(10,kKey4),kSingle);}
  std::cout<<"PASS combo clicks/holds, staggered/exact groups, member suppression, fallback, cancellation, boot-held, rollover and knob independence\n";
}
