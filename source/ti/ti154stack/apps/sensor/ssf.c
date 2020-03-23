/******************************************************************************

 @file ssf.c

 @brief Sensor Specific Functions

 Group: WCS LPC
 Target Device: cc13x2_26x2

 ******************************************************************************
 
 Copyright (c) 2016-2019, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 
 
 *****************************************************************************/

/******************************************************************************
 Includes
 *****************************************************************************/

#include <xdc/std.h>
#include <string.h>
#include <stdlib.h>

#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/Power.h>
#include <inc/hw_ints.h>
#include <aon_event.h>
#include <ioc.h>
#include <driverlib/aon_batmon.h>

#include "ti_drivers_config.h"

#include "util_timer.h"
#include "mac_util.h"
#include "cui.h"

#include "macconfig.h"

#ifdef ONE_PAGE_NV
#include "nvocop.h"
#else
#include "nvocmp.h"
#endif

#include "sensor.h"
#include "smsgs.h"
#include "ssf.h"
#include "ti_154stack_config.h"

#ifdef FEATURE_NATIVE_OAD
#include "oad_client.h"
#endif //FEATURE_NATIVE_OAD

#ifdef OSAL_PORT2TIRTOS
#include "osal_port.h"
#else
#include "icall.h"
#endif

/******************************************************************************
 Constants and definitions
 *****************************************************************************/

/* Initial timeout value for the reading clock */
#define READING_INIT_TIMEOUT_VALUE 100

/* SSF Events */
#define KEY_EVENT               0x0001
#define SENSOR_UI_INPUT_EVT     0x0002
#define SENSOR_SEND_COLLECTOR_IDENT_EVT  0x0004

/* NV Item ID - the device's network information */
#define SSF_NV_NETWORK_INFO_ID 0x0001
/* NV Item ID - Config information */
#define SSF_NV_CONFIG_INFO_ID  0x0002
/* NV Item ID - this devices frame counter */
#define SSF_NV_FRAMECOUNTER_ID 0x0004
/* NV Item ID - Assert reset reason */
#define SSF_NV_RESET_REASON_ID 0x0005
/* NV Item ID - Number of resets */
#define SSF_NV_RESET_COUNT_ID 0x0006
/* NV Item ID - OAD information */
#define SSF_NV_OAD_ID 0x0007
/* NV Item ID - Device Key information */
#define SSF_NV_DEVICE_KEY_ID  0x0008

/* timeout value for trickle timer initialization */
#define TRICKLE_TIMEOUT_VALUE       30000

/* timeout value for poll timer initialization */
#define POLL_TIMEOUT_VALUE          30000

#define FH_ASSOC_TIMER              2000

/* timeout value for poll timer initialization */
#define SCAN_BACKOFF_TIMEOUT_VALUE  60000

/*! NV driver item ID for reset reason */
#define NVID_RESET {NVINTF_SYSID_APP, SSF_NV_RESET_REASON_ID, 0}

/*! Additional Random Delay for Association */
#define ADD_ASSOCIATION_RANDOM_WINDOW 10000
/*
 The increment value needed to save a frame counter. Example, setting this
 constant to 100, means that the frame counter will be saved when the new
 frame counter is 100 more than the last saved frame counter.  Also, when
 the get frame counter function reads the value from NV it will add this value
 to the read value.
 */
#define FRAME_COUNTER_SAVE_WINDOW     25

#if defined(USE_DMM)
#define PROVISIONING_ASSOC_TIMER    1000
#define PROVISIONING_DISASSOC_TIMER 10
#endif

/* Structure to store the device information in NV */
typedef struct
{
    ApiMac_deviceDescriptor_t device;
    Llc_netInfo_t parent;
} nvDeviceInfo_t;



/******************************************************************************
 External variables
 *****************************************************************************/
#ifdef NV_RESTORE
/*! MAC Configuration Parameters */
extern mac_Config_t Main_user1Cfg;
#endif

/******************************************************************************
 Public variables
 *****************************************************************************/

/*!
 Assert reason for the last reset -  0 - no reason, 2 - HAL/ICALL,
 3 - MAC, 4 - TIRTOS
 */
uint8_t Ssf_resetReseason = 0;

/*! Number of times the device has reset */
uint16_t Ssf_resetCount = 0;

/******************************************************************************
 Local variables
 *****************************************************************************/

/* The application's semaphore */
#ifdef OSAL_PORT2TIRTOS
static Semaphore_Handle sensorSem;
#else
static ICall_Semaphore sensorSem;
#endif

/* Clock/timer resources */
static Clock_Struct readingClkStruct;
static Clock_Handle readingClkHandle;

/* Clock/timer resources for JDLLC */
/* trickle timer */
STATIC Clock_Struct tricklePASClkStruct;
STATIC Clock_Handle tricklePASClkHandle;
STATIC Clock_Struct tricklePCSClkStruct;
STATIC Clock_Handle tricklePCSClkHandle;
/* poll timer */
STATIC Clock_Struct pollClkStruct;
STATIC Clock_Handle pollClkHandle;
/* scan backoff timer */
STATIC Clock_Struct scanBackoffClkStruct;
STATIC Clock_Handle scanBackoffClkHandle;
/* FH assoc delay */
STATIC Clock_Struct fhAssocClkStruct;
STATIC Clock_Handle fhAssocClkHandle;

#ifdef USE_DMM
STATIC Clock_Struct provisioningClkStruct;
STATIC Clock_Handle provisioningClkHandle;
#endif /* USE_DMM */

/* Key press parameters */
static uint8_t keys = 0xFF;

/* pending events */
static uint16_t events = 0;

/* NV Function Pointers */
static NVINTF_nvFuncts_t *pNV = NULL;

/* The last saved frame counter */
static uint32_t lastSavedFrameCounter = 0;

/*! NV driver item ID for reset reason */
static const NVINTF_itemID_t nvResetId = NVID_RESET;

static bool started = false;

static bool led1State = false;

CUI_clientHandle_t ssfCuiHndl;
uint32_t sensorStatusLine;
uint32_t perStatusLine;

/******************************************************************************
 Local function prototypes
 *****************************************************************************/

static void processReadingTimeoutCallback(UArg a0);
static void processKeyChangeCallback(uint32_t _btn, Button_EventMask _events);
static void processPCSTrickleTimeoutCallback(UArg a0);
static void processPASTrickleTimeoutCallback(UArg a0);
static void processPollTimeoutCallback(UArg a0);
static void processScanBackoffTimeoutCallback(UArg a0);
static void processFHAssocTimeoutCallback(UArg a0);
#if defined(USE_DMM)
static void processProvisioningCallback(UArg a0);
#endif

#if !defined(POWER_MEAS)
static uint8_t moveCursorLeft(uint8_t col, uint8_t left_boundary, uint8_t right_boundary, uint8_t skip_space);
static uint8_t moveCursorRight(uint8_t col, uint8_t left_boundary, uint8_t right_boundary, uint8_t skip_space);
static void setPanIdAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo);
static void setChMaskAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo);
#if CONFIG_FH_ENABLE
static void setAsyncChMaskAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo);
#endif
#ifdef FEATURE_MAC_SECURITY
static void setNwkKeyAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo);
#endif
#ifdef FEATURE_SECURE_COMMISSIONING
static void setSmSetAuthModeAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo);
static void setSmPassKeyAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo);
#endif
static void assocAction(int32_t menuEntryInex);
static void disassocAction(int32_t menuEntryInex);
static void collectorLedIndentifyAction(int32_t menuEntryInex);
static void processMenuUpdate(void);

static void uintToString (uint32_t value, char * str, uint8_t base, uint8_t num_of_digists, bool pad0, bool reverse);

#endif

/* Menu */
#if !defined(POWER_MEAS)
#ifdef OAD_ONCHIP
#ifdef OAD_IMG_A
    #define SFF_MENU_TITLE " TI Sensor (Persistent App) "
#else /* OAD_IMG_A */
#define SFF_MENU_TITLE " TI Sensor (User App) "
#endif
#else
    #define SFF_MENU_TITLE " TI Sensor "
#endif

#if CONFIG_FH_ENABLE
#define FH_MENU_ENABLED 1
#else
#define FH_MENU_ENABLED 0
#endif

#ifdef FEATURE_SECURE_COMMISSIONING
#define SM_MENU_ENABLED 1
#else
#define SM_MENU_ENABLED 0
#endif

#ifdef FEATURE_MAC_SECURITY
#define SECURITY_MENU_ENABLED 1
#else
#define SECURITY_MENU_ENABLED 0
#endif

CUI_SUB_MENU(configureSubMenu, "<      CONFIGURE      >", 2 + FH_MENU_ENABLED + SM_MENU_ENABLED + SECURITY_MENU_ENABLED, ssfMainMenu)
    CUI_MENU_ITEM_INT_ACTION(  "<      SET PANID      >", setPanIdAction)
    CUI_MENU_ITEM_INT_ACTION(  "<    SET CHAN MASK    >", setChMaskAction)
#if CONFIG_FH_ENABLE
    CUI_MENU_ITEM_INT_ACTION(  "<  SET AS CHAN MASK   >", setAsyncChMaskAction)
#endif
#ifdef FEATURE_MAC_SECURITY
    CUI_MENU_ITEM_INT_ACTION(  "<     SET NWK KEY     >", setNwkKeyAction)
#endif
#ifdef FEATURE_SECURE_COMMISSIONING
    CUI_MENU_ITEM_INT_ACTION(  "<     AUTH METHOD     >", setSmSetAuthModeAction)
