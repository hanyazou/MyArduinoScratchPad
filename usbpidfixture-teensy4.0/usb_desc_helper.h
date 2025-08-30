#ifndef USBPIDFIXTURE_USB_DESC_HELPER_H_
#define USBPIDFIXTURE_USB_DESC_HELPER_H_

#include <stdint.h>
#include <unistd.h>

extern void get_usb_device_descriptor(uint8_t** desc, size_t* length);

#endif  // USBPIDFIXTURE_USB_DESC_HELPER_H_
