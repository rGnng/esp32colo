#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>
// Caller supplies synchronization. Backpressure drops new bytes, never blocks.
template<size_t Capacity> class SerialTxBuffer {
 public:
 size_t append(const uint8_t *data,size_t size){
   const size_t accepted=std::min(size,Capacity-used_);
   for(size_t i=0;i<accepted;++i)bytes_[(head_+used_+i)%Capacity]=data[i];
   used_+=accepted;dropped_+=size-accepted;return accepted;
 }
 size_t peek(uint8_t *out,size_t size)const{
   const size_t count=std::min(size,used_);
   for(size_t i=0;i<count;++i)out[i]=bytes_[(head_+i)%Capacity];
   return count;
 }
 void consume(size_t size){size=std::min(size,used_);head_=(head_+size)%Capacity;used_-=size;}
 void clear(){head_=used_=0;}
 size_t size()const{return used_;}
 uint32_t dropped()const{return dropped_;}
 private:
 uint8_t bytes_[Capacity]={};size_t head_=0,used_=0;uint32_t dropped_=0;
};
