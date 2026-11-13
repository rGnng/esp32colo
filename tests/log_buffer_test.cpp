#include "log_buffer.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
  LogBuffer<8> log;char out[8];
  auto read=log.read(0,false,out,8);assert(read.size==0&&read.total==0);
  log.append("abcdef",6);read=log.read(0,false,out,3);
  assert(read.next==3&&read.total==6&&read.size==3&&!std::memcmp(out,"abc",3));
  read=log.read(read.next,true,out,8);assert(read.next==6&&read.size==3&&!std::memcmp(out,"def",3));
  read=log.read(read.next,true,out,8);assert(read.size==0&&!read.reset);
  log.append("ghijkl",6);read=log.read(0,true,out,8);
  assert(read.reset&&read.size==8&&read.next==12&&!std::memcmp(out,"efghijkl",8));
  read=log.read(9,true,out,8);assert(!read.reset&&read.size==3&&!std::memcmp(out,"jkl",3));
  read=log.read(999,true,out,8);assert(read.reset&&read.size==8);
  log.append("0123456789abcdef",16);read=log.read(0,false,out,8);
  assert(!read.reset&&read.size==8&&!std::memcmp(out,"89abcdef",8));
  std::cout<<"PASS bounded log, chunk cursors, overwrite/reset and stale cursors\n";
}
