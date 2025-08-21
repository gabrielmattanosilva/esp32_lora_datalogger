Transmissor: https://docs.google.com/spreadsheets/d/18hyQP-xJTOHuD2x-FFXoMYN03g3FdHPbhSULwhc_LIo/edit?gid=0#gid=0
Receptor: https://docs.google.com/spreadsheets/d/1QqOL5jtSgsJz9jv0C3RTe7YPZ2_09yaQAmC2zK9jYEo/edit?gid=0#gid=0

#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#include <stdint.h>

static const uint8_t AES_KEY[16] = {
    0x00, 0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x5E, 0x0F, 0x10};

#define WIFI_SSID           "ssid"
#define WIFI_PASSWORD       "password"
#define THINGSPEAK_API_KEY  "ABCDEFGH12345678"

#endif /* CREDENTIALS_H */