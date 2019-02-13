/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"
#include "hal.h"
#include "chprintf.h"

#include "ccportab.h"
#include "portab.h"
#include "usbcfg.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// printf to USB for debugging
static void uprintf(const char *fmt, ...)
{
    va_list ap;
    char umsg[200];
    va_start(ap, fmt);
    uint32_t n = chvsnprintf(umsg, sizeof(umsg), fmt, ap);
    va_end(ap);
    chnWriteTimeout(&SDU1, (const uint8_t *)umsg, n, chTimeMS2I(100));
}

/*
 * SPI TX and RX buffers.
 */
CC_ALIGN(32) uint8_t txbuf[512];
CC_ALIGN(32) uint8_t rxbuf[512];

static SPIConfig spicfg = {
  false,
  NULL,
  0,
  0,
  0, // filled in below
  SPI_CFG2_CPOL | SPI_CFG2_CPHA
};

/*
  SPI transfer, send send_len bytes, and receive recv_len response
 */
static void spi_transfer(ioline_t nss,
                         const uint8_t *send, uint32_t send_len,
                         uint8_t *recv, uint32_t recv_len)
{
    uint16_t len = send_len + recv_len;

    osalDbgAssert(len < sizeof(txbuf), "transfer too large");
    
    spiAcquireBus(&PORTAB_SPI1);
    spiStart(&PORTAB_SPI1, &spicfg);

    memset(txbuf, 0, len);
    memset(rxbuf, 0, len);
    if (send_len) {
        memcpy(txbuf, send, send_len);
    }

    cacheBufferFlush(txbuf, (len+31)&~31);
    cacheBufferInvalidate(rxbuf, (len+31)&~31);

    palWriteLine(nss, 0);

    spiExchange(&PORTAB_SPI1, len, txbuf, rxbuf);

    palWriteLine(nss, 1);

    if (recv_len) {
        memcpy(recv, &rxbuf[send_len], recv_len);
    }

    spiReleaseBus(&PORTAB_SPI1);
    spiStop(&PORTAB_SPI1);
}

/*
  calculate cfg1 register for SPI given required bus speed
 */
static uint32_t get_spi_cfg1(uint32_t bus_speed)
{
    uint32_t spi_clock_freq = STM32_SPI3CLK;

    // find first divisor that brings us below the desired SPI clock
    uint32_t i = 0;
    while (spi_clock_freq > bus_speed && i<7) {
        spi_clock_freq >>= 1;
        i++;
    }
    
    // assuming the bitrate bits are consecutive in the CR1 register,
    // we can just multiply by MBR_0 to get the right bits for the desired
    // scaling
    return (i * SPI_CFG1_MBR_0) | SPI_CFG1_DSIZE_VALUE(7); // 8 bit transfers
}

struct {
    float ms5611_temperature_C;
    bool crc_ok;
    uint8_t mpu9250_whoami;
    float mpu9250_temperature_C;
    int16_t ax, ay, az;
    uint32_t counter;
} results;

static uint16_t ms5611_prom[8];

static uint16_t ms5611_read_prom_word(uint8_t word)
{
    const uint8_t CMD_MS56XX_PROM = 0xA0;
    const uint8_t reg = CMD_MS56XX_PROM + (word << 1);
    uint8_t val[2];
    spi_transfer(PORTAB_SPI1_NSS2, &reg, 1, val, sizeof(val));
    return (val[0] << 8) | val[1];
}

/**
 * MS56XX crc4 method from datasheet for 16 bytes (8 short values)
 */
static uint16_t crc4(uint16_t *data)
{
    uint16_t n_rem = 0;
    uint8_t n_bit;

    for (uint8_t cnt = 0; cnt < 16; cnt++) {
        /* uneven bytes */
        if (cnt & 1) {
            n_rem ^= (uint8_t)((data[cnt >> 1]) & 0x00FF);
        } else {
            n_rem ^= (uint8_t)(data[cnt >> 1] >> 8);
        }

        for (n_bit = 8; n_bit > 0; n_bit--) {
            if (n_rem & 0x8000) {
                n_rem = (n_rem << 1) ^ 0x3000;
            } else {
                n_rem = (n_rem << 1);
            }
        }
    }

    return (n_rem >> 12) & 0xF;
}