#endif
CUI_SUB_MENU_END



CUI_SUB_MENU(commissionSubMenu,"<   NETWORK ACTIONS   >", 2, ssfMainMenu)
    CUI_MENU_ITEM_ACTION(      "<      ASSOCIATE      >", assocAction)
    CUI_MENU_ITEM_ACTION(      "<    DISASSOCIATE     >", disassocAction)
CUI_SUB_MENU_END

CUI_SUB_MENU(appSubMenu,       "<         APP         >", 1, ssfMainMenu)
    CUI_MENU_ITEM_ACTION(      "<   SEND LED IDENT    >", collectorLedIndentifyAction)
CUI_SUB_MENU_END


/* This menu will be registered/de-registered at run time to create
a sort of "popup" menu for passkey entry. Since it is de-registered,
it is also completely hidden from the user at all other times.
Note: MAX_REGISTERED_MENUS must be >= 2 for both of the main
menus in this file. */
#ifdef FEATURE_SECURE_COMMISSIONING
CUI_MAIN_MENU(smPassKeyMenu, "<       SM MENU        >", 1, processMenuUpdate)
    CUI_MENU_ITEM_INT_ACTION(  "<    ENTER PASSKEY     >", setSmPassKeyAction)
CUI_MAIN_MENU_END
#endif

CUI_MAIN_MENU(ssfMainMenu, SFF_MENU_TITLE, 3, processMenuUpdate)
    CUI_MENU_ITEM_SUBMENU(configureSubMenu)
    CUI_MENU_ITEM_SUBMENU(commissionSubMenu)
    CUI_MENU_ITEM_SUBMENU(appSubMenu)
CUI_MAIN_MENU_END


#endif /* !POWER_MEAS */
/******************************************************************************
 Public Functions
 *****************************************************************************/

/*!
 The application calls this function during initialization

 Public function defined in ssf.h
 */
void Ssf_init(void *sem)
{
    CUI_clientParams_t clientParams;
#ifdef FEATURE_NATIVE_OAD
    OADClient_Params_t OADClientParams;
#endif //FEATURE_NATIVE_OAD

#ifdef NV_RESTORE
    /* Save off the NV Function Pointers */
    pNV = &Main_user1Cfg.nvFps;
#endif

    /* Save off the semaphore */
    sensorSem = sem;

    CUI_clientParamsInit(&clientParams);

    strncpy(clientParams.clientName, "154 Sensor", MAX_CLIENT_NAME_LEN);
    clientParams.maxStatusLines = 1;

#ifdef DISPLAY_PER_STATS
    clientParams.maxStatusLines++;
#endif
#ifdef FEATURE_SECURE_COMMISSIONING
    clientParams.maxStatusLines++;
#endif
#ifdef SECURE_MANAGER_DEBUG
    clientParams.maxStatusLines++;
#endif
#ifdef SECURE_MANAGER_DEBUG2
    clientParams.maxStatusLines++;
#endif
#ifdef FEATURE_NATIVE_OAD
    clientParams.maxStatusLines++;
#endif


    /* Open UI for key and LED */
    ssfCuiHndl = CUI_clientOpen(&clientParams);

#ifdef FEATURE_SECURE_COMMISSIONING
    /* Intialize the security manager and register callbacks */
    SM_init(sensorSem, ssfCuiHndl);
#endif //FEATURE_SECURE_COMMISSIONING

    /* Initialize Keys */
    CUI_btnRequest_t leftBtnReq;
    leftBtnReq.index = CONFIG_BTN_LEFT;
    leftBtnReq.appCB = processKeyChangeCallback;
    CUI_btnResourceRequest(ssfCuiHndl, &leftBtnReq);

    CUI_btnRequest_t rightBtnReq;
    rightBtnReq.index = CONFIG_BTN_RIGHT;
    rightBtnReq.appCB = NULL;
    CUI_btnResourceRequest(ssfCuiHndl, &rightBtnReq);

    bool btnState = false;
    CUI_btnGetValue(ssfCuiHndl, CONFIG_BTN_RIGHT, &btnState);
    if (!btnState) {
        /* Right key is pressed on power up, clear all NV */
        Ssf_clearAllNVItems();
    }

    CUI_btnSetCb(ssfCuiHndl, CONFIG_BTN_RIGHT, processKeyChangeCallback);

#if !defined(POWER_MEAS)
    /* Initialize the LEDs */
    CUI_ledRequest_t greenLedReq;
    greenLedReq.index = CONFIG_LED_GREEN;
    CUI_ledResourceRequest(ssfCuiHndl, &greenLedReq);

    CUI_ledRequest_t redLedReq;
    redLedReq.index = CONFIG_LED_RED;
    CUI_ledResourceRequest(ssfCuiHndl, &redLedReq);

    // Blink to indicate the application started up correctly
    CUI_ledBlink(ssfCuiHndl, CONFIG_LED_RED, 3);

    CUI_registerMenu(ssfCuiHndl, &ssfMainMenu);

    CUI_statusLineResourceRequest(ssfCuiHndl, "Status", &sensorStatusLine);
#ifdef DISPLAY_PER_STATS
    CUI_statusLineResourceRequest(ssfCuiHndl, "Sensor PER", &perStatusLine);
#endif
#endif /* POWER_MEAS */

    if((pNV != NULL) && (pNV->readItem != NULL))
    {
        /* Attempt to retrieve reason for the reset */
        (void)pNV->readItem(nvResetId, 0, 1, &Ssf_resetReseason);
    }

    if((pNV != NULL) && (pNV->deleteItem != NULL))
    {
        /* Only use this reason once */
        (void)pNV->deleteItem(nvResetId);
    }

    if((pNV != NULL) && (pNV->readItem != NULL))
    {
        NVINTF_itemID_t id;
        uint16_t resetCount = 0;

        /* Setup NV ID */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_RESET_COUNT_ID;
        id.subID = 0;

        /* Read the reset count */
        pNV->readItem(id, 0, sizeof(resetCount), &resetCount);

        Ssf_resetCount = resetCount;
        if(pNV->writeItem)
        {
          /* Update the reset count for the next reset */
          resetCount++;
          pNV->writeItem(id, sizeof(resetCount), &resetCount);
        }
    }

#ifdef FEATURE_NATIVE_OAD
    OADClientParams.pEvent = &Sensor_events;
    OADClientParams.eventSem = sensorSem;
    OADClientParams.pOadCuiHndl = &ssfCuiHndl;

    OADClient_open(&OADClientParams);
#endif //FEATURE_NATIVE_OAD

#ifndef POWER_MEAS
#if !defined(AUTO_START)
    CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Waiting...");
#else
    CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Starting");
#endif /* !POWER_MEAS */
#endif
}

/*!
 The application must call this function periodically to
 process any events that this module needs to process.

 Public function defined in ssf.h
 */
void Ssf_processEvents(void)
{
    /* Did a key press occur? */
    if(events & KEY_EVENT)
    {
        /* Right key press is a PAN disassociation request, if the device has started. */
        if((keys == CONFIG_BTN_RIGHT) && (started == true))
        {
            if ((Jdllc_getProvState() == Jdllc_states_joined) ||
                (Jdllc_getProvState() == Jdllc_states_rejoined))
                {
                /* Send disassociation request only if you are in a network */
#ifndef POWER_MEAS
                CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Leaving");
#endif
                Jdllc_sendDisassociationRequest();
                }
        }
        /* Left key press is for starting the sensor network */
        else if(keys == CONFIG_BTN_LEFT)
        {

            if(started == false)
            {
#if !defined(POWER_MEAS)
                CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Starting");
#endif

                /* Tell the sensor to start */
                Util_setEvent(&Sensor_events, SENSOR_START_EVT);
                /* Wake up the application thread when it waits for clock event */
                Semaphore_post(sensorSem);
            }
            else
            {   /* Send LED toggle request to identify collector */
                Sensor_sendIdentifyLedRequest();
            }
        }

        /* Clear the key press indication */
        keys = 0xFF;

        /* Clear the event */
        Util_clearEvent(&events, KEY_EVENT);
    }

#ifdef FEATURE_NATIVE_OAD
    /* Did a OAD event occur? */
#ifdef FEATURE_TOAD
    if(Sensor_events & SENSOR_OAD_TIMEOUT_EVT || Sensor_events & SENSOR_TOAD_DECODE_EVT)
#else
    if(Sensor_events & SENSOR_OAD_TIMEOUT_EVT)
#endif
    {
        OADClient_processEvent(&Sensor_events);
    }
#endif //FEATURE_NATIVE_OAD

#ifndef POWER_MEAS
    if(events & SENSOR_UI_INPUT_EVT)
    {
        CUI_processMenuUpdate();

        Util_clearEvent(&events, SENSOR_UI_INPUT_EVT);
    }
#endif

    if(events & SENSOR_SEND_COLLECTOR_IDENT_EVT)
    {
        Sensor_sendIdentifyLedRequest();

        Util_clearEvent(&events, SENSOR_SEND_COLLECTOR_IDENT_EVT);
    }

}

/*!
 The application calls this function to indicate that the
 Sensor's state has changed.

 Public function defined in ssf.h
 */
