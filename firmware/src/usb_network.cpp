#include "usb_network.h"
#include "usb_network_settings.h"
#include "usb_dhcp.h"
#include <Preferences.h>
#include <USB.h>
#include <coloplay_ecm.h>
#include <esp32-hal-tinyusb.h>
#include <device/usbd_pvt.h>
#include <esp_system.h>
#include <lwip/etharp.h>
#include <lwip/netif.h>
#include <lwip/netifapi.h>
#include <lwip/priv/tcpip_priv.h>
#include <lwip/tcpip.h>
#include <lwip/udp.h>
#include <netif/ethernet.h>
#include <atomic>
#include <esp_attr.h>

extern "C" {
void coloplay_usb_prepare_serial_phy(void);
// The ECM MAC descriptor is the host NIC's address. The device uses a
// different locally administered address on the other end of the link.
uint8_t tud_network_mac_address[6] = {};
}

namespace UsbNetwork {
namespace {
Preferences gPrefs;
UsbNetworkSettings::Stored gSettings, gActive;
bool gPrefsOpen = false, gStored = false, gRecovery = false, gStarted = false;
std::atomic<bool> gReady{false}, gLink{false};
std::atomic<uint32_t> gRx{0}, gTx{0}, gDrops{0}, gOffers{0}, gAcks{0}, gNaks{0};
int gError = 0;
netif gNetif = {};
udp_pcb *gDhcp = nullptr;
QueueHandle_t gTxQueue = nullptr;
UsbDhcp::Lease gLease;
portMUX_TYPE gLeaseLock = portMUX_INITIALIZER_UNLOCKED;
char gMacString[13] = {}, gSerial[25] = {};
uint8_t gInterface = 0;
enum Stage : uint32_t { kIdle, kSerialMode, kNetif, kDescriptors, kSerialStop,
  kUsbBegin, kDescriptorCallback, kDriverInit, kRunning, kHostInterface, kStageCount };
const char *const kStages[] = {"idle", "serial-jtag", "netif", "register-descriptors",
  "stop-serial", "usb-begin", "descriptor-callback", "driver-init", "running", "host-interface"};
constexpr uint32_t kStageMagic = 0xC010B055;
RTC_NOINIT_ATTR uint32_t gRetainedMagic, gRetainedStage;
Stage gStage = kIdle, gLastStage = kIdle;
void setStage(Stage stage) {
  gStage = stage; gRetainedStage = stage; gRetainedMagic = kStageMagic;
}

String address(const uint8_t ip[4]) {
  return String(ip[0]) + '.' + ip[1] + '.' + ip[2] + '.' + ip[3];
}

void pumpTx(void *) {
  if (!gTxQueue) return;
  pbuf *packet = nullptr;
  if (!gLink.load()) {
    while (xQueueReceive(gTxQueue, &packet, 0) == pdTRUE) { pbuf_free(packet); ++gDrops; }
    return;
  }
  if (tud_ready() && tud_network_can_xmit(1514) &&
      xQueueReceive(gTxQueue, &packet, 0) == pdTRUE) {
    // TinyUSB copies through tud_network_xmit_cb synchronously. All its
    // endpoint operations stay on the USB task, never the TCP/IP task.
    tud_network_xmit(packet, 0);
    pbuf_free(packet);
    ++gTx;
  }
}

err_t linkOutput(netif *, pbuf *packet) {
  if (!gLink.load() || !gTxQueue) return ERR_IF;
  if (packet->tot_len > 1514) return ERR_BUF;
  pbuf_ref(packet);
  if (xQueueSend(gTxQueue, &packet, 0) != pdTRUE) {
    pbuf_free(packet); ++gDrops; return ERR_MEM;
  }
  usbd_defer_func(pumpTx, nullptr, false);
  return ERR_OK;
}

err_t initNetif(netif *interface) {
  interface->name[0] = 'u'; interface->name[1] = 's';
  interface->mtu = 1500;
  interface->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
  interface->hwaddr_len = 6;
  std::memcpy(interface->hwaddr, tud_network_mac_address, 6);
  interface->hwaddr[5] ^= 1;
  interface->output = etharp_output;
  interface->linkoutput = linkOutput;
#if LWIP_NETIF_HOSTNAME
  interface->hostname = "coloplay";
#endif
  return ERR_OK;
}

void receiveDhcp(void *, udp_pcb *pcb, pbuf *packet, const ip_addr_t *, u16_t port) {
  if (port != 68 || !packet || packet->tot_len > 1500) { if (packet) pbuf_free(packet); return; }
  // lwIP's TCP/IP task has a small stack. This callback is serialized there.
  static uint8_t request[1500], reply[UsbDhcp::kReplySize];
  const size_t length = pbuf_copy_partial(packet, request, packet->tot_len, 0);
  pbuf_free(packet);
  portENTER_CRITICAL(&gLeaseLock);
  const size_t size = UsbDhcp::respond(request, length, reply, sizeof(reply), gActive, gLease, millis());
  portEXIT_CRITICAL(&gLeaseLock);
  if (!size) return;
  if (reply[242] == 2) ++gOffers;
  if (reply[242] == 5) ++gAcks;
  if (reply[242] == 6) ++gNaks;
  pbuf *response = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);
  if (!response) { ++gDrops; return; }
  pbuf_take(response, reply, size);
  ip_addr_t broadcast, source;
  IP_ADDR4(&broadcast, 255, 255, 255, 255);
  IP_ADDR4(&source, gActive.address[0], gActive.address[1], gActive.address[2], gActive.address[3]);
  if (udp_sendto_if_src(pcb, response, &broadcast, 68, &gNetif, &source) != ERR_OK) ++gDrops;
  pbuf_free(response);
}

err_t createNetif(tcpip_api_call_data *) {
  ip4_addr_t ip, mask, gateway;
  IP4_ADDR(&ip, gActive.address[0], gActive.address[1], gActive.address[2], gActive.address[3]);
  mask.addr = lwip_htonl(UsbNetworkSettings::mask(gActive.prefix));
  ip4_addr_set_zero(&gateway);
  if (!netif_add(&gNetif, &ip, &mask, &gateway, nullptr, initNetif, tcpip_input)) return ERR_IF;
  gDhcp = udp_new_ip_type(IPADDR_TYPE_V4);
  if (!gDhcp) { netif_remove(&gNetif); return ERR_MEM; }
  err_t result = udp_bind(gDhcp, IP_ANY_TYPE, 67);
  if (result != ERR_OK) {
    udp_remove(gDhcp); gDhcp = nullptr; netif_remove(&gNetif); return result;
  }
  udp_bind_netif(gDhcp, &gNetif);  // Never receive DHCP on WiFi.
  udp_recv(gDhcp, receiveDhcp, nullptr);
  netif_set_up(&gNetif);
  // Preserve the WiFi default route. USB has neither gateway nor DNS.
  gReady.store(true);
  return ERR_OK;
}

void updateLink(void *) {
  if (!gReady.load()) return;
  if (gLink.load()) netif_set_link_up(&gNetif);
  else netif_set_link_down(&gNetif);
}

void markLink(bool up) {
  if (gLink.exchange(up) != up && gReady.load()) tcpip_callback(updateLink, nullptr);
}

void resetDriver(uint8_t rhport) {
  markLink(false);
  netd_reset(rhport);
  pumpTx(nullptr);
}

bool control(uint8_t rhport, uint8_t stage, const tusb_control_request_t *request) {
  const bool ok = netd_control_xfer_cb(rhport, stage, request);
  if (ok && stage == CONTROL_STAGE_ACK &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
      request->bRequest == TUSB_REQ_SET_INTERFACE && request->wIndex == gInterface + 1) {
    markLink(request->wValue == 1);
    pumpTx(nullptr);
  }
  return ok;
}

bool transfer(uint8_t rhport, uint8_t endpoint, xfer_result_t result, uint32_t length) {
  const bool ok = netd_xfer_cb(rhport, endpoint, result, length);
  pumpTx(nullptr);
  return ok;
}

uint16_t descriptor(uint8_t *destination, uint8_t *interface) {
  setStage(kDescriptorCallback);
  const uint8_t dataEndpoint = tinyusb_get_free_duplex_endpoint();
  const uint8_t notifyEndpoint = tinyusb_get_free_in_endpoint();
  if (!dataEndpoint || !notifyEndpoint) return 0;
  const uint8_t label = tinyusb_add_string_descriptor("ColoPLAY Ethernet");
  const uint8_t mac = tinyusb_add_string_descriptor(gMacString);
  gInterface = *interface;
  const uint8_t bytes[] = {TUD_CDC_ECM_DESCRIPTOR(*interface, label, mac,
    uint8_t(0x80 | notifyEndpoint), 64, dataEndpoint, uint8_t(0x80 | dataEndpoint), 64, 1514)};
  std::memcpy(destination, bytes, sizeof(bytes));
  *interface += 2;
  return sizeof(bytes);
}
}  // namespace

