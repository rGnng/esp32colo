#include "usb_dhcp.h"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>

namespace {
using Packet = std::vector<uint8_t>;
using Reply = std::array<uint8_t, 300>;
UsbNetworkSettings::Stored settings;
Packet packet(uint8_t type, uint8_t client = 1, bool requested = false, bool server = false) {
  Packet p(240);
  p[0] = 1; p[1] = 1; p[2] = 6;
  p[4] = 0x12; p[5] = 0x34; p[6] = 0x56; p[7] = 0x78;
  p[28] = 2; p[33] = client;
  std::memcpy(p.data() + 236, "\x63\x82\x53\x63", 4);
  p.insert(p.end(), {53, 1, type});
  if (requested) { p.insert(p.end(), {50, 4}); p.insert(p.end(), settings.lease, settings.lease + 4); }
  if (server) { p.insert(p.end(), {54, 4}); p.insert(p.end(), settings.address, settings.address + 4); }
  p.push_back(255);
  return p;
}
const uint8_t *option(const Reply &r, uint8_t id) {
  for (size_t at = 240; at < r.size();) {
    const uint8_t key = r[at++];
    if (key == 255) return nullptr;
    if (!key) continue;
    const uint8_t size = r[at++];
    assert(at + size <= r.size());
    if (key == id) return r.data() + at;
    at += size;
  }
  assert(false); return nullptr;
}
size_t respond(const Packet &p, Reply &r, UsbDhcp::Lease &lease, uint32_t now = 1000) {
  return UsbDhcp::respond(p.data(), p.size(), r.data(), r.size(), settings, lease, now);
}

void settingsChecks() {
  using namespace UsbNetworkSettings;
  assert(valid(settings));
  uint8_t ip[4];
  assert(parse("172.32.29.1", ip) && !std::memcmp(ip, settings.address, 4));
  for (const auto *text : {"", "1.2.3", "1.2.3.4.5", "1.2.3.256", "1.2..3", "1.2.3.-1", "http://1.2.3.4"})
    assert(!parse(text, ip));
  auto invalid = settings;
  for (uint8_t prefix : {0, 31, 32, 255}) { invalid.prefix = prefix; assert(!valid(invalid)); }
  invalid = settings; invalid.lease[2] = 30; assert(!valid(invalid));
  invalid = settings; invalid.lease[3] = 255; assert(!valid(invalid));
  invalid = settings; invalid.address[3] = 0; assert(!valid(invalid));
  invalid = settings; std::memcpy(invalid.lease, invalid.address, 4); assert(!valid(invalid));
  invalid = settings; invalid.address[0] = invalid.lease[0] = 127; assert(!valid(invalid));
  invalid = settings; invalid.address[0] = invalid.lease[0] = 224; assert(!valid(invalid));
  assert(settings.prefix==30 && settings.lease[3]==2);
  uint8_t host[4]; assert(peer(settings.address,30,host) && !std::memcmp(host,settings.lease,4));
  assert(parse("172.32.29.2",ip) && peer(ip,30,host) && host[3]==1);
  assert(parse("172.32.29.3",ip) && !peer(ip,30,host));
  auto old=settings; old.prefix=24; old.address[3]=21; old.lease[3]=30;
  assert(legacyDefault(old)); old.address[3]=22; assert(!legacyDefault(old));
  std::cout << "PASS USB IPv4/subnet and one-address pool validation\n";
}

void exchangeChecks() {
  UsbDhcp::Lease lease;
  Reply r;
  const auto discover = packet(1);
  assert(respond(discover, r, lease) == r.size());
  assert(r[0] == 2 && !std::memcmp(r.data() + 4, discover.data() + 4, 4));
  assert(!std::memcmp(r.data() + 16, settings.lease, 4));
  assert(!std::memcmp(option(r, 54), settings.address, 4));
  assert(!std::memcmp(option(r, 1), "\xff\xff\xff\xfc", 4));
  assert(option(r, 53)[0] == 2);
  assert(!option(r, 3) && !option(r, 6) && !option(r, 121) && !option(r, 249));
  assert(!std::memcmp(option(r, 51), "\0\0\x0e\x10", 4));
  assert(respond(packet(3, 1, true, true), r, lease, 1100) == r.size());
  assert(option(r, 53)[0] == 5 && lease.bound);
  auto renewal = packet(3);
  std::memcpy(renewal.data() + 12, settings.lease, 4);
  assert(respond(renewal, r, lease, 500000) == r.size());
  assert(option(r, 53)[0] == 5 && lease.expires == 4100000);
  auto wrong = packet(3, 1, true, true); wrong[248] = 99;
  assert(respond(wrong, r, lease) == r.size() && option(r, 53)[0] == 6);
  assert(!std::memcmp(r.data() + 16, "\0\0\0\0", 4));
  std::cout << "PASS DHCP discover/request/renew/NAK wire replies without gateway or DNS\n";
}

void leaseChecks() {
  UsbDhcp::Lease lease;
  Reply r;
  assert(respond(packet(1), r, lease) == r.size());
  assert(respond(packet(1, 2), r, lease) == 0);
  assert(respond(packet(3, 2, true), r, lease) == r.size() && option(r, 53)[0] == 6);
  auto elsewhere = packet(3, 1, true, true); elsewhere[254] = 99;
  assert(respond(elsewhere, r, lease) == 0);
  assert(respond(packet(3, 1, true, true), r, lease) == r.size());
  const uint32_t deadline = lease.expires;
  assert(respond(packet(1), r, lease, 2000) == r.size() && lease.expires == deadline);
  auto release = packet(7);
  std::memcpy(release.data() + 12, settings.lease, 4);
  assert(respond(release, r, lease, 3000) == 0 && !lease.occupied);
  assert(respond(packet(1, 2), r, lease, 4000) == r.size());
  assert(respond(packet(4, 2, true, true), r, lease, 5000) == 0);
  assert(respond(packet(1, 2), r, lease, 5500) == 0);  // Decline quarantine.
  assert(respond(packet(1, 3), r, lease, 65001) == r.size());
  lease = UsbDhcp::Lease{};
  const uint32_t beforeWrap = 0xFFFF0000u;
  assert(respond(packet(3, 1, true, true), r, lease, beforeWrap) == r.size());
  assert(lease.active(beforeWrap + 1000));
  assert(respond(packet(1, 2), r, lease, beforeWrap + 3600001) == r.size());
  std::cout << "PASS sole DHCP lease ownership, release, decline, expiry and timer wrap\n";
}

void malformedChecks() {
  UsbDhcp::Lease lease;
  Reply r;
  const auto good = packet(1);
  for (size_t size = 0; size < good.size(); ++size) {
    Packet truncated(good.begin(), good.begin() + size);
    assert(respond(truncated, r, lease) == 0);
  }
  auto wrong = good; wrong[236] = 0; assert(respond(wrong, r, lease) == 0);
  wrong = good; wrong[241] = 250; assert(respond(wrong, r, lease) == 0);
  wrong = good; wrong[24] = 1; assert(respond(wrong, r, lease) == 0);  // No DHCP relay.
  wrong = good; wrong[28] = 1; assert(respond(wrong, r, lease) == 0);
  wrong = good; wrong.insert(wrong.end() - 1, {53, 1, 1}); assert(respond(wrong, r, lease) == 0);
  assert(!UsbDhcp::respond(good.data(), good.size(), r.data(), 299, settings, lease, 1000));
  std::cout << "PASS truncated/malformed DHCP, duplicate options and capacity bounds\n";
}
}  // namespace

int main() { settingsChecks(); exchangeChecks(); leaseChecks(); malformedChecks(); }
