#include "bitmap_font.h"
#include "playback_assets.h"
#include <cassert>
#include <iostream>
#include <string>
int main(){
 const BitmapFont::Font font(PlaybackAssets::kJapanese16,sizeof(PlaybackAssets::kJapanese16));
 assert(font.count()==PlaybackAssets::kGlyphCount&&font.count()>16000);
 uint32_t previous=0;
 for(uint32_t i=0;i<font.count();++i){const uint32_t point=BitmapFont::u32(PlaybackAssets::kJapanese16+12+i*16);assert(point>previous);previous=point;const auto glyph=font.glyph(point);assert(glyph.pixels);}
 for(const char *text:{u8"日本語",u8"ウルトラデザイア",u8"小春六花",u8"髙﨑",u8"café"}){
  const char *p=text;for(uint32_t c;(c=BitmapFont::next(p));)assert(font.glyph(c).pixels!=font.glyph('?').pixels);
 }
 assert(font.glyph(0x10ffff).pixels==font.glyph('?').pixels);
 const char *p=u8"A日本語𠮷";assert(BitmapFont::next(p)=='A'&&BitmapFont::next(p)==0x65e5&&BitmapFont::next(p)==0x672c&&BitmapFont::next(p)==0x8a9e&&BitmapFont::next(p)==0x20bb7&&!BitmapFont::next(p));
 p="\xe6\x97";assert(!BitmapFont::next(p)&&!*p);p="\xf0\x80\x80\xaf";assert(BitmapFont::next(p)=='?');
 p="\xed\xa0\x80";assert(BitmapFont::next(p)=='?');p="\xff";assert(BitmapFont::next(p)=='?');
 for(const char *text:{"Love Me As You Go",u8"ウルトラデザイアの長い日本語の曲名を表示します",u8"akatora feat. 小春六花",u8"A very long title with 日本語 and café that needs truncation"}){
  char line[64];font.fit(text,208,line,sizeof(line));assert(font.width(line)<=208);
  unsigned painted=0;
  for(int baseline:{115,139})font.draw(line,120-font.width(line)/2,baseline,[&](int x,int y,uint8_t alpha){assert(alpha>0&&alpha<=15);assert((x-120)*(x-120)+(y-120)*(y-120)<120*120);++painted;});
  assert(painted);
  if(font.width(text)>208)assert(std::string(line).substr(std::strlen(line)-3)=="...");
 }
 char tiny[5]={'a','b','c','d','z'};font.fit(u8"日本語",16,tiny,4);assert(tiny[4]=='z'&&font.width(tiny)<=16);
 char zero='z';font.fit("abc",0,&zero,0);assert(zero=='z');
 uint8_t broken[12]={};const BitmapFont::Font invalid(broken,sizeof(broken));assert(!invalid.count()&&!invalid.width("text"));
 bool translucent=false,solid=false;
 for(uint8_t byte:PlaybackAssets::kBluetoothConnecting32){translucent|=(byte>>4)>0&&(byte>>4)<15;solid|=(byte>>4)==15;}
 assert(translucent&&solid&&sizeof(PlaybackAssets::kBluetoothConnecting32)==512);
 std::cout<<"PASS Japanese/Latin coverage, all glyph bounds, UTF-8 validation, measured ellipsis, circular layout and duotone connection icon\n";
}
