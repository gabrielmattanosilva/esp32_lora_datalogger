/**
 * @file main.cpp
 * @brief Nó receptor/datalogger LoRa com descriptografia AES e envio ao ThingSpeak.
 *
 * Este firmware executa no gateway (ESP32) e implementa o seguinte pipeline:
 * 1) Inicialização de logging e SD; tentativa de sincronizar RTC interno via DS1307.
 * 2) Inicialização da criptografia simétrica (chave AES), Wi-Fi (com reconexão)
 *    e rádio LoRa (SX1278).
 * 3) Recepção de pacotes LoRa por interrupção (callback onReceive), com leitura
 *    do payload para um buffer global e captura de metadados (RSSI/SNR).
 * 4) No laço principal, cópia atômica (seção crítica com interrupções desabilitadas)
 *    do buffer global para um buffer local, seguida de:
 *      - Validações estruturais (tamanho mínimo, alinhamento a 16 bytes),
 *      - Descriptografia AES (conforme implementação da lib `crypto.h`),
 *      - Validação e parse do payload (checksum/estrutura),
 *      - Log dos campos decodificados,
 *      - Upload condicional ao ThingSpeak (se Wi-Fi estiver conectado).
 * 5) Rotação diária de arquivo de log e flush periódico no SD.
 *
 * @note Apenas comentários no estilo Doxygen e explicativos foram adicionados; a lógica
 *       e o código original não foram alterados.
 */

#include <Arduino.h>
#include <LoRa.h>
#include "credentials.h"
#include "pins.h"
#include "crypto.h"
#include "ds1307_rtc.h"
#include "logger.h"
#include "sd_card.h"
#include "utils.h"
#include "sx1278_lora.h"
#include "thingspeak_client.h"
#include "wifi_manager.h"

static const char *TAG = "MAIN";

/**
 * @brief Flag sinalizando que um pacote completo foi lido pela ISR.
 *
 * @details Marcada como @c volatile por ser acessada em contexto de interrupção e no loop.
 */
static volatile bool g_pkt_ready = false;

/**
 * @brief Tamanho, em bytes, do payload bruto recebido (antes de qualquer processamento).
 */
static volatile uint16_t g_pkt_len = 0;

/**
 * @brief RSSI do pacote no momento da recepção, conforme @c LoRa.packetRssi().
 */
static volatile int16_t g_pkt_rssi = 0;

/**
 * @brief SNR do pacote no momento da recepção, conforme @c LoRa.packetSnr().
 */
static volatile float g_pkt_snr = 0.0f;

/**
 * @brief Buffer circular/simples onde a ISR armazena os bytes recebidos.
 *
 * @warning Escrito no contexto de interrupção; a cópia para escopo local é feita
 *          com interrupções desabilitadas para evitar condições de corrida.
 */
static uint8_t g_pkt_buf[128];

/******************************** Protótipos **********************************/

/**
 * @brief Loga os campos decodificados de um @c PayloadPacked para depuração.
 *
 * @param p Ponteiro para estrutura preenchida após parse/validação.
 */
static void print_decoded(const PayloadPacked *p);

/**
 * @brief Callback de recepção LoRa (executada em contexto de interrupção).
 * @param packetSize Número de bytes disponíveis reportado pela lib LoRa.
 */
static void on_lora_rx_isr(int packetSize);

/******************************* Implementações ********************************/

static void on_lora_rx_isr(int packetSize)
{
    if (packetSize <= 0)
    {
        return;
    }

    if (packetSize > (int)sizeof(g_pkt_buf))
    {
        packetSize = sizeof(g_pkt_buf);  /* Trunca para caber no buffer. */
    }

    int n = 0;

    /* Leitura rápida do FIFO do rádio; manter a ISR curta. */
    while (LoRa.available() && n < packetSize)
    {
        g_pkt_buf[n++] = (uint8_t)LoRa.read();
    }

    g_pkt_len  = (uint16_t)n;
    g_pkt_rssi = LoRa.packetRssi();
    g_pkt_snr  = LoRa.packetSnr();
    g_pkt_ready = true;  /* Sinaliza ao loop principal que há dados prontos. */
}

