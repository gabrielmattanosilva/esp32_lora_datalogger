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
#define SPI_SS      5

/* Módulo LoRa SX1278 */
#define SX1278_DIO0     2
#define SX1278_RST      22

/* ADC - Tensão da Bateria */
#define BATT_VOLT       36

#endif /* PINS_H */