void prepareSerial() {
  if (esp_reset_reason() != ESP_RST_POWERON && gRetainedMagic == kStageMagic &&
      gRetainedStage < kStageCount) gLastStage = static_cast<Stage>(gRetainedStage);
  // USB-OTG's software PHY selection can survive a warm reset. Restore
  // the serial/JTAG controller before the early recovery console starts.
  coloplay_usb_prepare_serial_phy();
}

void loadSettings() {
  gPrefsOpen = gPrefs.begin("usbnet", false);
  UsbNetworkSettings::Stored stored;
  if (gPrefsOpen && gPrefs.getBytesLength("settings") == sizeof(stored) &&
      gPrefs.getBytes("settings", &stored, sizeof(stored)) == sizeof(stored) &&
      UsbNetworkSettings::valid(stored)) {
    gSettings = stored; gStored = true;
  } else if (gPrefsOpen) {
    gStored = gPrefs.putBytes("settings", &gSettings, sizeof(gSettings)) == sizeof(gSettings);
  }
}

void begin(bool recoveryMode) {
  gRecovery = recoveryMode;
  gActive = gSettings;
  if (gRecovery || gActive.mode == UsbNetworkSettings::kSerialJtag) { setStage(kSerialMode); return; }
  esp_efuse_mac_get_default(tud_network_mac_address);
  tud_network_mac_address[0] = (tud_network_mac_address[0] | 2) & 0xFE;
  tud_network_mac_address[5] ^= 0x42;
  snprintf(gMacString, sizeof(gMacString), "%02X%02X%02X%02X%02X%02X",
    tud_network_mac_address[0], tud_network_mac_address[1], tud_network_mac_address[2],
    tud_network_mac_address[3], tud_network_mac_address[4], tud_network_mac_address[5]);
  snprintf(gSerial, sizeof(gSerial), "coloplay-%s", gMacString);
  gTxQueue = xQueueCreate(8, sizeof(pbuf *));
  if (!gTxQueue) { gError = ESP_ERR_NO_MEM; return; }
  setStage(kNetif);
  tcpip_api_call_data call = {};
  gError = tcpip_api_call(createNetif, &call);
  if (gError) return;
  setStage(kDescriptors);
  gError = tinyusb_enable_interface(USB_INTERFACE_CUSTOM, TUD_CDC_ECM_DESC_LEN, descriptor);
  if (gError) return;
  USB.productName("ColoPLAY USB Network");
  USB.manufacturerName("ColoPLAY");
  USB.serialNumber(gSerial);
  USB.PID(0x4002);  // Development identity distinct from the fixed Serial/JTAG PID.
  USB.onEvent([](void *, esp_event_base_t, int32_t event, void *) {
    if (event == ARDUINO_USB_STOPPED_EVENT) markLink(false);
    if (event == ARDUINO_USB_RESUME_EVENT) usbd_defer_func(pumpTx, nullptr, false);
  });
  Serial.println("[usb] starting CDC-ECM; select serial/JTAG mode in web settings to restore JTAG");
  setStage(kSerialStop);
  Serial.end();
  delay(50);  // Let the host see removal of the fixed Serial/JTAG device.
  setStage(kUsbBegin);
  gStarted = USB.begin();
  setStage(kRunning);
  if (!gStarted) { gError = ESP_FAIL; Serial.begin(115200); }
}

