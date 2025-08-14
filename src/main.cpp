#include <Arduino.h>
#include <SPI.h>

#include "pins.h"
#include "credentials.h"
#include "utils.h"
#include "crypto.h"
#include "sx1278_lora.h"

static void print_decoded(const PayloadPacked *p) {
    const bool  irr_error = (p->irradiance == 0xFFFF);
    const float batt_V    = p->battery_voltage / 1000.0f;
    const float temp_C    = p->internal_temperature / 10.0f;

    Serial.println("---- Pacote decodificado ----");
    if (irr_error) {
        Serial.println("Irradiancia : ERRO (0xFFFF)");
    } else {
        Serial.print("Irradiancia : ");
        Serial.print(p->irradiance);
        Serial.println(" W/m^2");
    }

    Serial.print("Bateria     : ");
    Serial.print(batt_V, 3);
    Serial.println(" V");

    Serial.print("Temp. int.  : ");
    Serial.print(temp_C, 1);
    Serial.println(" C");

    Serial.print("Timestamp   : ");
    Serial.print(p->timestamp);
    Serial.println(" s");

    Serial.print("Checksum    : 0x");
    if (p->checksum < 16) Serial.print('0');
    Serial.println(p->checksum, HEX);
    Serial.println("-----------------------------");
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { /* aguarda USB/Serial */ }

    Serial.println();
    Serial.println("=== Receptor ESP32 (RX contínuo) ===");
    Serial.println("AES-128-CBC; Formato: IV(16) || CT(16*n) -> Payload(11B)");

    // IMPORTANTE: inicializa a chave da lib crypto
    crypto_init(AES_KEY);

    if (!lora_begin()) {
        Serial.println("Falha ao inicializar LoRa. Verifique conexões/pinos.");
        for (;;) { delay(1000); }
    }
    Serial.println("LoRa OK. Aguardando pacotes...");
}

void loop() {
    uint8_t  rxbuf[128];
    int16_t  rssi = 0;     // <- int16_t para casar com a função
    float    snr  = 0.0f;

    // Retorno agora é uint32_t e max_len é uint16_t
    uint32_t n = lora_read_packet(rxbuf, (uint16_t)sizeof(rxbuf), &rssi, &snr);
    if (n == 0) {
        delay(1);
        return;
    }

    Serial.print("RX ["); Serial.print(n);
    Serial.print(" B]  RSSI="); Serial.print(rssi);
    Serial.print("  SNR="); Serial.println(snr, 1);

    utils_hexdump(rxbuf, (int)n);   // utils_hexdump espera int

    // IV(16) + CT(>=16 e múltiplo de 16)
    if (n < 32u) {
        Serial.println(">> Pacote curto: esperado >= 32 bytes (IV16 + CT16+).");
        return;
    }
    const uint8_t *iv = &rxbuf[0];
    const uint8_t *ct = &rxbuf[16];
    uint16_t ct_len = (uint16_t)(n - 16u);

    if ((ct_len % 16u) != 0u) {
        Serial.println(">> Tamanho do ciphertext não múltiplo de 16.");
        return;
    }

    uint8_t plain[128];
    size_t  plain_len = 0;

    // Assinatura nova da lib crypto: (in, in_len, iv, out, out_len)
    if (!crypto_decrypt(ct, (size_t)ct_len, iv, plain, &plain_len)) {
        Serial.println(">> Falha na descriptografia (AES-128-CBC/PKCS#7).");
        return;
    }

    if (plain_len != sizeof(PayloadPacked)) {
        Serial.print(">> Tamanho inesperado após unpad: ");
        Serial.print(plain_len);
        Serial.print(" (esperado ");
        Serial.print(sizeof(PayloadPacked));
        Serial.println(").");
        return;
    }

    PayloadPacked p;
    if (!utils_parse_payload(plain, plain_len, &p)) {
        Serial.println(">> Payload inválido (checksum/estrutura).");
        return;
    }

    print_decoded(&p);
}