void Ssf_stateChangeUpdate(Jdllc_states_t state)
{
#if !defined(POWER_MEAS)
    if(state == Jdllc_states_initWaiting)
    {
        CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Waiting");
    }
    else if(state == Jdllc_states_orphan)
    {
        CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Orphaned");
    }
    else if(state == Jdllc_states_accessDenied)
    {
        CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Access Denied");
    }

    if(Jdllc_getPrevProvState() == Jdllc_states_orphan)
    {
        if(state == Jdllc_states_joined)
        {
            CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Joined");
        }
        else if(state == Jdllc_states_rejoined)
        {
            CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Rejoined");
        }
    }
#endif
}

/*!
 The application calls this function to indicate that it has
 started or restored the device in a network.

 Public function defined in ssf.h
 */
void Ssf_networkUpdate(bool rejoined,
                       ApiMac_deviceDescriptor_t *pDevInfo,
                       Llc_netInfo_t  *pParentInfo)
{

#if !defined(POWER_MEAS)
#if (CONFIG_MAC_BEACON_ORDER != NON_BEACON_ORDER)
    char macMode[4] = "BCN\0";
#elif (CONFIG_FH_ENABLE)
    char macMode[3] = "FH\0";
#else
    char macMode[5] = "NBCN\0";
#endif
#endif

    /* check for valid structure pointers, ignore if not */
    if((pDevInfo != NULL) && (pParentInfo != NULL))
    {
        if((pNV != NULL) && (pNV->writeItem != NULL))
        {
            NVINTF_itemID_t id;
            nvDeviceInfo_t nvItem;

            /* Setup NV ID */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = SSF_NV_NETWORK_INFO_ID;
            id.subID = 0;

            memcpy(&nvItem.device, pDevInfo, sizeof(ApiMac_deviceDescriptor_t));
            memcpy(&nvItem.parent, pParentInfo, sizeof(Llc_netInfo_t));

            /* Write the NV item */
            pNV->writeItem(id, sizeof(nvDeviceInfo_t), &nvItem);
        }


        if(pParentInfo->fh == false)
        {
#if !defined(POWER_MEAS)
            if(rejoined == false)
            {
                CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Joined--Mode=%s, Addr=0x%04x, PanId=0x%04x, Ch=%d",
                                     macMode, pDevInfo->shortAddress, pDevInfo->panID, pParentInfo->channel);
            }
            else
            {
                CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Rejoined--Mode=%s, Addr=0x%04x, PanId=0x%04x, Ch=%d",
                                     macMode, pDevInfo->shortAddress, pDevInfo->panID, pParentInfo->channel);
            }

#endif /* !POWER_MEAS */
            started = true;
        }
        else
        {
#if !defined(POWER_MEAS)
            if(rejoined == false)
            {
                CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Joined--Mode=%s, Addr=0x%04x, PanId=0x%04x, Ch=FH",
                                     macMode, pDevInfo->shortAddress, pDevInfo->panID, pParentInfo->channel);
            }
            else
            {
                CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Rejoined--Mode=%s, Addr=0x%04x, PanId=0x%04x, Ch=FH",
                                     macMode, pDevInfo->shortAddress, pDevInfo->panID, pParentInfo->channel);
            }
#endif /* !POWER_MEAS */

            started = true;
        }

#if !defined(POWER_MEAS)
        CUI_ledOn(ssfCuiHndl, CONFIG_LED_RED, NULL);
#endif
        led1State = true;
    }
}

#ifdef FEATURE_SECURE_COMMISSIONING
/*!
 The application calls this function to store the device key information to NV
 Public function defined in ssf.h
 */
void Ssf_DeviceKeyInfoUpdate(nvDeviceKeyInfo_t *pDevKeyInfo)
{
    /* check for valid structure pointers, ignore if not */
    if(pDevKeyInfo != NULL)
    {
        if((pNV != NULL) && (pNV->writeItem != NULL))
        {
            NVINTF_itemID_t id;
            nvDeviceKeyInfo_t nvItem;

            /* Setup NV ID */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = SSF_NV_DEVICE_KEY_ID;
            id.subID = 0;

            memcpy(&nvItem, pDevKeyInfo, sizeof(nvDeviceKeyInfo_t));

            /* Write the NV item */
            pNV->writeItem(id, sizeof(nvDeviceKeyInfo_t), &nvItem);
        }
    }
}

/*!
 The application calls this function to get the device
 *              Key information.

 Public function defined in ssf.h
 */
bool Ssf_getDeviceKeyInfo(nvDeviceKeyInfo_t *pDevKeyInfo)
{
    if((pNV != NULL) && (pNV->readItem != NULL) && (pDevKeyInfo != NULL))
    {
        NVINTF_itemID_t id;
        nvDeviceKeyInfo_t nvItem;

        /* Setup NV ID */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_DEVICE_KEY_ID;
        id.subID = 0;

        /* Read Network Information from NV */
        if(pNV->readItem(id, 0, sizeof(nvDeviceKeyInfo_t),
                         &nvItem) == NVINTF_SUCCESS)
        {
            memcpy(pDevKeyInfo, &nvItem,sizeof(nvDeviceKeyInfo_t));
            return (true);
        }
    }
    return (false);
}

/*!

 Clear device key information in NV

 Public function defined in ssf.h
 */
void Ssf_clearDeviceKeyInfo( void )
{
    if((pNV != NULL) && (pNV->deleteItem != NULL))
    {
        NVINTF_itemID_t id;

        /* Setup NV ID */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_DEVICE_KEY_ID;
        id.subID = 0;
        pNV->deleteItem(id);

        /* sensor ready to associate again */
        started = false;
    }
}

#endif



/*!
 The application calls this function to get the device
 *              information in a network.

 Public function defined in ssf.h
 */
bool Ssf_getNetworkInfo(ApiMac_deviceDescriptor_t *pDevInfo,
                        Llc_netInfo_t  *pParentInfo)
{
    if((pNV != NULL) && (pNV->readItem != NULL)
                    && (pDevInfo != NULL) && (pParentInfo != NULL))
    {
        NVINTF_itemID_t id;
        nvDeviceInfo_t nvItem;

        /* Setup NV ID */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_NETWORK_INFO_ID;
        id.subID = 0;

        /* Read Network Information from NV */
        if(pNV->readItem(id, 0, sizeof(nvDeviceInfo_t),
                         &nvItem) == NVINTF_SUCCESS)
        {
            memcpy(pDevInfo, &nvItem.device,
                   sizeof(ApiMac_deviceDescriptor_t));
            memcpy(pParentInfo, &nvItem.parent, sizeof(Llc_netInfo_t));

            return (true);
        }
    }
    return (false);
}



#ifdef FEATURE_NATIVE_OAD
/*!
 The application calls this function to update the OAD info in NV.

 Public function defined in ssf.h
 */
void Ssf_oadInfoUpdate(uint16_t *pOadBlock, uint8_t *pOadImgHdr, uint8_t *pOadImgId, ApiMac_sAddr_t *pOadServerAddr)
{
    NVINTF_itemID_t id;

    /* Setup NV ID */
    id.systemID = NVINTF_SYSID_APP;
    id.itemID = SSF_NV_OAD_ID;
    id.subID = 0;
    /* Write the NV item */
    pNV->writeItem(id, sizeof(uint16_t), pOadBlock);

    if(pOadImgHdr != NULL)
    {
        id.subID = 1;
        /* Write the NV item */
        pNV->writeItem(id, sizeof(uint8_t) * OADProtocol_IMAGE_ID_LEN, pOadImgHdr);
    }

    if(pOadImgId != NULL)
    {
        id.subID = 2;
        /* Write the NV item */
        pNV->writeItem(id, sizeof(uint8_t), pOadImgId);
    }

    if(pOadServerAddr != NULL)
    {
        id.subID = 3;
        /* Write the NV item */
        pNV->writeItem(id, sizeof(ApiMac_sAddr_t), pOadServerAddr);
    }
}

/*!
 The application calls this function to get the device
 *              information in a network.

 Public function defined in ssf.h
 */
bool Ssf_getOadInfo(uint16_t *pOadBlock, uint8_t *pOadImgHdr, uint8_t *pOadImgId, ApiMac_sAddr_t *pOadServerAddr)
{
    NVINTF_itemID_t id;
    bool status = false;

    if((pNV != NULL) && (pNV->readItem != NULL)
                    && (pOadBlock != NULL) && (pOadImgHdr != NULL))
    {
        /* Setup NV ID */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_OAD_ID;
        id.subID = 0;

        /* Read OAD Block from NV */
        if(pNV->readItem(id, 0, sizeof(uint16_t),
                         pOadBlock) == NVINTF_SUCCESS)
        {
            status = true;
        }
    }

    if(status == true)
    {
        id.subID = 1;
        /* Read OAD image hdr from NV */
        if(pNV->readItem(id, 0, sizeof(uint8_t) * OADProtocol_IMAGE_ID_LEN,
                         pOadImgHdr) != NVINTF_SUCCESS)
        {
            status = false;
        }
    }

    if(status == true)
    {
        id.subID = 2;
        /* Read OAD image ID from NV */
        if(pNV->readItem(id, 0, sizeof(uint8_t),
                         pOadImgId) != NVINTF_SUCCESS)
        {
            status = false;
        }
    }

    if(status == true)
    {
        id.subID = 3;
        /* Read OAD image ID from NV */
        if(pNV->readItem(id, 0, sizeof(ApiMac_sAddr_t),
                         pOadServerAddr) != NVINTF_SUCCESS)
        {
            status = false;
        }
    }

    return status;
}

/*!

 Clear OAD information in NV

 Public function defined in ssf.h
 */
