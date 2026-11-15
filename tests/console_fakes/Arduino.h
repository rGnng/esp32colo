#pragma once
#include <cstdint>
#include <cstddef>
#include <deque>
#include <vector>
#include <algorithm>
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void fakeLock(int *){}
#define portENTER_CRITICAL(p) fakeLock(p)
#define portEXIT_CRITICAL(p) fakeLock(p)
class Stream {
 public:
 virtual ~Stream()=default;
 virtual int available()=0;
 virtual int read()=0;
 virtual int peek()=0;
 virtual void flush()=0;
 virtual size_t write(uint8_t)=0;
 virtual size_t write(const uint8_t *,size_t)=0;
};
struct FakeSerial:Stream {
 bool connected=true;size_t space=16;std::deque<uint8_t> rx;std::vector<uint8_t> tx;
 int available()override{return rx.size();}
 int read()override{if(rx.empty())return -1;const int c=rx.front();rx.pop_front();return c;}
 int peek()override{return rx.empty()?-1:rx.front();}
 void flush()override{}
 size_t write(uint8_t c)override{return write(&c,1);}
 size_t write(const uint8_t *p,size_t n)override{n=std::min(n,space);tx.insert(tx.end(),p,p+n);return n;}
 int availableForWrite(){return space;}
 explicit operator bool()const{return connected;}
};
extern FakeSerial Serial;
