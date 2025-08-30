#define USB_DESC_LIST_DEFINE
#include <usb_desc.h>
#include "usb_desc.h"

void get_usb_device_descriptor(uint8_t** desc, size_t* length) {
    *desc = nullptr;
    *length = 0;
    for (int i = 0; usb_descriptor_list[i].wValue != 0x0000; i++) {
        if (usb_descriptor_list[i].wValue == 0x0100 && usb_descriptor_list[i].wIndex == 0x0000) {
            auto ptr = reinterpret_cast<const uint8_t*>(usb_descriptor_list[i].addr);
            *desc = const_cast<uint8_t*>(ptr);
            *length = usb_descriptor_list[i].length;
            return;
        }
    }
}
