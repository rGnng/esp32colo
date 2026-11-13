#include "gesture_engine.h"
#include <vector>
#include <cassert>
#include <iostream>
using namespace SceneSettings;
struct Event {int input,gesture,repeat;};
struct Simulation {
 GestureEngine engine; bool raw[kInputCount]={}; uint32_t now=1000; std::vector<Event> events;
 Simulation() {engine.seed(raw,now);}
 void step(uint32_t ms=0,int turns=0){now+=ms;engine.tick(raw,turns,now,[&](int i,int g,int n){events.push_back({i,g,n});});}
 void set(int input,bool down){raw[input]=down;step();}
 void tap(int i){set(i,true);step(25);step(40);set(i,false);step(25);step(20);}
};
int main(){
 for(int clicks=1;clicks<=3;++clicks){Simulation s;for(int n=0;n<clicks;++n)s.tap(kKey1);s.step(300);assert(s.events.size()==1&&s.events[0].gesture==clicks-1);}
 {Simulation s;s.set(kKey1,true);s.step(25);s.step(600);assert(s.events.size()==1&&s.events[0].gesture==kHold);s.step(2000);assert(s.events.size()==1);s.set(kKey1,false);s.step(25);s.step(400);assert(s.events.size()==2&&s.events[1].gesture==kHoldRelease);}
 {Simulation s;s.set(kKnob,true);s.step(25);s.step(10,-2);assert(s.events.size()==1&&s.events[0].gesture==kPressLeft&&s.events[0].repeat==2);s.step(700);s.set(kKnob,false);s.step(25);s.step(400);assert(s.events.size()==1);s.step(0,3);assert(s.events.back().gesture==kRight&&s.events.back().repeat==3);}
 {Simulation s;s.tap(kKey1);s.tap(kKey2);s.step(300);assert(s.events.size()==2&&s.events[0].input==kKey1&&s.events[1].input==kKey2);}
 {Simulation s;s.set(kKey1,true);s.step(10);s.set(kKey1,false);s.step(10);s.set(kKey1,true);s.step(25);s.step(40);s.set(kKey1,false);s.step(25);s.step(300);assert(s.events.size()==1&&s.events[0].gesture==kSingle);}
 {Simulation s;s.tap(kKey1);s.engine.cancel();s.step(400);assert(s.events.empty());s.set(kKnob,true);s.step(25);s.engine.cancel();s.step(1000,1);s.set(kKnob,false);s.step(25);s.step(400);assert(s.events.empty());}
 {Simulation s;s.now=0xFFFFFF80;s.engine.seed(s.raw,s.now);s.tap(kKey1);s.step(300);assert(s.events.size()==1&&s.events[0].gesture==kSingle);}
 {Simulation s;s.raw[kKnob]=true;s.engine.seed(s.raw,s.now);s.step(1000);s.set(kKnob,false);s.step(25);s.step(400);assert(s.events.empty());}
 {Simulation s;s.set(kKey1,true);s.step(25);s.now+=900;s.set(kKey1,false);s.step(25);assert(s.events.size()==2&&s.events[0].gesture==kHold&&s.events[1].gesture==kHoldRelease);}
 {Simulation s;s.set(kKnob,true);s.step(10,1);s.step(15);s.step(700);s.set(kKnob,false);s.step(25);s.step(400);assert(s.events.size()==1&&s.events[0].gesture==kPressRight);}
 {Simulation s;s.set(kKey1,true);s.step(10);s.engine.cancel();s.step(25);s.step(700);s.set(kKey1,false);s.step(25);s.step(400);assert(s.events.empty());}
 std::cout<<"PASS single/double/triple, hold/release, modified turns, click suppression, independent inputs, debounce, scene cancellation and timer wrap\n";
}
