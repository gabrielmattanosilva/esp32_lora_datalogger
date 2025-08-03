import serial
from datetime import datetime

# CONFIGURAÇÕES

porta_serial = "COM6"
baud_rate = 115200
timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
arquivo_txt = f"lora_log_{timestamp}.txt"

# Inicia conexão serial
ser = serial.Serial(porta_serial, baud_rate, timeout=1)
print(f"Conectado à {porta_serial} @ {baud_rate} baud")

try:
    with open(arquivo_txt, mode='a', encoding='utf-8') as log:
        while True:
            linha = ser.readline().decode('utf-8', errors='ignore').strip()
            if linha.startswith("Received packet '") and "' with RSSI " in linha:
                try:
                    msg = linha.split("Received packet '")[1].split("' with RSSI")[0].strip()
                    rssi = linha.split("with RSSI")[1].strip()
                    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                    linha_log = f"[{timestamp}] {msg} (RSSI: {rssi})\n"
                    print(linha_log.strip())
                    log.write(linha_log)
                    log.flush()  # Garante que grava em tempo real
                except Exception as e:
                    print(f"Erro ao processar linha: {linha}")
except KeyboardInterrupt:
    print("\nEncerrado pelo usuário.")
finally:
    ser.close()
