// Keep this IDF 4.4 LL header in C: one of its unrelated inline helpers
// copies a volatile register union in a way that cannot compile as C++.
#include "hal/usb_phy_ll.h"

void coloplay_usb_prepare_serial_phy(void) {
    usb_phy_ll_int_jtag_enable(&USB_SERIAL_JTAG);
}

#ifdef COLOPLAY_USB_CONCURRENCY_TEST
#include <stdint.h>
void coloplay_usb_test_otg_phy(void) {
    usb_phy_ll_int_otg_enable(&USB_WRAP);
}
// Read only. Bit 0: software mux override; bit 1: OTG owns internal PHY;
// bit 2: Serial/JTAG pads enabled; bit 3: Serial/JTAG external-PHY select.
uint32_t coloplay_usb_test_phy_state(void) {
    return (RTCCNTL.usb_conf.sw_hw_usb_phy_sel ? 1u : 0u) |
           (RTCCNTL.usb_conf.sw_usb_phy_sel ? 2u : 0u) |
           (USB_SERIAL_JTAG.conf0.usb_pad_enable ? 4u : 0u) |
           (USB_SERIAL_JTAG.conf0.phy_sel ? 8u : 0u);
}
#endif
