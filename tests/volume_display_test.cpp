#include "volume_display.h"
#include "volume_grid.h"
#include <cassert>
#include <limits>
#include <iostream>

int main(){
 using namespace VolumeDisplay;
 const float nan=std::numeric_limits<float>::quiet_NaN();
 const float inf=std::numeric_limits<float>::infinity();
 assert(value(true,true,false,1.0f/64,-1)==1.0f/64);
 assert(value(true,true,true,1.0f/64,2.0f/64)==2.0f/64);
 assert(value(true,true,false,1.0f/64,2.0f/64)==1.0f/64);
 assert(value(true,true,true,.5f,-1)==.5f);
 assert(value(false,true,true,.5f,.75f)==-1);
 assert(value(true,false,true,.5f,.75f)==-1);
 assert(value(true,true,false,-1,-1)==-1);
 assert(value(true,true,false,nan,-1)==-1);
 assert(value(true,true,false,inf,-1)==-1);
 assert(value(true,true,true,.5f,nan)==.5f);
 assert(pixels(-1,180)==0&&pixels(nan,180)==0&&pixels(inf,180)==0);
 assert(pixels(0,180)==0&&pixels(1,180)==180&&pixels(.5f,180)==90);
 assert(pixels(.5f,0)==0&&pixels(.5f,-1)==0);
 // Every fine detent moves the bar, with exact preview/confirmed agreement.
 for(int grid:{16,64}){
   int previous=-1;
   for(int step=0;step<=grid;++step){
     const float level=float(step)/grid;
     const int filled=pixels(value(true,true,false,level,-1),180);
     assert(filled>previous&&filled>=0&&filled<=180);previous=filled;
     if(step<grid){
       const float preview=volume_grid_step(level,1,grid);
       assert(pixels(value(true,true,true,level,preview),180)==pixels(preview,180));
     }
   }
 }
 assert(pixels(1.0f/64,180)==3); // Not a rounded 2% (four-pixel) step.
 std::cout<<"PASS fractional volume bar, preview reconciliation, stale/unavailable values and complete fine/coarse grids\n";
}
