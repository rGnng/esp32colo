#include "serial_tx_buffer.h"
#include <cassert>
#include <deque>
#include <iostream>
int main(){
 SerialTxBuffer<8> queue;uint8_t bytes[20]={};
 const uint8_t first[]={1,2,3,4,5,6};assert(queue.append(first,6)==6);
 assert(queue.peek(bytes,3)==3&&bytes[0]==1&&bytes[2]==3);queue.consume(3);
 assert(queue.append(first,6)==5&&queue.dropped()==1&&queue.size()==8);
 assert(queue.peek(bytes,sizeof(bytes))==8&&bytes[0]==4&&bytes[3]==1&&bytes[7]==5);
 queue.consume(20);assert(!queue.size());queue.clear();
 // Model sustained host backpressure, wraparound and partial writes.
 std::deque<uint8_t> expected;uint32_t dropped=queue.dropped();
 for(unsigned i=0;i<50000;++i){
  if(i%3){const size_t size=i%7+1;for(size_t j=0;j<size;++j)bytes[j]=(i+j)%256;
   const size_t accepted=queue.append(bytes,size);dropped+=size-accepted;for(size_t j=0;j<accepted;++j)expected.push_back(bytes[j]);
  }else{
   const size_t count=queue.peek(bytes,i%6+1);assert(count<=expected.size());
   for(size_t j=0;j<count;++j)assert(bytes[j]==expected[j]);
   const size_t sent=count/2;queue.consume(sent);for(size_t j=0;j<sent;++j)expected.pop_front();
  }
  assert(queue.size()==expected.size()&&queue.dropped()==dropped);
 }
 queue.clear();assert(queue.size()==0&&queue.dropped()==dropped);
 std::cout<<"PASS bounded serial TX, unread host, wraparound, partial drains and overflow accounting\n";
}