bool receiveFrame(const uint8_t *bytes, uint16_t length) {
  if (!gReady.load() || length < 14 || length > 1514) { ++gDrops; return false; }
  pbuf *packet = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
  if (!packet) { ++gDrops; return false; }
  pbuf_take(packet, bytes, length);
  if (gNetif.input(packet, &gNetif) != ERR_OK) { pbuf_free(packet); ++gDrops; }
  else ++gRx;
  // The frame is copied into lwIP-owned memory; rearm the USB buffer now.
  return false;
}

void networkUp() { setStage(kHostInterface); markLink(true); }
void initDriver() { setStage(kDriverInit); netd_init(); }

String statusJson() {
  UsbDhcp::Lease lease;
  portENTER_CRITICAL(&gLeaseLock);
  lease = gLease;
  portEXIT_CRITICAL(&gLeaseLock);
  const bool bound = lease.active(millis()) && lease.bound;
  char client[18] = {};
  if (bound) snprintf(client, sizeof(client), "%02x:%02x:%02x:%02x:%02x:%02x",
    lease.mac[0], lease.mac[1], lease.mac[2], lease.mac[3], lease.mac[4], lease.mac[5]);
  String json;
  json.reserve(700);
  json += "{\"mode\":\"" + String(gSettings.mode == UsbNetworkSettings::kEthernet ? "ethernet" : "serial_jtag");
  json += "\",\"active_mode\":\"" + String(gStarted ? "ethernet" : "serial_jtag");
  json += "\",\"address\":\"" + address(gSettings.address);
  json += "\",\"prefix\":" + String(gSettings.prefix);
  json += ",\"dhcp_address\":\"" + address(gSettings.lease);
  json += "\",\"gateway\":null,\"dns\":null,\"dhcp_pool_size\":1";
  json += ",\"protocol\":\"CDC-ECM\",\"stored\":" + String(gStored ? "true" : "false");
  json += ",\"ready\":" + String(gStarted && gReady.load() ? "true" : "false");
  json += ",\"connected\":" + String(gStarted && tud_connected() ? "true" : "false");
  json += ",\"link_up\":" + String(gLink.load() ? "true" : "false");
  json += ",\"jtag_available\":" + String(gStarted ? "false" : "true");
  json += ",\"recovery_override\":" + String(gRecovery ? "true" : "false");
  json += ",\"error\":" + String(gError);
  json += ",\"stage\":\"" + String(kStages[gStage]) + "\",\"last_boot_stage\":\"" + kStages[gLastStage] + "\"";
  json += ",\"rx_frames\":" + String(gRx.load()) + ",\"tx_frames\":" + String(gTx.load());
  json += ",\"drops\":" + String(gDrops.load());
  json += ",\"dhcp_offers\":" + String(gOffers.load()) + ",\"dhcp_acks\":" + String(gAcks.load());
  json += ",\"dhcp_naks\":" + String(gNaks.load());
  json += ",\"lease_active\":" + String(bound ? "true" : "false");
  json += ",\"client_mac\":\"" + String(client) + "\"}";
  return json;
}

