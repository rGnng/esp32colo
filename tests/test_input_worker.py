"""Replay physical GPIO capture through the production sampling task/queue."""
from pathlib import Path
import json,re,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
PREFIX=r'''
#include "input_worker.h"
#include "chord_engine.h"
#include "rotary_detents.h"
#include "board_pins.h"
#include <cassert>
#include <cstring>
#include <deque>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
using TickType_t=uint32_t;using TaskHandle_t=void*;using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define pdTRUE 1
#define pdPASS 1
#define LOW 0
#define HIGH 1
#define ESP_OK 0
#define PCNT_UNIT_0 0
#define ARDUINO_RUNNING_CORE 1
#define pdMS_TO_TICKS(x) (x)
struct Queue {unsigned capacity,item;std::deque<std::vector<uint8_t>> items;};using QueueHandle_t=Queue*;
static uint32_t tick=0,stop=0;static int mode=0,liveQueues=0;static bool failQueue=false,failTask=false,changed=false;static uint32_t newer=0;static void(*task)(void*);
struct Done{};struct Edge{uint32_t at;bool down;};
static QueueHandle_t xQueueCreate(unsigned capacity,unsigned item){if(failQueue)return nullptr;++liveQueues;return new Queue{capacity,item,{}};}
static int xQueueSend(Queue *q,const void *item,int){if(q->items.size()==q->capacity)return 0;const auto *bytes=(const uint8_t*)item;q->items.emplace_back(bytes,bytes+q->item);return pdTRUE;}
static int xQueueReceive(Queue *q,void *item,int){if(q->items.empty())return 0;std::memcpy(item,q->items.front().data(),q->item);q->items.pop_front();return pdTRUE;}
static void xQueueReset(Queue *q){q->items.clear();}
static void vQueueDelete(Queue *q){delete q;--liveQueues;}
static int xTaskCreatePinnedToCore(void(*fn)(void*),const char*,unsigned stack,void*,unsigned priority,void **out,int core){assert(stack>=4096&&priority>1&&core==1);if(failTask)return 0;task=fn;*out=(void*)1;return pdPASS;}
static TickType_t xTaskGetTickCount(){return tick;}
static uint32_t millis(){return tick;}
static int64_t esp_timer_get_time(){return int64_t(tick)*1000;}
namespace InputTrace {void sample(bool){}void debounced(bool){}void gesture(int,int){}}
static int pcnt_get_counter_value(int,int16_t *value){*value=mode==4?(tick/20)*2:0;return ESP_OK;}
static void pcnt_counter_pause(int){}static void pcnt_counter_clear(int){}static void pcnt_counter_resume(int){}
'''
SUFFIX=r'''
static int digitalRead(int pin){
 bool down=false;
 if(pin==Board::kKey2){
  if(mode==1){for(auto e:edges)if(e.at<=tick*1000)down=e.down;else break;}
  else if(mode==2)down=(tick>=50&&tick<110)||(tick>=250&&tick<310)||(tick>=380&&tick<440);
  else if(mode==3)down=tick%70>=30&&tick%70<65;
 }
 if(pin==Board::kKey4&&mode==5)down=tick>=50&&tick<600;
 if(pin==Board::kKey2&&mode==5)down=(tick>=100&&tick<160)||(tick>=230&&tick<290);
 if(pin==Board::kKey2&&mode==6)down=(tick>=50&&tick<250)||(tick>=300&&tick<350);
 if(pin==Board::kKnobButton&&mode==4)down=tick>=200&&tick<400;
 return down?LOW:HIGH;
}
static void vTaskDelayUntil(TickType_t *wake,unsigned delay){
 assert(delay==1);*wake+=delay;tick=*wake;
 if(mode==2&&tick==200){GestureBindings b;b.physical[SceneSettings::kKey2]=(1u<<SceneSettings::kSingle)|(1u<<SceneSettings::kDouble);newer=InputWorker::configure(0,b,2);changed=true;}
 if(mode==5&&tick==200){GestureBindings b;b.ordered[SceneSettings::kKey4][10]=1u<<SceneSettings::kSingle;newer=InputWorker::configure(1u<<10,b,2);changed=true;}
 if(mode==6&&tick==100){InputWorker::discard();GestureBindings none;InputWorker::configure(0,none,1);}
 if(mode==6&&tick==200){GestureBindings b;b.physical[SceneSettings::kKey2]=1u<<SceneSettings::kSingle;newer=InputWorker::configure(0,b,1);changed=true;}
 if(tick>=stop)throw Done{};
}
int main(int argc,char **argv){
 using namespace SceneSettings;assert(argc==2);const std::string name=argv[1];GestureBindings b;uint16_t groups=0;
 if(name=="trace"){
  mode=1;stop=edges.back().at/1000+600;b.physical[kKey2]=(1u<<kSingle)|(1u<<kDouble);
  b.combos[7]=b.combos[11]=1u<<kSingle;b.ordered[kKey2][3]=b.ordered[kKey2][10]=b.ordered[kKey4][10]=b.ordered[kKey4][12]=1u<<kSingle;
  groups=(1u<<7)|(1u<<11)|(1u<<3)|(1u<<10)|(1u<<12);
 }else if(name=="generation"){mode=2;stop=700;b.physical[kKey2]=1u<<kSingle;}
 else if(name=="overflow"){mode=3;stop=5000;b.physical[kKey2]=1u<<kSingle;}
 else if(name=="encoder"){mode=4;stop=501;b.physical[kKnob]=(1u<<kSingle)|(1u<<kRight)|(1u<<kPressRight);}
 else if(name=="held-modifier"){mode=5;stop=700;b.ordered[kKey4][10]=1u<<kSingle;groups=1u<<10;}
 else if(name=="pause-resume"){mode=6;stop=700;b.physical[kKey2]=1u<<kSingle;}
 else if(name=="queue-failure"){failQueue=true;}
 else if(name=="task-failure"){failTask=true;}
 else assert(false);
 const auto first=InputWorker::configure(groups,b,1);
 if(failQueue||failTask){assert(!InputWorker::begin(false)&&liveQueues==0);return 0;}
 assert(InputWorker::begin(mode==4));try{task(nullptr);}catch(Done&){}
 const auto state=InputWorker::snapshot();assert(state.ready&&state.samples==stop&&state.maxGapUs==1000);
 std::vector<InputWorker::Event> received;InputWorker::Event event;while(InputWorker::next(event))received.push_back(event);
 if(mode==1){assert(received.size()==8&&state.presses[kKey2]==16&&!state.dropped);for(auto e:received)assert(e.input==kKey2&&e.gesture==kDouble&&e.generation==first);}
 if(mode==2){assert(changed&&received.size()==2);assert(received[0].generation==first&&received[0].gesture==kSingle);assert(received[1].generation==newer&&received[1].gesture==kDouble);assert(state.generation==newer);}
 if(mode==3){assert(received.size()==64&&state.dropped>0&&state.presses[kKey2]>64);InputWorker::discard();assert(!InputWorker::next(event));InputWorker::resetTiming();assert(!InputWorker::snapshot().maxGapUs&&InputWorker::snapshot().dropped==state.dropped);}
 if(mode==4){assert(state.position==25&&state.edges==50);int plain=0,modified=0;for(auto e:received){assert(e.input==kKnob&&e.repeat==1);if(e.gesture==kRight)++plain;else if(e.gesture==kPressRight)++modified;else assert(false);}assert(plain>0&&modified>0&&plain+modified==25);}
 if(mode==5){assert(changed&&received.size()==2&&state.presses[kKey4]==1&&state.presses[kKey2]==2);for(auto e:received)assert(e.input==comboInput(10,kKey4)&&e.gesture==kSingle);assert(received[0].generation==first&&received[1].generation==newer);}
 if(mode==6){assert(changed&&received.size()==1&&state.presses[kKey2]==2);assert(received[0].generation==newer&&received[0].gesture==kSingle&&received[0].atMs>=300);}
 std::cout<<"Input worker "<<name<<" passed\n";
}
'''
class InputWorkerTests(unittest.TestCase):
 def test_production_task_boundaries_and_physical_trace(self):
  trace=json.loads((ROOT/'tests/fixtures/key2_double_trace.json').read_text())
  values=','.join('{%d,%s}'%(at,'true' if down else 'false')for at,down in trace['edges'])
  source=(ROOT/'firmware/src/input_worker.cpp').read_text()
  source=re.sub(r'^#include .*$', '',source,flags=re.M)
  # Fakes model task creation/sleep and the bounded RTOS queue; production run()
  # owns recognition. Main consumes nothing throughout the 14-second capture.
  prefix=PREFIX+'\nstatic const std::vector<Edge> edges={'+values+'};\nstatic int digitalRead(int);\nstatic void vTaskDelayUntil(TickType_t*,unsigned);\n'
  with tempfile.TemporaryDirectory()as tmp:
   path=Path(tmp);file=path/'worker.cpp';file.write_text(prefix+source+SUFFIX);binary=path/'worker'
   subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(ROOT/'firmware/include'),str(file),'-o',str(binary)],check=True)
   for case in('trace','generation','overflow','encoder','held-modifier','pause-resume','queue-failure','task-failure'):
    with self.subTest(case=case):subprocess.run([str(binary),case],check=True)
if __name__=='__main__':unittest.main()