static void ms5611_reset(void)
{
    uint8_t CMD_MS56XX_RESET = 0x1E;
    spi_transfer(PORTAB_SPI1_NSS2, &CMD_MS56XX_RESET, 1, NULL, 0);
}

static void ms5611_read_prom(void)
{
    bool all_zero = true;
    for (uint8_t i = 0; i < 8; i++) {
        ms5611_prom[i] = ms5611_read_prom_word(i);
        if (ms5611_prom[i] != 0) {
            all_zero = false;
        }
    }
    if (all_zero) {
        return;
    }

    /* save the read crc */
    const uint16_t crc_read = ms5611_prom[7] & 0xf;

    /* remove CRC byte */
    ms5611_prom[7] &= 0xff00;

    results.crc_ok = (crc_read == crc4(ms5611_prom));
}

static uint32_t ms5611_read_adc(void)
{
    uint8_t val[3];
    uint8_t cmd = 0; // read ADC
    spi_transfer(PORTAB_SPI1_NSS2, &cmd, 1, val, sizeof(val));
    return (val[0] << 16) | (val[1] << 8) | val[2];
}

static void read_ms5611(void)
{
    uint8_t ADDR_CMD_CONVERT_TEMPERATURE = 0x54;
    spi_transfer(PORTAB_SPI1_NSS2, &ADDR_CMD_CONVERT_TEMPERATURE, 1, NULL, 0);

    uint32_t D2 = ms5611_read_adc();
    float dT;
    float TEMP;
    float OFF;
    float SENS;

    uint16_t c1 = ms5611_prom[1];
    uint16_t c2 = ms5611_prom[2];
    uint16_t c3 = ms5611_prom[3];
    uint16_t c4 = ms5611_prom[4];
    uint16_t c5 = ms5611_prom[5];
    uint16_t c6 = ms5611_prom[6];
    
    dT = D2-(((uint32_t)c5)<<8);
    TEMP = (dT * c6)/8388608;
    OFF = c2 * 65536.0f + (c4 * dT) / 128;
    SENS = c1 * 32768.0f + (c3 * dT) / 256;

    if (TEMP < 0) {
        // second order temperature compensation when under 20 degrees C
        float T2 = (dT*dT) / 0x80000000;
        float Aux = TEMP*TEMP;
        float OFF2 = 2.5f*Aux;
        float SENS2 = 1.25f*Aux;
        TEMP = TEMP - T2;
        OFF = OFF - OFF2;
        SENS = SENS - SENS2;
    }

    results.ms5611_temperature_C = (TEMP + 2000) * 0.01f;
}

static void read_mpu9250(void)
{
    uint8_t val[14];
    uint8_t cmd = 0x3B | 0x80; // XOUT_H
    uint8_t cmd_whoami = 0x75 | 0x80;

    spi_transfer(PORTAB_SPI1_NSS, &cmd_whoami, 1, val, 1);
    results.mpu9250_whoami = val[0];

    spi_transfer(PORTAB_SPI1_NSS, &cmd, 1, val, sizeof(val));

    // a larger junk transfer to simulate reading the fifo
    uint8_t val2[11*14];
    spi_transfer(PORTAB_SPI1_NSS, &cmd, 1, val2, sizeof(val2));

    int16_t temperature = (int16_t)((val[6]<<8) | val[7]);
    const float temp_sensitivity = 1.0/340;
    const float temp_zero = 21;
    results.mpu9250_temperature_C = temperature * temp_sensitivity + temp_zero;

    int16_t ax = (int16_t)((val[0]<<8) | val[1]);
    int16_t ay = (int16_t)((val[2]<<8) | val[3]);
    int16_t az = (int16_t)((val[4]<<8) | val[5]);
    // results in milliG
    results.ax = ax / 16;
    results.ay = ay / 16;
    results.az = az / 16;
}

