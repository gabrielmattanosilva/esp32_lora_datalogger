/**
 * @file sx1278_lora.cpp
 * @brief Implementação das funções para comunicação LoRa com o módulo SX1278.
 */

#include "sx1278_lora.h"
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "pins.h"
#include "crypto.h"
#include "credentials.h"

/**
 * @brief Inicializa o módulo LoRa.
 * @return Verdadeiro se a inicialização foi bem-sucedida, falso caso contrário.
 */
bool lora_begin(void)
{
    pinMode(SPI_SS, OUTPUT);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);
    LoRa.setSPI(SPI);
    LoRa.setPins(SPI_SS, SX1278_RST, SX1278_DIO0);

    if (!LoRa.begin(433E6))
    {
        return false;
    }

    LoRa.setSyncWord(0xA5);

    crypto_init(AES_KEY);
    return true;
}

uint32_t lora_read_packet(uint8_t *buf, uint16_t max_len, int16_t *out_rssi, float *out_snr)
{
    if (!buf || max_len == 0)
    {
        return 0;
    }

    int packetSize = LoRa.parsePacket();

    if (packetSize <= 0)
    {
        return 0;
    }

    uint32_t n = 0;

    while (LoRa.available() && n < max_len)
    {
        buf[n++] = (uint8_t)LoRa.read();
    }

    if (out_rssi)
    {
        *out_rssi = LoRa.packetRssi();
    }

    if (out_snr)
    {
        *out_snr = LoRa.packetSnr();
    }

    return n;
}
