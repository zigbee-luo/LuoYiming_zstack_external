/******************************************************************************

 @file CUI.c

 @brief This file contains the interface implementation of the Combined User
         Interface.

 @detail The interface is designed to be shared between clients.
         As such a client can request access to resources whether they be
         Buttons, LEDs or UART Display without the fear that another client
         already has ownership over that resource.

         If a resource is already taken by another client then the interface
         will respond with that information.

         Only a client that has been given access to a resource may utilize
         the resource. Therefore, any calls a client makes to read/write a
         resource will be ignored if the client does not have the access
         required.

 Group: LPRF SW RND
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2016 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

#include <ti/drivers/dpl/HwiP.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC26XX.h>
#include <ti/drivers/apps/Button.h>
#include <ti/drivers/apps/LED.h>

#include <xdc/runtime/System.h>
#include DeviceFamily_constructPath(driverlib/cpu.h)
#include "ti_drivers_config.h"

#include "cui.h"


#define CUI_INITIAL_STATUS_OFFSET 5
#define CUI_LABEL_VAL_SEP ": "
#define CUI_MAX_LABEL_AND_SEP_LEN (MAX_STATUS_LINE_LABEL_LEN + (sizeof(CUI_LABEL_VAL_SEP)))

/* LED blink period in milliseconds */
#define LED_BLINK_PERIOD      500

/*
 * Ascii Escape characters to be used by testing scripts to bookend the
 * information being printed to the UART
 */
#define CUI_MENU_START_CHAR         0x01 // SOH (start of heading) ascii character
#define CUI_STATUS_LINE_START_CHAR  0x02 // SOT (start of text) ascii character
#define CUI_END_CHAR                0x03 // ETX (end of text) ascii character

#define CUI_NL_CR               "\n\r" // New line carriage return

#define CUI_ESC_UP              "\033[A"
#define CUI_ESC_DOWN            "\033[B"
#define CUI_ESC_RIGHT           "\033[C"
#define CUI_ESC_LEFT            "\033[D"
#define CUI_ESC_ESC             "\033\0\0\0\0"

/*
 * Escape sequences for terminal control.
 * Any sequences with '%' in them require require additional information to be used
 *  as is.
 */
#define CUI_ESC_TRM_MODE            "\033[20"    // Set line feed mode for the terminal

#define CUI_ESC_CLR                 "\033[2J"    // Clear the entire screen
#define CUI_ESC_CLR_UP              "\033[1J"    // Clear screen from cursor up
#define CUI_ESC_CLR_STAT_LINE_VAL   "\033[2K"    // Clear the status line

#define CUI_ESC_CUR_HIDE            "\033[?25l"  // Hide cursor
#define CUI_ESC_CUR_SHOW            "\033[?25h"  // Show cursor
#define CUI_ESC_CUR_HOME            "\033[H"     // Move cursor to the top left of the terminal
#define CUI_ESC_CUR_MENU_BTM        "\033[3;%dH" // Move cursor to the bottom right of the menu
#define CUI_ESC_CUR_LINE            "\033[%d;0H" // Move cursor to a line of choice
#define CUI_ESC_CUR_ROW_COL         "\033[%d;%dH"// Move cursor to row and col



#define CUI_LED_ASSERT_PERIOD 500000

#define CUI_NUM_UART_CHARS  5

/******************************************************************************
 Constants
 *****************************************************************************/
typedef enum
{
    CUI_MENU_NAV_LEFT,
    CUI_MENU_NAV_RIGHT,
} CUI_menuNavDir_t;

typedef enum
{
    CUI_RELEASED = 0,
    // Some specific value so that uninitialized memory does not cause problems
    CUI_ACQUIRED = 0xDEADBEEF,
} CUI_rscStatus_t;

// Internal representation of a button resource
typedef struct
{
    uint32_t clientHash;
    Button_Handle btnHandle;
    CUI_btnPressCB_t appCb;
} CUI_btnResource_t;

// Internal representation of a led resource
typedef struct
{
    uint32_t clientHash;
    LED_Handle ledHandle;
} CUI_ledResource_t;

// Internal representation of a menu
typedef struct
{
    CUI_menu_t* pMenu;
    uint32_t clientHash;
} CUI_menuResource_t;

// Internal representation of a status line
typedef struct
{
    uint32_t clientHash;
    uint32_t lineOffset;
    char label[CUI_MAX_LABEL_AND_SEP_LEN];
    CUI_rscStatus_t status;
} CUI_statusLineResource_t;

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/* Obtain BTN info from ti_drivers_config.h */
extern Button_Config Button_config[];
extern const uint_least8_t Button_count;

/* Obtain LED info from ti_drivers_config.h */
extern LED_Config LED_config[];
extern const uint_least8_t LED_count;

/*
 * [General Global Variables]
 */
static bool gModuleInitialized = false;
static bool gManageBtns = false;
static bool gManageLeds = false;
static bool gManageUart = false;
static Semaphore_Params gSemParams;

static CUI_clientHandle_t gClientHandles[MAX_CLIENTS];
static uint32_t gMaxStatusLines[MAX_CLIENTS];
static Semaphore_Handle gClientsSem;
static Semaphore_Struct gClientsSemStruct;

/*
 * [Button Related Global Variables]
 *
 * At compile time, create an array of Button Resources to track usage depending
 * on the size of the Button Pin Table. Subtract 1 for the `PIN_TERMINATE` value
 * in the Pin Table.
 */
static CUI_btnResource_t *gButtonResources;

/*
 * [LED Related Global Variables]
 *
 * At compile time, create an array of LED Resources to track usage depending
 * on the size of the LED Pin Table. Subtract 1 for the `PIN_TERMINATE` value
 * in the Pin Table.
 */
static CUI_ledResource_t *gLedResources;

/*
 * [UART Specific Global Variables]
 */
static UART_Params gUartParams;
static UART_Handle gUartHandle = NULL;
static uint8_t gUartTxBuffer[CUI_NUM_UART_CHARS];
static uint8_t gUartRxBuffer[CUI_NUM_UART_CHARS];
static bool gUartWriteComplete = false;
static Semaphore_Handle gUartSem;
static Semaphore_Struct gUartSemStruct;

/*
 * [Menu Global Variables]
 */
static CUI_menu_t* gpCurrMenu;
static CUI_menu_t* gpMainMenu;
static int32_t gCurrMenuItemEntry = 0;
static int32_t gPrevMenuItemEntry = 0;
static bool gCursorActive;
static CUI_cursorInfo_t gCursorInfo;

static CUI_menuResource_t gMenuResources[MAX_REGISTERED_MENUS];
static Semaphore_Handle gMenuSem;
static Semaphore_Struct gMenuSemStruct;

char menuBuff[MAX_MENU_LINE_LEN + 2     // Additional new line and return char
              + MAX_MENU_LINE_LEN + 2   // Additional new line and return char
              + MAX_MENU_LINE_LEN + 1]; // Additional ETX char

static char gpMultiMenuTitle[] = " TI DMM Application ";

/*
 * This a special menu that is only utilized when 2 or more menus have been
 * registered to the CUI. This menu will then be the top most Main Menu where
 * each menu that was registered will now be a sub menu.
 */
CUI_menu_t cuiMultiMenu =
{
    /*
     * The uart update fn will be that of the first menu's that was registered
     */
    .uartUpdateFn = NULL,
    .pTitle =  gpMultiMenuTitle,
    // Allocate 1 more for the Help screen
    .numItems = MAX_REGISTERED_MENUS + 1,
    // This menu will never have a upper or parent menu
    .pUpper = NULL,
    // Allocate enough space for the number of submenus that are possible
    .menuItems[MAX_REGISTERED_MENUS + 1] = NULL,
};

/*
 * [Status Line Variables]
 */
static CUI_statusLineResource_t* gStatusLineResources[MAX_CLIENTS];
static Semaphore_Handle gStatusSem;
static Semaphore_Struct gStatusSemStruct;

/******************************************************************************
 Local Functions Prototypes
 *****************************************************************************/
static CUI_retVal_t CUI_publicAPIChecks(const CUI_clientHandle_t _clientHandle);
static CUI_retVal_t CUI_acquireBtn(const CUI_clientHandle_t _clientHandle, const CUI_btnRequest_t* const _pRequest);
static CUI_retVal_t CUI_acquireLed(const CUI_clientHandle_t _clientHandle, const uint32_t _index);
static CUI_retVal_t CUI_acquireStatusLine(const CUI_clientHandle_t _clientHandle, const char* _pLabel, uint32_t* _pLineId);
static CUI_retVal_t CUI_validateHandle(const CUI_clientHandle_t _clientHandle);
static void CUI_menuActionNavigate(CUI_menuNavDir_t _navDir);
static void CUI_menuActionExecute(void);
static CUI_retVal_t CUI_writeString(void * _buffer, size_t _size);
static void CUI_dispMenu(bool _menuPopulated);
static void UartWriteCallback(UART_Handle _handle, void *_buf, size_t _size);
static void CUI_callMenuUartUpdateFn();
static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size);
static void CUI_updateCursor(void);
static bool CUI_handleMenuIntercept(CUI_menuItem_t* _pItemEntry, uint8_t _input);
static int CUI_getClientIndex(const CUI_clientHandle_t _clientHandle);
static CUI_retVal_t CUI_updateRemLen(size_t* _currRemLen, char* _buff, size_t _buffSize);
static CUI_retVal_t CUI_findMenu(CUI_menu_t* _pMenu, CUI_menu_t* _pDesiredMenu, uint32_t* _pPrevItemIndex);
static CUI_retVal_t CUI_publicBtnsAPIChecks(const CUI_clientHandle_t _clientHandle);
static CUI_retVal_t CUI_publicLedsAPIChecks(const CUI_clientHandle_t _clientHandle);
static CUI_retVal_t CUI_publicUartAPIChecks(const CUI_clientHandle_t _clientHandle);
static CUI_retVal_t CUI_ledAssert();