static void print_decoded(const PayloadPacked *p)
{
    const bool  irr_error = (p->irradiance == 0xFFFF);
    const float batt_V    = p->battery_voltage / 1000.0f; /* mV -> V */
    const float temp_C    = p->internal_temperature / 10.0f; /* décimos -> °C */

    LOG(TAG, "---- Pacote decodificado ----");
    if (irr_error)
    {
        LOG(TAG, "Irradiancia : ERRO (0xFFFF)");
    }
    else
    {
        LOG(TAG, "Irradiancia : %u W/m^2", (unsigned)p->irradiance);
    }
    LOG(TAG, "Bateria     : %.3f V", batt_V);
    LOG(TAG, "Temp. int.  : %.1f C", temp_C);
    LOG(TAG, "Timestamp   : %lu s", (unsigned long)p->timestamp);
    LOG(TAG, "Checksum    : 0x%02X", p->checksum);
    LOG(TAG, "-----------------------------");
}

/**
 * @brief Rotina de inicialização do dispositivo (Arduino core).
 *
 * Passos principais:
 *  - Inicializa logger/SD e define o RTC interno em epoch0, depois tenta sincronizar via DS1307.
 *  - Inicializa criptografia com a @c AES_KEY fornecida em @c credentials.h.
 *  - Inicializa Wi-Fi e força reconexão imediata.
 *  - Inicializa o rádio LoRa via @c lora_begin(); em caso de falha, entra em laço infinito.
 *  - Registra o callback de recepção (@c on_lora_rx_isr) e coloca o rádio em modo RX contínuo.
 */
void setup()
{
    /* Logging e SD */
    logger_init_epoch0();
    sdcard_begin();
    logger_begin();

    /* Tenta sincronizar o RTC interno com o DS1307 se disponível. */
    if (ds1307_rtc_sync_at_boot())
    {
        LOG(TAG, "RTC interno sincronizado a partir do DS1307");
    }
    else
    {
        LOG(TAG, "DS1307 ausente/invalido, mantendo epoch0 ate ter hora valida");
    }

    /* Criptografia simétrica — utiliza chave definida em credentials.h */
    crypto_init(AES_KEY);

    /* Wi-Fi e reconexão proativa */
    wifi_begin(WIFI_SSID, WIFI_PASSWORD);
    wifi_force_reconnect();

    /* Rádio LoRa (SX1278): parâmetros e pinos definidos em sx1278_lora/pins */
    if (!lora_begin())
    {
        LOG("LORA", "Falha ao inicializar LoRa");

        /* Em sistemas embarcados, permanecer aqui evita seguir com estado inconsistente. */
        for (;;)
        {
            delay(1000);
        }
    }

    /* Registra callback de RX e entra em modo de recepção contínua. */
    LoRa.onReceive(on_lora_rx_isr);
    LoRa.receive();
    LOG(TAG, "LoRa inicializado, aguardando pacotes...");
}

/**
 * @brief Laço principal: trata pacotes recebidos, descriptografa, valida e envia ao ThingSpeak.
 *
 * Fluxo por iteração:
 *  1) Manutenção: rotação de SD e tick do gerenciador Wi-Fi.
 *  2) Seção crítica: copia estado do pacote da ISR para variáveis locais (sem interrupções).
 *  3) Caso haja pacote:
 *     - Loga metadados (RSSI/SNR) e hexdump.
 *     - Verifica tamanho mínimo (>= 32 B: 16 de IV + pelo menos 16 de CT).
 *     - Separa IV (16 B) e CT (restante). Checa se CT é múltiplo de 16 B (blocos AES).
 *     - Descriptografa em @c plain[] e valida tamanho final == sizeof(PayloadPacked).
 *     - Faz parse e valida (checksum). Loga campos decodificados.
 *     - Envia ao ThingSpeak se Wi-Fi conectado; loga sucesso/falha.
 *     - @c sdcard_flush() para persistir logs do evento.
 */
