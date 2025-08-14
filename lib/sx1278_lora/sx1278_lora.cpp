#include "sx1278_lora.h"
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "pins.h"

bool lora_init(void) {
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);
    LoRa.setPins(SPI_SS, SX1278_RST, SX1278_DIO0);
    if (!LoRa.begin(433E6)) {
        return false;
    }
    LoRa.setSyncWord(0xA5);
    /* Ajustes adicionais se desejar: SF/BW/CR, ganho, etc. */
    return true;
}

int lora_read_packet(uint8_t *buf, int max_len, int *out_rssi, float *out_snr) {
    if (!buf || max_len <= 0) return 0;

    int packetSize = LoRa.parsePacket();
    if (packetSize <= 0) return 0;

    int n = 0;
    while (LoRa.available() && n < max_len) {
        buf[n++] = (uint8_t)LoRa.read();
    }
    if (out_rssi) *out_rssi = LoRa.packetRssi();
    if (out_snr)  *out_snr  = LoRa.packetSnr();
    return n;
}