void Ssf_clearOadInfo()
{
    if((pNV != NULL) && (pNV->deleteItem != NULL))
    {
        NVINTF_itemID_t id;

        /* Setup NV ID */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_OAD_ID;
        id.subID = 0;
        pNV->deleteItem(id);

        id.subID = 1;
        pNV->deleteItem(id);

        id.subID = 2;
        pNV->deleteItem(id);

        id.subID = 3;
        pNV->deleteItem(id);

    }
}
#endif /* FEATURE_NATIVE_OAD */

/*!
 The application calls this function to indicate a Configuration
 Request message.

 Public function defined in ssf.h
 */
void Ssf_configurationUpdate(Smsgs_configRspMsg_t *pRsp)
{
    if((pNV != NULL) && (pNV->writeItem != NULL) && (pRsp != NULL))
    {
        NVINTF_itemID_t id;
        Ssf_configSettings_t configInfo;

        /* Setup NV ID */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_CONFIG_INFO_ID;
        id.subID = 0;

        configInfo.frameControl = pRsp->frameControl;
        configInfo.reportingInterval = pRsp->reportingInterval;
        configInfo.pollingInterval = pRsp->pollingInterval;

        /* Write the NV item */
        pNV->writeItem(id, sizeof(Ssf_configSettings_t), &configInfo);
    }
}

/*!
 The application calls this function to get the saved device configuration.

 Public function defined in ssf.h
 */
bool Ssf_getConfigInfo(Ssf_configSettings_t *pInfo)
{
    if((pNV != NULL) && (pNV->readItem != NULL) && (pInfo != NULL))
    {
        NVINTF_itemID_t id;

        /* Setup NV ID */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_CONFIG_INFO_ID;
        id.subID = 0;

        /* Read Network Information from NV */
        if(pNV->readItem(id, 0, sizeof(Ssf_configSettings_t),
                         pInfo) == NVINTF_SUCCESS)
        {
            return (true);
        }
    }
    return (false);
}

/*!
 The application calls this function to indicate that a tracking message
 was received.

 Public function defined in ssf.h
 */
void Ssf_trackingUpdate(ApiMac_sAddr_t *pSrcAddr)
{
}

/*!
 The application calls this function to indicate sensor data.

 Public function defined in ssf.h
 */
void Ssf_sensorReadingUpdate(Smsgs_sensorMsg_t *pMsg)
{
}

/*!
 Initialize the reading clock.

 Public function defined in ssf.h
 */
void Ssf_initializeReadingClock(void)
{
    /* Initialize the timers needed for this application */
    readingClkHandle = Timer_construct(&readingClkStruct,
                                        processReadingTimeoutCallback,
                                        READING_INIT_TIMEOUT_VALUE,
                                        0,
                                        false,
                                        0);
}

/*!
 Set the reading clock.

 Public function defined in ssf.h
 */
void Ssf_setReadingClock(uint32_t readingTime)
{
    /* Stop the Reading timer */
    if(Timer_isActive(&readingClkStruct) == true)
    {
        Timer_stop(&readingClkStruct);
    }
#ifdef POWER_MEAS
    if(POWER_TEST_PROFILE != DATA_ACK)
    {
        /* Do not sent data in other power test profiles */
        return;
    }
#endif
    /* Setup timer */
    if ( readingTime )
    {
        Timer_setTimeout(readingClkHandle, readingTime);
        Timer_start(&readingClkStruct);
    }
}

/*!
 Ssf implementation for memory allocation

 Public function defined in ssf.h
 */
void *Ssf_malloc(uint16_t size)
{
#ifdef OSAL_PORT2TIRTOS
    return OsalPort_malloc(size);
#else
    return ICall_malloc(size);
#endif
}

/*!
 Ssf implementation for memory de-allocation

 Public function defined in ssf.h
 */
void Ssf_free(void *ptr)
{
    if(ptr != NULL)
    {
#ifdef OSAL_PORT2TIRTOS
        OsalPort_free(ptr);
#else
        ICall_free(ptr);
#endif
    }
}

/*!
 Initialize the trickle clock.

 Public function defined in ssf.h
 */
void Ssf_initializeTrickleClock(void)
{
    /* Initialize trickle timer */
    tricklePASClkHandle = Timer_construct(&tricklePASClkStruct,
                                         processPASTrickleTimeoutCallback,
                                         TRICKLE_TIMEOUT_VALUE,
                                         0,
                                         false,
                                         0);

    tricklePCSClkHandle = Timer_construct(&tricklePCSClkStruct,
                                         processPCSTrickleTimeoutCallback,
                                         TRICKLE_TIMEOUT_VALUE,
                                         0,
                                         false,
                                         0);
}

/*!
 Set the trickle clock.

 Public function defined in ssf.h
 */
void Ssf_setTrickleClock(uint16_t trickleTime, uint8_t frameType)
{
    uint16_t randomNum = 0;
    if(frameType == ApiMac_wisunAsyncFrame_advertisementSolicit)
    {
        /* Stop the PA trickle timer */
        if(Timer_isActive(&tricklePASClkStruct) == true)
        {
            Timer_stop(&tricklePASClkStruct);
        }

        if(trickleTime > 0)
        {
            /* Trickle Time has to be a value chosen random between [t/2, t] */
            randomNum = ((ApiMac_randomByte() << 8) + ApiMac_randomByte());
            trickleTime = (trickleTime >> 1) +
                          (randomNum % (trickleTime >> 1));
            /* Setup timer */
            Timer_setTimeout(tricklePASClkHandle, trickleTime);
            Timer_start(&tricklePASClkStruct);
        }
    }
    else if(frameType == ApiMac_wisunAsyncFrame_configSolicit)
    {
        /* Stop the PC trickle timer */
        if(Timer_isActive(&tricklePCSClkStruct) == true)
        {
            Timer_stop(&tricklePCSClkStruct);
        }

        if(trickleTime > 0)
        {
            /* Setup timer */
            /* Trickle Time has to be a value chosen random between [t/2, t] */
            /* Generate a 16 bit random number */
            randomNum = ((ApiMac_randomByte() << 8) + ApiMac_randomByte());
            trickleTime = (trickleTime >> 1) +
                          (randomNum % (trickleTime >> 1));
            Timer_setTimeout(tricklePCSClkHandle, trickleTime);
            Timer_start(&tricklePCSClkStruct);
        }
    }
}

/*!
 Initialize the poll clock.

 Public function defined in ssf.h
 */
void Ssf_initializePollClock(void)
{
    /* Initialize the timers needed for this application */
    pollClkHandle = Timer_construct(&pollClkStruct,
                                     processPollTimeoutCallback,
                                     POLL_TIMEOUT_VALUE,
                                     0,
                                     false,
                                     0);
}

/*!
 Set the poll clock.

 Public function defined in ssf.h
 */
void Ssf_setPollClock(uint32_t pollTime)
{
    /* Stop the Reading timer */
    if(Timer_isActive(&pollClkStruct) == true)
    {
        Timer_stop(&pollClkStruct);
    }
#ifdef POWER_MEAS
    if ((POWER_TEST_PROFILE == DATA_ACK) || (POWER_TEST_PROFILE == SLEEP))
    {
        return;
    }
#endif
#ifdef FEATURE_SECURE_COMMISSIONING
    /* Setup timer */
    if(pollTime > 0)
    {
        if(SM_Current_State == SM_CM_InProgress) {
            Timer_setTimeout(pollClkHandle, SM_POLLING_INTERVAL);
        }
        else {
            Timer_setTimeout(pollClkHandle, pollTime);
        }
        Timer_start(&pollClkStruct);
    }
#else
    /* Setup timer */
    if(pollTime > 0)
    {
        Timer_setTimeout(pollClkHandle, pollTime);
        Timer_start(&pollClkStruct);
    }
#endif /*FEATURE_SECURE_COMMISSIONING*/
}

/*!
 Get the poll clock.

 Public function defined in ssf.h
 */
uint32_t Ssf_getPollClock(void)
{
    return Timer_getTimeout(pollClkHandle);
}

/*!
 Initialize the scan backoff clock.

 Public function defined in ssf.h
 */
void Ssf_initializeScanBackoffClock(void)
{
    /* Initialize the timers needed for this application */
    scanBackoffClkHandle = Timer_construct(&scanBackoffClkStruct,
                                           processScanBackoffTimeoutCallback,
                                           SCAN_BACKOFF_TIMEOUT_VALUE,
                                           0,
                                           false,
                                           0);
}

/*!
 Set the scan backoff clock.

 Public function defined in ssf.h
 */
void Ssf_setScanBackoffClock(uint32_t scanBackoffTime)
{
    /* Stop the Reading timer */
    if(Timer_isActive(&scanBackoffClkStruct) == true)
    {
        Timer_stop(&scanBackoffClkStruct);
    }

    /* Setup timer */
    if(scanBackoffTime > 0)
    {
        Timer_setTimeout(scanBackoffClkHandle, scanBackoffTime);
        Timer_start(&scanBackoffClkStruct);
    }
}

/*!
 Stop the scan backoff clock.

 Public function defined in ssf.h
 */
void Ssf_stopScanBackoffClock(void)
{
    /* Stop the Reading timer */
    if(Timer_isActive(&scanBackoffClkStruct) == true)
    {
        Timer_stop(&scanBackoffClkStruct);
    }
}

/*!
 Initialize the FH Association delay clock.

 Public function defined in ssf.h
 */
