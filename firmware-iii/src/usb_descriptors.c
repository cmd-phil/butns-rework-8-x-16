// USB descriptors: CDC (REPL / monome serial) + MIDI.
// Structure follows the TinyUSB device examples (MIT License, Ha Thach).

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "device.h"
#include "pico/unique_id.h"
#include "tusb.h"

// iii mode: CAFE:1101, the id diii looks for.
// monome mode: Raspberry Pi VID with the Pico PID, as used by the Arduino firmware.
#define USB_VID_III 0xCAFE
#define USB_PID_III 0x1101
#define USB_VID_MONOME 0x2E8A
#define USB_PID_MONOME 0x000A

enum { STRID_LANGID = 0, STRID_MANUFACTURER, STRID_PRODUCT, STRID_SERIAL, STRID_CDC, STRID_MIDI };

static tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID_MONOME,
    .idProduct = USB_PID_MONOME,
    .bcdDevice = 0x0100,
    .iManufacturer = STRID_MANUFACTURER,
    .iProduct = STRID_PRODUCT,
    .iSerialNumber = STRID_SERIAL,
    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void) {
  if (g_mode == MODE_III) {
    desc_device.idVendor = USB_VID_III;
    desc_device.idProduct = USB_PID_III;
  }
  return (uint8_t const *)&desc_device;
}

enum { ITF_NUM_CDC = 0, ITF_NUM_CDC_DATA, ITF_NUM_MIDI, ITF_NUM_MIDI_STREAMING, ITF_NUM_TOTAL };

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82
#define EPNUM_MIDI_OUT 0x03
#define EPNUM_MIDI_IN 0x83

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MIDI_DESC_LEN)

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 500),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, STRID_MIDI, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return desc_configuration;
}

// serial number: "m" + 7 digits derived from the flash unique id
static char serial[9];

static const char *serial_str(void) {
  if (!serial[0]) {
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    uint32_t h = 2166136261u;
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) h = (h ^ id.id[i]) * 16777619u;
    snprintf(serial, sizeof(serial), "m%07lu", (unsigned long)(h % 10000000u));
  }
  return serial;
}

static uint16_t desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  size_t chr_count;
  if (index == STRID_LANGID) {
    desc_str[1] = 0x0409;
    chr_count = 1;
  } else {
    const char *str;
    switch (index) {
      case STRID_MANUFACTURER: str = device_str1(); break;
      case STRID_PRODUCT: str = device_str2(); break;
      case STRID_SERIAL: str = serial_str(); break;
      case STRID_CDC: str = g_mode == MODE_III ? "iii" : "monome"; break;
      case STRID_MIDI: str = "iii midi"; break;
      default: return NULL;
    }
    chr_count = strlen(str);
    if (chr_count > 32) chr_count = 32;
    for (size_t i = 0; i < chr_count; i++) desc_str[1 + i] = (uint8_t)str[i];
  }
  desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return desc_str;
}
