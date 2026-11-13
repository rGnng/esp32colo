#pragma once
#include "usb_network_settings.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

// DHCP wire codec for this point-to-point Ethernet link. No router, DNS or
// classless-route options are offered. Exactly one MAC can hold the sole lease.
namespace UsbDhcp {
constexpr uint32_t kLeaseSeconds = 3600;
constexpr size_t kReplySize = 300;
struct Lease {
  bool occupied = false, bound = false, declined = false;
  uint8_t mac[6] = {};
  uint32_t expires = 0;
  bool active(uint32_t now) const {
    return occupied && static_cast<int32_t>(expires - now) > 0;
  }
};
inline void put32(uint8_t *p, uint32_t value) {
  for (int i = 3; i >= 0; --i) { p[i] = value; value >>= 8; }
}

inline size_t respond(const uint8_t *request, size_t length, uint8_t *reply,
                      size_t capacity, const UsbNetworkSettings::Stored &settings,
                      Lease &lease, uint32_t now) {
  if (length < 240 || capacity < kReplySize || !UsbNetworkSettings::valid(settings) ||
      request[0] != 1 || request[1] != 1 || request[2] != 6 ||
      (request[28] & 1) || !std::memcmp(request + 28, "\0\0\0\0\0\0", 6) ||
      std::memcmp(request + 236, "\x63\x82\x53\x63", 4) ||
      std::memcmp(request + 24, "\0\0\0\0", 4)) return 0;
  uint8_t type = 0;
  const uint8_t *requested = nullptr, *server = nullptr;
  bool ended = false;
  for (size_t at = 240; at < length;) {
    const uint8_t option = request[at++];
    if (option == 255) { ended = true; break; }
    if (!option) continue;
    if (at >= length) return 0;
    const size_t size = request[at++];
    if (size > length - at) return 0;
    if (option == 53) { if (size != 1 || type) return 0; type = request[at]; }
    if (option == 50) { if (size != 4 || requested) return 0; requested = request + at; }
    if (option == 54) { if (size != 4 || server) return 0; server = request + at; }
    at += size;
  }
  if (!ended || !type || (server && std::memcmp(server, settings.address, 4))) return 0;
  if (!lease.active(now)) lease = Lease{};
  const bool same = lease.occupied && !std::memcmp(lease.mac, request + 28, 6);
  if (type == 7) {
    if (same && !std::memcmp(request + 12, settings.lease, 4)) lease = Lease{};
    return 0;
  }
  if (type == 4) {
    if (same && requested && !std::memcmp(requested, settings.lease, 4)) {
      lease.declined = true; lease.bound = false; lease.expires = now + 60000;
    }
    return 0;
  }
  uint8_t response = 0;
  if (type == 1) {
    if (lease.declined || (lease.occupied && !same)) return 0;
    response = 2;
  } else if (type == 3) {
    if (!requested) requested = request + 12;
    if (!std::memcmp(requested, "\0\0\0\0", 4)) return 0;
    response = std::memcmp(requested, settings.lease, 4) || lease.declined ||
               (lease.occupied && !same) ? 6 : 5;
  } else if (type == 8) {
    response = 5;  // DHCPINFORM acknowledges configuration, without a lease.
  } else return 0;
  if (response == 2 || (response == 5 && type != 8)) {
    lease.occupied = true;
    std::memcpy(lease.mac, request + 28, 6);
    if (response == 5) lease.bound = true;
    // A repeated DISCOVER must not shorten an already-bound lease.
    if (response == 5 || !lease.bound)
      lease.expires = now + (lease.bound ? kLeaseSeconds * 1000 : 60000);
  }
  std::memset(reply, 0, kReplySize);
  reply[0] = 2; reply[1] = 1; reply[2] = 6;
  std::memcpy(reply + 4, request + 4, 4);  // Transaction ID.
  reply[10] = 0x80;  // Replies are broadcast, including clients without an IP.
  if (response != 6 && type != 8) std::memcpy(reply + 16, settings.lease, 4);
  std::memcpy(reply + 20, settings.address, 4);
  std::memcpy(reply + 28, request + 28, 6);
  std::memcpy(reply + 236, "\x63\x82\x53\x63", 4);
  size_t at = 240;
  reply[at++] = 53; reply[at++] = 1; reply[at++] = response;
  reply[at++] = 54; reply[at++] = 4;
  std::memcpy(reply + at, settings.address, 4); at += 4;
  if (response != 6) {
    reply[at++] = 1; reply[at++] = 4;
    put32(reply + at, UsbNetworkSettings::mask(settings.prefix)); at += 4;
    if (type != 8) {
      for (const auto option : {uint8_t(51), uint8_t(58), uint8_t(59)}) {
        reply[at++] = option; reply[at++] = 4;
        put32(reply + at, option == 51 ? kLeaseSeconds : option == 58 ?
              kLeaseSeconds / 2 : kLeaseSeconds * 7 / 8); at += 4;
      }
    }
  }
  reply[at] = 255;
  return kReplySize;
}
}  // namespace UsbDhcp