void loop()
{
    /* Manutenção: rotação diária do arquivo e andamento do gerenciador de Wi-Fi. */
    sdcard_tick_rotate();
    wifi_tick(millis());

    /* Buffers/variáveis locais evitam acessar diretamente os voláteis fora da seção crítica. */
    uint8_t local_buf[128];
    uint16_t local_len = 0;
    int16_t  local_rssi = 0;
    float    local_snr = 0.0f;

    /* Seção crítica: copia os dados da ISR de forma atômica. */
    noInterrupts();

    if (g_pkt_ready)
    {
        local_len  = g_pkt_len;
        local_rssi = g_pkt_rssi;
        local_snr  = g_pkt_snr;

        for (uint16_t i = 0; i < local_len; ++i)
        {
            local_buf[i] = g_pkt_buf[i];
        }
        g_pkt_ready = false;  /* Consome o pacote. */
    }

    interrupts();

    /* Se não há pacote, cede CPU e retorna. */
    if (local_len == 0)
    {
        delay(1);
        return;
    }

    /* Log dos metadados do pacote recebido. */
    LOG("LORA", "RX [%u B]  RSSI=%d  SNR=%.1f", (unsigned)local_len, (int)local_rssi, local_snr);
    LOGHEX("LORA", local_buf, local_len);

    /* Tamanho mínimo: 16 B de IV + ao menos 16 B de ciphertext. */
    if (local_len < 32u)
    {
        LOG(TAG, "Pacote curto (IV16 + CT16+), DESCARTADO");
        sdcard_flush();
        return;
    }

    /* Separação de IV e CT (modo de operação definido pela lib de crypto). */
    const uint8_t *iv = &local_buf[0];
    const uint8_t *ct = &local_buf[16];
    const uint16_t ct_len = (uint16_t)(local_len - 16u);

    /* AES opera em blocos de 16 bytes; ciphertext deve ser múltiplo de 16. */
    if ((ct_len % 16u) != 0u)
    {
        LOG(TAG, "Ciphertext nao multiplo de 16, DESCARTADO");
        sdcard_flush();
        return;
    }

    /* Descriptografia para buffer plano. */
    uint8_t plain[128];
    size_t  plain_len = 0;

    if (!crypto_decrypt(ct, (size_t)ct_len, iv, plain, &plain_len))
    {
        LOG(TAG, "AES fail, DESCARTADO");
        sdcard_flush();
        return;
    }

    /* Após remoção de padding, esperamos exatamente o tamanho de PayloadPacked. */
    if (plain_len != sizeof(PayloadPacked))
    {
        LOG(TAG, "Tamanho apos unpad invalido (%u), DESCARTADO", (unsigned)plain_len);
        sdcard_flush();
        return;
    }

    /* Validação estrutural e de checksum do payload. */
    PayloadPacked p;

    if (!lora_parse_payload(plain, plain_len, &p))
    {
        LOG(TAG, "Payload invalido (checksum/estrutura), DESCARTADO");
        sdcard_flush();
        return;
    }

    /* Log amigável dos campos decodificados. */
    print_decoded(&p);

    /* Conversões/flags para envio ao canal IoT. */
    const bool  irr_error = (p.irradiance == 0xFFFF);
    const float irr_Wm2   = irr_error ? -1.0f : (float)p.irradiance;
    const float batt_V    = p.battery_voltage / 1000.0f;
    const float temp_C    = p.internal_temperature / 10.0f;

    /* Envio condicional ao ThingSpeak (somente se rede estiver pronta). */
    if (wifi_is_connected())
    {
        bool ok = thingspeak_update(THINGSPEAK_API_KEY, irr_Wm2, batt_V, temp_C, p.timestamp);
        if (ok)
        {
            LOG("TS", "envio OK");
        }
        else
        {
            LOG("TS", "FALHA no envio");
        }
    }
    else
    {
        LOG("TS", "sem conexao Wi-Fi, pacote NAO enviado");
    }

    /* Garante persistência do evento e mensagens no SD. */
    sdcard_flush();
}
