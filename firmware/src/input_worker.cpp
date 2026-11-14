#include "input_worker.h"
#include "input_trace.h"
#include "board_pins.h"
#include "chord_engine.h"
#include "rotary_detents.h"
#include <Arduino.h>
#include <driver/pcnt.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace InputWorker {
namespace {
constexpr int kPins[]={Board::kKey1,Board::kKey2,Board::kKey3,Board::kKey4,Board::kKnobButton,Board::kOpenButton,Board::kRecoveryButton};
struct Config {uint32_t generation=0,epoch=0;uint16_t combinations=0;GestureBindings bindings;};
Config gConfig;State gState;
portMUX_TYPE gMux=portMUX_INITIALIZER_UNLOCKED;
QueueHandle_t gEvents=nullptr;TaskHandle_t gTask=nullptr;bool gEncoder=false;
ChordEngine gEngine;RotaryDetents gDetents;
void read(bool *raw){for(int i=0;i<SceneSettings::kInputCount;++i)raw[i]=digitalRead(kPins[i])==LOW;}
void run(void *){
  bool raw[SceneSettings::kInputCount];read(raw);gEngine.seed(raw,millis());
  uint32_t applied=0;uint8_t previousDown=0;int16_t previousEncoder=0;
  for(int i=0;i<SceneSettings::kInputCount;++i)if(raw[i])previousDown|=1u<<i;
  TickType_t wake=xTaskGetTickCount();uint64_t previousSample=0;
  Config config;
  for(;;){
    bool changed=false;
    portENTER_CRITICAL(&gMux);if(gConfig.generation!=applied){config=gConfig;changed=true;}portEXIT_CRITICAL(&gMux);
    if(changed){gEngine.configure(config.combinations,config.bindings);applied=config.generation;}
    const uint64_t nowUs=esp_timer_get_time();const uint32_t now=nowUs/1000;
    const uint32_t gap=previousSample?uint32_t(nowUs-previousSample):0;previousSample=nowUs;
    read(raw);InputTrace::sample(raw[SceneSettings::kKey2]);
    int32_t edges=0,detents=0;
    if(gEncoder){
      int16_t value=0;
      if(pcnt_get_counter_value(PCNT_UNIT_0,&value)==ESP_OK){
        edges=int32_t(value)-previousEncoder;previousEncoder=value;
        if(value>20000||value<-20000){pcnt_counter_pause(PCNT_UNIT_0);pcnt_counter_clear(PCNT_UNIT_0);previousEncoder=0;pcnt_counter_resume(PCNT_UNIT_0);}
        detents=gDetents.consume(edges);
      }
    }
    gEngine.tick(raw,detents,now,[&](int input,int gesture,int repeat){
      InputTrace::gesture(input,gesture);Event event;event.generation=applied;event.atMs=now;event.input=input;event.gesture=gesture;event.repeat=repeat;
      if(xQueueSend(gEvents,&event,0)!=pdTRUE){portENTER_CRITICAL(&gMux);++gState.dropped;portEXIT_CRITICAL(&gMux);}
    });
    uint8_t down=0;for(int i=0;i<SceneSettings::kInputCount;++i)if(gEngine.down(i))down|=1u<<i;
    InputTrace::debounced((down&(1u<<SceneSettings::kKey2))!=0);
    const uint8_t pressed=down&~previousDown;previousDown=down;
    portENTER_CRITICAL(&gMux);
    gState.ready=true;gState.down=down;gState.generation=applied;++gState.samples;if(gap>gState.maxGapUs)gState.maxGapUs=gap;
    for(int i=0;i<SceneSettings::kInputCount;++i)if(pressed&(1u<<i))++gState.presses[i];
    if(pressed||detents)++gState.activity;gState.edges+=edges;gState.position+=detents;
    portEXIT_CRITICAL(&gMux);
    // Sampling owns no display, networking, NVS or action dispatch work.
    const TickType_t tick=xTaskGetTickCount();if(TickType_t(tick-wake)>pdMS_TO_TICKS(2))wake=tick;
    vTaskDelayUntil(&wake,pdMS_TO_TICKS(1));
  }
}
}
uint32_t configure(uint16_t combinations,const GestureBindings &bindings,uint32_t epoch){
  portENTER_CRITICAL(&gMux);if(++gConfig.generation==0)++gConfig.generation;
  gConfig.epoch=epoch;gConfig.combinations=combinations;gConfig.bindings=bindings;const uint32_t token=gConfig.generation;
  portEXIT_CRITICAL(&gMux);return token;
}
bool begin(bool encoderReady){
  if(gTask)return true;gEncoder=encoderReady;gEvents=xQueueCreate(64,sizeof(Event));if(!gEvents)return false;
  if(xTaskCreatePinnedToCore(run,"input-scan",4096,nullptr,3,&gTask,ARDUINO_RUNNING_CORE)!=pdPASS){vQueueDelete(gEvents);gEvents=nullptr;return false;}
  return true;
}
void resetTiming(){portENTER_CRITICAL(&gMux);gState.samples=gState.maxGapUs=0;portEXIT_CRITICAL(&gMux);}
State snapshot(){portENTER_CRITICAL(&gMux);const auto out=gState;portEXIT_CRITICAL(&gMux);return out;}
bool next(Event &event){return gEvents&&xQueueReceive(gEvents,&event,0)==pdTRUE;}
void discard(){if(gEvents)xQueueReset(gEvents);}
}
