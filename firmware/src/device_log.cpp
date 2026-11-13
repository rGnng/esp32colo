#include "device_log.h"
#include "log_buffer.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <esp_system.h>

namespace DeviceLog {
namespace {
constexpr size_t kCapacity=4096,kChunk=2048,kLine=384;
LogBuffer<kCapacity> gBuffer;
portMUX_TYPE gLock=portMUX_INITIALIZER_UNLOCKED;
uint32_t gBoot=0;
void record(const char *text,size_t length,bool truncated=false) {
  char prefix[24];
  const int size=snprintf(prefix,sizeof(prefix),"[%10lu ms] ",static_cast<unsigned long>(millis()));
  portENTER_CRITICAL(&gLock);
  gBuffer.append(prefix,size);
  gBuffer.append(text,length);
  if(truncated)gBuffer.append(" [truncated]",12);
  if(!length||text[length-1]!='\n')gBuffer.append("\n",1);
  portEXIT_CRITICAL(&gLock);
}
String quote(const char *bytes,size_t length) {
  String out;out.reserve(length+64);out+='"';
  for(size_t i=0;i<length;++i) {
    const uint8_t c=bytes[i];
    if(c=='"'||c=='\\') {out+='\\';out+=char(c);}
    else if(c=='\n')out+="\\n";
    else if(c=='\r')out+="\\r";
    else if(c=='\t')out+="\\t";
    else if(c<32) {char escaped[7];snprintf(escaped,sizeof(escaped),"\\u%04x",c);out+=escaped;}
    else out+=char(c);
  }
  return out+'"';
}
}
void printf(const char *format,...) {
  char line[kLine];va_list args;va_start(args,format);
  const int length=vsnprintf(line,sizeof(line),format,args);va_end(args);
  if(length<0)return;
  const size_t used=static_cast<size_t>(length)<sizeof(line)?length:sizeof(line)-1;
  Serial.write(reinterpret_cast<const uint8_t *>(line),used);
  record(line,used,static_cast<size_t>(length)>=sizeof(line));
}
void println(const char *message) {
  Serial.println(message);
  const size_t length=strlen(message),used=length<kLine-1?length:kLine-1;
  record(message,used,length>used);
}
void println(const String &message) {println(message.c_str());}
void setup(WebServer &server) {
  gBoot=esp_random();
  server.on("/api/log",HTTP_GET,[&server] {
    uint32_t after=0;const bool have=server.hasArg("after");
    if(have) {
      const String value=server.arg("after");
      bool valid=value.length()>0&&value.length()<=10;
      for(size_t i=0;i<value.length();++i)if(value[i]<'0'||value[i]>'9')valid=false;
      const unsigned long long number=strtoull(value.c_str(),nullptr,10);
      if(!valid||number>0xFFFFFFFFull) {server.send(400,"application/json","{\"error\":\"after must be an unsigned 32-bit cursor\"}");return;}
      after=static_cast<uint32_t>(number);
    }
    char chunk[kChunk];
    portENTER_CRITICAL(&gLock);
    const auto read=gBuffer.read(after,have,chunk,sizeof(chunk));
    portEXIT_CRITICAL(&gLock);
    String json="{\"boot\":"+String(gBoot)+",\"capacity\":"+String(kCapacity)+",\"next\":"+String(read.next)+",\"total\":"+String(read.total);
    json+=",\"reset\":"+String(read.reset?"true":"false")+",\"text\":"+quote(chunk,read.size)+"}";
    server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",json);
  });
}
}  // namespace DeviceLog
