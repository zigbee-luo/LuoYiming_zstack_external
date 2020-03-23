/*
 *  ======== ti_drivers_config.h ========
 *  Configured TI-Drivers module declarations
 *
 *  DO NOT EDIT - This file is generated for the CC1352P_2_LAUNCHXL
 *  by the SysConfig tool.
 */
#ifndef ti_drivers_config_h
#define ti_drivers_config_h

#define CONFIG_SYSCONFIG_PREVIEW

#define CONFIG_CC1352P_2_LAUNCHXL

#ifndef DeviceFamily_CC13X2
#define DeviceFamily_CC13X2
#endif

#include <ti/devices/DeviceFamily.h>

#include <stdint.h>

/* support C++ sources */
#ifdef __cplusplus
extern "C" {
#endif


/*
 *  ======== AESCBC ========
 */

#define CONFIG_AESCBC_0             0


/*
 *  ======== AESCCM ========
 */

#define CONFIG_AESCCM_0             0


/*
 *  ======== AESECB ========
 */

#define CONFIG_AESECB_0             0


/*
 *  ======== ECDH ========
 */

#define CONFIG_ECDH_0               0


/*
 *  ======== ECDSA ========
 */

#define CONFIG_ECDSA_0              0


/*
 *  ======== ECJPAKE ========
 */

#define CONFIG_ECJPAKE_0            0


/*
 *  ======== GPIO ========
 */

#define CONFIG_GPIO_0               0
#define CONFIG_GPIO_1               1
#define CONFIG_GPIO_2               2
#define CONFIG_GPIO_3               3
#define CONFIG_GPIO_4               4

/* LEDs are active high */
#define CONFIG_GPIO_LED_ON  (1)
#define CONFIG_GPIO_LED_OFF (0)

#define CONFIG_LED_ON  (CONFIG_GPIO_LED_ON)
#define CONFIG_LED_OFF (CONFIG_GPIO_LED_OFF)


/*
 *  ======== NVS ========
 */

#define CONFIG_NVSINTERNAL           0
#define CONFIG_NVSEXTERNAL           1


/*
 *  ======== PIN ========
 */

/* Includes */
#include <ti/drivers/PIN.h>

/* Externs */
extern const PIN_Config BoardGpioInitTable[];

/* LaunchPad Button BTN-1 (left), Parent Signal: CONFIG_GPIO_2 GPIO Pin, (DIO15) */
#define CONFIG_PIN_BTN1    0x0000000f
/* LaunchPad Button BTN-2 (right), Parent Signal: CONFIG_GPIO_3 GPIO Pin, (DIO14) */
#define CONFIG_PIN_BTN2    0x0000000e
/* XDS110 UART, Parent Signal: CONFIG_UART_0 TX, (DIO13) */
#define CONFIG_PIN_UART_TX    0x0000000d
/* XDS110 UART, Parent Signal: CONFIG_UART_0 RX, (DIO12) */
#define CONFIG_PIN_UART_RX    0x0000000c
/* LaunchPad LED Red, Parent Signal: CONFIG_GPIO_0 GPIO Pin, (DIO6) */
#define CONFIG_PIN_RLED    0x00000006
/* LaunchPad LED Green, Parent Signal: CONFIG_GPIO_1 GPIO Pin, (DIO7) */
#define CONFIG_PIN_GLED    0x00000007
/* SPI Flash Device Chip Select, Parent Signal: CONFIG_GPIO_4 GPIO Pin, (DIO20) */
#define CONFIG_PIN_2    0x00000014
/* SPI Bus, Parent Signal: CONFIG_SPI_0 SCLK, (DIO10) */
#define CONFIG_PIN_SPI_SCLK    0x0000000a
/* SPI Bus, Parent Signal: CONFIG_SPI_0 MISO, (DIO8) */
#define CONFIG_PIN_SPI_MISO    0x00000008
/* SPI Bus, Parent Signal: CONFIG_SPI_0 MOSI, (DIO9) */
#define CONFIG_PIN_SPI_MOSI    0x00000009
/* CONFIG_RF_24GHZ (DIO28) */
#define CONFIG_RF_24GHZ    0x0000001c
/* CONFIG_RF_HIGH_PA (DIO29) */
#define CONFIG_RF_HIGH_PA    0x0000001d
/* CONFIG_RF_SUB1GHZ (DIO30) */
#define CONFIG_RF_SUB1GHZ    0x0000001e


/*
 *  ======== RF ========
 */
#define Board_DIO30_RFSW 0x0000001e


/*
 *  ======== SHA2 ========
 */

#define CONFIG_SHA2_0               0


/*
 *  ======== SPI ========
 */

#define CONFIG_SPI_0                0


/*
 *  ======== TRNG ========
 */

#define CONFIG_TRNG_0               0


/*
 *  ======== UART ========
 */

#define CONFIG_UART_0               0


/*
 *  ======== Board_init ========
 *  Perform all required TI-Drivers initialization
 *
 *  This function should be called once at a point before any use of
 *  TI-Drivers.
 */
extern void Board_init(void);

/*
 *  ======== Board_initGeneral ========
 *  (deprecated)
 *
 *  Board_initGeneral() is defined purely for backward compatibility.
 *
 *  All new code should use Board_init() to do any required TI-Drivers
 *  initialization _and_ use <Driver>_init() for only where specific drivers
 *  are explicitly referenced by the application.  <Driver>_init() functions
 *  are idempotent.
 */
#define Board_initGeneral Board_init

#ifdef __cplusplus
}
#endif

#endif /* include guard */