void Ssf_initializeFHAssocClock(void)
{
    /* Initialize the timers needed for this application */
    fhAssocClkHandle = Timer_construct(&fhAssocClkStruct,
                                       processFHAssocTimeoutCallback,
                                       FH_ASSOC_TIMER,
                                        0,
                                        false,
                                        0);
}

/*!
 Set the FH Association delay clock.

 Public function defined in ssf.h
 */
void Ssf_setFHAssocClock(uint32_t fhAssocTime)
{
    /* Stop the Reading timer */
    if(Timer_isActive(&fhAssocClkStruct) == true)
    {
        Timer_stop(&fhAssocClkStruct);
    }

    /* Setup timer */
    if ( fhAssocTime )
    {
        if(!CERTIFICATION_TEST_MODE)
        {
            /* Adding an additional random delay */
            fhAssocTime = fhAssocTime + (((ApiMac_randomByte() << 8) +
                          ApiMac_randomByte()) % ADD_ASSOCIATION_RANDOM_WINDOW);
        }
        Timer_setTimeout(fhAssocClkHandle, fhAssocTime);
        Timer_start(&fhAssocClkStruct);
    }
}

#ifdef USE_DMM

/*!
 Initialize the provisioning timeout clock.

 Public function defined in ssf.h
 */
void Ssf_initializeProvisioningClock(void)
{
    /* Initialize the timers needed for this application */
    provisioningClkHandle = Timer_construct(&provisioningClkStruct,
                                       processProvisioningCallback,
                                       FH_ASSOC_TIMER,
                                        0,
                                        false,
                                        0);
}

/*!
 Set the provisioning timeout clock.

 Public function defined in ssf.h
 */
void Ssf_setProvisioningClock(bool provision)
{
    /* Stop the Provisioning timer */
    if(Timer_isActive(&provisioningClkStruct) == true)
    {
        Timer_stop(&provisioningClkStruct);
    }

    /* Setup timer for provisioning association timeout */
    if (provision)
    {
        Timer_setTimeout(provisioningClkHandle, PROVISIONING_ASSOC_TIMER);
    }
    /* Setup timer for disassociation delay */
    else
    {
        Timer_setTimeout(provisioningClkHandle, PROVISIONING_DISASSOC_TIMER);
    }

    Timer_setFunc(provisioningClkHandle, processProvisioningCallback, provision);
    Timer_start(&provisioningClkStruct);
}


static void processProvisioningCallback(UArg provision)
{
    static bool updateProvPolicy = true;

    //Arg used to select provision and associate to a network or disassociate
    if (provision)
    {
        if (updateProvPolicy)
        {
            Ssf_setProvisioningClock(true);

            /* Update policy */
            Util_setEvent(&Sensor_events, SENSOR_PROV_EVT);

            updateProvPolicy = false;
        }
        else
        {
            CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Starting");
            Util_setEvent(&Sensor_events, SENSOR_START_EVT);
            updateProvPolicy = true;
        }
    }
    else
    {
        CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Leaving");
        Util_setEvent(&Sensor_events, SENSOR_DISASSOC_EVT);
    }

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sensorSem);
}
#endif

/*!
 Update the Frame Counter

 Public function defined in ssf.h
 */
void Ssf_updateFrameCounter(ApiMac_sAddr_t *pDevAddr, uint32_t frameCntr)
{
    if(pDevAddr == NULL)
    {
        if((pNV != NULL) && (pNV->writeItem != NULL) && (frameCntr >=
              (lastSavedFrameCounter + FRAME_COUNTER_SAVE_WINDOW)))
        {
            NVINTF_itemID_t id;

            /* Setup NV ID */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = SSF_NV_FRAMECOUNTER_ID;
            id.subID = 0;

            /* Write the NV item */
            if(pNV->writeItem(id, sizeof(uint32_t), &frameCntr)
                            == NVINTF_SUCCESS)
            {
                lastSavedFrameCounter = frameCntr;
            }
        }
    }
}

/*!
 Get the Frame Counter

 Public function defined in ssf.h
 */
bool Ssf_getFrameCounter(ApiMac_sAddr_t *pDevAddr, uint32_t *pFrameCntr)
{
    /* Check for valid pointer */
    if(pFrameCntr != NULL)
    {
        /*
         A pDevAddr that is null means to get the frame counter for this device
         */
        if(pDevAddr == NULL)
        {
            if((pNV != NULL) && (pNV->readItem != NULL))
            {
                NVINTF_itemID_t id;

                /* Setup NV ID */
                id.systemID = NVINTF_SYSID_APP;
                id.itemID = SSF_NV_FRAMECOUNTER_ID;
                id.subID = 0;

                /* Read Network Information from NV */
                if(pNV->readItem(id, 0, sizeof(uint32_t), pFrameCntr)
                                == NVINTF_SUCCESS)
                {
                    /* Set to the next window */
                    *pFrameCntr += FRAME_COUNTER_SAVE_WINDOW;
                    return(true);
                }
                else
                {
                    /*
                     Wasn't found, so write 0, so the next time it will be
                     greater than 0
                     */
                    uint32_t fc = 0;

                    /* Setup NV ID */
                    id.systemID = NVINTF_SYSID_APP;
                    id.itemID = SSF_NV_FRAMECOUNTER_ID;
                    id.subID = 0;

                    /* Write the NV item */
                    pNV->writeItem(id, sizeof(uint32_t), &fc);
                }
            }
        }

        *pFrameCntr = 0;
    }
    return (false);
}

/*!
 Display Error

 Public function defined in ssf.h
 */
void Ssf_displayError(uint8_t *pTxt, uint8_t code)
{
#ifndef POWER_MEAS
    CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "%s0x%02x", pTxt, code);
#endif
}

/*!
 Assert Indication

 Public function defined in ssf.h
 */
void Ssf_assertInd(uint8_t reason)
{
    if((pNV != NULL) && (pNV->writeItem != NULL))
    {
        /* Attempt to save reason to read after reset */
        (void)pNV->writeItem(nvResetId, 1, &reason);
    }
}

/*!

 Clear network information in NV

 Public function defined in ssf.h
 */
void Ssf_clearNetworkInfo()
{
    if((pNV != NULL) && (pNV->deleteItem != NULL))
    {
        NVINTF_itemID_t id;

        /* Setup NV ID */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_NETWORK_INFO_ID;
        id.subID = 0;
        pNV->deleteItem(id);

        /* sensor ready to associate again */
        started = false;
    }
}

/*!
 Clear all the NV Items

 Public function defined in ssf.h
 */
void Ssf_clearAllNVItems(void)

{
#ifdef ONE_PAGE_NV
    if((pNV != NULL) && (pNV->deleteItem != NULL))
    {
        NVINTF_itemID_t id;

        /* Clear all information */
        id.systemID = 0xFF;
        id.itemID = 0xFFFF;
        id.subID = 0xFFFF;
        pNV->deleteItem(id);
    }
#else
    /* Clear Network Information */
    Ssf_clearNetworkInfo();

#ifdef FEATURE_NATIVE_OAD
    /* Clear OAD Information */
    Ssf_clearOadInfo();
#endif

    if((pNV != NULL) && (pNV->deleteItem != NULL))
    {
        NVINTF_itemID_t id;

        /* Clear the device tx frame counter */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_FRAMECOUNTER_ID;
        id.subID = 0;
        pNV->deleteItem(id);

        /* Clear the reset reason */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_RESET_REASON_ID;
        id.subID = 0;
        pNV->deleteItem(id);

        /* Clear the reset count */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_RESET_COUNT_ID;
        id.subID = 0;
        pNV->deleteItem(id);

#ifdef FEATURE_SECURE_COMMISSIONING
        /* Clear the key info*/
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = SSF_NV_DEVICE_KEY_ID;
        id.subID = 0;
        pNV->deleteItem(id);
#endif

    }
#endif
}

/*!
 Read the on-board temperature sensors

 Public function defined in ssf.h
 */
int16_t Ssf_readTempSensor(void)
{
#ifdef POWER_MEAS
    return (0);
#else
    return ((int16_t)AONBatMonTemperatureGetDegC());
#endif /* POWER_MEAS */
}

/*!
 The application calls this function to toggle an LED.

 Public function defined in ssf.h
 */
bool Ssf_toggleLED(void)
{
    if(led1State == true)
    {
        led1State = false;
#if !defined(POWER_MEAS)
        CUI_ledOff(ssfCuiHndl, CONFIG_LED_RED);
#endif
    }
    else
    {
        led1State = true;
#if !defined(POWER_MEAS)
        CUI_ledOn(ssfCuiHndl, CONFIG_LED_RED, NULL);
#endif
    }

    return(led1State);
}

/*!
 The application calls this function to switch on LED.

 Public function defined in ssf.h
 */
void Ssf_OnLED(void)
{
    if(led1State == false)
    {
        led1State = true;
#ifndef POWER_MEAS
        CUI_ledOn(ssfCuiHndl, CONFIG_LED_RED, NULL);
#endif
    }
}

/*!
 The application calls this function to switch off LED.

 Public function defined in ssf.h
 */
void Ssf_OffLED(void)
{
    if(led1State == true)
    {
        led1State = false;
#ifndef POWER_MEAS
        CUI_ledOff(ssfCuiHndl, CONFIG_LED_RED);
#endif
    }
}

/*!
  A callback calls this function to post the application task semaphore.

 Public function defined in ssf.h
 */
void Ssf_PostAppSem(void)
{
    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sensorSem);
}

#ifdef FEATURE_SECURE_COMMISSIONING
/*!
 The application calls this function to get a passkey.

 Public function defined in ssf.h
 */
