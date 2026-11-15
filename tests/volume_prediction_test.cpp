#include "volume_prediction.h"
#include <cassert>
#include <cmath>
#include <iostream>
static float curve(int i){return (i/64.0f)*(i/64.0f);}
static bool near(float a,float b){return std::fabs(a-b)<.000001f;}
int main(){
 VolumePrediction p;
 assert(!p.ready()&&p.queued(1,1,0)&&p.pending()==0);
 p.configure(true,64);assert(!p.ready());
 assert(p.observe(.253953f,.25f,64,0,0,CP_PENDING,10));
 assert(p.ready()&&near(p.display(10,.253953f),.5f));
 assert(p.queued(1,1,20)&&near(p.display(20,.253953f),33.0f/64));
 assert(p.queued(2,1,30)&&near(p.display(30,.253953f),34.0f/64));
 // Accepted and periodic snapshots do not retire unprocessed turns.
 p.observe(.253953f,.25f,64,0,2,CP_ACCEPTED,40);
 assert(p.pending()==2&&near(p.predicted(),curve(34)));
 // First completion leaves the second turn in the projection.
 p.observe(.261951f,curve(33),64,1,1,CP_APPLIED,50);
 assert(p.pending()==1&&near(p.display(50,.261951f),34.0f/64));
 p.observe(.278319f,curve(34),64,2,2,CP_APPLIED,60);
 assert(!p.pending()&&near(p.display(61,.278319f),34.0f/64));
 assert(!p.observe(.261951f,curve(33),64,1,1,CP_APPLIED,70));
 assert(p.completed()==2&&near(p.predicted(),curve(34)));
 // Quantized readback returns to the same selected curve position: no bounce.
 assert(near(p.display(180,.278319f),34.0f/64));
 assert(near(p.display(230,.278319f),34.0f/64));
 assert(near(p.display(280,.278319f),34.0f/64)&&!p.active(280));
 // Fast reversals reuse the host's grid reference rather than rounded readback.
 assert(p.queued(3,-1,300)&&near(p.predicted(),curve(33)));
 assert(p.queued(4,1,301)&&near(p.predicted(),curve(34)));
 p.observe(.278319f,curve(34),64,4,3,CP_APPLIED,310);
 assert(!p.pending()); // Snapshot watermark can be newer than its ACK envelope.
 // External changes supply an unanchored base; queued intent is replayed.
 p.observe(.3f,-1,64,4,0,CP_PENDING,600);
 p.queued(5,-1,601);assert(near(p.predicted(),curve(34)));
 p.configure(true,16);assert(!p.ready());
 p.observe(.3f,-1,16,4,0,CP_PENDING,602);
 assert(near(p.predicted(),.25f));
 p.observe(.25f,.25f,16,5,5,CP_APPLIED,610);
 // Rejected commands and stale error snapshots clear optimism, never real state.
 p.queued(6,1,700);p.queued(7,1,701);
 p.observe(.25f,.25f,16,5,7,CP_BUSY,702);
 assert(p.pending()==1&&!p.ready()&&!p.active(703));
 p.observe(.3125f,.3125f,16,6,6,CP_APPLIED,710);
 assert(p.ready()&&!p.pending()&&!p.active(711));
 p.queued(8,1,800);assert(!p.expire(5800)&&p.expire(5801)&&!p.ready());
 p.observe(.4f,-1,16,8,8,CP_FAILED,5802);
 p.queued(9,1,5803);p.observe(-1,-1,16,9,9,CP_UNSUPPORTED,5804);
 assert(!p.ready()&&!p.pending());
 p.reset();p.configure(true,64);p.observe(0,0,64,0,0,CP_PENDING,0);
 for(unsigned i=1;i<=VolumePrediction::kCapacity;++i)assert(p.queued(i,1,i));
 assert(!p.canQueue()&&!p.queued(65,1,70)&&near(p.predicted(),1));
 // Half-range-safe IDs and elapsed-time arithmetic across counter wrap.
 p.reset();p.configure(true,64);p.observe(.25f,.25f,64,65534,0,CP_PENDING,0xfffffff0u);
 p.queued(65535,1,0xfffffff1u);p.queued(1,-1,0xfffffff2u);
 p.observe(.25f,.25f,64,1,1,CP_APPLIED,10);
 assert(!p.pending()&&p.completed()==1&&!p.observe(.6f,-1,64,65535,65535,CP_APPLIED,11));
 assert(near(p.display(300,.25f),.5f));
 // Many non-volume commands can separate volume IDs by more than half-range.
 p.reset();p.configure(true,64);p.observe(.25f,.25f,64,10,0,CP_PENDING,0);
 p.queued(40000,1,10);p.observe(.261951f,curve(33),64,40000,40000,CP_APPLIED,20);
 assert(p.completed()==40000&&!p.pending());
 assert(!p.observe(.25f,.25f,64,10,10,CP_APPLIED,21));
 assert(!p.queued(40000,1,30)); // Sender skips an ID equal to the old watermark.
 p.configure(false,16);assert(!p.ready()&&!p.pending()&&p.steps()==0);
 for(int mode:{2,5}){
   p.reset();p.configure(true,mode);assert(p.observe(.61f,-1,mode,0,0,CP_PENDING,0));
   assert(p.queued(1,1,1));const float target=mode==2?.62f:.65f;
   assert(near(p.predicted(),target)&&near(p.display(1,.61f),target));
   p.observe(target,target,mode,1,1,CP_APPLIED,20);
   assert(near(p.display(300,target),target));
 }
 std::cout<<"PASS host-synchronized prediction, pending replay, stale/accepted replies, reversals, grid changes, settling, failures, bounds and wrap\n";
}
