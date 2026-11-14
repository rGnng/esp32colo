#include "playback_clock.h"
#include "playback_ring.h"
#include <cassert>
#include <iostream>

static void sample(PlaybackClock &clock,uint32_t elapsed,uint32_t now,int playback=1,int seek=0,bool available=true,uint32_t duration=100000){clock.update(available,playback,seek,elapsed,duration,now);}
int main(){
  // Reproduce MPD at 1 Hz / BLE at 2 Hz, with extra ACKs at arbitrary times.
  // Both outputs must move forward even on a duplicate old sample.
  {PlaybackClock c;sample(c,10000,0);uint32_t previous=10000;int ring=100;
   for(uint32_t t=0;t<20000;t+=25){
    if(t%500==0||t%175==0)sample(c,10000+(t/1000)*1000,t);
    const uint32_t current=c.position(t);assert(current>=previous&&current-previous<=30);
    const int next=PlaybackRing::progress(c.valid(t),1,current,100000);assert(next>=ring);ring=next;previous=current;
    if(t==500)assert(current==10500);
   }}
  // New samples ease +/- errors without any jump at receipt or backward motion.
  for(int offset:{-400,400}){PlaybackClock c;sample(c,10000,0);const auto before=c.position(1000);sample(c,11000+offset,1000);assert(c.position(1000)==before);
   uint32_t previous=before;for(uint32_t t=1025;t<=2500;t+=25){const auto current=c.position(t);assert(current>=previous&&current-previous<=29);previous=current;}
   assert(offset<0?c.position(1500)<11500:c.position(1500)>11500);}
  // Delayed/older cached samples do not roll back or repeatedly restart the clock.
  {PlaybackClock c;sample(c,10000,0);sample(c,11000,1000);sample(c,10900,1200);assert(c.position(1200)==11200);sample(c,11000,1500);assert(c.position(1500)==11500);sample(c,12000,2200);assert(c.position(2200)==12200);}
  // Uneven delivery intervals and source rounding must remain continuous.
  {PlaybackClock c;sample(c,10000,0);uint32_t previous=10000;
   const uint32_t arrivals[]={475,1010,1535,2110,2500,3210,3700,4500};unsigned index=0;
   for(uint32_t t=25;t<4900;t+=25){while(index<8&&arrivals[index]<=t){const uint32_t raw=10000+(arrivals[index]/1000)*1000;const auto at=c.position(t);sample(c,raw,t);assert(c.position(t)==at);++index;}const auto pos=c.position(t);assert(pos>=previous);previous=pos;}}
  // Pause freezes, resume reanchors; seeking is always authoritative.
  {PlaybackClock c;sample(c,10000,0);sample(c,10800,1000,2);assert(c.position(2000)==10800);sample(c,10800,3000,2);assert(c.position(4000)==10800);sample(c,10800,4500);assert(c.position(5000)==11300);
   sample(c,16000,5100,1,1);assert(c.position(5500)==16000);sample(c,21000,5600,1,1);assert(c.position(5800)==21000);sample(c,21000,5900);assert(c.position(6000)==21100);
   sample(c,15000,6100,1,2);assert(c.position(6400)==15000);sample(c,15000,6500);assert(c.position(6700)==15200);}
  // External seeks / repeat of the same track also snap in either direction.
  {PlaybackClock c;sample(c,30000,0);sample(c,10000,500);assert(c.position(500)==10000);sample(c,60000,1000);assert(c.position(1000)==60000);sample(c,0,1500);assert(!c.position(1500));}
  // Metadata refresh on the same track preserves timing; a new track resets it.
  {PlaybackClock c;c.track(1);sample(c,80000,0);c.track(1);assert(c.position(500)==80500);c.track(2);assert(!c.valid(500)&&!c.position(500));sample(c,500,600);assert(c.valid(600)&&c.position(600)==500);c.reset();assert(!c.valid(700)&&!c.position(700));}
  // Bound extrapolation during stalled backend reports, even with live heartbeats.
  {PlaybackClock c;sample(c,10000,0);for(uint32_t t=500;t<=7000;t+=500)sample(c,10000,t);assert(c.valid(7000)&&c.position(7000)==13000);sample(c,10100,7500);assert(c.position(7500)==10100);}
  // Freshness, reconnect, missing backend/mixer-independent state, and zero duration.
  {PlaybackClock c;sample(c,10000,0);assert(c.valid(4999)&&!c.valid(5000));sample(c,30000,6000);assert(c.position(6000)==30000);sample(c,30000,6100,1,0,false);assert(!c.valid(6100));sample(c,40000,6200);assert(c.position(6200)==40000);sample(c,0,6300,0);assert(!c.position(7000));sample(c,0,7100,1,0,true,0);assert(!c.valid(7100));}
  // End clamping and millis() rollover preserve all of the above invariants.
  {PlaybackClock c;sample(c,99999,0);assert(c.position(1000)==100000);sample(c,1000,0xfffffff0);assert(c.position(16)==1032);sample(c,1000,32);assert(c.position(32)==1048);sample(c,1500,500);assert(c.position(500)==1516);}
  std::cout<<"PASS shared playback clock: duplicate/ACK samples, gradual corrections, delayed delivery, seeks, pause, tracks, stalls, freshness and rollover\n";
}
