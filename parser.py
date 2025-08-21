import serial
from datetime import datetime
import sys

# =======================
# CONFIGURAÇÕES
# =======================
PORTA_SERIAL = "COM16"          # ex.: "COM6" (Windows) ou "/dev/ttyUSB0" (Linux)
BAUD_RATE    = 115200
ARQ_TS       = datetime.now().strftime("%Y%m%d_%H%M%S")
ARQUIVO_TXT  = f"lora_log_{ARQ_TS}.txt"

# =======================
# CONEXÃO SERIAL
# =======================
try:
    ser = serial.Serial(PORTA_SERIAL, BAUD_RATE, timeout=1)
except serial.SerialException as e:
    print(f"Erro ao abrir {PORTA_SERIAL}: {e}")
    sys.exit(1)

print(f"Conectado a {PORTA_SERIAL} @ {BAUD_RATE} baud")
print(f"Gravando em: {ARQUIVO_TXT}")

try:
    with open(ARQUIVO_TXT, mode='a', encoding='utf-8') as log:
        while True:
            line_bytes = ser.readline()
            if not line_bytes:
                continue  # timeout sem dados

            # Decodifica e limpa quebras
            line = line_bytes.decode('utf-8', errors='replace').rstrip('\r\n')
            if line == "":
                continue

            # Timestamp do PC (com milissegundos)
            ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            line_out = f"[{ts}] {line}"

            # Mostra no terminal e salva no arquivo
            print(line_out, flush=True)
            log.write(line_out + "\n")
            log.flush()
except KeyboardInterrupt:
    print("\nEncerrado pelo usuário.")
except Exception as e:
    print(f"Erro: {e}")
finally:
    try:
        ser.close()
    except Exception:
        pass
