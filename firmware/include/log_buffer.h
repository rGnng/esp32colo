#pragma once
#include <cstddef>
#include <cstdint>

// Byte cursor with bounded storage; unsigned differences also handle wrap.
// Caller serializes access when used from several firmware tasks.
template<size_t Capacity> class LogBuffer {
 public:
  struct Read { uint32_t next,total; size_t size; bool reset; };
  void append(const char *data,size_t length) {
    for(size_t i=0;i<length;++i) { bytes_[write_]=data[i];write_=(write_+1)%Capacity;if(size_<Capacity)++size_;++total_; }
  }
  Read read(uint32_t cursor,bool haveCursor,char *out,size_t limit) const {
    const uint32_t distance=total_-cursor;
    const bool reset=haveCursor&&distance>size_;
    const size_t available=haveCursor&&!reset?distance:size_;
    const size_t take=available<limit?available:limit;
    const uint32_t start=total_-available;
    const size_t at=(write_+Capacity-available)%Capacity;
    for(size_t i=0;i<take;++i)out[i]=bytes_[(at+i)%Capacity];
    return {start+static_cast<uint32_t>(take),total_,take,reset};
  }
 private:
  static_assert(Capacity>0,"empty log buffer");
  char bytes_[Capacity] = {};
  size_t write_=0,size_=0;
  uint32_t total_=0;
};