static void handleButtonCallback(Button_Handle handle, Button_EventMask events)
{
    for (uint8_t i = 0; i < Button_count; i++)
    {
        if (handle == gButtonResources[i].btnHandle)
        {
            if (gButtonResources[i].appCb != NULL)
            {
                gButtonResources[i].appCb(i, events);
            }
        }
    }

}

/******************************************************************************
 * Public CUI APIs
 *****************************************************************************/
/*********************************************************************
 * @fn          CUI_init
 *
 * @brief       Initialize the CUI module. This function must be called
 *                  before any other CUI functions.
 *
 * @params      _pParams - A pointer to a CUI_params_t struct
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_init(CUI_params_t* _pParams)
{
    /*
     *  Do nothing if the module has already been initialized or if
     *  CUI_init has been called without trying to manage any of the three
     *  resources (btns, leds, uart)
     */
    if (!gModuleInitialized && (_pParams->manageBtns || _pParams->manageLeds
                    || _pParams->manageUart))
    {

        // Semaphore Setup
        Semaphore_Params_init(&gSemParams);
        //set all sems in this module to be binary sems
        gSemParams.mode = Semaphore_Mode_BINARY;

        // Client Setup
        {
            Semaphore_construct(&gClientsSemStruct, 1, &gSemParams);
            gClientsSem = Semaphore_handle(&gClientsSemStruct);

            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                gClientHandles[i] = NULL;
            }
        }

        // Button Setup
        if (_pParams->manageBtns)
        {
            Button_init();
            gManageBtns = true;

            // Allocate memory for button resources
            gButtonResources = malloc(Button_count * sizeof(gButtonResources[0]));
            if (gButtonResources == NULL)
            {
                return CUI_FAILURE;
            }

            // Initialize button resource specifics
            memset(gButtonResources, 0, Button_count * sizeof(gButtonResources[0]));
            Button_Params params;
            Button_Params_init(&params);

#ifdef CUI_LONG_PRESS_DUR_MS
            params.longPressDuration = CUI_LONG_PRESS_DUR_MS;
#else
            params.longPressDuration = 1000; // 1000 ms
#endif

            for (int i = 0; i < Button_count; i++)
            {
                // Obtain button handle for each button resource
                gButtonResources[i].btnHandle = Button_open(i, handleButtonCallback, &params);
            }
        }

        // LED Setup
        if (_pParams->manageLeds)
        {
            LED_init();
            gManageLeds = true;

            // Allocate memory for led resources
            gLedResources = malloc(LED_count * sizeof(gLedResources[0]));
            if (gLedResources == NULL)
            {
                return CUI_FAILURE;
            }

            // Initialize led resource specifics
            LED_Params ledParams;
            LED_Params_init(&ledParams);
            ledParams.blinkPeriod = LED_BLINK_PERIOD;
            memset(gLedResources, 0, LED_count * sizeof(gLedResources[0]));
            for(uint8_t i = 0; i < LED_count; i++)
            {
                // Obtain led handle for each button resource
                gLedResources[i].ledHandle = LED_open(i, &ledParams);
            }
        }

        if (_pParams->manageUart)
        {
            gManageUart = true;

            // UART semaphore setup
            Semaphore_construct(&gUartSemStruct, 1, &gSemParams);
            gUartSem = Semaphore_handle(&gUartSemStruct);
            Semaphore_construct(&gMenuSemStruct, 1, &gSemParams);
            gMenuSem = Semaphore_handle(&gMenuSemStruct);

            {
                // General UART setup
                UART_init();
                UART_Params_init(&gUartParams);
                gUartParams.baudRate = 115200;
                gUartParams.writeMode     = UART_MODE_CALLBACK;
                gUartParams.writeDataMode = UART_DATA_BINARY;
                gUartParams.writeCallback = UartWriteCallback;
                gUartParams.readMode      = UART_MODE_CALLBACK;
                gUartParams.readDataMode  = UART_DATA_BINARY;
                gUartParams.readCallback  = UartReadCallback;
                gUartHandle = UART_open(CONFIG_DISPLAY_UART, &gUartParams);
                if (NULL == gUartHandle)
                {
                    return CUI_FAILURE;
                }
                else
                {
                    UART_read(gUartHandle, gUartRxBuffer, sizeof(gUartRxBuffer));
                    UART_control(gUartHandle, UARTCC26XX_CMD_RETURN_PARTIAL_ENABLE, NULL);

                    gUartWriteComplete = true;

                    strncpy(menuBuff, CUI_ESC_CLR CUI_ESC_TRM_MODE CUI_ESC_CUR_HIDE, sizeof(menuBuff));
                    if (CUI_SUCCESS != CUI_writeString(menuBuff, strlen(menuBuff)))
                    {
                        UART_close(gUartHandle);
                        return CUI_FAILURE;
                    }

                    memset(menuBuff, 0, sizeof(menuBuff));
                }
            }

            // Multi Menu Initialization
            {
                memset(gMenuResources, 0, sizeof(gMenuResources) / sizeof(gMenuResources[0]));
                /*
                 *  No additional initialization is needed in the case of a single
                 * menu being registered to the CUI module. In the case of 2 or more
                 * menus being registered the global cuiMultiMenu object will
                 * be used as the top level menu and every registered menu will be a
                 * sub menu of the cuiMultiMenu instead.
                 */
                for (int i = 0; i < MAX_REGISTERED_MENUS + 1; i++)
                {
                    memset(&cuiMultiMenu.menuItems[i], 0, sizeof(cuiMultiMenu.menuItems[i]));
                }
            }

            // Status Lines Setup
            {
                Semaphore_construct(&gStatusSemStruct, 1, &gSemParams);
                gStatusSem = Semaphore_handle(&gStatusSemStruct);
            }
        }

        gModuleInitialized = true;
        return CUI_SUCCESS;
    }

    return CUI_FAILURE;
}

/*********************************************************************
 * @fn          CUI_paramsInit
 *
 * @brief       Initialize a CUI_clientParams_t struct to a known state.
 *                  The known state in this case setting each resource
 *                  management flag to true
 *
 * @params      _pParams - A pointer to an un-initialized CUI_params_t struct
 *
 * @return      none
 */
void CUI_paramsInit(CUI_params_t* _pParams)
{
    _pParams->manageBtns = true;
    _pParams->manageLeds = true;
    _pParams->manageUart = true;
}

/*********************************************************************
 * @fn          CUI_clientOpen
 *
 * @brief       Open a client with the CUI module. A client is required
 *                  to request/acquire resources
 *
 * @param       _pParams - Pointer to a CUI client params struct.
 *                  _pParams.clientName must be set before passing to CUI_open
 *
 * @return      NULL on failure. Otherwise success.
 */
CUI_clientHandle_t CUI_clientOpen(CUI_clientParams_t* _pParams)
{
    size_t numClients = 0;

    if (!gModuleInitialized)
    {
        return NULL;
    }

    Semaphore_pend(gClientsSem, BIOS_WAIT_FOREVER);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (gClientHandles[i] != NULL)
        {
            numClients++;
        }
    }

    if (numClients >= MAX_CLIENTS)
    {
        return NULL;
    }

    // +1 for the null char
    size_t nameLen = strlen(_pParams->clientName) + 1;

    /*
     * A very simple hash is calculated in order to perform quick client
     * verification. Comparing two uint32_t's rather than performing strcmp
     * on two strings.
     */
    uint32_t hash = NULL;
    for (int i = 0; i < nameLen; i++)
    {
         hash += _pParams->clientName[i];
    }


    gClientHandles[numClients] = hash;

    if (_pParams->maxStatusLines && gManageUart)
    {
        gMaxStatusLines[numClients] = _pParams->maxStatusLines;
        gStatusLineResources[numClients] = malloc(_pParams->maxStatusLines * sizeof(gStatusLineResources[0][0]));
        if (gStatusLineResources[numClients] == NULL)
        {
            return CUI_FAILURE;
        }
        memset(gStatusLineResources[numClients], 0, _pParams->maxStatusLines * sizeof(gStatusLineResources[0][0]));
    }

    Semaphore_post(gClientsSem);

    return hash;
}

/*********************************************************************
 * @fn          CUI_clientParamsInit
 *
 * @brief       Initialize a CUI_clientParams_t struct to a known state.
 *
 * @param       _pClientParams - Pointer to params struct
 *
 * @return      void
 */
