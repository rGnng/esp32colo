#pragma once
#include "USBCDC.h"
inline bool tud_cdc_n_connected(int){return FakeUsb::connected;}
inline size_t tud_cdc_n_write(int,const uint8_t *p,size_t n){n=std::min(n,FakeUsb::space);FakeUsb::tx.insert(FakeUsb::tx.end(),p,p+n);return n;}
inline void tud_cdc_n_write_flush(int){}
