#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Lossless bounded LZ records for small NVS blobs. The in-memory scene ABI stays
// unchanged. Literal tags 0..127 carry 1..128 bytes; other tags carry a 3..130
// byte back-reference followed by a little-endian distance. Overlap is allowed.
namespace SceneRecordCodec {
constexpr size_t kHeader=12,kWindow=512;
inline size_t capacity(size_t bytes){return kHeader+bytes+(bytes+127)/128;}
inline uint16_t u16(const uint8_t *p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline uint32_t u32(const uint8_t *p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
inline void put16(uint8_t *p,uint16_t n){p[0]=uint8_t(n);p[1]=uint8_t(n>>8);}
inline void put32(uint8_t *p,uint32_t n){put16(p,uint16_t(n));put16(p+2,uint16_t(n>>16));}
inline uint32_t crc(const uint8_t *p,size_t size){
  uint32_t value=~uint32_t(0);
  for(size_t i=0;i<size;++i){value^=p[i];for(int bit=0;bit<8;++bit)value=(value>>1)^(0xedb88320u&uint32_t(-int32_t(value&1)));}
  return ~value;
}
inline bool encoded(const uint8_t *p,size_t size){return size>=kHeader&&!std::memcmp(p,"CSR1",4)&&u16(p+6)==1;}
inline size_t encode(const uint8_t *src,size_t size,uint8_t *out,size_t room){
  if(!src||!out||!size||size>65535||room<kHeader)return 0;
  size_t write=kHeader,literal=0,literalAt=0,read=0;
  auto flush=[&](){
    if(!literal)return true;
    if(write+1+literal>room)return false;
    out[write++]=uint8_t(literal-1);std::memcpy(out+write,src+literalAt,literal);write+=literal;literal=0;return true;
  };
  while(read<size){
    size_t best=0,distance=0,limit=size-read;if(limit>130)limit=130;
    const size_t window=read<kWindow?read:kWindow;
    if(limit>=4)for(size_t back=1;back<=window;++back){
      if(src[read]!=src[read-back]||src[read+1]!=src[read-back+1]||src[read+2]!=src[read-back+2])continue;
      size_t match=3;while(match<limit&&src[read+match]==src[read-back+match])++match;
      if(match>best){best=match;distance=back;if(best==limit)break;}
    }
    if(best>=4){
      if(!flush()||write+3>room)return 0;
      out[write++]=uint8_t(128|(best-3));put16(out+write,uint16_t(distance));write+=2;read+=best;
    }else{
      if(!literal)literalAt=read;
      ++literal;++read;if(literal==128&&!flush())return 0;
    }
  }
  if(!flush())return 0;
  std::memcpy(out,"CSR1",4);put16(out+4,uint16_t(size));put16(out+6,1);put32(out+8,crc(src,size));return write;
}
inline bool decode(const uint8_t *src,size_t size,uint8_t *out,size_t bytes){
  if(!src||!out||!encoded(src,size)||u16(src+4)!=bytes)return false;
  size_t read=kHeader,write=0;
  while(read<size&&write<bytes){
    const uint8_t tag=src[read++];
    if(tag<128){
      const size_t count=size_t(tag)+1;if(count>size-read||count>bytes-write)return false;
      std::memcpy(out+write,src+read,count);read+=count;write+=count;
    }else{
      const size_t count=(tag&127)+3;if(size-read<2||count>bytes-write)return false;
      const size_t back=u16(src+read);read+=2;if(!back||back>write)return false;
      for(size_t i=0;i<count;++i){out[write]=out[write-back];++write;}
    }
  }
  return write==bytes&&read==size&&crc(out,bytes)==u32(src+8);
}
}