void CUI_clientParamsInit(CUI_clientParams_t* _pClientParams)
{
    strcpy(_pClientParams->clientName, "");
    _pClientParams->maxStatusLines = 0;
}

/*********************************************************************
 * @fn          CUI_close
 *
 * @brief       Close the CUI module. Release all resources and memory.
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_close()
{
    // Only close the module if it's been initialized
    if (gModuleInitialized)
    {
        if (gManageBtns)
        {
            for (uint8_t i = 0; i < Button_count; i++)
            {
                Button_close(gButtonResources[i].btnHandle);
            }
            free(gButtonResources);
        }

        if (gManageLeds)
        {
            for (uint8_t i = 0; i < LED_count; i++)
            {
                LED_close(gLedResources[i].ledHandle);
            }
            free(gLedResources);
        }

        if (gManageUart)
        {
            Semaphore_pend(gStatusSem, BIOS_WAIT_FOREVER);
            strncpy(menuBuff, CUI_ESC_CLR CUI_ESC_TRM_MODE CUI_ESC_CUR_HIDE,
                sizeof(menuBuff));
            CUI_writeString(menuBuff, strlen(menuBuff));
            for (uint8_t i = 0; i < MAX_CLIENTS; i++)
            {
                if (gStatusLineResources[i])
                {
                    free(gStatusLineResources[i]);
                }
            }
            UART_close(gUartHandle);
            Semaphore_post(gStatusSem);
        }

        // Clear out the client handles
        memset(gClientHandles, 0, sizeof(gClientHandles[0]) * MAX_CLIENTS);
    }

    gModuleInitialized = false;
    gManageUart = false;
    gManageLeds = false;
    gManageBtns = false;

    return CUI_SUCCESS;

}

/******************************************************************************
 * Button CUI APIs
 *****************************************************************************/
/*********************************************************************
 * @fn          CUI_btnResourceRequest
 *
 * @brief       Request access to a button resource
 *
 * @param       _clientHandle - Valid client handle
 *              _pRequest - Pointer to a button request struct
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_btnResourceRequest(const CUI_clientHandle_t _clientHandle, const CUI_btnRequest_t* _pRequest)
{
    CUI_retVal_t retVal = CUI_publicBtnsAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal) {
        return retVal;
    }

    if (NULL == _pRequest) {
        return CUI_INVALID_PARAM;
    }

    retVal = CUI_acquireBtn(_clientHandle, _pRequest);

    return retVal;
}

/*********************************************************************
 * @fn          CUI_btnSetCb
 *
 * @brief       Set the CUI_btnPressCB of a button resource that is currently
 *                  acquired
 *
 * @param       _clientHandle - Client handle that owns the btn
 *              _index - index of the btn you wish to set the appCb of
 *              _appCb - New AppCB
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_btnSetCb(const CUI_clientHandle_t _clientHandle, const uint32_t _index, const  CUI_btnPressCB_t _appCb)
{
    CUI_retVal_t retVal = CUI_publicBtnsAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal) {
        return retVal;
    }

    if (_clientHandle != gButtonResources[_index].clientHash)
    {
        return CUI_INVALID_CLIENT_HANDLE;
    }

    gButtonResources[_index].appCb = _appCb;

    return CUI_SUCCESS;
}

/*********************************************************************
 * @fn          CUI_btnGetValue
 *
 * @brief       Set the CUI_btnPressCB of a button resource that is currently
 *                  acquired
 *
 * @param       _clientHandle - Client handle that owns the btn
 *              _index - index of the btn to get the value of
 *              _pBtnState - return param to denote the state of the btn upon
 *                  request
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_btnGetValue(const CUI_clientHandle_t _clientHandle, const uint32_t _index, bool* _pBtnState)
{
//    CUI_retVal_t retVal = CUI_publicBtnsAPIChecks(_clientHandle);
//    if (CUI_SUCCESS != retVal) {
//        return retVal;
//    }
//
//    if (_clientHandle != gButtonResources[_gpio].clientHash) {
//        return CUI_INVALID_CLIENT_HANDLE;
//    }

    *_pBtnState = GPIO_read(((Button_HWAttrs*)Button_config[_index].hwAttrs)->gpioIndex);

    return CUI_SUCCESS;
}

/*********************************************************************
 * @fn          CUI_btnResourceRelease
 *
 * @brief       Release access to a button resource that is currently acquired
 *
 * @param       _clientHandle - Client handle that owns the btn
 *              _index - index of the btn you want to release
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_btnResourceRelease(const CUI_clientHandle_t _clientHandle, const uint32_t _index)
{
    CUI_retVal_t retVal = CUI_publicBtnsAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    if (_clientHandle != gButtonResources[_index].clientHash)
    {
        return CUI_INVALID_CLIENT_HANDLE;
    }

    gButtonResources[_index].clientHash = NULL;
    gButtonResources[_index].appCb = NULL;

    return CUI_SUCCESS;
}

/******************************************************************************
 * LED CUI APIs
 *****************************************************************************/
/*********************************************************************
 * @fn          CUI_ledResourceRequest
 *
 * @brief       Request access to a led resource
 *
 * @param       _clientHandle - Valid client handle
 *              _pRequest - Pointer to a led request struct
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_ledResourceRequest(const CUI_clientHandle_t _clientHandle, const CUI_ledRequest_t* _pRequest)
{
    CUI_retVal_t retVal = CUI_publicLedsAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    if (NULL == _pRequest)
    {
        return CUI_INVALID_PARAM;
    }

    retVal = CUI_acquireLed(_clientHandle, _pRequest->index);

    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    return retVal;
}

/*********************************************************************
 * @fn          CUI_ledResourceRelease
 *
 * @brief       Release access to a led resource that is currently acquired
 *
 * @param       _clientHandle - Client handle that owns the led
 *              _index - index of the led you want to release
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_ledResourceRelease(const CUI_clientHandle_t _clientHandle, const uint32_t _index)
{
    CUI_retVal_t retVal = CUI_publicLedsAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    if (_clientHandle != gLedResources[_index].clientHash)
    {
        return CUI_INVALID_CLIENT_HANDLE;
    }

    /* Go Green! If no one is home, turn off the lights. */
    LED_setOff(gLedResources[_index].ledHandle);

    gLedResources[_index].clientHash = NULL;

    return CUI_SUCCESS;
}

/*********************************************************************
 * @fn          CUI_ledOn
 *
 * @brief       Turn a led on
 *
 * @param       _clientHandle - Client handle that owns the led
 *              _index - index of the led you want to change the state of
 *              _brightness - brightness to set the led to. Only available when
 *                              the led has been configured as a PWM LED.
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_ledOn(const CUI_clientHandle_t _clientHandle, const uint32_t _index, const uint8_t _brightness)
{
    CUI_retVal_t retVal = CUI_publicLedsAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    if (_clientHandle != gLedResources[_index].clientHash)
    {
        return CUI_INVALID_CLIENT_HANDLE;
    }

    if (LED_getState(gLedResources[_index].ledHandle) == LED_STATE_BLINKING)
    {
        LED_stopBlinking(gLedResources[_index].ledHandle);
    }

    LED_setOn(gLedResources[_index].ledHandle, _brightness);

    return CUI_SUCCESS;
}

/*********************************************************************
 * @fn          CUI_ledOff
 *
 * @brief       Turn a led off
 *
 * @param       _clientHandle - Client handle that owns the led
 *              _index - index of the led you want to change the state of
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_ledOff(const CUI_clientHandle_t _clientHandle, const uint32_t _index)
{
    CUI_retVal_t retVal = CUI_publicLedsAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    if (_clientHandle != gLedResources[_index].clientHash)
    {
        return CUI_INVALID_CLIENT_HANDLE;
    }

    if (LED_getState(gLedResources[_index].ledHandle) == LED_STATE_BLINKING)
    {
        LED_stopBlinking(gLedResources[_index].ledHandle);
    }

    LED_setOff(gLedResources[_index].ledHandle);

    return CUI_SUCCESS;
}

/*********************************************************************
 * @fn          CUI_ledToggle
 *
 * @brief       Toggle the state of a led [on/off]
 *
 * @param       _clientHandle - Client handle that owns the led
 *              _index - index of the led you want to change the state of
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_ledToggle(const CUI_clientHandle_t _clientHandle, const uint32_t _index)
{
    CUI_retVal_t retVal = CUI_publicLedsAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    if (_clientHandle != gLedResources[_index].clientHash)
    {
        return CUI_INVALID_CLIENT_HANDLE;
    }

    if (LED_getState(gLedResources[_index].ledHandle) == LED_STATE_BLINKING)
    {
        LED_stopBlinking(gLedResources[_index].ledHandle);
    }

    LED_toggle(gLedResources[_index].ledHandle);

    return CUI_SUCCESS;
}

/*********************************************************************
 * @fn          CUI_ledBlink
 *
 * @brief       Start blinking a led. Blinking will be at a rate of LED_BLINK_PERIOD ms
 *
 * @param       _clientHandle - Client handle that owns the led
 *              _index - index of the led you want to change the state of
 *              _numBlinks - number of blinks. or CUI_BLINK_CONTINUOUS to blink forever
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_ledBlink(const CUI_clientHandle_t _clientHandle, const uint32_t _index, const uint16_t _numBlinks)
{
    CUI_retVal_t retVal = CUI_publicLedsAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    if (_clientHandle != gLedResources[_index].clientHash)
    {
        return CUI_INVALID_CLIENT_HANDLE;
    }

    LED_startBlinking(gLedResources[_index].ledHandle, LED_BLINK_PERIOD, _numBlinks);

    return CUI_SUCCESS;
}

/*********************************************************************
 *  @fn         CUI_ledAssert
 *
 *  @brief      Without requiring a cuiHandle_t or permission to the leds,
 *                flash blink all leds to indicate an assert.
 *
 *              Note: This function will close all existing clients that have
 *                been opened and then enter an infinite loop.
 *
 *                This function should only be used in the case of an assert
 *                where application functionality should be ended and further
 *                functionality of the application is assumed to have been
 *                broken.
 *
 *  @return     CUI_MODULE_UNINITIALIZED If module is initialized or module
 *                  not in control of leds
 *              Otherwise function will not return due to infinite loop.
 */
