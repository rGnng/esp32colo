#include "scene_record_codec.h"
#include "tab5_navigation_profile.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <vector>
#include <type_traits>
static void roundtrip(const uint8_t *data,size_t bytes){
 std::vector<uint8_t> packed(SceneRecordCodec::capacity(bytes)),decoded(bytes+2,0xa5);
 const size_t n=SceneRecordCodec::encode(data,bytes,packed.data(),packed.size());assert(n&&n<=packed.size());
 assert(SceneRecordCodec::decode(packed.data(),n,decoded.data()+1,bytes));
 assert(decoded.front()==0xa5&&decoded.back()==0xa5&&!std::memcmp(data,decoded.data()+1,bytes));
 for(size_t cut=0;cut<n;cut++)assert(!SceneRecordCodec::decode(packed.data(),cut,decoded.data()+1,bytes));
 packed.resize(n+1);packed[n]=0;assert(!SceneRecordCodec::decode(packed.data(),n+1,decoded.data()+1,bytes));
 for(size_t at=0;at<n;at++){
  packed[at]^=0x80;const bool ok=SceneRecordCodec::decode(packed.data(),n,decoded.data()+1,bytes);
  assert(!ok||!std::memcmp(data,decoded.data()+1,bytes));assert(decoded.front()==0xa5&&decoded.back()==0xa5);packed[at]^=0x80;
 }
 assert(!SceneRecordCodec::decode(packed.data(),n,decoded.data(),bytes+1));
 assert(!SceneRecordCodec::encode(data,bytes,packed.data(),11));
}
int main(){
 using namespace SceneSettings;static_assert(std::is_trivially_copyable<Stored>::value,"scene object representation must be stable");
 auto scenes=defaults();assert(Tab5NavigationProfile::install(scenes,1));
 std::vector<uint8_t> encoded(SceneRecordCodec::capacity(sizeof(scenes)));Stored restored;
 size_t n=SceneRecordCodec::encode(reinterpret_cast<uint8_t*>(&scenes),sizeof(scenes),encoded.data(),encoded.size());
 assert(n&&n<1024&&SceneRecordCodec::decode(encoded.data(),n,reinterpret_cast<uint8_t*>(&restored),sizeof(restored)));
 assert(valid(restored)&&!std::memcmp(&scenes,&restored,sizeof(scenes)));
 roundtrip(reinterpret_cast<uint8_t*>(&scenes),sizeof(scenes));
 std::mt19937 random(13169);
 for(size_t bytes:{size_t(1),size_t(2),size_t(3),size_t(4),size_t(127),size_t(128),size_t(129),size_t(512),size_t(3304)}){
  std::vector<uint8_t> data(bytes);
  for(int shape=0;shape<3;shape++){
   for(size_t i=0;i<bytes;i++)data[i]=shape==0?0:shape==1?uint8_t(i%17):uint8_t(random());
   roundtrip(data.data(),bytes);
  }
 }
 uint8_t bad[15]={};std::memcpy(bad,"CSR1",4);SceneRecordCodec::put16(bad+4,8);SceneRecordCodec::put16(bad+6,1);
 bad[12]=0x85;bad[13]=0;bad[14]=0;uint8_t result[8];assert(!SceneRecordCodec::decode(bad,sizeof(bad),result,sizeof(result)));
 bad[13]=1;assert(!SceneRecordCodec::decode(bad,sizeof(bad),result,sizeof(result)));
 std::cout<<"PASS compact scenes: "<<sizeof(scenes)<<" -> "<<n<<" bytes, exact roundtrip, raw noise, overlap, truncation, corruption and bounds\n";
}