void Ssf_SmPasskeyEntry(SM_passkeyEntry_t passkeyAction)
{
    static uint8_t smMenuUsed = 0;
    // if passkey is selected
    if(passkeyAction == SM_passkeyEntryReq)
    {
        // deregister main menu when you switch to SM menu
        CUI_deRegisterMenu(ssfCuiHndl, &ssfMainMenu);
        smMenuUsed = 1;

        // request a menu if available
        CUI_registerMenu(ssfCuiHndl, &smPassKeyMenu);

        // Open the menu itself
        // there is only 1 item in smPassKeyMenu list.
        CUI_menuNav(ssfCuiHndl, &smPassKeyMenu, 0);
    }
    else
    {
        CUI_deRegisterMenu(ssfCuiHndl, &smPassKeyMenu);

        // Only re-enable the main menu if it was previously disabled
        if(smMenuUsed == 1)
        {
            // re-register the main menu again
            CUI_registerMenu(ssfCuiHndl, &ssfMainMenu);
        }

        // Go back to the help screen which is the last menu in the list.
        // third argument represents the index of the menu to travel to.
        CUI_menuNav(ssfCuiHndl, &ssfMainMenu, ssfMainMenu.numItems - 1);
    }
}
#endif

/******************************************************************************
 Local Functions
 *****************************************************************************/

/*!
 * @brief   Reading timeout handler function.
 *
 * @param   a0 - ignored
 */
static void processReadingTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Sensor_events, SENSOR_READING_TIMEOUT_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sensorSem);
}

/*!
 * @brief       Key event handler function
 *
 * @param       keysPressed - keys that are pressed
 */
static void processKeyChangeCallback(uint32_t _btn, Button_EventMask _events)
{
    if (_events & Button_EV_CLICKED)
    {
        keys = _btn;
        events |= KEY_EVENT;

        /* Wake up the application thread when it waits for keys event */
        Semaphore_post(sensorSem);
    }
}

/*!
 * @brief       Trickle timeout handler function for PA .
 *
 * @param       a0 - ignored
 */
static void processPASTrickleTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Jdllc_events, JDLLC_PAS_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sensorSem);
}

/*!
 * @brief       Trickle timeout handler function for PC.
 *
 * @param       a0 - ignored
 */
static void processPCSTrickleTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Jdllc_events, JDLLC_PCS_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sensorSem);
}

/*!
 * @brief       Poll timeout handler function  .
 *
 * @param       a0 - ignored
 */
static void processPollTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Jdllc_events, JDLLC_POLL_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sensorSem);
}

/*!
 * @brief       Scan backoff timeout handler function  .
 *
 * @param       a0 - ignored
 */
static void processScanBackoffTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Jdllc_events, JDLLC_SCAN_BACKOFF);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sensorSem);
}

/*!
 * @brief       FH Assoc Delay timeout handler function  .
 *
 * @param       a0 - ignored
 */
static void processFHAssocTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Jdllc_events, JDLLC_ASSOCIATE_REQ_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sensorSem);
}

#if defined(DISPLAY_PER_STATS)
/*!
 * @brief       The application calls this function to print updated sensor stats to the display.
 */
void Ssf_displayPerStats(Smsgs_msgStatsField_t* pstats)
{
    int per;
    int failures = pstats->macAckFailures + pstats->otherDataRequestFailures;
    per = (100000 * failures) / (pstats->msgsSent + failures);
#ifndef POWER_MEAS
    CUI_statusLinePrintf(ssfCuiHndl, perStatusLine, "%d.%03d%%", (per / 1000), (per % 1000));
#endif
}
#endif /* DISPLAY_PER_STATS */

#ifndef POWER_MEAS
/**
 *  @brief Callback to be called when the UI sets PAN ID.
 */

static uint8_t moveCursorLeft(uint8_t col, uint8_t left_boundary, uint8_t right_boundary, uint8_t skip_space)
{
    // If you haven't hit the end of left boundary, keep moving cursor left.
    if (left_boundary != col)
    {
        col--;

    }
    else
    {
        col = right_boundary;
    }

    if(0 != skip_space)
    {
        //skip the white space, by continuing to move left over it
        if((col % 3) == 0)
            col--;
    }

    return col;
}

static uint8_t moveCursorRight(uint8_t col, uint8_t left_boundary, uint8_t right_boundary, uint8_t skip_space)
{
    // If you haven't hit the end of modifiable lines, keep moving cursor right.
    if (right_boundary != col)
    {
        col++;
    }
    else
    {
        col = left_boundary;
    }

    // if skip_space is true
    if(0 != skip_space)
    {
        //skip the white space, by continuing to move right over it
        if((col % 3) == 0)
            col++;
    }
    return col;
}


static void setPanIdAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo)
{
  static CUI_cursorInfo_t cursor = {0, 6};
  static bool initPanId = true;
  static uint16_t panId;

  const char tmpInput[2] = {_input, '\0'};

  if(initPanId)
  {
      Jdllc_getJoiningPanId(&panId);
      initPanId = false;
  }

  switch (_input) {
      case CUI_ITEM_INTERCEPT_START:
      {
          Jdllc_getJoiningPanId(&panId);
          break;
      }
      // Submit the final modified value
      case CUI_ITEM_INTERCEPT_STOP:
      {
          Jdllc_setJoiningPanId(panId);

          // Reset the local cursor info
          cursor.col = 6;
          break;
      }
      // Show the value of this screen w/o making changes
      case CUI_ITEM_PREVIEW:
          Jdllc_getJoiningPanId(&panId);
          break;
      // Move the cursor to the left
      case CUI_INPUT_LEFT:
      {
          // 6 is the left boundary of the cursor, and we don't need to skip spaces.
          cursor.col = moveCursorLeft(cursor.col, 6, 9, 0);
          break;
      }
      // Move the cursor to the right
      case CUI_INPUT_RIGHT:
      {
          cursor.col = moveCursorRight(cursor.col, 6, 9, 0);
          break;
      }
      case CUI_INPUT_UP:
          break;

      case CUI_INPUT_DOWN:
          break;

      case CUI_INPUT_BACK:
      {
          // multiply by 4 to use binary
          // get the position of the nibble you want to change.
          uint8_t shift = (uint8_t)(4 * (9 - cursor.col));

          // make sure you don't exceed the left boundary
          if (6 <= cursor.col)
          {
              // clear the nibble to default to F
              panId |= (uint32_t)(0x0F<<shift) ;
              cursor.col = moveCursorLeft(cursor.col, 6, 9, 0);

          }
          break;
      }
      case CUI_INPUT_EXECUTE:
          break;
      default:
      {
          // is it a hex number
          if(CUI_IS_INPUT_HEX(_input))
          {
              /* multiply by 4 because you're working with binary numbers,
              and half a byte of hex = 4 bits. Calculate the bit shift
              based on the end of the line - the current column, to
              modify specific values.  */
              uint8_t shift = 4 * (9 - cursor.col);

              // convert from ascii to hex
              uint8_t digit = strtol(tmpInput, NULL, 16);

              // first clear the specific hex half byte, in the desired spot
              panId &= ~((uint32_t)0xF << shift);
              // then set the digit you have typed in.
              panId |= (uint32_t)digit << shift;

              cursor.col = moveCursorRight(cursor.col, 6, 9, 0);


          }
      }
  }

  if (panId == 0xFFFF)
  {
    strcpy(_pLines[0], "    0xFFFF (any)");
  }
  else
  {
    char tmp[4];
    uintToString( panId, tmp, 16, 4, TRUE, FALSE);
    strcpy(_pLines[0], "    0x");
    strncat(_pLines[0], tmp, 4);
  }

  if (_input != CUI_ITEM_PREVIEW) {
      strcpy(_pLines[2], "     PAN ID");
      _pCurInfo->row = 1;
      _pCurInfo->col = cursor.col+1;
  }
}

/**
 *  @brief Callback to be called when the UI sets Channel Mask.
 */