static CUI_retVal_t CUI_ledAssert()
{
    if (!gModuleInitialized || !gManageLeds)
    {
       return CUI_MODULE_UNINITIALIZED;
    }

    while(1)
    {
        CPUdelay(CUI_LED_ASSERT_PERIOD);
        for (int i = 0; i < LED_count; i++)
        {
            LED_toggle(gLedResources[i].ledHandle);
        }
    }
}

/******************************************************************************
 * Menu CUI APIs
 *****************************************************************************/
/*********************************************************************
 * @fn          CUI_registerMenu
 *
 * @brief       Register a menu with the CUI module
 *
 * @param       _clientHandle - Client to register the menu to
 *              _pMenu - Pointer to a CUI_menu_t struct
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_registerMenu(const CUI_clientHandle_t _clientHandle, CUI_menu_t* _pMenu)
{
    CUI_retVal_t retVal = CUI_publicUartAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    if (NULL == _pMenu)
    {
        return CUI_INVALID_PARAM;
    }

    if (NULL == _pMenu->uartUpdateFn)
    {
        return CUI_MISSING_UART_UPDATE_FN;
    }

    Semaphore_pend(gMenuSem, BIOS_WAIT_FOREVER);

    int freeIndex = -1;
    int numMenus = 0;
    for (int i = 0; i < MAX_REGISTERED_MENUS; i++)
    {
        if ((NULL == gMenuResources[i].clientHash) &&
              (NULL == gMenuResources[i].pMenu))
        {
            if (-1 == freeIndex)
            {
                freeIndex = i;
            }
        }
        else
        {
            numMenus++;
        }
    }

    if (-1 == freeIndex)
    {
        Semaphore_post(gMenuSem);
        return CUI_MAX_MENUS_REACHED;
    }

    gMenuResources[freeIndex].clientHash = _clientHandle;
    gMenuResources[freeIndex].pMenu = _pMenu;

    if (numMenus > 0)
    {
        if (1 == numMenus)
        {
            /*
             * Someone (a rtos task) needs to own the processing time for the
             * cuiMultiMenu. The task that first registered a menu will be
             * that owner. Any additional menu's processing time will be owned
             * by the task that registered them.
             */
            cuiMultiMenu.uartUpdateFn = gpMainMenu->uartUpdateFn;

            /*
             * The first menu that was registered needs to be added as the first
             * sub menu of the cuiMultiMenu object
             */
            cuiMultiMenu.menuItems[0].interceptable = false;
            cuiMultiMenu.menuItems[0].interceptActive = false;
            cuiMultiMenu.menuItems[0].pDesc = NULL;
            cuiMultiMenu.menuItems[0].item.pSubMenu = gpMainMenu;
            gpMainMenu->pUpper = &cuiMultiMenu;

            /*
             * Change the old main menu Help action to a back action
             */
            gpMainMenu->menuItems[gpMainMenu->numItems - 1].interceptable = false;
            gpMainMenu->menuItems[gpMainMenu->numItems - 1].interceptActive = false;
            gpMainMenu->menuItems[gpMainMenu->numItems - 1].pDesc = CUI_MENU_ACTION_BACK_DESC;
            gpMainMenu->menuItems[gpMainMenu->numItems - 1].item.pFnAction = (CUI_pFnAction_t) CUI_menuActionBack;

            /*
             * The first time through the global menu pointers need to be
             * modified to reflect the new menu structure
             */
            gpMainMenu = &cuiMultiMenu;
            gpCurrMenu = &cuiMultiMenu;
        }

        /*
         * Add the new menu being registered to the cuiMultiMenu as a sub
         * menu object
         */
        cuiMultiMenu.menuItems[numMenus].interceptable = false;
        cuiMultiMenu.menuItems[numMenus].interceptActive = false;
        cuiMultiMenu.menuItems[numMenus].pDesc = NULL;
        cuiMultiMenu.menuItems[numMenus].item.pSubMenu = _pMenu;
        _pMenu->pUpper = &cuiMultiMenu;

        /*
         * Change the registering menu Help action to a back action
         */
        _pMenu->menuItems[_pMenu->numItems - 1].interceptable = false;
        _pMenu->menuItems[_pMenu->numItems - 1].interceptActive = false;
        _pMenu->menuItems[_pMenu->numItems - 1].pDesc = CUI_MENU_ACTION_BACK_DESC;
        _pMenu->menuItems[_pMenu->numItems - 1].item.pFnAction = (CUI_pFnAction_t) CUI_menuActionBack;

        /*
         * The Help screen must always be the last initialized item in the
         * cuiMultiMenu
         */
        cuiMultiMenu.menuItems[numMenus + 1].interceptable = true;
        cuiMultiMenu.menuItems[numMenus + 1].interceptActive = false;
        cuiMultiMenu.menuItems[numMenus + 1].pDesc = CUI_MENU_ACTION_HELP_DESC;
        cuiMultiMenu.menuItems[numMenus + 1].item.pFnIntercept = (CUI_pFnIntercept_t) CUI_menuActionHelp;

        if (1 == numMenus)
        {
            /*
             * At this point there should be 3 items.
             * [previous menu]
             * [new menu]
             * [help action]
             */
            cuiMultiMenu.numItems = 3;
        }
        else
        {
            /*
             * At this point there should be one more item.
             * [previous menu]
             *  ...
             * [new menu]
             * [help action]
             */
            cuiMultiMenu.numItems++;
        }
    }
    else
    {
        /*
         * Set global pointers to the new main menu
         */
        gpMainMenu = _pMenu;
        gpCurrMenu = _pMenu;
    }

    /* Default to the Help item that was given to it */
    gCurrMenuItemEntry = gpMainMenu->numItems - 1;

    CUI_dispMenu(false);
    Semaphore_post(gMenuSem);

    return CUI_SUCCESS;
}

