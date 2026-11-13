// Exercises the actual HTTP upload callbacks with fake flash and transport.
#include "http_ota.h"
#include "hardware_demo.h"
#include "app_image.h"
#include <Update.h>
#include <esp_ota_ops.h>
#include <cassert>
#include <iostream>
#include <vector>
FakeSerial Serial;
FakeEsp ESP;
FakeUpdate Update;
uint32_t testMillis = 100;
esp_partition_t testPartition{0x480000};
bool paused = false;
namespace HardwareDemo { void pause(bool value) { paused = value; } }

static void event(WebServer &server, int status, uint8_t *data = nullptr, size_t size = 0, size_t total = 0) {
  server.input.status = status;
  server.input.buf = data;
  server.input.currentSize = size;
  server.input.totalSize = total;
  server.chunk();
}
static std::vector<uint8_t> image() {
  std::vector<uint8_t> data(2048, 0xAA);
  data[0] = 0xE9; data[1] = 3; data[12] = 9; data[13] = 0;
  data[28] = 0; data[29] = 1; data[30] = data[31] = 0;
  data[32] = 0x32; data[33] = 0x54; data[34] = 0xCD; data[35] = 0xAB;
  return data;
}
static void feed(WebServer &server, std::vector<uint8_t> &data, size_t size) {
  for (size_t offset = 0; offset < data.size(); offset += size) {
    const size_t count = std::min(size, data.size() - offset);
    event(server, UPLOAD_FILE_WRITE, data.data() + offset, count);
  }
}
int main(int argc, char **argv) {
  assert(argc >= 2);
  const std::string name = argv[1];
  WebServer server;
  HttpOta::setup(server, "test-password");
  auto data = image();
  if (name == "unauthorized") {
    server.authorized = false;
    event(server, UPLOAD_FILE_START); feed(server, data, 29);
    event(server, UPLOAD_FILE_END, nullptr, 0, data.size()); server.finish();
    assert(server.response == 401 && !Update.begins && !Update.commits && !paused);
  } else if (name == "wrong-chip" || name == "bootloader" || name == "partition") {
    if (name == "wrong-chip") data[12] = 0;
    if (name == "bootloader") data[32] = 0;
    if (name == "partition") testPartition.size = 0x100000;
    event(server, UPLOAD_FILE_START); feed(server, data, 29);
    event(server, UPLOAD_FILE_END, nullptr, 0, data.size()); server.finish();
    assert(server.response == 400 && !Update.begins && !Update.commits && !paused);
  } else if (name == "empty" || name == "truncated") {
    event(server, UPLOAD_FILE_START);
    if (name == "truncated") event(server, UPLOAD_FILE_WRITE, data.data(), 35);
    event(server, UPLOAD_FILE_END, nullptr, 0, name == "empty" ? 0 : 35); server.finish();
    assert(server.response == 400 && !Update.begins && !Update.commits && !paused);
  } else if (name == "timeout") {
    event(server, UPLOAD_FILE_START); feed(server, data, 37);
    event(server, UPLOAD_FILE_END, nullptr, 0, data.size());
    testMillis += 15001; HttpOta::tick();
    assert(!HttpOta::busy() && !Update.running && !Update.commits && !paused);
    event(server, UPLOAD_FILE_START); feed(server, data, 37);
    event(server, UPLOAD_FILE_END, nullptr, 0, data.size()); server.finish();
    assert(server.response == 200 && Update.commits == 1);
  } else if (name == "invalid-md5") {
    server.md5 = "not-a-digest";
    event(server, UPLOAD_FILE_START); feed(server, data, 37);
    event(server, UPLOAD_FILE_END, nullptr, 0, data.size()); server.finish();
    assert(server.response == 400 && !Update.commits && !Update.running && !paused);
  } else if (name == "abort") {
    event(server, UPLOAD_FILE_START); feed(server, data, 37);
    assert(paused && Update.running);
    event(server, UPLOAD_FILE_ABORTED); server.finish();
    assert(server.response == 400 && Update.aborts == 1 && !Update.commits && !paused);
  } else if (name == "multiple-files") {
    event(server, UPLOAD_FILE_START); feed(server, data, 128);
    event(server, UPLOAD_FILE_END, nullptr, 0, data.size());
    assert(!Update.commits);
    event(server, UPLOAD_FILE_START); feed(server, data, 128);
    event(server, UPLOAD_FILE_END, nullptr, 0, data.size()); server.finish();
    assert(server.response == 400 && Update.aborts == 1 && !Update.commits && !paused);
  } else if (name == "verification-failure") {
    Update.endOk = false;
    event(server, UPLOAD_FILE_START); feed(server, data, 128);
    event(server, UPLOAD_FILE_END, nullptr, 0, data.size()); server.finish();
    assert(server.response == 400 && Update.ends == 1 && !Update.commits && !paused);
    // A subsequent valid request can recover without resetting the device.
    Update.endOk = true;
    event(server, UPLOAD_FILE_START); feed(server, data, 1);
    event(server, UPLOAD_FILE_END, nullptr, 0, data.size()); server.finish();
    assert(server.response == 200 && Update.commits == 1);
  } else if (name == "success") {
    assert(argc == 3); const size_t chunk = std::stoul(argv[2]);
    event(server, UPLOAD_FILE_START); feed(server, data, chunk);
    assert(Update.data == data && paused && HttpOta::busy());
    event(server, UPLOAD_FILE_END, nullptr, 0, data.size());
    assert(!Update.commits); // Never select a slot before the complete HTTP request.
    server.finish();
    assert(server.response == 200 && Update.commits == 1 && paused);
    HttpOta::tick(); assert(!ESP.restarts);
    testMillis += 1001; HttpOta::tick(); assert(ESP.restarts == 1);
  } else { return 2; }
  std::cout << "PASS " << name << '\n';
}
