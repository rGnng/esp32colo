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
 // Physical top is slot 0: bottom pairs 4/5, 3/6, 2/7, 1/8, then top 0.
 const int half[]={0,0,250,1000,1000,1000,1000,250,0};
 for(int i=0;i<9;++i)assert(coverage(500,i,9,RingFill::kBottomToTop)==half[i]);
 for(int p=0;p<=1000;++p){
  int sum=0;
  for(int i=0;i<9;++i){
   const int current=coverage(p,i,9,RingFill::kBottomToTop);
   assert(current==coverage(p,(9-i)%9,9,RingFill::kBottomToTop));
   assert(current>=0&&current<=1000);
   if(p)assert(current>=coverage(p-1,i,9,RingFill::kBottomToTop));
   if(!p)assert(current==0);
   if(p==1000)assert(current==1000);
   sum+=current;
  }
  assert(sum<=p*9&&sum>=p*9-1);
 }
 assert(coverage(100,4,9,RingFill::kBottomToTop)==450&&coverage(100,0,9,RingFill::kBottomToTop)==0);
 assert(coverage(900,0,9,RingFill::kBottomToTop)==100);
 assert(coverage(-1,0,9,RingFill::kBottomToTop)==0&&coverage(2000,0,9,RingFill::kBottomToTop)==1000);
 // Rotated origin, even ring, invalid geometry and unchanged clockwise fill.
 for(int i=0;i<9;++i)assert(coverage(500,(i+2)%9,9,RingFill::kBottomToTop,2)==half[i]);
 assert(coverage(125,4,8,RingFill::kBottomToTop)==1000);
 assert(coverage(1,-1,9)==0&&coverage(1,9,9)==0&&coverage(1,0,0)==0);
 for(int p=0;p<=1000;p+=10)for(int i=0;i<9;++i){int expected=p*9-i*1000;if(expected<0)expected=0;if(expected>1000)expected=1000;assert(coverage(p,i,9)==expected);}
 std::cout<<"PASS playback ring shared-position consumption, unavailable/stopped bounds and partial pixel\n";
}
