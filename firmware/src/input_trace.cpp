#include "input_trace.h"
#include "input_worker.h"
#include "board_pins.h"
#include "gesture_engine.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include <memory>
#include <new>

namespace InputTrace {
namespace {
constexpr unsigned kCapacity=256;
enum Kind:uint8_t {kIrq,kSample,kDebounced,kGesture,kPollGap};
struct Event {uint64_t atUs;uint32_t gapUs;uint8_t kind,value;uint16_t reserved;};
Event gEvents[kCapacity];unsigned gWrite=0,gCount=0;uint32_t gTotal=0;
portMUX_TYPE gMux=portMUX_INITIALIZER_UNLOCKED;
uint64_t gLastEdge=0,gLastPoll=0;uint32_t gMaxGap=0,gPolls=0,gOver25=0,gOver50=0;
bool gRaw=false,gDown=false,gStarted=false;
void IRAM_ATTR append(uint8_t kind,uint8_t value,uint64_t now,uint32_t gap){
  gEvents[gWrite]={now,gap,kind,value,0};gWrite=(gWrite+1)%kCapacity;
  if(gCount<kCapacity)++gCount;++gTotal;
}
void IRAM_ATTR edge(){
  const uint64_t now=esp_timer_get_time();const bool down=gpio_get_level(gpio_num_t(Board::kKey2))==0;
  portENTER_CRITICAL_ISR(&gMux);gLastEdge=now;append(kIrq,down,now,0);portEXIT_CRITICAL_ISR(&gMux);
}
void clear(){portENTER_CRITICAL(&gMux);gWrite=gCount=gTotal=gMaxGap=gPolls=gOver25=gOver50=0;gLastEdge=gLastPoll=0;portEXIT_CRITICAL(&gMux);InputWorker::resetTiming();}
String json(){
  std::unique_ptr<Event[]> events(new(std::nothrow) Event[kCapacity]);if(!events)return "{\"error\":\"trace snapshot allocation failed\"}";
  portENTER_CRITICAL(&gMux);
  const unsigned count=gCount;const uint32_t total=gTotal,maxGap=gMaxGap,polls=gPolls,over25=gOver25,over50=gOver50;
  const bool started=gStarted,raw=gRaw,down=gDown;
  for(unsigned i=0;i<count;++i)events[i]=gEvents[(gWrite+kCapacity-count+i)%kCapacity];
  portEXIT_CRITICAL(&gMux);
  String out;out.reserve(384+count*84);
  out="{\"key\":\"key2\",\"started\":"+String(started?"true":"false")+",\"debounce_ms\":"+String(GestureEngine::kDebounceMs)+",\"click_window_ms\":"+String(GestureEngine::kClickMs)+",\"capacity\":"+String(kCapacity)+",\"total\":"+String(total)+",\"raw\":"+String(raw?"true":"false")+",\"down\":"+String(down?"true":"false")+",\"polls\":"+String(polls)+",\"max_poll_gap_us\":"+String(maxGap)+",\"gaps_over_25ms\":"+String(over25)+",\"gaps_over_50ms\":"+String(over50)+",\"events\":[";
  const char *kinds[]={"irq","sample","debounced","gesture","poll_gap"};
  for(unsigned i=0;i<count;++i){const auto &e=events[i];char at[24];snprintf(at,sizeof(at),"%llu",(unsigned long long)e.atUs);if(i)out+=',';out+="{\"at_us\":"+String(at)+",\"kind\":\""+String(kinds[e.kind])+"\",\"value\":"+String(e.value)+",\"gap_us\":"+String(e.gapUs);if(e.kind==kGesture)out+=",\"gesture\":\""+String(SceneSettings::kGestures[e.value])+"\"";out+='}';}
  return out+"]}";
}
}
void begin(bool raw,bool down){gRaw=raw;gDown=down;gStarted=true;attachInterrupt(digitalPinToInterrupt(Board::kKey2),edge,CHANGE);}
void sample(bool raw){
  if(!gStarted)return;const uint64_t now=esp_timer_get_time();
  portENTER_CRITICAL(&gMux);const uint32_t gap=gLastPoll?uint32_t(now-gLastPoll):0;gLastPoll=now;++gPolls;if(gap>gMaxGap)gMaxGap=gap;if(gap>25000)++gOver25;if(gap>50000)++gOver50;
  if(raw!=gRaw){gRaw=raw;append(kSample,raw,now,gap);}
  if(gLastEdge&&now-gLastEdge<500000&&gap>10000)append(kPollGap,0,now,gap);
  portEXIT_CRITICAL(&gMux);
}
void debounced(bool down){if(!gStarted)return;const uint64_t now=esp_timer_get_time();portENTER_CRITICAL(&gMux);if(down!=gDown){gDown=down;append(kDebounced,down,now,0);}portEXIT_CRITICAL(&gMux);}
void gesture(int input,int gesture){if(!gStarted||input!=SceneSettings::kKey2)return;portENTER_CRITICAL(&gMux);append(kGesture,gesture,esp_timer_get_time(),0);portEXIT_CRITICAL(&gMux);}
void setup(WebServer &server){
  server.on("/api/input-trace",HTTP_GET,[&server]{server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",json());});
  server.on("/api/input-trace",HTTP_POST,[&server]{if(server.arg("op")!="clear"){server.send(400,"application/json","{\"error\":\"op must be clear\"}");return;}clear();server.send(200,"application/json","{\"cleared\":true}");});
}
}
