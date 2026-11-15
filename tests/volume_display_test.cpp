#include "volume_display.h"
#include <cassert>
#include <limits>
#include <iostream>

int main(){
 using namespace VolumeDisplay;
 const float nan=std::numeric_limits<float>::quiet_NaN();
 const float inf=std::numeric_limits<float>::infinity();
 assert(value(true,true,1.0f/64)==1.0f/64);
 assert(value(false,true,.5f)==-1&&value(true,false,.5f)==-1);
 assert(value(true,true,-1)==-1&&value(true,true,nan)==-1&&value(true,true,inf)==-1);
 assert(percent(1.0f/64)==2&&percent(-1)==-1&&percent(nan)==-1);
 assert(pixels(-1,180)==0&&pixels(nan,180)==0&&pixels(inf,180)==0);
 assert(pixels(0,180)==0&&pixels(1,180)==180&&pixels(.5f,180)==90);
 assert(pixels(.5f,0)==0&&pixels(.5f,-1)==0);
 // Every fine detent moves the bar, with exact returned-value coverage.
 for(int grid:{16,64}){
   int previous=-1;
   for(int step=0;step<=grid;++step){
     const float level=float(step)/grid;
     const int filled=pixels(value(true,true,level),180);
     assert(filled>previous&&filled>=0&&filled<=180);previous=filled;
   }
 }
 assert(pixels(1.0f/64,180)==3); // Not a rounded 2% (four-pixel) step.
 std::cout<<"PASS fractional volume bar, stale/unavailable values and complete fine/coarse grids\n";
}
