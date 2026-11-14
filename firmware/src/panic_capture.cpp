// Retain a bounded panic record across reset so a USB cable is not required
// to diagnose a device reached through the HTTP relay. No allocation or I/O
// occurs in the panic wrapper; the SDK's original handler still performs reset.
#include "panic_capture.h"
#include <esp_attr.h>
#include <esp_ota_ops.h>
#include <esp_private/panic_internal.h>
#include <esp_debug_helpers.h>
#include <xtensa/xtensa_context.h>
namespace {
struct Record {uint32_t magic,core,exception,address,excvaddr,count,pcs[16];char reason[96],image[65];};
RTC_NOINIT_ATTR Record retained;
char currentImage[65]={};
}
extern "C" void __real_esp_panic_handler(panic_info_t *info);
extern "C" void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t *info) {
  retained.magic=0x50414e32;retained.core=info->core;retained.exception=info->exception;
  retained.address=(uintptr_t)info->addr;retained.count=0;
  for(unsigned n=0;n<65;++n)retained.image[n]=currentImage[n];
  unsigned i=0;if(info->reason)for(;i<95&&info->reason[i];++i)retained.reason[i]=info->reason[i];retained.reason[i]=0;
  if(info->frame){
    const auto *f=(const XtExcFrame*)info->frame;retained.excvaddr=f->excvaddr;
    esp_backtrace_frame_t trace={f->pc,f->a1,f->a0,info->frame};
    do {retained.pcs[retained.count++]=trace.pc;}while(retained.count<16&&trace.next_pc&&esp_backtrace_get_next_frame(&trace));
  }
  __real_esp_panic_handler(info);
}
void PanicCapture::begin(){
  const auto *app=esp_ota_get_app_description();
  static const char digits[]="0123456789abcdef";
  for(unsigned i=0;i<32;++i){currentImage[i*2]=digits[app->app_elf_sha256[i]>>4];currentImage[i*2+1]=digits[app->app_elf_sha256[i]&15];}
}
String PanicCapture::json(){
  if(retained.magic!=0x50414e32)return "null";
  String out="{\"core\":"+String(retained.core)+",\"exception\":"+String(retained.exception)+",\"address\":\""+String(retained.address,HEX)+"\",\"excvaddr\":\""+String(retained.excvaddr,HEX)+"\",\"reason\":\"";
  for(const char *p=retained.reason;*p;++p){if(*p=='"'||*p=='\\')out+='\\';if((unsigned char)*p>=32)out+=*p;}
  out+="\",\"image_elf_sha256\":\""+String(retained.image)+"\",\"backtrace\":[";
  for(unsigned i=0;i<retained.count&&i<16;++i){if(i)out+=',';out+='"';out+=String(retained.pcs[i],HEX);out+='"';}
  return out+"]}";
}