/*
 * SPI thread for reading from MPU9250 and MS5611
 * Reads accel, gyro and temperature at 1kHz from MPU9250 and temp/press
 * at 100Hz from MS5611
 */
static THD_WORKING_AREA(spi_thread_wa, 1024);
static THD_FUNCTION(spi_thread, p) {
  (void)p;
  chRegSetThreadName("SPI MPU9250");

  ms5611_reset();
  chThdSleepMilliseconds(100);
  ms5611_read_prom();

  while (true) {
      read_mpu9250();
      if (results.counter++ % 10 == 0) {
          palToggleLine(PORTAB_LINE_LED1);
          read_ms5611();
      }
      chThdSleepMilliseconds(1);
  }
}

/*
 * LED blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {
  (void)arg;
  chRegSetThreadName("blinker");
  while (true) {
    palToggleLine(PORTAB_LINE_LED2);
    chThdSleepMilliseconds(250);
  }
}

static void print_result(void)
{
    uprintf("MS5611(%u) Temp: %dC  MPU9250(0x%02x) Temp: %dC count=%u Accel(%d,%d,%d)\n",
            results.crc_ok,
            (int)(results.ms5611_temperature_C+0.5),
            results.mpu9250_whoami,
            (int)(results.mpu9250_temperature_C+0.5),
            results.counter,
            results.ax, results.ay, results.az);
}

#define TEST_MEM_CHUNKS 500
#define TEST_CHUNK_SIZE (1024/4)

uint32_t *mem_chunks[TEST_MEM_CHUNKS];

static uint32_t test_value(uint32_t chunk, uint32_t index)
{
    return chunk * 1753 + index * 37271;
}

static void check_corruption(void)
{
    static uint32_t counter;
    for (uint32_t i=0; i<TEST_MEM_CHUNKS; i++) {
        for (uint32_t j=0; j<TEST_CHUNK_SIZE; j++) {
            if (mem_chunks[i][j] != test_value(i, j)) {
                uprintf("Corruption 0x%08x should be 0x%08x at %u/%u\n",
                        mem_chunks[i][j], test_value(i, j),
                        i, j);
                break;
            }
        }
    }
    // pick one row to re-init, so we get some write activity to memory
    uint32_t i = (counter++) % TEST_MEM_CHUNKS;
    for (uint32_t j=0; j<TEST_CHUNK_SIZE; j++) {
        mem_chunks[i][j] = test_value(i, j);
    }
}

/*
 * Application entry point.
 */
int main(void) {
  halInit();
  chSysInit();

  portab_setup();

  // run both devices at 4MHz
  spicfg.cfg1 = get_spi_cfg1(4000000U);

  sduObjectInit(&PORTAB_SDU1);
  sduStart(&PORTAB_SDU1, &serusbcfg);

  usbDisconnectBus(serusbcfg.usbp);
  chThdSleepMilliseconds(1500);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);
  
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  chThdCreateStatic(spi_thread_wa, sizeof(spi_thread_wa),
                    NORMALPRIO + 1, spi_thread, NULL);

  for (uint32_t i=0; i<TEST_MEM_CHUNKS; i++) {
      mem_chunks[i] = chHeapAllocAligned(NULL, TEST_CHUNK_SIZE*4, 8);
      if (!mem_chunks[i]) {
          uprintf("malloc failed at %u\n", i);
      }
      for (uint32_t j=0; j<TEST_CHUNK_SIZE; j++) {
          mem_chunks[i][j] = test_value(i, j);
      }
  }
  
  /*
    print results at 10Hz
   */
  while (true) {
    print_result();
    for (uint16_t i=0; i<100; i++) {
        check_corruption();
        chThdSleepMilliseconds(1);
    }
  }
  return 0;
}