/*********************************************************************
 * @fn          CUI_deRegisterMenu
 *
 * @brief       De-registers a menu with the CUI module
 *
 * @param       _clientHandle - Client that owns the menu
 *              _pMenu - Pointer to the CUI_menu_t struct to remove
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_deRegisterMenu(const CUI_clientHandle_t _clientHandle, CUI_menu_t* _pMenu)
{
    static char buff[32];
    CUI_retVal_t retVal = CUI_publicUartAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    if (NULL == _pMenu)
    {
        return CUI_INVALID_PARAM;
    }

    if (NULL == _pMenu->uartUpdateFn)
    {
        return CUI_MISSING_UART_UPDATE_FN;
    }

    Semaphore_pend(gMenuSem, BIOS_WAIT_FOREVER);

    int matchingIndex = -1;
    int numMenus = 0;
    for (int i = 0; i < MAX_REGISTERED_MENUS; i++)
    {
        if ((_clientHandle == gMenuResources[i].clientHash) &&
              (_pMenu == gMenuResources[i].pMenu))
        {
            if (-1 == matchingIndex)
            {
                matchingIndex = i;
            }
        }
        if ((NULL != gMenuResources[i].clientHash) &&
              (NULL != gMenuResources[i].pMenu))
        {
            numMenus++;
        }
    }

    if (-1 == matchingIndex)
    {
        Semaphore_post(gMenuSem);
        return CUI_RESOURCE_NOT_ACQUIRED;
    }

    if (numMenus > 1)
    {
        /*
         * Reduce the number of menus in the multi menu by 1
         */
        if (3 == cuiMultiMenu.numItems)
        {
            /*
             * We should go back to a single menu. Remove the multi Menu.
             * There will only be one other valid menu in the array. Find it,
             * and use that as the single main menu.
             */
            uint8_t newMainMenuIndex = 0;
            for (int i = 0; i < MAX_REGISTERED_MENUS; i++)
            {
                if (NULL != gMenuResources[i].clientHash &&
                        NULL != gMenuResources[i].pMenu &&
                        i != matchingIndex)
                {
                    newMainMenuIndex = i;
                    break;
                }
            }

            gpMainMenu = gMenuResources[newMainMenuIndex].pMenu;
            gpCurrMenu = gpMainMenu;

            cuiMultiMenu.numItems = 0;

            /* Default to the Help item that was given to it */
            gCurrMenuItemEntry = gpMainMenu->numItems - 1;

            gpMainMenu->menuItems[gpMainMenu->numItems - 1].interceptable = true;
            gpMainMenu->menuItems[gpMainMenu->numItems - 1].interceptActive = false;
            gpMainMenu->menuItems[gpMainMenu->numItems - 1].pDesc = CUI_MENU_ACTION_HELP_DESC;
            gpMainMenu->menuItems[gpMainMenu->numItems - 1].item.pFnIntercept = (CUI_pFnIntercept_t) CUI_menuActionHelp;

        }
        else
        {
            /*
             * Shift the remaining items in the cuiMultiMenu down to cover the
             * menu that is being de-registered.
             */
            for (int i = matchingIndex; i < MAX_REGISTERED_MENUS; i++)
            {
                /*
                 *  It is safe to use this i+1 value because cuiMultiMenu was
                 * declared to contain MAX_REGISTERED_MENUS + 1 menuItems.
                 */
                memcpy(&cuiMultiMenu.menuItems[i], &cuiMultiMenu.menuItems[i+1], sizeof(cuiMultiMenu.menuItems[0]));
            }

            if (gCurrMenuItemEntry == (cuiMultiMenu.numItems - 1 ))
            {
                gCurrMenuItemEntry--;
            }
            cuiMultiMenu.numItems--;
        }

        CUI_dispMenu(false);

    }
    else
    {

        gpMainMenu = NULL;
        gpCurrMenu = NULL;

        /* Default to the Help item that was given to it */
        gCurrMenuItemEntry = 0;

        System_snprintf(buff, sizeof(buff),
            CUI_ESC_CUR_HIDE CUI_ESC_CUR_MENU_BTM CUI_ESC_CLR_UP CUI_ESC_CUR_HOME,
            MAX_MENU_LINE_LEN);
        CUI_writeString(buff, strlen(buff));
    }

    gMenuResources[matchingIndex].clientHash = NULL;
    gMenuResources[matchingIndex].pMenu = NULL;

    Semaphore_post(gMenuSem);
    return CUI_SUCCESS;
}

/*********************************************************************
 * @fn          CUI_updateMultiMenuTitle
 *
 * @brief       De-registers a menu with the CUI module
 *
 * @param       _pTitle - Pointer to the new multi-menu title
 *
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_updateMultiMenuTitle(const char* _pTitle)
{
    if (NULL == _pTitle)
    {
        return CUI_INVALID_PARAM;
    }

    cuiMultiMenu.pTitle = _pTitle;

    /*
     * Display the updated title if the top level menu is already
     *  being shown.
     */
    if (gpCurrMenu == &cuiMultiMenu)
    {
        CUI_dispMenu(false);
    }

    return CUI_SUCCESS;
}

/*********************************************************************
 * @fn          CUI_menuNav
 *
 * @brief       Navigate to a specific entry of a menu that has already been
 *              registered
 *
 * @param       _clientHandle - Client that owns the menu
 *              _pMenu - Pointer to an already registered menu
 *              _itemIndex - The index of the menuItems[] array to select from
 *                  the menu
 *
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_menuNav(const CUI_clientHandle_t _clientHandle, CUI_menu_t* _pMenu, const uint32_t _itemIndex)
{
    CUI_retVal_t retVal = CUI_publicUartAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    if (NULL == _pMenu)
    {
        return CUI_INVALID_PARAM;
    }

    if (_itemIndex > _pMenu->numItems - 1)
    {
        return CUI_INVALID_PARAM;
    }

    CUI_retVal_t menuRetVal;
    uint32_t prevItemIndex = 0;
    for (int i = 0; i < MAX_REGISTERED_MENUS; i++)
    {
        prevItemIndex = i;
        /*
         * Verify that the menu is apart of a registered Main Menu
         */
        menuRetVal = CUI_findMenu(gMenuResources[i].pMenu, _pMenu, &prevItemIndex);
        if (CUI_SUCCESS == menuRetVal)
        {
            /*
             * Make sure that the client Attempting to navigate to this menu
             * is the owner of the menu.
             */
            if (gMenuResources[i].clientHash != _clientHandle)
            {
                return CUI_INVALID_CLIENT_HANDLE;
            }
            break;
        }
    }

    if (menuRetVal)
    {
        return CUI_INVALID_PARAM;
    }

    /*
     * If the menu is found to be already registered, then it is safe to nav
     * there. It is guaranteed that the user can navigate away afterwards
     */
    gPrevMenuItemEntry = prevItemIndex;
    gCurrMenuItemEntry = _itemIndex;
    gpCurrMenu = _pMenu;

    CUI_dispMenu(false);

    return CUI_SUCCESS;
}

/*********************************************************************
 * @fn          CUI_processMenuUpdate
 *
 * @brief       This function should be called whenever there is UART input
 *                  to be processed.
 *
 *              This update process begins by the CUI module calling the
 *                  CUI_pFnClientUartUpdate_t of the main menu that was
 *                  registered. At that point the CUI_pFnClientUartUpdate_t
 *                  function is responsible for calling CUI_processUartUpdate.
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_processMenuUpdate(void)
{
    if (!gModuleInitialized || !gManageUart)
    {
        return CUI_FAILURE;
    }

    CUI_menuItem_t* pItemEntry = &(gpCurrMenu->menuItems[gCurrMenuItemEntry]);
    uint8_t input = gUartTxBuffer[0];
    bool inputBad = false;

    // Decode special escape sequences
    if (input == CUI_INPUT_ESC)
    {
        /*
         * If the first character is CUI_INPUT_ESC, then look
         *  for the accepted sequences.
         */
        if (memcmp(gUartTxBuffer, CUI_ESC_UP, sizeof(CUI_ESC_UP)) == 0)
        {
            input = CUI_INPUT_UP;
        }
        else if (memcmp(gUartTxBuffer, CUI_ESC_DOWN, sizeof(CUI_ESC_DOWN)) == 0)
        {
            input = CUI_INPUT_DOWN;
        }
        else if (memcmp(gUartTxBuffer, CUI_ESC_RIGHT, sizeof(CUI_ESC_RIGHT)) == 0)
        {
            input = CUI_INPUT_RIGHT;
        }
        else if (memcmp(gUartTxBuffer, CUI_ESC_LEFT, sizeof(CUI_ESC_LEFT)) == 0)
        {
            input = CUI_INPUT_LEFT;
        }
        else if (memcmp(gUartTxBuffer, CUI_ESC_ESC, sizeof(CUI_ESC_ESC)))
        {
            // The rx buffer is full of junk. Let's ignore it just in case.
            inputBad = true;
        }
    }

    if (!inputBad)
    {
        // If it is an upper case letter, convert to lowercase
        if (input >= 'A' && input <= 'Z')
        {
            input += 32; // converts any uppercase letter to a lowercase letter
        }
        else
        {
            /*
             * Assume that further input is intended to be handled by an
             * interceptable action
             */
        }

        bool interceptState = pItemEntry->interceptActive;
        /*
         *  Allow the interceptable action, if it is being shown, the chance to
         *  handle the uart input and display output if necessary.
         */
        bool updateHandled = CUI_handleMenuIntercept(pItemEntry, input);

        if (false == updateHandled)
        {
            switch(input)
            {
                case CUI_INPUT_UP:
                    break;
                case CUI_INPUT_RIGHT:
                    CUI_menuActionNavigate(CUI_MENU_NAV_RIGHT);
                    break;
                case CUI_INPUT_DOWN:
                    break;
                case CUI_INPUT_LEFT:
                    CUI_menuActionNavigate(CUI_MENU_NAV_LEFT);
                    break;
                case CUI_INPUT_EXECUTE:
                    CUI_menuActionExecute();
                    break;
                case CUI_INPUT_BACK:
                    if (gpCurrMenu->pUpper)
                    {
                        gpCurrMenu = gpCurrMenu->pUpper;
                        gCurrMenuItemEntry = gPrevMenuItemEntry;
                    }
                    else
                    {
                        // We are already at the main menu.
                        // go back to the help screen
                        gCurrMenuItemEntry = gpCurrMenu->numItems - 1;
                    }
                    CUI_dispMenu(false);
                    break;
                case CUI_INPUT_ESC:
                    if (interceptState && !pItemEntry->interceptActive)
                    {
                        /*
                         * Nothing special to do here. Just display the
                         * menu item outside of intercept.
                         */
                    }
                    else
                    {
                        gpCurrMenu = gpMainMenu;
                        // Display the help screen
                        gCurrMenuItemEntry = gpMainMenu->numItems - 1;
                    }
                    CUI_dispMenu(false);
                    break;
                default :
                    break;
            }
        }
    }

    //Clear the buffer
    memset(gUartTxBuffer, '\0', sizeof(gUartTxBuffer));

    UART_read(gUartHandle, gUartRxBuffer, sizeof(gUartRxBuffer));
    return CUI_SUCCESS;
}

