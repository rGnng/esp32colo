#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Small flash-backed UTF-8 bitmap renderer. No font table is copied to RAM.
namespace BitmapFont {
inline uint32_t u32(const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline uint32_t next(const char *&text){
  const auto *p=reinterpret_cast<const uint8_t*>(text);if(!*p)return 0;
  const uint8_t first=*p++;text=reinterpret_cast<const char*>(p);
  if(first<0x80)return first;
  unsigned count=0;uint32_t value=0,minimum=0;
  if(first>=0xc2&&first<0xe0){count=1;value=first&31;minimum=0x80;}
  else if(first>=0xe0&&first<0xf0){count=2;value=first&15;minimum=0x800;}
  else if(first>=0xf0&&first<0xf5){count=3;value=first&7;minimum=0x10000;}
  else return '?';
  for(unsigned i=0;i<count;++i){if(!*p){text=reinterpret_cast<const char*>(p);return 0;}if((*p&0xc0)!=0x80)return '?';value=(value<<6)|(*p++&63);}
  text=reinterpret_cast<const char*>(p);
  return value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff)?'?':value;
}
struct Glyph {const uint8_t *pixels=nullptr;uint8_t width=0,height=0,advance=0;int8_t left=0,top=0;};
class Font {
 public:
  Font(const uint8_t *data,size_t bytes):data_(data),bytes_(bytes){
    if(bytes>=12&&!std::memcmp(data,"CPF1",4)){count_=u32(data+4);bitmap_=u32(data+8);if(count_>(bytes-12)/16||bitmap_!=12+count_*16||bitmap_>bytes)count_=0;}
  }
  uint32_t count()const{return count_;}
  Glyph glyph(uint32_t codepoint)const {
    uint32_t low=0,high=count_;
    while(low<high){const uint32_t mid=(low+high)/2;if(u32(data_+12+mid*16)<codepoint)low=mid+1;else high=mid;}
    if(low>=count_||u32(data_+12+low*16)!=codepoint)return codepoint=='?'?Glyph{}:glyph('?');
    const auto *record=data_+12+low*16;const uint32_t offset=u32(record+4);const size_t length=(size_t(record[8])*record[9]+1)/2;
    if(offset>bytes_-bitmap_||length>bytes_-bitmap_-offset)return Glyph{};
    Glyph out;out.pixels=data_+bitmap_+offset;out.width=record[8];out.height=record[9];out.advance=record[10];out.left=int8_t(record[11]);out.top=int8_t(record[12]);return out;
  }
  int width(const char *text)const {int pixels=0;for(uint32_t c;(c=next(text));)pixels+=glyph(c).advance;return pixels;}
  void fit(const char *text,int maxWidth,char *out,size_t capacity)const {
    if(!capacity)return;
    out[0]=0;size_t used=0;int pixels=0;bool clipped=false;
    while(*text){const char *start=text;const uint32_t c=next(text);if(!c)break;const size_t n=text-start;const int advance=glyph(c).advance;
      if(used+n>=capacity||pixels+advance>maxWidth){clipped=true;break;}
      std::memcpy(out+used,start,n);used+=n;out[used]=0;pixels+=advance;
    }
    if(!clipped)return;
    const int dots=width("...");
    while(used&&(pixels+dots>maxWidth||used+3>=capacity)){
      size_t start=used-1;while(start&&(uint8_t(out[start])&0xc0)==0x80)--start;
      const char *last=out+start;pixels-=glyph(next(last)).advance;used=start;out[used]=0;
    }
    if(dots<=maxWidth&&used+3<capacity)std::memcpy(out+used,"...",4);
  }
  template<typename Plot>void draw(const char *text,int x,int baseline,Plot plot)const {
    for(uint32_t c;(c=next(text));){const auto g=glyph(c);
      for(int row=0;row<g.height;++row)for(int col=0;col<g.width;++col){const unsigned pixel=row*g.width+col;const uint8_t packed=g.pixels[pixel/2],alpha=pixel&1?packed&15:packed>>4;if(alpha)plot(x+g.left+col,baseline+g.top+row,alpha);}
      x+=g.advance;
    }
  }
 private:
  const uint8_t *data_;size_t bytes_;uint32_t count_=0,bitmap_=0;
};
}
