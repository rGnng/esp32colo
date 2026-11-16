// Keep this IDF 4.4 LL header in C: one of its unrelated inline helpers
// copies a volatile register union in a way that cannot compile as C++.
#include "hal/usb_phy_ll.h"

void coloplay_usb_prepare_serial_phy(void) {
    usb_phy_ll_int_jtag_enable(&USB_SERIAL_JTAG);
}