/******************************************************************************
 * Status Line CUI APIs
 *****************************************************************************/
/*********************************************************************
 * @fn          CUI_statusLineResourceRequest
 *
 * @brief       Request access to a new status line
 *
 * @param       _clientHandle - Valid client handle
 *              _pLabel - C string label for the new status line
 *              _pLineId - Pointer to an unsigned integer. The value of the
 *                  unsigned integer will be set and represent the line number
 *                  you were given access to.
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_statusLineResourceRequest(const CUI_clientHandle_t _clientHandle, const char _pLabel[MAX_STATUS_LINE_LABEL_LEN], uint32_t* _pLineId)
{
    CUI_retVal_t retVal = CUI_publicUartAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    if (NULL == _pLabel || NULL == _pLineId)
    {
        return CUI_INVALID_PARAM;
    }

    retVal = CUI_acquireStatusLine(_clientHandle, _pLabel, _pLineId);
    if (CUI_SUCCESS != retVal)
    {
        /*
         * Set the value of _pLineId to a invalid lineId in case
         * the user tries to print to this line even though it was
         * not successfully acquired.
         */
        //TODO: change _pLineId to be a signed integer so that -1 may be used.
        *_pLineId = 0xFF;
        return retVal;
    }

    /*
     * Print a default '--' value to the line
     */
    CUI_statusLinePrintf(_clientHandle, *_pLineId, "--");

    return CUI_SUCCESS;
}

/*********************************************************************
 * @fn          CUI_statusLinePrintf
 *
 * @brief        Update an acquired status line
 *
 * @param       _clientHandle - Client handle that owns the status line
 *              _lineId - unsigned integer of the line that you are updating.
 *                  This is the value set by CUI_statusLineResourceRequest().
 *              _format - C string printf style format.
 *              ... - Var args to be formated by _format
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CUI_retVal_t CUI_statusLinePrintf(const CUI_clientHandle_t _clientHandle,
        const uint32_t _lineId, const char *_format, ...)
{
    /*
     * This buffer will be passed to CUI_writeString(). The address must be
     * valid at all times. Using a ping pong buffer system will allow a second
     * quick call to CUI_statusLinePrintf to not effect the buffer of a
     * previous unfinished call.
     */
    static char statusLineBuff[2][CUI_MAX_LABEL_AND_SEP_LEN + MAX_STATUS_LINE_VALUE_LEN + 64]; // plus 64 for cursor movement/clearing
    static uint8_t currStatusBuff = 0;
    va_list args;

    CUI_retVal_t retVal = CUI_publicUartAPIChecks(_clientHandle);
    if (CUI_SUCCESS != retVal)
    {
        return retVal;
    }

    // Known to be good since the public api check was successful
    int clientIndex = CUI_getClientIndex(_clientHandle);

    if (_clientHandle != gStatusLineResources[clientIndex][_lineId].clientHash)
    {
        return CUI_INVALID_CLIENT_HANDLE;
    }

    if (CUI_ACQUIRED != gStatusLineResources[clientIndex][_lineId].status)
    {
        return CUI_RESOURCE_NOT_ACQUIRED;
    }

    Semaphore_pend(gStatusSem, BIOS_WAIT_FOREVER);

    uint32_t offset;
    if (MAX_REGISTERED_MENUS == 0)
    {
        offset = 0;
    }
    else
    {
        offset = CUI_INITIAL_STATUS_OFFSET;
    }
    offset += gStatusLineResources[clientIndex][_lineId].lineOffset;

    //TODO: Remove magic length number
#if !defined(CUI_SCROLL_PRINT)
    System_snprintf(statusLineBuff[currStatusBuff], 64,
        CUI_ESC_CUR_HIDE CUI_ESC_CUR_HOME CUI_ESC_CUR_LINE CUI_ESC_CLR_STAT_LINE_VAL "%c",
         offset, CUI_STATUS_LINE_START_CHAR);
#endif
    size_t availableLen = sizeof(statusLineBuff[currStatusBuff]) - 1;
    size_t buffSize = availableLen;

    // Label must be printed for testing scripts to parse the output easier
    strncat(statusLineBuff[currStatusBuff], gStatusLineResources[clientIndex][_lineId].label,
            availableLen);

    retVal = CUI_updateRemLen(&availableLen, statusLineBuff[currStatusBuff], buffSize);
    if (CUI_SUCCESS != retVal)
    {
        Semaphore_post(gStatusSem);
        return retVal;
    }

    va_start(args, _format);
    System_vsnprintf(&statusLineBuff[currStatusBuff][strlen(statusLineBuff[currStatusBuff])], availableLen, _format, args);
    va_end(args);

    retVal = CUI_updateRemLen(&availableLen, statusLineBuff[currStatusBuff], buffSize);
    if (CUI_SUCCESS != retVal)
    {
        Semaphore_post(gStatusSem);
        return retVal;
    }

#if !defined(CUI_SCROLL_PRINT)
    char endChar[] = {CUI_END_CHAR};
    strncat(statusLineBuff[currStatusBuff], endChar, 1);
#else
    strncat(statusLineBuff[currStatusBuff], CUI_NL_CR, sizeof(CUI_NL_CR));
#endif

    retVal = CUI_writeString(statusLineBuff[currStatusBuff], strlen(statusLineBuff[currStatusBuff]));
    if (CUI_SUCCESS != retVal)
    {
        Semaphore_post(gStatusSem);
        return retVal;
    }

    // Switch which buffer we use for the next call to CUI_statusLinePrintf()
    currStatusBuff = !currStatusBuff;

    // This will check if a cursor is active and put the cursor back
    //  if it is necessary
    CUI_updateCursor();

    Semaphore_post(gStatusSem);
    return CUI_SUCCESS;
}

/*********************************************************************
 *  @fn         CUI_assert
 *
 *  @brief      Without requiring a cuiHandle_t you may print an assert
 *                string and optionally spinLock while flashing the leds.
 *
 * @param       _assertMsg - Char pointer of the message to print
 *              _spinLock - Whether or not to spinLock
 *
 * @return      CUI_retVal_t representing success or failure.
 */
void CUI_assert(const char* _assertMsg, const bool _spinLock)
{
    if (BIOS_ThreadType_Main == BIOS_getThreadType())
    {
        /*
         *  UART requires the bios to have been started. If you get stuck
         *  here it is because CUI_assert is being called before
         *  BIOS_start().
         */
        CUI_ledAssert();
    }
    if (!gModuleInitialized)
    {
        CUI_params_t params;
        CUI_paramsInit(&params);
        CUI_init(&params);
    }

    static char statusLineBuff[MAX_STATUS_LINE_VALUE_LEN];
    static char tmp[64];

    // Display this in the line between the menu and the status lines
    uint32_t offset = CUI_INITIAL_STATUS_OFFSET - 1;

    System_snprintf(tmp, sizeof(tmp),
        CUI_ESC_CUR_HIDE CUI_ESC_CUR_HOME CUI_ESC_CUR_LINE CUI_ESC_CLR_STAT_LINE_VAL "%c",
        offset, CUI_STATUS_LINE_START_CHAR);
    CUI_writeString(tmp, strlen(tmp));

    System_snprintf(statusLineBuff, sizeof(statusLineBuff),  CUI_COLOR_RED "%s%c" CUI_COLOR_RESET, _assertMsg, CUI_END_CHAR);

    CUI_writeString(statusLineBuff, strlen(statusLineBuff));

    // If _spinLock is true, infinite loop and flash the leds
    if (_spinLock)
    {
        CUI_ledAssert();
    }
}

void CUI_menuActionBack(const int32_t _itemEntry)
{
    if (NULL != gpCurrMenu->pUpper)
    {
       gpCurrMenu = gpCurrMenu->pUpper;
       gCurrMenuItemEntry = gPrevMenuItemEntry;
    }
}

void CUI_menuActionHelp(char _input, char* _lines[3], CUI_cursorInfo_t* _curInfo)
{
    if (_input == CUI_ITEM_PREVIEW)
    {
        strncpy(_lines[1], "Press Enter for Help", MAX_MENU_LINE_LEN);
    }
    else
    {
        strncpy(_lines[0], "[Arrow Keys] Navigate Menus | [Enter] Perform Action, Enter Submenu", MAX_MENU_LINE_LEN);
        strncpy(_lines[1], "----------------------------|--------------------------------------", MAX_MENU_LINE_LEN);
        strncpy(_lines[2], "[Esc] Return to Main Menu   | [Backspace] Return to Parent Menu", MAX_MENU_LINE_LEN);
    }
}

/*********************************************************************
 * Private Functions
 */
static CUI_retVal_t CUI_updateRemLen(size_t* _currRemLen, char* _buff, size_t _buffSize)
{
    size_t newLen = strlen(_buff);

    if (newLen >= _buffSize - 1)
    {
        return CUI_FAILURE;
    }

    *_currRemLen = (_buffSize - newLen -1);
    return CUI_SUCCESS;
}