static void setChMaskAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo)
{
    static uint8_t channelMask[APIMAC_154G_CHANNEL_BITMAP_SIZ];
    static bool initChanMask = true;
    static CUI_cursorInfo_t cursor = {0, 1};
    uint8_t chanMaskByteIdx = 0;
    uint8_t chanMaskStrIdx = 0;

    const char tmpInput[2] = {_input, '\0'};

    if(initChanMask)
    {
        Jdllc_getChanMask(channelMask);
        initChanMask = false;
    }

    switch (_input) {
        case CUI_ITEM_INTERCEPT_START:
        {
            // Reset the local cursor info
            cursor.col = 1;
            Jdllc_getChanMask(channelMask);
            break;
        }
        // Submit the final modified value
        case CUI_ITEM_INTERCEPT_STOP:
        {
            Jdllc_setChanMask(channelMask);

            // Reset the local cursor info
            cursor.col = 1;
            break;
        }
        // Show the value of this screen w/o making changes
        case CUI_ITEM_PREVIEW:
            Jdllc_getChanMask(channelMask);
            break;
        // Move the cursor to the left
        case CUI_INPUT_LEFT:
        {
            cursor.col = moveCursorLeft(cursor.col, 1, 50, 1);
            break;
        }
        // Move the cursor to the right
        case CUI_INPUT_RIGHT:
        {
            cursor.col = moveCursorRight(cursor.col, 1, 50, 1);
            break;
        }
        case CUI_INPUT_UP:
            break;
        case CUI_INPUT_DOWN:
            break;
        case CUI_INPUT_BACK:
        {
            // get the exact nibble you want to modify.
            uint8_t nibbleIdx = (cursor.col-1) - ((cursor.col -1)/3);
            uint8_t byteIdx = nibbleIdx / 2;

            // don't exceed left boundary
            if (1 <= cursor.col)
            {
                if(cursor.col % 3 == 2)
                {
                    channelMask[byteIdx] |= (uint32_t)(0x0F) ;

                    /* You are at the right side of the byte */
                }
                else if(cursor.col % 3 == 1)
                {
                    /* You are at the left side of the byte */
                    channelMask[byteIdx] |= (uint32_t)(0x0F<<4) ;
                }

                cursor.col = moveCursorLeft(cursor.col, 1, 50, 1);

            }
        }
        case CUI_INPUT_EXECUTE:
            break;

        default:
        {
            // Make sure channel mask input is valid hex
            if(CUI_IS_INPUT_HEX(_input))
            {
                // get the position of the entire hex byte based on the column you're at
                uint8_t nibbleIdx = (cursor.col-1) - ((cursor.col -1)/3);
                // get the position of the nibble that you'd like to set the value of
                uint8_t byteIdx = nibbleIdx / 2;

                // you're at the left side
                if(cursor.col % 3 == 1)
                {
                    // First, clear the left side, keep the right side
                    channelMask[byteIdx] &= (uint32_t)(0x0F);
                    // Next, shift the input left, and or it with the existing side.
                    channelMask[byteIdx] |= (uint32_t)(strtol(tmpInput, NULL, 16) << 4);
                }
                // you're at the right side
                else if(cursor.col % 3 == 2)
                {
                    // First, clear the right side, keep the left side
                    channelMask[byteIdx] &= (uint32_t)(0xF0);
                    // Next, use the input to or it with the existing side.
                    channelMask[byteIdx] |= (uint32_t)(strtol(tmpInput, NULL, 16));
                }
                cursor.col = moveCursorRight(cursor.col, 1, 50, 1);
            }
        }
    }

    for(chanMaskByteIdx = 0; chanMaskByteIdx < APIMAC_154G_CHANNEL_BITMAP_SIZ; chanMaskByteIdx++)
    {
        char tmp[2];

        uintToString( channelMask[chanMaskByteIdx], tmp, 16, 2, TRUE, FALSE);

        // clear the label
        strcpy((_pLines[0] + chanMaskStrIdx), " ");
        chanMaskStrIdx += 1;
        strncat((_pLines[0] + chanMaskStrIdx), tmp, 2);
        chanMaskStrIdx += 2;
    }

    if (_input != CUI_ITEM_PREVIEW)
    {
        // set the label in the right place.
        strcpy(_pLines[2], "     CHAN MASK");

        // set the label at the right place.
        _pCurInfo->row = 1;
        _pCurInfo->col = cursor.col+1;
    }
}

#if CONFIG_FH_ENABLE
/**
 *  @brief Callback to be called when the UI sets Channel Mask.
 */
static void setAsyncChMaskAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo)
{
    static uint8_t channelMask[APIMAC_154G_CHANNEL_BITMAP_SIZ];
    static bool initChanMask = true;
    static CUI_cursorInfo_t cursor = {0, 1};
    uint8_t chanMaskByteIdx = 0;
    uint8_t chanMaskStrIdx = 0;

    const char tmpInput[2] = {_input, '\0'};

    if(initChanMask)
    {
        Jdllc_getAsyncChanMask(channelMask);
        initChanMask = false;
    }

    switch (_input) {
        case CUI_ITEM_INTERCEPT_START:
        {
            Jdllc_getAsyncChanMask(channelMask);
            break;
        }
        // Submit the final modified value
        case CUI_ITEM_INTERCEPT_STOP:
        {
            Jdllc_setAsyncChanMask(channelMask);

            // Reset the local cursor info
            cursor.col = 1;
            break;
        }
        // Show the value of this screen w/o making changes
        case CUI_ITEM_PREVIEW:
            Jdllc_getAsyncChanMask(channelMask);
            break;
        // Move the cursor to the left
        case CUI_INPUT_LEFT:
            cursor.col = moveCursorLeft(cursor.col, 1, 50, 1);
            break;

        // Move the cursor to the right
        case CUI_INPUT_RIGHT:
            cursor.col = moveCursorRight(cursor.col, 1, 50, 1);
            break;

        case CUI_INPUT_UP:
            break;

        case CUI_INPUT_DOWN:
            break;

        case CUI_INPUT_EXECUTE:
            break;

        case CUI_INPUT_BACK:
            {
                // get exact position of nibble you want to modify
                uint8_t nibbleIdx = (cursor.col-1) - ((cursor.col -1)/3);
                uint8_t byteIdx = nibbleIdx / 2;

                // don't exceed left boundary
                if (1 <= cursor.col)
                {
                    if(cursor.col % 3 == 2)
                    {
                        channelMask[byteIdx] |= (uint32_t)(0x0F) ;
                        /* You are at the right side of the byte */
                    }
                    else if(cursor.col % 3 == 1)
                    {
                        /* You are at the left side of the byte */
                        channelMask[byteIdx] |= (uint32_t)(0x0F<<4) ;
                    }

                    cursor.col = moveCursorLeft(cursor.col, 1, 50, 1);

                }
                break;
                }
        default:

            // Make sure channel mask input is valid hex
            if(CUI_IS_INPUT_HEX(_input))
            {
            // get exact position of nibble you want to modify
            uint8_t nibbleIdx = (cursor.col-1) - ((cursor.col -1)/3);
            uint8_t byteIdx = nibbleIdx / 2;

            // you're at the left side
            if(nibbleIdx % 2 == 0)
            {
                // First, clear the left side, keep the right side
                channelMask[byteIdx] &= (uint32_t)(0x0F);
                // Next, shift the input left, and or it with the existing side.
                channelMask[byteIdx] |= (uint32_t)(strtol(tmpInput, NULL, 16) << 4);
            }
            // you're at the left side
            else
            {
                // First, clear the right side, keep the left side
                channelMask[byteIdx] &= (uint32_t)(0xF0);
                // Next, use the input to or it with the existing side.
                // strtol will convert from hex ascii to hex integers
                channelMask[byteIdx] |= (uint32_t)(strtol(tmpInput, NULL, 16));
            }
            cursor.col = moveCursorRight(cursor.col, 1, 50, 1);
        }

    }

    for(chanMaskByteIdx = 0; chanMaskByteIdx < APIMAC_154G_CHANNEL_BITMAP_SIZ; chanMaskByteIdx++)
    {
        char tmp[2];

        uintToString( channelMask[chanMaskByteIdx], tmp, 16, 2, TRUE, FALSE);

        strcpy((_pLines[0] + chanMaskStrIdx), " ");
        chanMaskStrIdx += 1;
        strncat((_pLines[0] + chanMaskStrIdx), tmp, 2);
        chanMaskStrIdx += 2;
    }

    if (_input != CUI_ITEM_PREVIEW)
    {
        strcpy(_pLines[2], "     ASYNC CHAN MASK");
        _pCurInfo->row = 1;
        _pCurInfo->col = cursor.col+1;
    }
}
#endif

#ifdef FEATURE_MAC_SECURITY
/**
 *  @brief Callback to be called when the UI sets Default Network Key.
 */
