/**
 * @file pins.h
 * @brief Definição dos pinos utilizados no hardware.
 */

#ifndef PINS_H
#define PINS_H

/* Interface I2C */
#define I2C_SDA 21
#define I2C_SCL 22

/* Interface SPI */
#define SPI_SCK     18
#define SPI_MISO    19
#define SPI_MOSI    23

/* Módulo LoRa SX1278 */
#define SX1278_DIO0     2
#define SX1278_RST      26
#define SX1278_SPI_SS   5

/* Módulo de Cartão SD */
#define SD_SPI_CS       4

#endif /* PINS_H */