static void CUI_callMenuUartUpdateFn()
{
    /*
     * When a menu is registered it is guaranteed to contain a non NULL
     * menu update function. So if a sub menu doesn't have a valid uart update
     * function, at some point in the menu tree between the current menu and
     * the top level menu there will be a valid uart update function.
     *
     * If somehow the menu object has been corrupted and there is no non NULL
     * uart update function then nothing will be called.
     */
    CUI_menu_t* menu = gpCurrMenu;
    while (NULL != menu)
    {
        if (menu->uartUpdateFn)
        {
            menu->uartUpdateFn();
            break;
        }
        // Try the upper/parent menu to look for a uart update function
        menu = menu->pUpper;
    }

    /*
     * Somehow from gpCurrMenu to the top most menu there was no valid
     * uart update function to call. There is no way to account for this.
     */
}

static void UartWriteCallback(UART_Handle _handle, void *_buf, size_t _size)
{
    uint32_t key = HwiP_disable();
    gUartWriteComplete = true;
    /* Exit critical section */
    HwiP_restore(key);
}

static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size)
{
    // Make sure we received all expected bytes
    if (_size)
    {
        // If cleared, then read it
        if(gUartTxBuffer[0] == 0)
        {
            // Copy bytes from RX buffer to TX buffer
            for(size_t i = 0; i < _size; i++)
            {
                gUartTxBuffer[i] = ((uint8_t*)_buf)[i];
            }
        }
        memset(_buf, '\0', _size);
        CUI_callMenuUartUpdateFn();
    }
    else
    {
        // Handle error or call to UART_readCancel()
        UART_readCancel(gUartHandle);
    }
}

static void CUI_updateCursor(void)
{
    /*
     *  This buffer will be passed to CUI_writeString().
     *  The address must be valid at all times.
     */
    static char buff[32];
    if (gCursorActive)
    {
        System_snprintf(buff, sizeof(buff),
            CUI_ESC_CUR_HOME CUI_ESC_CUR_ROW_COL CUI_ESC_CUR_SHOW,
            gCursorInfo.row, gCursorInfo.col);
        CUI_writeString(buff, strlen(buff));
    }
}

static bool CUI_handleMenuIntercept(CUI_menuItem_t* _pItemEntry, uint8_t _input)
{
    bool updateHandled = false;
    bool interceptStarted = false;

    char *line[3];
    memset(menuBuff, '\0', sizeof(menuBuff));

    line[0] = &menuBuff[0];
    line[1] = &menuBuff[MAX_MENU_LINE_LEN + 2];
    line[2] = &menuBuff[MAX_MENU_LINE_LEN + 2 + MAX_MENU_LINE_LEN + 2];

    CUI_cursorInfo_t curInfo = {-1, -1};

    if (_pItemEntry->interceptable)
    {
        if (_pItemEntry->interceptActive)
        {
            // If intercept is active, pressing 'E' should disable it so that
            // normal navigation may continue
            if (CUI_INPUT_EXECUTE == _input)
            {
                _pItemEntry->interceptActive = false;

                // send key to application for handling
                if (_pItemEntry->item.pFnIntercept)
                {
                    _pItemEntry->item.pFnIntercept(CUI_ITEM_INTERCEPT_STOP,
                                                   line, &curInfo);
                }
                gCursorActive = false;
                updateHandled = true;
                CUI_dispMenu(false);
            }
            else if (CUI_INPUT_ESC == _input)
            {
                _pItemEntry->interceptActive = false;

                // send key to application for handling
                if (_pItemEntry->item.pFnIntercept)
                {
                    _pItemEntry->item.pFnIntercept(CUI_ITEM_INTERCEPT_CANCEL,
                                                   line, &curInfo);
                }
                gCursorActive = false;
                updateHandled = true;
                CUI_dispMenu(false);
            }
        }
        else if (CUI_INPUT_EXECUTE == _input)
        {
            /*
             * Since this screen is interceptable, pressing 'E' should start
             *  allowing the application to intercept the key presses.
             */
            _pItemEntry->interceptActive = true;
            interceptStarted = true;
        }

        if (_pItemEntry->interceptActive)
        {
            // Tell the Action if interception just started, else send the input directly
            char input = (interceptStarted ? CUI_ITEM_INTERCEPT_START : _input);

            // send key to application for handling
            if (_pItemEntry->item.pFnIntercept)
            {
                _pItemEntry->item.pFnIntercept(input, line, &curInfo);
            }

            updateHandled = true;
            CUI_dispMenu(true);

            // If a cursor should be shown, add this at the end of the string
            if ((curInfo.col != -1) && (curInfo.row != -1))
            {
                gCursorActive = true;
                gCursorInfo.col = curInfo.col;
                gCursorInfo.row = curInfo.row;
                CUI_updateCursor();
            }
            else
            {
                gCursorActive = false;
            }
        }
    }

    return updateHandled;
}

static CUI_retVal_t CUI_writeString(void * _buffer, size_t _size)
{
    /*
     * Since the UART driver is in Callback mode which is non blocking.
     *  If UART_write is called before a previous call to UART_write
     *  has completed it will not be printed. By taking a quick
     *  nap we can attempt to perform the subsequent write. If the
     *  previous call still hasn't finished after this nap the write
     *  will be skipped as it would have been before.
     */

    //Error if no buffer
    if((gUartHandle == NULL) || (_buffer == NULL) )
    {
        return CUI_UART_FAILURE;
    }

    bool uartReady = false;
    /* Enter critical section so this function is thread safe*/
    uint32_t key = HwiP_disable();
    if (gUartWriteComplete)
    {
        uartReady = true;
    }

    /* Exit critical section */
    HwiP_restore(key);

    Semaphore_pend(gUartSem, BIOS_WAIT_FOREVER);

    if (!uartReady)
    {
        /*
         * If the uart driver is not yet done with the previous call to
         * UART_write, then we can attempt to wait a small period of time.
         *
         * let's sleep 5000 ticks at 1000 tick intervals and keep checking
         * on the readiness of the UART driver.
         *
         * If it never becomes ready, we have no choice but to abandon this
         * UART_write call by returning CUI_PREV_WRITE_UNFINISHED.
         */
        uint8_t i;
        for (i = 0; i < 10; i++)
        {
            Task_sleep(1000);
            uint32_t key = HwiP_disable();
            if (gUartWriteComplete)
            {
                uartReady = true;
            }
            /* Exit critical section */
            HwiP_restore(key);
            if (uartReady)
            {
                break;
            }
        }

        // If it still isn't ready, the only option we have is to ignore
        // this print and hope that it wont be noticeable
        if (!uartReady)
        {
            return CUI_PREV_WRITE_UNFINISHED;
        }
    }

    key = HwiP_disable();
    gUartWriteComplete = false;
    HwiP_restore(key);

    // UART_write ret val ignored because we are in callback mode. The result
    // will always be zero.
    if (0 != UART_write(gUartHandle, (const void *)_buffer, _size))
    {
        Semaphore_post(gUartSem);
        return CUI_UART_FAILURE;
    }

    Semaphore_post(gUartSem);
    return CUI_SUCCESS;
}

static void CUI_dispMenu(bool _menuPopulated)
{
    //TODO: Remove magic 64 number
    // extra 64 is for cursor movement/clearing and start of menu char
    static char dispBuff[2][sizeof(menuBuff) + 64];
    static uint8_t currDispBuff = 0;
    char *line[3];

    line[0] = &menuBuff[0];
    line[1] = &menuBuff[MAX_MENU_LINE_LEN + 2];
    line[2] = &menuBuff[MAX_MENU_LINE_LEN + 2 + MAX_MENU_LINE_LEN + 2];

    if (false == _menuPopulated)
    {
        CUI_cursorInfo_t cursorInfo;
        CUI_menuItem_t* itemEntry = &(gpCurrMenu->menuItems[gCurrMenuItemEntry]);

        memset(menuBuff, '\0', sizeof(menuBuff));

        // Show the menu Title. Whenever possible to provide context
        if (gpCurrMenu == gpMainMenu)
        {
            strncpy(line[0], gpCurrMenu->pTitle, MAX_MENU_LINE_LEN);
        }
        else if (gCurrMenuItemEntry != (gpCurrMenu->numItems - 1))
        {
            /*
             *  If the current Menu Item is the 'back' item, leave the
             *  first line empty to keep the back screen clean.
             */
            CUI_menu_t* pMenu = gpCurrMenu;
            while((pMenu->pUpper) && (pMenu->pUpper != &cuiMultiMenu))
            {
                pMenu = pMenu->pUpper;
            }
            strncpy(line[0], pMenu->pTitle, MAX_MENU_LINE_LEN);
        }

        // If this is an interceptable item, instead of the title, allow a preview
        if (itemEntry->interceptable)
        {
            if (itemEntry->item.pFnIntercept)
            {
                itemEntry->item.pFnIntercept(CUI_ITEM_PREVIEW, line, &cursorInfo);
            }
        }

        // Guarantee the last line is not overwritten by the intercept function
        if (gpCurrMenu->menuItems[gCurrMenuItemEntry].pDesc == NULL)
        {
            // If the curr item is a sub menu, display the sub menu title
            strncpy(line[2], itemEntry->item.pSubMenu->pTitle, MAX_MENU_LINE_LEN);
        }
        else
        {
            // If not, display the items description
            strncpy(line[2], itemEntry->pDesc, MAX_MENU_LINE_LEN);
        }
    }

    /*
 * Clear the menu screen and prep it for re-draw
 */
#if !defined(CUI_SCROLL_PRINT)
    System_snprintf(dispBuff[currDispBuff], sizeof(dispBuff[currDispBuff]),
        CUI_ESC_CUR_HIDE CUI_ESC_CUR_MENU_BTM CUI_ESC_CLR_UP CUI_ESC_CUR_HOME "%c",
        MAX_MENU_LINE_LEN, CUI_MENU_START_CHAR);
#else
    dispBuff[currDispBuff][0] = '\0';
#endif


    /*
     * Start copying the menu into the dispBuff for writing to the UART
     *
     * Copy the first line, then add the newline and carriage return.
     * Do this for all three lines.
     *
     * The memory for the carriage returns, newlines, and the final
     *  CUI_END_CHAR are accounted for in the menuBuff already.
     */
    strncat(dispBuff[currDispBuff], line[0], MAX_MENU_LINE_LEN);

    //Set the newline and carriage return
    strncat(dispBuff[currDispBuff], CUI_NL_CR, sizeof(CUI_NL_CR));

    strncat(dispBuff[currDispBuff], line[1], MAX_MENU_LINE_LEN);
    strncat(dispBuff[currDispBuff], CUI_NL_CR, sizeof(CUI_NL_CR));

    strncat(dispBuff[currDispBuff], line[2], MAX_MENU_LINE_LEN);

#if !defined(CUI_SCROLL_PRINT)
    char endChar[1] = {CUI_END_CHAR};
    strncat(dispBuff[currDispBuff], endChar, 1);
#else
    strncat(dispBuff[currDispBuff], CUI_NL_CR, sizeof(CUI_NL_CR));
#endif
    CUI_writeString(dispBuff[currDispBuff], strlen(dispBuff[currDispBuff]));

    // Switch which buffer we use for the next call to CUI_statusLinePrintf()
    currDispBuff = !currDispBuff;
}

