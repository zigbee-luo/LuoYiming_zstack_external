/*
 *  ======== ti_drivers_config.c ========
 *  Configured TI-Drivers module definitions
 *
 *  DO NOT EDIT - This file is generated for the CC1352R1_LAUNCHXL
 *  by the SysConfig tool.
 */

#include <stddef.h>

#ifndef DeviceFamily_CC13X2
#define DeviceFamily_CC13X2
#endif

#include <ti/devices/DeviceFamily.h>

#include "ti_drivers_config.h"

/*
 *  =============================== AESCBC ===============================
 */

#include <ti/drivers/AESCBC.h>
#include <ti/drivers/aescbc/AESCBCCC26XX.h>

#define CONFIG_AESCBC_COUNT 1

AESCBCCC26XX_Object aescbcCC26XXObjects[CONFIG_AESCBC_COUNT];

/*
 *  ======== aescbcCC26XXHWAttrs ========
 */
const AESCBCCC26XX_HWAttrs aescbcCC26XXHWAttrs[CONFIG_AESCBC_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const AESCBC_Config AESCBC_config[CONFIG_AESCBC_COUNT] = {
    {   /* CONFIG_AESCBC_0 */
        .object  = &aescbcCC26XXObjects[CONFIG_AESCBC_0],
        .hwAttrs = &aescbcCC26XXHWAttrs[CONFIG_AESCBC_0]
    },
};

const uint_least8_t AESCBC_count = CONFIG_AESCBC_COUNT;


/*
 *  =============================== AESCCM ===============================
 */

#include <ti/drivers/AESCCM.h>
#include <ti/drivers/aesccm/AESCCMCC26XX.h>

#define CONFIG_AESCCM_COUNT 1

AESCCMCC26XX_Object aesccmCC26XXObjects[CONFIG_AESCCM_COUNT];

/*
 *  ======== aesccmCC26XXHWAttrs ========
 */
const AESCCMCC26XX_HWAttrs aesccmCC26XXHWAttrs[CONFIG_AESCCM_COUNT] = {
    {
        .intPriority = 0x40,
    },
};

const AESCCM_Config AESCCM_config[CONFIG_AESCCM_COUNT] = {
    {   /* CONFIG_AESCCM_0 */
        .object  = &aesccmCC26XXObjects[CONFIG_AESCCM_0],
        .hwAttrs = &aesccmCC26XXHWAttrs[CONFIG_AESCCM_0]
    },
};

const uint_least8_t AESCCM_count = CONFIG_AESCCM_COUNT;


/*
 *  =============================== AESECB ===============================
 */

#include <ti/drivers/AESECB.h>
#include <ti/drivers/aesecb/AESECBCC26XX.h>

#define CONFIG_AESECB_COUNT 1

AESECBCC26XX_Object aesecbCC26XXObjects[CONFIG_AESECB_COUNT];

/*
 *  ======== aesecbCC26XXHWAttrs ========
 */
const AESECBCC26XX_HWAttrs aesecbCC26XXHWAttrs[CONFIG_AESECB_COUNT] = {
    {
        .intPriority = 0x20,
    },
};

const AESECB_Config AESECB_config[CONFIG_AESECB_COUNT] = {
    {   /* CONFIG_AESECB_0 */
        .object  = &aesecbCC26XXObjects[CONFIG_AESECB_0],
        .hwAttrs = &aesecbCC26XXHWAttrs[CONFIG_AESECB_0]
    },
};


const uint_least8_t AESECB_count = CONFIG_AESECB_COUNT;



/*
 *  =============================== DMA ===============================
 */

#include <ti/drivers/dma/UDMACC26XX.h>
#include <ti/devices/cc13x2_cc26x2/driverlib/udma.h>
#include <ti/devices/cc13x2_cc26x2/inc/hw_memmap.h>

UDMACC26XX_Object udmaCC26XXObject;

const UDMACC26XX_HWAttrs udmaCC26XXHWAttrs = {
    .baseAddr        = UDMA0_BASE,
    .powerMngrId     = PowerCC26XX_PERIPH_UDMA,
    .intNum          = INT_DMA_ERR,
    .intPriority     = (~0)
};

const UDMACC26XX_Config UDMACC26XX_config[1] = {
    {
        .object         = &udmaCC26XXObject,
        .hwAttrs        = &udmaCC26XXHWAttrs,
    },
};


/*
 *  =============================== ECDH ===============================
 */

#include <ti/drivers/ECDH.h>
#include <ti/drivers/ecdh/ECDHCC26X2.h>

#define CONFIG_ECDH_COUNT 1

ECDHCC26X2_Object ecdhCC26X2Objects[CONFIG_ECDH_COUNT];

/*
 *  ======== ecdhCC26X2HWAttrs ========
 */
const ECDHCC26X2_HWAttrs ecdhCC26X2HWAttrs[CONFIG_ECDH_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const ECDH_Config ECDH_config[CONFIG_ECDH_COUNT] = {
    {   /* CONFIG_ECDH_0 */
        .object         = &ecdhCC26X2Objects[CONFIG_ECDH_0],
        .hwAttrs        = &ecdhCC26X2HWAttrs[CONFIG_ECDH_0]
    },
};

const uint_least8_t ECDH_count = CONFIG_ECDH_COUNT;


/*
 *  =============================== ECDSA ===============================
 */

#include <ti/drivers/ECDSA.h>
#include <ti/drivers/ecdsa/ECDSACC26X2.h>

#define CONFIG_ECDSA_COUNT 1

ECDSACC26X2_Object ecdsaCC26X2Objects[CONFIG_ECDSA_COUNT];

/*
 *  ======== ecdsaCC26X2HWAttrs ========
 */
const ECDSACC26X2_HWAttrs ecdsaCC26X2HWAttrs[CONFIG_ECDSA_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const ECDSA_Config ECDSA_config[CONFIG_ECDSA_COUNT] = {
    {   /* CONFIG_ECDSA_0 */
        .object         = &ecdsaCC26X2Objects[CONFIG_ECDSA_0],
        .hwAttrs        = &ecdsaCC26X2HWAttrs[CONFIG_ECDSA_0]
    },
};

const uint_least8_t ECDSA_count = CONFIG_ECDSA_COUNT;


/*
 *  =============================== ECJPAKE ===============================
 */

#include <ti/drivers/ECJPAKE.h>
#include <ti/drivers/ecjpake/ECJPAKECC26X2.h>

#define CONFIG_ECJPAKE_COUNT 1

ECJPAKECC26X2_Object ecjpakeCC26X2Objects[CONFIG_ECJPAKE_COUNT];

/*
 *  ======== ecjpakeCC26X2HWAttrs ========
 */
const ECJPAKECC26X2_HWAttrs ecjpakeCC26X2HWAttrs[CONFIG_ECJPAKE_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const ECJPAKE_Config ECJPAKE_config[CONFIG_ECJPAKE_COUNT] = {
    {   /* CONFIG_ECJPAKE_0 */
        .object         = &ecjpakeCC26X2Objects[CONFIG_ECJPAKE_0],
        .hwAttrs        = &ecjpakeCC26X2HWAttrs[CONFIG_ECJPAKE_0]
    },
};

const uint_least8_t ECJPAKE_count = CONFIG_ECJPAKE_COUNT;


/*
 *  =============================== GPIO ===============================
 */

#include <ti/drivers/GPIO.h>
#include <ti/drivers/gpio/GPIOCC26XX.h>

/*
 *  ======== gpioPinConfigs ========
 *  Array of Pin configurations
 */
GPIO_PinConfig gpioPinConfigs[] = {
    /* CONFIG_GPIO_RLED : LaunchPad LED Red */
    GPIOCC26XX_DIO_06 | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_STR_MED | GPIO_CFG_OUT_LOW,
    /* CONFIG_GPIO_GLED : LaunchPad LED Green */
    GPIOCC26XX_DIO_07 | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_STR_MED | GPIO_CFG_OUT_LOW,
    /* CONFIG_GPIO_BTN1 : LaunchPad Button BTN-1 (Left) */
    GPIOCC26XX_DIO_15 | GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_NONE,
    /* CONFIG_GPIO_BTN2 : LaunchPad Button BTN-2 (Right) */
    GPIOCC26XX_DIO_14 | GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_NONE,
    /* SPI Flash Slave Select GPIO Instance */
    GPIOCC26XX_DIO_20 | GPIO_CFG_OUT_STD | GPIO_CFG_OUT_STR_MED | GPIO_CFG_OUT_HIGH,
};

/*
 *  ======== gpioCallbackFunctions ========
 *  Array of callback function pointers
 *
 *  NOTE: Unused callback entries can be omitted from the callbacks array to
 *  reduce memory usage by enabling callback table optimization
 *  (GPIO.optimizeCallbackTableSize = true)
 */
GPIO_CallbackFxn gpioCallbackFunctions[] = {
    /* CONFIG_GPIO_RLED : LaunchPad LED Red */
    NULL,
    /* CONFIG_GPIO_GLED : LaunchPad LED Green */
    NULL,
    /* CONFIG_GPIO_BTN1 : LaunchPad Button BTN-1 (Left) */
    NULL,
    /* CONFIG_GPIO_BTN2 : LaunchPad Button BTN-2 (Right) */
    NULL,
    /* SPI Flash Slave Select GPIO Instance */
    NULL,
};

/*
 *  ======== GPIOCC26XX_config ========
 */
const GPIOCC26XX_Config GPIOCC26XX_config = {
    .pinConfigs = (GPIO_PinConfig *)gpioPinConfigs,
    .callbacks = (GPIO_CallbackFxn *)gpioCallbackFunctions,
    .numberOfPinConfigs = 5,
    .numberOfCallbacks = 5,
    .intPriority = (~0)
};


/*
 *  =============================== NVS ===============================
 */

#include <ti/drivers/NVS.h>
#include <ti/drivers/nvs/NVSCC26XX.h>

/*
 *  NVSCC26XX Internal NVS flash region definitions
 *
 * Place uninitialized char arrays at addresses
 * corresponding to the 'regionBase' addresses defined in
 * the configured NVS regions. These arrays are used as
 * place holders so that the linker will not place other
 * content there.
 *
 * For GCC targets, the char arrays are each placed into
 * the shared ".nvs" section. The user must add content to
 * their GCC linker command file to place the .nvs section
 * at the lowest 'regionBase' address specified in their NVS
 * regions.
 */

#if defined(__TI_COMPILER_VERSION__)

#pragma LOCATION(flashBuf0, 0x52000);
#pragma NOINIT(flashBuf0);
static char flashBuf0[0x4000];

#elif defined(__IAR_SYSTEMS_ICC__)

__no_init static char flashBuf0[0x4000] @ 0x52000;

#elif defined(__GNUC__)

__attribute__ ((section (".nvs")))
static char flashBuf0[0x4000];

#endif

NVSCC26XX_Object nvsCC26XXObjects[1];

static const NVSCC26XX_HWAttrs nvsCC26XXHWAttrs[1] = {
    /* CONFIG_NVSINTERNAL */
    {
        .regionBase = (void *) flashBuf0,
        .regionSize = 0x4000
    },
};

#include <ti/drivers/nvs/NVSSPI25X.h>

/*
 *  NVSSPI25X External NVS flash region definitions
 */

/*
 * Provide write verification buffer whose size is
 * the largest specified Verification Buffer Size
 */

static uint8_t verifyBuf[64];

NVSSPI25X_Object nvsSPI25XObjects[1];

static const NVSSPI25X_HWAttrs nvsSPI25XHWAttrs[1] = {
    /* CONFIG_NVSEXTERNAL */
    /* MX25R8035F 1MB SPI Flash */
    {
        .regionBaseOffset = 0x0,
        .regionSize = 0x256000,
        .sectorSize = 0x1000,
        .verifyBuf = verifyBuf,
        .verifyBufSize = 64,
        /* NVS opens SPI */
        .spiHandle = NULL,
        /* SPI driver index */
        .spiIndex = CONFIG_GPIO_SPI_SS,
        .spiBitRate = 4000000,
        /* GPIO driver pin index */
        .spiCsnGpioIndex = CONFIG_GPIO_0,
        .statusPollDelayUs = 100
    },
};

#define CONFIG_NVS_COUNT 2

const NVS_Config NVS_config[CONFIG_NVS_COUNT] = {
    /* CONFIG_NVSINTERNAL */
    {
        .fxnTablePtr = &NVSCC26XX_fxnTable,
        .object = &nvsCC26XXObjects[0],
        .hwAttrs = &nvsCC26XXHWAttrs[0],
    },
    /* CONFIG_NVSEXTERNAL */
    /* MX25R8035F 1MB SPI Flash */
    {
        .fxnTablePtr = &NVSSPI25X_fxnTable,
        .object = &nvsSPI25XObjects[0],
        .hwAttrs = &nvsSPI25XHWAttrs[0],
    },
};

const uint_least8_t NVS_count = CONFIG_NVS_COUNT;


/*
 *  =============================== PIN ===============================
 */

#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>

const PIN_Config BoardGpioInitTable[] = {
    /* XDS110 UART, Parent Signal: CONFIG_DISPLAY_UART TX, (DIO13) */
    CONFIG_PIN_UART_TX | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MED,
    /* XDS110 UART, Parent Signal: CONFIG_DISPLAY_UART RX, (DIO12) */
    CONFIG_PIN_UART_RX | PIN_INPUT_EN | PIN_PULLDOWN | PIN_IRQ_DIS,
    /* LaunchPad LED Red, Parent Signal: CONFIG_GPIO_RLED GPIO Pin, (DIO6) */
    CONFIG_PIN_RLED | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MED,
    /* LaunchPad LED Green, Parent Signal: CONFIG_GPIO_GLED GPIO Pin, (DIO7) */
    CONFIG_PIN_GLED | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MED,
    /* LaunchPad Button BTN-1 (Left), Parent Signal: CONFIG_GPIO_BTN1 GPIO Pin, (DIO15) */
    CONFIG_PIN_BTN1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_DIS,
    /* LaunchPad Button BTN-2 (Right), Parent Signal: CONFIG_GPIO_BTN2 GPIO Pin, (DIO14) */
    CONFIG_PIN_BTN2 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_DIS,
    /* SPI Flash Device Chip Select, Parent Signal: CONFIG_GPIO_0 GPIO Pin, (DIO20) */
    CONFIG_PIN_0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MED,
    /* SPI Bus, Parent Signal: CONFIG_GPIO_SPI_SS SCLK, (DIO10) */
    CONFIG_PIN_SPI_SCLK | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MED,
    /* SPI Bus, Parent Signal: CONFIG_GPIO_SPI_SS MISO, (DIO8) */
    CONFIG_PIN_SPI_MISO | PIN_INPUT_EN | PIN_NOPULL | PIN_IRQ_DIS,
    /* SPI Bus, Parent Signal: CONFIG_GPIO_SPI_SS MOSI, (DIO9) */
    CONFIG_PIN_SPI_MOSI | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MED,

    PIN_TERMINATE
};

const PINCC26XX_HWAttrs PINCC26XX_hwAttrs = {
    .intPriority = (~0),
    .swiPriority = 0
};


/*
 *  =============================== Power ===============================
 */
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26X2.h>
#include "ti_drivers_config.h"

extern void PowerCC26XX_standbyPolicy(void);
extern bool PowerCC26XX_calibrate(unsigned int);

const PowerCC26X2_Config PowerCC26X2_config = {
    .enablePolicy             = true,
    .policyInitFxn            = NULL,
    .policyFxn                = PowerCC26XX_standbyPolicy,
    .calibrateFxn             = PowerCC26XX_calibrate,
    .calibrateRCOSC_LF        = true,
    .calibrateRCOSC_HF        = true,
    .enableTCXOFxn            = NULL
};



/*
 *  =============================== RF Driver ===============================
 */

#include <ti/drivers/rf/RF.h>

const RFCC26XX_HWAttrsV2 RFCC26XX_hwAttrs = {
    .hwiPriority        = (~0),
    .swiPriority        = (uint8_t)0,
    .xoscHfAlwaysNeeded = true,
    .globalCallback     = NULL,
    .globalEventMask    = 0
};



/*
 *  =============================== SHA2 ===============================
 */

#include <ti/drivers/SHA2.h>
#include <ti/drivers/sha2/SHA2CC26X2.h>

#define CONFIG_SHA2_COUNT 1

SHA2CC26X2_Object sha2CC26X2Objects[CONFIG_SHA2_COUNT];

/*
 *  ======== sha2CC26X2HWAttrs ========
 */
const SHA2CC26X2_HWAttrs sha2CC26X2HWAttrs[CONFIG_SHA2_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const SHA2_Config SHA2_config[CONFIG_SHA2_COUNT] = {
    {   /* CONFIG_SHA2_0 */
        .object         = &sha2CC26X2Objects[CONFIG_SHA2_0],
        .hwAttrs        = &sha2CC26X2HWAttrs[CONFIG_SHA2_0]
    },
};

const uint_least8_t SHA2_count = CONFIG_SHA2_COUNT;


/*
 *  =============================== SPI DMA ===============================
 */
#include <ti/drivers/SPI.h>
#include <ti/drivers/spi/SPICC26X2DMA.h>

#define CONFIG_SPI_COUNT 1

/*
 *  ======== spiCC26X2DMAObjects ========
 */
SPICC26X2DMA_Object spiCC26X2DMAObjects[CONFIG_SPI_COUNT];

/*
 *  ======== spiCC26X2DMAHWAttrs ========
 */
const SPICC26X2DMA_HWAttrs spiCC26X2DMAHWAttrs[CONFIG_SPI_COUNT] = {
    /* CONFIG_GPIO_SPI_SS */
    /* SPI Bus */
    {
        .baseAddr = SSI0_BASE,
        .intNum = INT_SSI0_COMB,
        .intPriority = (~0),
        .swiPriority = 0,
        .powerMngrId = PowerCC26XX_PERIPH_SSI0,
        .defaultTxBufValue = ~0,
        .rxChannelBitMask = 1<<UDMA_CHAN_SSI0_RX,
        .txChannelBitMask = 1<<UDMA_CHAN_SSI0_TX,
        .minDmaTransferSize = 10,
        .mosiPin = IOID_9,
        .misoPin = IOID_8,
        .clkPin  = IOID_10,
        .csnPin  = PIN_UNASSIGNED
    },
};

/*
 *  ======== SPI_config ========
 */
const SPI_Config SPI_config[CONFIG_SPI_COUNT] = {
    /* CONFIG_GPIO_SPI_SS */
    /* SPI Bus */
    {
        .fxnTablePtr = &SPICC26X2DMA_fxnTable,
        .object = &spiCC26X2DMAObjects[CONFIG_GPIO_SPI_SS],
        .hwAttrs = &spiCC26X2DMAHWAttrs[CONFIG_GPIO_SPI_SS]
    },
};

const uint_least8_t SPI_count = CONFIG_SPI_COUNT;


/*
 *  =============================== TRNG ===============================
 */

#include <ti/drivers/TRNG.h>
#include <ti/drivers/trng/TRNGCC26XX.h>

#define CONFIG_TRNG_COUNT 1

TRNGCC26XX_Object trngCC26XXObjects[CONFIG_TRNG_COUNT];

/*
 *  ======== trngCC26XXHWAttrs ========
 */
static const TRNGCC26XX_HWAttrs trngCC26XXHWAttrs[CONFIG_TRNG_COUNT] = {
    {
        .intPriority = (~0),
        .swiPriority = 0,
        .samplesPerCycle = 240000
    },
};

const TRNG_Config TRNG_config[CONFIG_TRNG_COUNT] = {
    {   /* CONFIG_TRNG_0 */
        .object         = &trngCC26XXObjects[CONFIG_TRNG_0],
        .hwAttrs        = &trngCC26XXHWAttrs[CONFIG_TRNG_0]
    },
};

const uint_least8_t TRNG_count = CONFIG_TRNG_COUNT;


/*
 *  =============================== UART ===============================
 */

#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC26XX.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26X2.h>
#include <ti/devices/cc13x2_cc26x2/inc/hw_memmap.h>
#include <ti/devices/cc13x2_cc26x2/inc/hw_ints.h>

#define CONFIG_UART_COUNT 1

UARTCC26XX_Object uartCC26XXObjects[CONFIG_UART_COUNT];

static unsigned char uartCC26XXRingBuffer0[32];

static const UARTCC26XX_HWAttrsV2 uartCC26XXHWAttrs[CONFIG_UART_COUNT] = {
  {
    .baseAddr           = UART0_BASE,
    .intNum             = INT_UART0_COMB,
    .intPriority        = (~0),
    .swiPriority        = 0,
    .powerMngrId        = PowerCC26XX_PERIPH_UART0,
    .ringBufPtr         = uartCC26XXRingBuffer0,
    .ringBufSize        = sizeof(uartCC26XXRingBuffer0),
    .rxPin              = IOID_12,
    .txPin              = IOID_13,
    .ctsPin             = PIN_UNASSIGNED,
    .rtsPin             = PIN_UNASSIGNED,
    .txIntFifoThr       = UARTCC26XX_FIFO_THRESHOLD_1_8,
    .rxIntFifoThr       = UARTCC26XX_FIFO_THRESHOLD_4_8,
    .errorFxn           = NULL
  },
};

const UART_Config UART_config[CONFIG_UART_COUNT] = {
    {   /* CONFIG_DISPLAY_UART */
        .fxnTablePtr = &UARTCC26XX_fxnTable,
        .object      = &uartCC26XXObjects[0],
        .hwAttrs     = &uartCC26XXHWAttrs[0]
    },
};

const uint_least8_t UART_count = CONFIG_UART_COUNT;


#include <ti/drivers/Board.h>

/*
 *  ======== Board_initHook ========
 *  Perform any board-specific initialization needed at startup.  This
 *  function is declared weak to allow applications to override it if needed.
 */
void __attribute__((weak)) Board_initHook(void)
{
}

/*
 *  ======== Board_init ========
 *  Perform any initialization needed before using any board APIs
 */
void Board_init(void)
{
    /* ==== /ti/drivers/Power initialization ==== */
    Power_init();

    /* ==== /ti/drivers/PIN initialization ==== */
    if (PIN_init(BoardGpioInitTable) != PIN_SUCCESS) {
        /* Error with PIN_init */
        while (1);
    }

    Board_initHook();
}
