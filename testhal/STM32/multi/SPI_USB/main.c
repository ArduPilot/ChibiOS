/*
  demonstration of memory corruption with USB and SPI on a STM32H743
*/

#pragma GCC optimize("O0")

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

#define HAL_SPI_USE_POLLED 0

// printf to USB
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
  0, // cfg1 filled in below
  SPI_CFG2_CPOL | SPI_CFG2_CPHA
};

#define LINE_NSS               PAL_LINE(GPIOF, 5U)

static bool data_ok;

/*
  SPI transfer, send send_len bytes, and receive recv_len response
 */
static void spi_transfer(const uint8_t *send, uint32_t send_len,
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

    cacheBufferFlush(txbuf, len);
    memset(rxbuf, 0x41, sizeof(rxbuf));
    cacheBufferFlush(rxbuf, sizeof(rxbuf));
    cacheBufferInvalidate(rxbuf, len);

    palWriteLine(LINE_NSS, 0);

#if HAL_SPI_USE_POLLED == 1
    for (uint16_t i=0; i<len; i++) {
        uint8_t ret = spiPolledExchange(&PORTAB_SPI1, txbuf[i]);
        rxbuf[i] = ret;
    }
#else
    spiExchange(&PORTAB_SPI1, len, txbuf, rxbuf);
#endif

    palWriteLine(LINE_NSS, 1);

    if (recv_len) {
        memcpy(recv, &rxbuf[send_len], recv_len);
    }

    spiReleaseBus(&PORTAB_SPI1);

    // the spiStop is not needed, but is done by ArduPilot, so is
    // included here
    spiStop(&PORTAB_SPI1);
}

/*
  calculate cfg1 register for SPI given required bus speed
 */
static uint32_t get_spi_cfg1(uint32_t bus_speed)
{
    uint32_t spi_clock_freq = STM32_SPI1CLK;

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
    uint32_t counter;
    uint32_t corrupt_count;
    mutex_t mtx;
} results;

/*
 LED blinker thread
 */
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {
  (void)arg;
  chRegSetThreadName("blinker");
  while (true) {
      palToggleLine(HAL_GPIO_PIN_LED_GREEN);
      // use rapid blink when we have corruption
      uint16_t time = results.corrupt_count?50:500;
      chThdSleepMilliseconds(time);
  }
}

/*
 * Application entry point.
 */
int main(void) {
  halInit();
  chSysInit();

  portab_setup();

#ifdef HAL_GPIO_PIN_nVDD_5V_PERIPH_EN
    palWriteLine(HAL_GPIO_PIN_nVDD_5V_PERIPH_EN, 0);
#endif
#ifdef HAL_GPIO_PIN_nVDD_5V_HIPOWER_EN
    palWriteLine(HAL_GPIO_PIN_nVDD_5V_HIPOWER_EN, 0);
#endif
#ifdef HAL_GPIO_PIN_VDD_3V3_SENSORS_EN
    palWriteLine(HAL_GPIO_PIN_VDD_3V3_SENSORS_EN, 1);
#endif

  // run SPI at 4MHz
  spicfg.cfg1 = get_spi_cfg1(4000000U);

  sduObjectInit(&PORTAB_SDU1);
  sduStart(&PORTAB_SDU1, &serusbcfg);

  usbDisconnectBus(serusbcfg.usbp);
  chThdSleepMilliseconds(1500);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);

  chMtxObjectInit(&results.mtx);

  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  while (true) {
      palToggleLine(HAL_GPIO_PIN_LED_BLUE);

      chMtxLock(&results.mtx);
      results.counter++;
      chMtxUnlock(&results.mtx);

      uint8_t reg = 0x9f;
      uint8_t val[18];
      spi_transfer(&reg, 1, val, sizeof(val));

      for (uint8_t i=0; i<sizeof(val); i++) {
          uprintf("%02x ", val[i]);
      }
      uprintf("\n");
      bool new_data_ok = true;
      for (uint8_t i=0; i<9; i++) {
          if (val[i] == 0) {
              new_data_ok = false;
          }
      }
      data_ok = new_data_ok;

      if (data_ok) {
          chThdSleepMilliseconds(500);
      } else {
          chThdSleepMilliseconds(50);
      }
  }
  return 0;
}