static CUI_retVal_t CUI_publicBtnsAPIChecks(const CUI_clientHandle_t _clientHandle)
{
    if (!gManageBtns)
    {
        return CUI_NOT_MANAGING_BTNS;
    }

    return CUI_publicAPIChecks(_clientHandle);
}

static CUI_retVal_t CUI_publicLedsAPIChecks(const CUI_clientHandle_t _clientHandle)
{
    if (!gManageLeds)
    {
        return CUI_NOT_MANAGING_LEDS;
    }

    return CUI_publicAPIChecks(_clientHandle);
}

static CUI_retVal_t CUI_publicUartAPIChecks(const CUI_clientHandle_t _clientHandle)
{
    if (!gManageUart)
    {
        return CUI_NOT_MANAGING_UART;
    }

    return CUI_publicAPIChecks(_clientHandle);
}

static CUI_retVal_t CUI_publicAPIChecks(const CUI_clientHandle_t _clientHandle)
{
    if (!gModuleInitialized)
    {
        return CUI_MODULE_UNINITIALIZED;
    }

    return CUI_validateHandle(_clientHandle);
}

static CUI_retVal_t CUI_validateHandle(const CUI_clientHandle_t _clientHandle)
{
    if (NULL == _clientHandle)
    {
        return CUI_INVALID_CLIENT_HANDLE;
    }

    if (CUI_getClientIndex(_clientHandle) == -1)
    {
        return CUI_INVALID_CLIENT_HANDLE;
    }
    else
    {
        return CUI_SUCCESS;
    }
}

static CUI_retVal_t CUI_acquireBtn(const CUI_clientHandle_t _clientHandle, const CUI_btnRequest_t* const _pRequest)
{
    if (NULL != gButtonResources[_pRequest->index].clientHash)
    {
        return CUI_FAILURE;
    }

    gButtonResources[_pRequest->index].clientHash = _clientHandle;
    gButtonResources[_pRequest->index].appCb = _pRequest->appCB;

    return CUI_SUCCESS;
}

static CUI_retVal_t CUI_acquireLed(const CUI_clientHandle_t _clientHandle, const uint32_t _index)
{
    if (NULL != gLedResources[_index].clientHash)
    {
        return CUI_FAILURE;
    }

    gLedResources[_index].clientHash = _clientHandle;

    return CUI_SUCCESS;
}

static CUI_retVal_t CUI_acquireStatusLine(const CUI_clientHandle_t _clientHandle, const char _pLabel[MAX_STATUS_LINE_LABEL_LEN], uint32_t* _pLineId)
{
    Semaphore_pend(gStatusSem, BIOS_WAIT_FOREVER);

    int clientIndex = CUI_getClientIndex(_clientHandle);

    int freeIndex = -1;
    for (int i = 0; i < gMaxStatusLines[clientIndex]; i++)
    {
        if (CUI_RELEASED == gStatusLineResources[clientIndex][i].status)
        {
            freeIndex = i;
            break;
        }
    }

    Semaphore_post(gStatusSem);

    if (-1 == freeIndex)
    {
        return CUI_NO_ASYNC_LINES_RELEASED;
    }

    uint32_t offset = 0;
    for (uint32_t i = 0; i < clientIndex; i++)
    {
        offset += gMaxStatusLines[i];
        offset ++; // allow 1 empty line between clients
    }

    offset += freeIndex;

    //Add a ": " to every label
    memset(gStatusLineResources[clientIndex][freeIndex].label, '\0', sizeof(gStatusLineResources[clientIndex][freeIndex].label));
    System_snprintf(gStatusLineResources[clientIndex][freeIndex].label,
             MAX_STATUS_LINE_LABEL_LEN + strlen(CUI_LABEL_VAL_SEP),
             "%s%s", _pLabel, CUI_LABEL_VAL_SEP);
    gStatusLineResources[clientIndex][freeIndex].lineOffset = offset;
    gStatusLineResources[clientIndex][freeIndex].clientHash = _clientHandle;
    gStatusLineResources[clientIndex][freeIndex].status = CUI_ACQUIRED;


    /*
     * Save this "line id" as a way to directly control the line, similarly to
     * how a client can directly control a led or button through pinIds.
     */
    *_pLineId = freeIndex;

    return CUI_SUCCESS;
}

static void CUI_menuActionNavigate(CUI_menuNavDir_t _navDir)
{
    // No menu change necessary. There is only one screen
    if (1 == gpCurrMenu->numItems)
    {
        return;
    }

    if (CUI_MENU_NAV_LEFT == _navDir)
    {
        // Wrap menu around from left to right
        gCurrMenuItemEntry =  (gCurrMenuItemEntry - 1 + gpCurrMenu->numItems) % (gpCurrMenu->numItems);
    }
    else if (CUI_MENU_NAV_RIGHT == _navDir)
    {
        // Wrap menu around from right to left
        gCurrMenuItemEntry =  (gCurrMenuItemEntry + 1 + gpCurrMenu->numItems) % (gpCurrMenu->numItems);
    }

    CUI_dispMenu(false);
}

static void CUI_menuActionExecute(void)
{
    if (NULL == gpCurrMenu->menuItems[gCurrMenuItemEntry].pDesc)
    {
        /*
         * If Item executed was a SubMenu, then preserve gCurrMenuItemEntry and enter the
         *  submenu.
         */
        gpCurrMenu = gpCurrMenu->menuItems[gCurrMenuItemEntry].item.pSubMenu;
        gPrevMenuItemEntry = gCurrMenuItemEntry;
        gCurrMenuItemEntry = 0;
    }
    else
    {
        /*
         * If Item executed was an Action, execute the action function.
         */
        CUI_pFnAction_t actionFn;
        actionFn = gpCurrMenu->menuItems[gCurrMenuItemEntry].item.pFnAction;
        if (actionFn)
        {
            actionFn(gCurrMenuItemEntry);
        }
    }

    CUI_dispMenu(false);
    return;
}

static CUI_retVal_t CUI_findMenu(CUI_menu_t* _pMenu, CUI_menu_t* _pDesiredMenu, uint32_t* _pPrevItemIndex)
{
    if (_pMenu == _pDesiredMenu)
    {
        return CUI_SUCCESS;
    }

    uint32_t numItems = _pMenu->numItems;
    for (int i = 0; i < numItems; i++)
    {
        *_pPrevItemIndex = i;
        /*
         * If pDesc is NULL, it is a subMenu
         */
        if (NULL == _pMenu->menuItems[i].pDesc)
        {
            CUI_menu_t* subMenu = _pMenu->menuItems[i].item.pSubMenu;

            if (CUI_SUCCESS == CUI_findMenu(subMenu, _pDesiredMenu, _pPrevItemIndex))
            {
                return CUI_SUCCESS;
            }
        }
    }
    return CUI_FAILURE;
}

static int CUI_getClientIndex(const CUI_clientHandle_t _clientHandle)
{
    for (uint32_t i = 0; i < MAX_CLIENTS; i++)
    {
        if (_clientHandle == gClientHandles[i])
        {
            return i;
        }
    }
    return -1;
}
