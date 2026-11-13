#pragma once
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace UsbNetworkSettings {
enum Mode : uint8_t { kEthernet = 0, kSerialJtag = 1 };
struct Stored {
  uint16_t version = 1;
  uint8_t mode = kEthernet, prefix = 30;
  uint8_t address[4] = {172, 32, 29, 1};
  uint8_t lease[4] = {172, 32, 29, 2};
};
static_assert(sizeof(Stored) == 12, "USB settings storage layout changed");
inline uint32_t number(const uint8_t ip[4]) {
  return uint32_t(ip[0]) << 24 | uint32_t(ip[1]) << 16 | uint32_t(ip[2]) << 8 | ip[3];
}
inline uint32_t mask(uint8_t prefix) { return 0xFFFFFFFFu << (32 - prefix); }
inline bool parse(const char *text, uint8_t ip[4]) {
  for (int part = 0; part < 4; ++part) {
    unsigned value = 0, digits = 0;
    while (*text >= '0' && *text <= '9') {
      value = value * 10 + *text++ - '0';
      if (++digits > 3 || value > 255) return false;
    }
    if (!digits) return false;
    ip[part] = value;
    if (part < 3) { if (*text++ != '.') return false; }
    else if (*text) return false;
  }
  return true;
}
inline bool peer(const uint8_t device[4], uint8_t prefix, uint8_t result[4]) {
  if (prefix<1||prefix>30) return false;
  const uint32_t ip=number(device), net=ip&mask(prefix), broadcast=net|~mask(prefix);
  const uint32_t host=ip==net+1?net+2:net+1;
  if (ip==net||ip==broadcast||host>=broadcast) return false;
  for (int i=0;i<4;++i) result[i]=host>>(24-i*8);
  return true;
}
inline bool legacyDefault(const Stored &s) {
  const uint8_t oldDevice[]={172,32,29,21}, oldHost[]={172,32,29,30};
  return s.prefix==24&&!std::memcmp(s.address,oldDevice,4)&&!std::memcmp(s.lease,oldHost,4);
}
inline bool valid(const Stored &settings) {
  if (settings.version != 1 || settings.mode > kSerialJtag ||
      settings.prefix < 1 || settings.prefix > 30) return false;
  const uint32_t ip = number(settings.address), lease = number(settings.lease);
  const uint32_t netmask = mask(settings.prefix), hostmask = ~netmask;
  for (const auto *value : {settings.address, settings.lease}) {
    if (!value[0] || value[0] == 127 || value[0] >= 224) return false;
    const uint32_t host = number(value) & hostmask;
    if (!host || host == hostmask) return false;
  }
  return ip != lease && (ip & netmask) == (lease & netmask);
}
}  // namespace UsbNetworkSettings
