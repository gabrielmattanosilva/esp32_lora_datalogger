/**
 * @file pins.h
 * @brief Definição dos pinos utilizados no hardware.
 */

#ifndef PINS_H
#define PINS_H

/* Interface SPI */
#define SPI_SCK     18
#define SPI_MISO    19
#define SPI_MOSI    23

/* Módulo LoRa SX1278 */
#define SX1278_DIO0     2
#define SX1278_RST      22
#define SX1278_SPI_SS   5

/* Módulo de Cartão SD */
#define SD_SPI_CS       4

/* ADC - Tensão da Bateria */
#define BATT_VOLT       36

#endif /* PINS_H */
