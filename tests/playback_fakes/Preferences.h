#pragma once
#include <cstring>
#include <vector>
namespace FakeNvs {extern std::vector<unsigned char> saved;extern bool open,fail;extern int writes;}
class Preferences {
 public:
 bool begin(const char *name,bool readOnly){return !std::strcmp(name,"bt-playback")&&!readOnly&&FakeNvs::open;}
 size_t getBytesLength(const char *)const{return FakeNvs::saved.size();}
 size_t getBytes(const char *,void *out,size_t bytes){if(bytes!=FakeNvs::saved.size())return 0;std::memcpy(out,FakeNvs::saved.data(),bytes);return bytes;}
 size_t putBytes(const char *,const void *data,size_t bytes){++FakeNvs::writes;if(FakeNvs::fail)return 0;const auto *p=static_cast<const unsigned char*>(data);FakeNvs::saved.assign(p,p+bytes);return bytes;}
};