bool configure(WebServer &server, String &error) {
  if (!server.hasArg("mode") && !server.hasArg("address") &&
      !server.hasArg("prefix") && !server.hasArg("dhcp_address")) {
    error = "USB settings required"; return false;
  }
  auto candidate = gSettings;
  if (server.hasArg("mode")) {
    const String mode = server.arg("mode");
    if (mode != "ethernet" && mode != "serial_jtag") { error = "invalid USB mode"; return false; }
    candidate.mode = mode == "ethernet" ? UsbNetworkSettings::kEthernet : UsbNetworkSettings::kSerialJtag;
  }
  if ((server.hasArg("address") && !UsbNetworkSettings::parse(server.arg("address").c_str(), candidate.address)) ||
      (server.hasArg("dhcp_address") && !UsbNetworkSettings::parse(server.arg("dhcp_address").c_str(), candidate.lease))) {
    error = "USB and DHCP addresses must be IPv4 addresses"; return false;
  }
  if (server.hasArg("prefix")) {
    const String prefix = server.arg("prefix");
    if (!prefix.length() || prefix.length() > 2 ||
        prefix[0] < '0' || prefix[0] > '9' ||
        (prefix.length() == 2 && (prefix[1] < '0' || prefix[1] > '9'))) {
      error = "prefix must be 1..30"; return false;
    }
    candidate.prefix = prefix.toInt();
  }
  if (!UsbNetworkSettings::valid(candidate)) {
    error = "use two different host addresses in the same subnet, with prefix 1..30"; return false;
  }
  if (!gPrefsOpen || gPrefs.putBytes("settings", &candidate, sizeof(candidate)) != sizeof(candidate)) {
    error = "could not save USB settings to NVS"; return false;
  }
  gSettings = candidate; gStored = true;
  return true;
}
}  // namespace UsbNetwork

extern "C" {
const usbd_class_driver_t *usbd_app_driver_get_cb(uint8_t *count) {
  static const auto driver = [] {
    usbd_class_driver_t d = {};
    d.init = UsbNetwork::initDriver;
    d.reset = UsbNetwork::resetDriver;
    d.open = netd_open;
    d.control_xfer_cb = UsbNetwork::control;
    d.xfer_cb = UsbNetwork::transfer;
    return d;
  }();
  *count = 1;
  return &driver;
}
bool tud_network_recv_cb(const uint8_t *bytes, uint16_t length) {
  return UsbNetwork::receiveFrame(bytes, length);
}
uint16_t tud_network_xmit_cb(uint8_t *destination, void *reference, uint16_t) {
  auto *packet = static_cast<pbuf *>(reference);
  return pbuf_copy_partial(packet, destination, packet->tot_len, 0);
}
void tud_network_init_cb() { UsbNetwork::networkUp(); }
}
