#include "playback_ring.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace PlaybackRing;
 assert(progress(true,1,11000,100000)==110);
 assert(progress(true,2,10000,100000)==100);
 assert(progress(true,0,10000,100000)==0);
 assert(progress(false,1,10000,100000)==-1);
 assert(progress(true,255,10000,100000)==-1);
 assert(progress(true,1,10000,0)==-1);
 assert(progress(true,1,100001,100000)==1000);
 assert(coverage(0,0,9)==0&&coverage(1000,8,9)==1000);
 assert(coverage(500,3,9)==1000&&coverage(500,4,9)==500&&coverage(500,5,9)==0);
 std::cout<<"PASS playback ring shared-position consumption, unavailable/stopped bounds and partial pixel\n";
}