static void setNwkKeyAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo)
{
    static uint8_t defaultNwkKey[APIMAC_KEY_MAX_LEN];
    static bool initChanMask = true;
    static CUI_cursorInfo_t cursor = {0, 1};
    uint8_t defaultNwkKeyByteIdx = 0;
    uint8_t defaultNwkKeyStrIdx = 0;

    const char tmpInput[2] = {_input, '\0'};

    if(initChanMask)
    {
        Jdllc_getDefaultKey(defaultNwkKey);
        initChanMask = false;
    }

    switch (_input) {
        case CUI_ITEM_INTERCEPT_START:
        {
            Jdllc_getDefaultKey(defaultNwkKey);
            break;
        }
        // Submit the final modified value
        case CUI_ITEM_INTERCEPT_STOP:
        {
            Jdllc_setDefaultKey(defaultNwkKey);

            // Reset the local cursor info
            cursor.col = 1;
            break;
        }
        // Show the value of this screen w/o making changes
        case CUI_ITEM_PREVIEW:
            Jdllc_getDefaultKey(defaultNwkKey);
            break;
        // Move the cursor to the left
        case CUI_INPUT_LEFT:
        {
            cursor.col = moveCursorLeft(cursor.col, 1, 47, 1);
            break;
        }
        // Move the cursor to the right
        case CUI_INPUT_RIGHT:
        {
            cursor.col = moveCursorRight(cursor.col, 1, 47, 1);
            break;
        }
        case CUI_INPUT_UP:
            break;
        case CUI_INPUT_DOWN:
            break;
        case CUI_INPUT_BACK:
        {
            // get exact position of nibble you want to modify
            uint8_t nibbleIdx = (cursor.col-1) - ((cursor.col -1)/3);
            uint8_t byteIdx = nibbleIdx / 2;

            // don't exceed left boundary
            if (1 <= cursor.col)
            {
                if(cursor.col % 3 == 2)
                {
                    defaultNwkKey[byteIdx] |= (uint32_t)(0x0F) ;
                    /* You are at the right side of the byte */
                }
                else if(cursor.col % 3 == 1)
                {
                    /* You are at the left side of the byte */
                    defaultNwkKey[byteIdx] |= (uint32_t)(0x0F<<4) ;
                }

                cursor.col = moveCursorLeft(cursor.col, 1, 47, 1);

            }
            break;
        }
        case CUI_INPUT_EXECUTE:
            break;
        case CUI_INPUT_ESC:
            break;
        default:
         {
             // Make sure channel mask input is valid hex
             if(CUI_IS_INPUT_HEX(_input))
                 {
                     // get exact position of nibble you want to modify
                     uint8_t nibbleIdx = (cursor.col-1) - ((cursor.col -1)/3);
                     uint8_t byteIdx = nibbleIdx / 2;

                     // you're at the left side
                     if(nibbleIdx % 2 == 0)
                         {
                             // First, clear the left side, keep the right side
                             defaultNwkKey[byteIdx] &= (uint32_t)(0x0F);
                             // Next, shift the input left, and or it with the existing side.
                             defaultNwkKey[byteIdx] |= (uint32_t)(strtol(tmpInput, NULL, 16) << 4);
                         }
                     // you're at the left side
                     else
                         {
                             // First, clear the right side, keep the left side
                             defaultNwkKey[byteIdx] &= (uint32_t)(0xF0);
                             // Next, use the input to or it with the existing side.
                             // strtol will convert from hex ascii to hex integers
                             defaultNwkKey[byteIdx] |= (uint32_t)(strtol(tmpInput, NULL, 16));
                         }

                     cursor.col = moveCursorRight(cursor.col, 1, 47, 1);
                 }

     }
    }

    for(defaultNwkKeyByteIdx = 0; defaultNwkKeyByteIdx < APIMAC_KEY_MAX_LEN; defaultNwkKeyByteIdx++)
    {
        char tmp[2];

        uintToString( defaultNwkKey[defaultNwkKeyByteIdx], tmp, 16, 2, TRUE, FALSE);

        // clear the label
        strcpy((_pLines[0] + defaultNwkKeyStrIdx), " ");
        defaultNwkKeyStrIdx += 1;
        strncat((_pLines[0] + defaultNwkKeyStrIdx), tmp, 2);
        defaultNwkKeyStrIdx += 2;
    }

    // set the label if you're not doing a preview.
    if (_input != CUI_ITEM_PREVIEW)
    {
        strcpy(_pLines[2], "     DEFAULT NWK KEY");
        _pCurInfo->row = 1;
        _pCurInfo->col = cursor.col+1;
    }
}
#endif /* FEATURE_MAC_SECURITY */

#ifdef FEATURE_SECURE_COMMISSIONING
static void setSmSetAuthModeAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo)
{
    static CUI_cursorInfo_t cursor = {0, 0};
    static SMMsgs_authMethod_t smAuthMethod;

    switch (_input) {
        case CUI_ITEM_INTERCEPT_START:
        {
            smAuthMethod = Sensor_getSmAuthMethod();
            break;
        }
        // Submit the final modified value
        case CUI_ITEM_INTERCEPT_STOP:
        {
            //Set Auth Method
            Sensor_setSmAuthMethod(smAuthMethod);
            break;
        }
        // Show the value of this screen w/o making changes
        case CUI_ITEM_PREVIEW:
            smAuthMethod = Sensor_getSmAuthMethod();
            break;
        // Decrease authMode
        case CUI_INPUT_LEFT:
            if(smAuthMethod == SMMsgs_authMethod_passkey)
            {
                smAuthMethod = SMMsgs_authMethod_justAllowed;
            }
            else if(smAuthMethod == SMMsgs_authMethod_justAllowed)
            {
                smAuthMethod = SMMsgs_authMethod_defaultCode;
            }
            else
            {
                smAuthMethod = SMMsgs_authMethod_passkey;
            }
            break;

        // Increase authMode
        case CUI_INPUT_RIGHT:
            if(smAuthMethod == SMMsgs_authMethod_passkey)
            {
                smAuthMethod = SMMsgs_authMethod_defaultCode;
            }
            else if(smAuthMethod == SMMsgs_authMethod_defaultCode)
            {
                smAuthMethod = SMMsgs_authMethod_justAllowed;
            }
            else
            {
                smAuthMethod = SMMsgs_authMethod_passkey;
            }
            break;
        case CUI_INPUT_DOWN:
            break;
        case CUI_INPUT_UP:
            break;
        case CUI_INPUT_EXECUTE:
            break;
        default:
            break;
    }

    if (_input != CUI_ITEM_PREVIEW) {
        strcpy(_pLines[2], "   AUTH METHOD    ");

        _pCurInfo->row = 1;
        _pCurInfo->col = cursor.col+1;

        if(smAuthMethod == SMMsgs_authMethod_passkey)
        {
            strcpy(_pLines[0], "<   PASSKEY    >");
        }
        else if(smAuthMethod == SMMsgs_authMethod_defaultCode)
        {
            strcpy(_pLines[0], "< DEFAULT CODE >");
        }
        else
        {
            strcpy(_pLines[0], "<  JUST ALLOW  >");
        }
    }
    else
    {
        if(smAuthMethod == SMMsgs_authMethod_passkey)
        {
            strcpy(_pLines[0], "    PASSKEY     ");
        }
        else if(smAuthMethod == SMMsgs_authMethod_defaultCode)
        {
            strcpy(_pLines[0], "  DEFAULT CODE  ");
        }
        else
        {
            strcpy(_pLines[0], "   JUST ALLOW  ");
        }
    }

}

static void setSmPassKeyAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo)
{
    // Update the title in case of entering the multi menu
    static const char passkeyTitle[] = "TI Passkey Menu";
    CUI_updateMultiMenuTitle(passkeyTitle);

    static CUI_cursorInfo_t cursor = {0, 4};
    static char passkeyASCII[SM_PASSKEY_LEN + 1] = {0};

    switch (_input) {
        case CUI_ITEM_INTERCEPT_START:
        {
            strcpy(passkeyASCII, "000000");
            cursor.col = 4;
            break;
        }
        // Submit the final modified value
        case CUI_ITEM_INTERCEPT_STOP:
        {
            uint32_t passkeyValue = atoi(passkeyASCII);
            //Set passkey in SM
            SM_setPasskey(passkeyValue);

            break;
        }
        // Show the value of this screen w/o making changes
        case CUI_ITEM_PREVIEW:
            break;
        // Move the cursor to the left
        case CUI_INPUT_LEFT:
            cursor.col = moveCursorLeft(cursor.col, 4, 9, 0);
            break;

        // Move the cursor to the right
        case CUI_INPUT_RIGHT:
            cursor.col = moveCursorRight(cursor.col, 4, 9, 0);
            break;

        case CUI_INPUT_UP:
        {
            break;
        }

        case CUI_INPUT_DOWN:
        {
            break;
        }
        case CUI_INPUT_BACK:
            passkeyASCII[cursor.col-4] = 'F';
            cursor.col = moveCursorLeft(cursor.col, 4, 9, 0);
            break;
        case CUI_INPUT_EXECUTE:
            break;
        default:
        {
            //is it a number
            if(CUI_IS_INPUT_HEX(_input) )
            {
                passkeyASCII[cursor.col-4] = _input;

                cursor.col = moveCursorRight(cursor.col, 4, 9, 0);
            }
        }
    }

    strcpy(_pLines[0], "    ");
    strncat(_pLines[0], passkeyASCII, 8);

    if (_input != CUI_ITEM_PREVIEW) {
        strcpy(_pLines[2], "   ENTER PASSKEY  ");
        _pCurInfo->row = 1;
        _pCurInfo->col = cursor.col+1;
    }
}
#endif

/**
 *  @brief Callback to be called when the UI associates.
 */
static void assocAction(int32_t menuEntryInex)
{
    CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Starting");

    /* Tell the sensor to start */
    Util_setEvent(&Sensor_events, SENSOR_START_EVT);
    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sensorSem);
}

/**
 *  @brief Callback to be called when the UI disassociates.
 */
static void disassocAction(int32_t menuEntryInex)
{

    if ((Jdllc_getProvState() == Jdllc_states_joined) ||
    (Jdllc_getProvState() == Jdllc_states_rejoined))
    {
        // Only send the disassociation if you're in the network.
        CUI_statusLinePrintf(ssfCuiHndl, sensorStatusLine, "Leaving");
        Jdllc_sendDisassociationRequest();
    }
}

/**
 *  @brief Callback to be called when the UI sets PAN ID.
 */
static void collectorLedIndentifyAction(int32_t menuEntryInex)
{
    events |= SENSOR_SEND_COLLECTOR_IDENT_EVT;

    // Wake up the application thread when it waits for clock event
    Semaphore_post(sensorSem);
}

/**
 *  @brief Send process menu event.
 */
static void processMenuUpdate(void)
{
    events |= SENSOR_UI_INPUT_EVT;

    // Wake up the application thread when it waits for clock event
    Semaphore_post(sensorSem);
}

/**
 *  @brief called when the UI converts Uint to string.
 */
static void uintToString (uint32_t value, char * str, uint8_t base, uint8_t num_of_digists, bool pad0, bool reverse)
{
  int i;
  uint8_t index;

  for (i = 0; i < num_of_digists; i++)
  {
    index = (reverse ? i : num_of_digists - 1 - i);
    str[index] = '0' + (value % base);
    if (str[index] > '9')
    {
      str[index] += 'A' - '0' - 10;
    }
    value /= base;
    if ((!pad0) && (value == 0))
    {
      break;
    }
  }
}
#endif
