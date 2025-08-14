#include <LoRa.h>
#include <SPI.h>

// ======= Pines do rádio (ajuste se necessário) =======
#define ss   5
#define rst  22
#define dio0 2

// ======= Chave AES-128 (16 bytes) =======
static const uint8_t AES_KEY[16] = {
  0x00,0x01,0x03,0x04,0x05,0x06,0x07,0x08,
  0x09,0x0A,0x0B,0x0C,0x0D,0x5E,0x0F,0x10
};

// ======= Estrutura do payload (11 bytes) =======
typedef struct __attribute__((packed)) {
  uint16_t irradiance;           // W/m² (0..2000, 0xFFFF = erro)
  uint16_t battery_voltage;      // mV
  int16_t  internal_temperature; // °C ×10
  uint32_t timestamp;            // s
  uint8_t  checksum;             // soma 8-bit dos 10 primeiros bytes
} PayloadPacked;
static_assert(sizeof(PayloadPacked) == 11, "Payload deve ter 11 bytes");

// ======= Utilitários =======
static uint8_t checksum8(const uint8_t *data, size_t len) {
  uint8_t s = 0;
  for (size_t i = 0; i < len; ++i) s += data[i];
  return s;
}

static uint16_t rd_le_u16(const uint8_t *b) {
  return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}
static int16_t rd_le_i16(const uint8_t *b) {
  return (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}
static uint32_t rd_le_u32(const uint8_t *b) {
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

// ======= AES-128 CBC (mbedTLS) =======
#include <mbedtls/aes.h>

static bool pkcs7_unpad(uint8_t *buf, size_t in_len, size_t *out_len) {
  if (in_len == 0 || (in_len % 16) != 0) return false;
  uint8_t pad = buf[in_len - 1];
  if (pad == 0 || pad > 16) return false;
  for (size_t i = 0; i < pad; ++i) {
    if (buf[in_len - 1 - i] != pad) return false;
  }
  *out_len = in_len - pad;
  return true;
}

static bool aes128_cbc_decrypt(const uint8_t key[16],
                               const uint8_t iv[16],
                               const uint8_t *in, size_t in_len,
                               uint8_t *out, size_t *out_len)
{
  if ((in_len % 16) != 0) return false;

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  if (mbedtls_aes_setkey_dec(&ctx, key, 128) != 0) {
    mbedtls_aes_free(&ctx);
    return false;
  }

  uint8_t iv_copy[16];
  memcpy(iv_copy, iv, 16);

  int rc = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, in_len, iv_copy, in, out);
  mbedtls_aes_free(&ctx);
  if (rc != 0) return false;

  return pkcs7_unpad(out, in_len, out_len);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { /* aguarda USB */ }

  Serial.println("LoRa Receiver (AES-128-CBC + IV || CT)");

  LoRa.setPins(ss, rst, dio0);
  while (!LoRa.begin(433E6)) {
    Serial.println(".");
    delay(500);
  }
  LoRa.setSyncWord(0xA5);
  Serial.println("LoRa Initializing OK!");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) return;

  // Lê tudo para buffer (até 128 bytes por segurança)
  uint8_t buf[128];
  int n = 0;
  while (LoRa.available() && n < (int)sizeof(buf)) {
    buf[n++] = (uint8_t)LoRa.read();
  }

  Serial.print("RX [");
  Serial.print(n);
  Serial.print(" B] RSSI=");
  Serial.print(LoRa.packetRssi());
  Serial.print(" SNR=");
  Serial.println(LoRa.packetSnr());

  // Hexdump
  for (int i = 0; i < n; ++i) {
    if (i && (i % 16 == 0)) Serial.println();
    if (buf[i] < 16) Serial.print('0');
    Serial.print(buf[i], HEX);
    Serial.print(' ');
  }
  Serial.println();

  // Valida tamanho mínimo: IV(16) + CT(>=16 e múltiplo de 16)
  if (n < 32) {
    Serial.println(">> Pacote muito curto. Esperado >= 32 bytes (IV16 + CT16+).");
    return;
  }
  int ct_len = n - 16;
  if ((ct_len % 16) != 0) {
    Serial.println(">> Tamanho do ciphertext não é múltiplo de 16.");
    return;
  }

  const uint8_t *iv = &buf[0];
  const uint8_t *ct = &buf[16];

  // Descriptografa (OUT no máximo = ct_len; após unpad deve dar 11 bytes)
  uint8_t out[128];
  size_t out_len = 0;
  if (!aes128_cbc_decrypt(AES_KEY, iv, ct, (size_t)ct_len, out, &out_len)) {
    Serial.println(">> Falha na descriptografia (CBC/PKCS#7).");
    return;
  }

  if (out_len != sizeof(PayloadPacked)) {
    Serial.print(">> Tamanho após unpad inesperado: ");
    Serial.print(out_len);
    Serial.print(" (esperado ");
    Serial.print(sizeof(PayloadPacked));
    Serial.println(").");
    return;
  }

  // Valida checksum do payload (soma dos 10 primeiros bytes)
  uint8_t sum = checksum8(out, sizeof(PayloadPacked) - 1);
  if (sum != out[sizeof(PayloadPacked) - 1]) {
    Serial.print(">> Checksum invalido! calc=0x");
    if (sum < 16) Serial.print('0');
    Serial.print(sum, HEX);
    Serial.print(" recv=0x");
    if (out[sizeof(PayloadPacked) - 1] < 16) Serial.print('0');
    Serial.println(out[sizeof(PayloadPacked) - 1], HEX);
    return;
  }

  // Decodifica campos (little-endian)
  PayloadPacked p;
  p.irradiance           = rd_le_u16(&out[0]);
  p.battery_voltage      = rd_le_u16(&out[2]);
  p.internal_temperature = rd_le_i16(&out[4]);
  p.timestamp            = rd_le_u32(&out[6]);
  p.checksum             = out[10];

  // Converte para unidades amigáveis
  bool  irr_error = (p.irradiance == 0xFFFF);
  float batt_V    = p.battery_voltage / 1000.0f;
  float temp_C    = p.internal_temperature / 10.0f;

  // Imprime
  Serial.println("---- Decodificado (AES-CBC) ----");
  if (irr_error) {
    Serial.println("Irradiancia: ERRO (0xFFFF)");
  } else {
    Serial.print("Irradiancia: ");
    Serial.print(p.irradiance);
    Serial.println(" W/m^2");
  }

  Serial.print("Bateria: ");
  Serial.print(batt_V, 3);
  Serial.println(" V");

  Serial.print("Temp. interna: ");
  Serial.print(temp_C, 1);
  Serial.println(" C");

  Serial.print("Timestamp: ");
  Serial.print(p.timestamp);
  Serial.println(" s");

  Serial.print("Checksum: 0x");
  if (p.checksum < 16) Serial.print('0');
  Serial.println(p.checksum, HEX);
  Serial.println("--------------------------------");
}
