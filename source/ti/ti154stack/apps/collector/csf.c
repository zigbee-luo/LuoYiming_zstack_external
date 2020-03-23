
/******************************************************************************

 @file csf.c

 @brief Collector Specific Functions

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
#include <ti/drivers/PIN.h>
#include <inc/hw_ints.h>
#include <aon_event.h>
#include <ioc.h>

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
#include "api_mac.h"
#include "collector.h"
#include "cllc.h"
#include "csf.h"
#include "ti_154stack_config.h"

#if defined(MT_CSF)
#include "mt_csf.h"
#endif

#ifdef OSAL_PORT2TIRTOS
#include "osal_port.h"
#else
#include "icall.h"
#endif

#if defined(DEVICE_TYPE_MSG)
#include <ti/devices/DeviceFamily.h>
#include <ti/boards/device_type.h>
#endif /* DEVICE_TYPE_MSG */

/******************************************************************************
 Constants and definitions
 *****************************************************************************/

/* Initial timeout value for the tracking clock */
#define TRACKING_INIT_TIMEOUT_VALUE 100

/* NV Item ID - the device's network information */
#define CSF_NV_NETWORK_INFO_ID 0x0001
/* NV Item ID - the number of device list entries */
#define CSF_NV_DEVICELIST_ENTRIES_ID 0x0004
/* NV Item ID - the device list, use sub ID for each record in the list */
#define CSF_NV_DEVICELIST_ID 0x0005
/* NV Item ID - this devices frame counter */
#define CSF_NV_FRAMECOUNTER_ID 0x0006
/* NV Item ID - reset reason */
#define CSF_NV_RESET_REASON_ID 0x0007

/* Maximum number of device list entries */
#define CSF_MAX_DEVICELIST_ENTRIES CONFIG_MAX_DEVICES

/*
 Maximum sub ID for a device list item, this is failsafe.  This is
 not the maximum number of items in the list
 */
#define CSF_MAX_DEVICELIST_IDS (2*CONFIG_MAX_DEVICES)

/* timeout value for trickle timer initialization */
#define TRICKLE_TIMEOUT_VALUE       20

/* timeout value for join timer */
#define JOIN_TIMEOUT_VALUE       20
/* timeout value for config request delay */
#define CONFIG_TIMEOUT_VALUE 1000

#if defined(USE_DMM)
#define FH_ASSOC_TIMER              2000
#define PROVISIONING_ASSOC_TIMER    1000
#endif

/*
 The increment value needed to save a frame counter. Example, setting this
 constant to 100, means that the frame counter will be saved when the new
 frame counter is 100 more than the last saved frame counter.  Also, when
 the get frame counter function reads the value from NV it will add this value
 to the read value.
 */
#define FRAME_COUNTER_SAVE_WINDOW     25

/* Value returned from findDeviceListIndex() when not found */
#define DEVICE_INDEX_NOT_FOUND  -1

/*! NV driver item ID for reset reason */
#define NVID_RESET {NVINTF_SYSID_APP, CSF_NV_RESET_REASON_ID, 0}

/******************************************************************************
 External variables
 *****************************************************************************/

#ifdef NV_RESTORE
/*! MAC Configuration Parameters */
extern mac_Config_t Main_user1Cfg;
#endif

#ifdef FEATURE_SECURE_COMMISSIONING
/* Security manager latest state */
extern SM_lastState_t SM_Current_State;

/* Need to re-do commissioning*/
extern bool fCommissionRequired;
#endif
/******************************************************************************
 Local variables
 *****************************************************************************/

/* The application's semaphore */
#ifdef OSAL_PORT2TIRTOS
static Semaphore_Handle collectorSem;
#else
static ICall_Semaphore collectorSem;
#endif

/* Clock/timer resources */
static Clock_Struct trackingClkStruct;
static Clock_Handle trackingClkHandle;
static Clock_Struct broadcastClkStruct;
static Clock_Handle broadcastClkHandle;

/* Clock/timer resources for CLLC */
/* trickle timer */
STATIC Clock_Struct tricklePAClkStruct;
STATIC Clock_Handle tricklePAClkHandle;
STATIC Clock_Struct tricklePCClkStruct;
STATIC Clock_Handle tricklePCClkHandle;

/* timer for join permit */
STATIC Clock_Struct joinClkStruct;
STATIC Clock_Handle joinClkHandle;

/* timer for config request delay */
STATIC Clock_Struct configClkStruct;
STATIC Clock_Handle configClkHandle;

/* timer for LED blink timeout*/
STATIC Clock_Struct identifyClkStruct;
STATIC Clock_Handle identifyClkHandle;

#ifdef USE_DMM
STATIC Clock_Struct provisioningClkStruct;
STATIC Clock_Handle provisioningClkHandle;
#endif /* USE_DMM */

/* NV Function Pointers */
static NVINTF_nvFuncts_t *pNV = NULL;

static bool started = false;

/* The last saved coordinator frame counter */
static uint32_t lastSavedCoordinatorFrameCounter = 0;

#if defined(MT_CSF)
/*! NV driver item ID for reset reason */
static const NVINTF_itemID_t nvResetId = NVID_RESET;
#endif

/******************************************************************************
 Global variables
 *****************************************************************************/
/* Key press parameters */
uint8_t Csf_keys = 0xFF;

/* pending Csf_events */
uint16_t Csf_events = 0;

/* Saved CLLC state */
Cllc_states_t savedCllcState = Cllc_states_initWaiting;

/* Permit join setting */
bool permitJoining = false;

CUI_clientHandle_t csfCuiHndl;
uint32_t collectorStatusLine;
uint32_t deviceStatusLine;
uint32_t numJoinDevStatusLine;

static uint16_t SelectedSensor;
static uint32_t reportInterval;
static uint8_t Csf_sensorAction;


/******************************************************************************
 Local function prototypes
 *****************************************************************************/

static void processTrackingTimeoutCallback(UArg a0);
static void processBroadcastTimeoutCallback(UArg a0);
static void processKeyChangeCallback(uint32_t _btn, Button_EventMask _events);
static void processPATrickleTimeoutCallback(UArg a0);
static void processPCTrickleTimeoutCallback(UArg a0);
static void processJoinTimeoutCallback(UArg a0);
static void processConfigTimeoutCallback(UArg a0);
static void processidentifyTimeoutCallback(UArg a0);
static uint16_t getNumActiveDevices(void);
#if defined(USE_DMM)
static void processProvisioningCallback(UArg a0);
#endif
static bool addDeviceListItem(Llc_deviceListItem_t *pItem, bool *pNewDevice);
static void updateDeviceListItem(Llc_deviceListItem_t *pItem);
static int findDeviceListIndex(ApiMac_sAddrExt_t *pAddr);
static int findUnusedDeviceListIndex(void);
static void saveNumDeviceListEntries(uint16_t numEntries);
#if defined(TEST_REMOVE_DEVICE)
static void removeTheFirstDevice(void);
#else
static uint16_t getTheFirstDevice(void);
#endif

#if !defined(POWER_MEAS)
static void updateCollectorStatusLine(bool restored, Llc_netInfo_t *pNetworkInfo);
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
static void setSmPassKeyAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo);
#endif
static void formNwkAction(int32_t menuEntryInex);
static void openNwkAction(int32_t menuEntryInex);
static void closeNwkAction(int32_t menuEntryInex);
//static void disassocAction(void);
static void sensorSelectAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo);
static void sensorSetReportInterval(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo);
static void sensorLedToggleAction(int32_t menuEntryInex);
static void sensorDisassocAction(int32_t menuEntryInex);
#if defined(DEVICE_TYPE_MSG)
static void sensorDeviceTypeRequestAction(int32_t menuEntryInex);
#endif /* DEVICE_TYPE_MSG */
static void processMenuUpdate(void);
static void sendToggleAndUpdateUser(uint16_t shortAddr);
static void uintToString (uint32_t value, char * str, uint8_t base, uint8_t num_of_digists, bool pad0, bool reverse);

#endif

/* POWER_MEAS protects the UART in these functions */
static void formNwkAndUpdateUser(void);
static void openCloseNwkAndUpdateUser(bool openNwkRequest);

/* Menu */
#if !defined(POWER_MEAS)

#define SFF_MENU_TITLE " TI Collector "

#if CONFIG_FH_ENABLE
#define FH_MENU_ENABLED 1
#else
#define FH_MENU_ENABLED 0
#endif

#ifdef FEATURE_MAC_SECURITY
#define SECURITY_MENU_ENABLED 1
#else
#define SECURITY_MENU_ENABLED 0
#endif

CUI_SUB_MENU(configureSubMenu, "<      CONFIGURE      >", 2 + FH_MENU_ENABLED + SECURITY_MENU_ENABLED, csfMainMenu)
    CUI_MENU_ITEM_INT_ACTION(  "<      SET PANID      >", setPanIdAction)
    CUI_MENU_ITEM_INT_ACTION(  "<    SET CHAN MASK    >", setChMaskAction)
#if CONFIG_FH_ENABLE
    CUI_MENU_ITEM_INT_ACTION(  "<  SET AS CHAN MASK   >", setAsyncChMaskAction)
#endif
#ifdef FEATURE_MAC_SECURITY
    CUI_MENU_ITEM_INT_ACTION(  "<     SET NWK KEY     >", setNwkKeyAction)
#endif
CUI_SUB_MENU_END

CUI_SUB_MENU(commissionSubMenu,"<   NETWORK ACTIONS   >", 3, csfMainMenu)
    CUI_MENU_ITEM_ACTION(      "<       FORM NWK      >", formNwkAction)
    CUI_MENU_ITEM_ACTION(      "<       OPEN NWK      >", openNwkAction)
    CUI_MENU_ITEM_ACTION(      "<       CLOSE NWK     >", closeNwkAction)
//    CUI_MENU_ITEM_ACTION(    "<    DISASSOCIATE     >", disassocAction)
CUI_SUB_MENU_END

/* This menu will be registered/de-registered at run time to create
a sort of "popup" menu for passkey entry. Since it is de-registered,
it is also completely hidden from the user at all other times.
Note: MAX_REGISTERED_MENUS must be >= 2 for both of the main
menus in this file. */

#ifdef FEATURE_SECURE_COMMISSIONING
CUI_MAIN_MENU(smPassKeyMenu, "<       SM MENU       >", 1, processMenuUpdate)
    CUI_MENU_ITEM_INT_ACTION(  "<    ENTER PASSKEY    >", setSmPassKeyAction)
CUI_MAIN_MENU_END
#endif


#if defined(DEVICE_TYPE_MSG)
CUI_SUB_MENU(appSubMenu,       "<         APP         >", 5, csfMainMenu)
#else
CUI_SUB_MENU(appSubMenu,       "<         APP         >", 4, csfMainMenu)
#endif /* DEVICE_TYPE_MSG */

    CUI_MENU_ITEM_INT_ACTION(  "<     SELECT SENSOR   >", sensorSelectAction)
    CUI_MENU_ITEM_INT_ACTION(  "< SET REPORT INTERVAL >", sensorSetReportInterval)
    CUI_MENU_ITEM_ACTION(      "<     SEND TOGGLE     >", sensorLedToggleAction)
    CUI_MENU_ITEM_ACTION(      "< SEND DISASSOCIATION >", sensorDisassocAction)
#if defined(DEVICE_TYPE_MSG)
    CUI_MENU_ITEM_ACTION(      "<  SEND TYPE REQUEST  >", sensorDeviceTypeRequestAction)
#endif /* DEVICE_TYPE_MSG */

CUI_SUB_MENU_END

CUI_MAIN_MENU(csfMainMenu, SFF_MENU_TITLE, 3, processMenuUpdate)
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

 Public function defined in csf.h
 */
void Csf_init(void *sem)
{
    CUI_clientParams_t clientParams;
#ifdef NV_RESTORE
    /* Save off the NV Function Pointers */
    pNV = &Main_user1Cfg.nvFps;
#endif

    /* Save off the semaphore */
    collectorSem = sem;

    /* Open UI for key and LED */
    CUI_clientParamsInit(&clientParams);

    strncpy(clientParams.clientName, "154 Collector", MAX_CLIENT_NAME_LEN);
    clientParams.maxStatusLines = 3;

#ifdef FEATURE_SECURE_COMMISSIONING
    clientParams.maxStatusLines++;
#endif
#ifdef SECURE_MANAGER_DEBUG
    clientParams.maxStatusLines++;
#endif
#ifdef SECURE_MANAGER_DEBUG2
    clientParams.maxStatusLines++;
#endif

    csfCuiHndl = CUI_clientOpen(&clientParams);

#ifdef FEATURE_SECURE_COMMISSIONING
    /* Intialize the security manager and register callbacks */
    SM_init(collectorSem, csfCuiHndl);
#endif //FEATURE_SECURE_COMMISSIONING

    /* Initialize Keys */
    CUI_btnRequest_t leftBtnReq;
    leftBtnReq.index = CONFIG_BTN_LEFT;
    leftBtnReq.appCB = processKeyChangeCallback;
    CUI_btnResourceRequest(csfCuiHndl, &leftBtnReq);

    CUI_btnRequest_t rightBtnReq;
    rightBtnReq.index = CONFIG_BTN_RIGHT;
    rightBtnReq.appCB = NULL;
    CUI_btnResourceRequest(csfCuiHndl, &rightBtnReq);

    bool btnState = false;
    CUI_btnGetValue(csfCuiHndl, CONFIG_BTN_RIGHT, &btnState);
    if (!btnState) {
       /* Right key is pressed on power up, clear all NV */
        Csf_clearAllNVItems();
    }

    CUI_btnSetCb(csfCuiHndl, CONFIG_BTN_RIGHT, processKeyChangeCallback);


#if !defined(POWER_MEAS)
    /* Initialize the LEDs */
    CUI_ledRequest_t greenLedReq;
    greenLedReq.index = CONFIG_LED_GREEN;
    CUI_ledResourceRequest(csfCuiHndl, &greenLedReq);

    CUI_ledRequest_t redLedReq;
    redLedReq.index = CONFIG_LED_RED;
    CUI_ledResourceRequest(csfCuiHndl, &redLedReq);

    // Blink to indicate the application started up correctly
    CUI_ledBlink(csfCuiHndl, CONFIG_LED_RED, 3);

    CUI_registerMenu(csfCuiHndl, &csfMainMenu);

    CUI_statusLineResourceRequest(csfCuiHndl, "Status", &collectorStatusLine);
    CUI_statusLineResourceRequest(csfCuiHndl, "Device Status", &deviceStatusLine);
    CUI_statusLineResourceRequest(csfCuiHndl, "Number of Joined Devices", &numJoinDevStatusLine);
#endif /* POWER_MEAS */

#if !defined(AUTO_START)
#ifndef POWER_MEAS
    CUI_statusLinePrintf(csfCuiHndl, collectorStatusLine, "Waiting...");
#endif
#endif /* AUTO_START */

#if defined(MT_CSF)
    {
        uint8_t resetReseason = 0;

        if(pNV != NULL)
        {
            if(pNV->readItem != NULL)
            {
                /* Attempt to retrieve reason for the reset */
                (void)pNV->readItem(nvResetId, 0, 1, &resetReseason);
            }

            if(pNV->deleteItem != NULL)
            {
                /* Only use this reason once */
                (void)pNV->deleteItem(nvResetId);
            }
        }

        /* Start up the MT message handler */
        MTCSF_init(resetReseason);

        /* Did we reset because of assert? */
        if(resetReseason > 0)
        {
            CUI_statusLinePrintf(csfCuiHndl, collectorStatusLine, "Restarting...");

            /* Tell the collector to restart */
            Csf_events |= CSF_KEY_EVENT;
            Csf_keys |= CONFIG_BTN_LEFT;
        }
    }
#endif
}

/*!
 The application must call this function periodically to
 process any Csf_events that this module needs to process.

 Public function defined in csf.h
 */
void Csf_processEvents(void)
{
    /* Did a key press occur? LaunchPad only supports CONFIG_BTN_LEFT and CONFIG_BTN_RIGHT */
    if(Csf_events & CSF_KEY_EVENT)
    {
        /* Process the Left Key */
        if(Csf_keys == CONFIG_BTN_LEFT)
        {
            if(started == false)
            {
                /* Tell the collector to start */
                formNwkAndUpdateUser();
            }
            else
            {
#if defined(TEST_REMOVE_DEVICE)
                /*
                 Remove the first device found in the device list.
                 Nobody would do something like this, it's just
                 and example on the use of the device list and remove
                 function.
                 */
                removeTheFirstDevice();
#else
                /*
                 Send a Toggle LED request to the first device
                 in the device list if left button is pressed
                 */
#ifndef POWER_MEAS
                ApiMac_sAddr_t firstDev;
                firstDev.addr.shortAddr = getTheFirstDevice();
                sendToggleAndUpdateUser(firstDev.addr.shortAddr);
#endif /* POWER_MEAS */
#endif /* TEST_REMOVE_DEVICE */
            }
        }

        /* Process the Right Key */
        if(Csf_keys == CONFIG_BTN_RIGHT)
        {
            openCloseNwkAndUpdateUser(!permitJoining);
        }
        /* Clear the key press indication */
        Csf_keys = 0xFF;

        /* Clear the event */
        Util_clearEvent(&Csf_events, CSF_KEY_EVENT);
    }

    if(Csf_events & COLLECTOR_UI_INPUT_EVT)
    {
        CUI_processMenuUpdate();

        /* Clear the event */
        Util_clearEvent(&Csf_events, COLLECTOR_UI_INPUT_EVT);
    }

    if(Csf_events & COLLECTOR_SENSOR_ACTION_EVT)
    {
        ApiMac_sAddr_t SelectedSensorAddr;

        SelectedSensorAddr.addrMode = ApiMac_addrType_short;
        SelectedSensorAddr.addr.shortAddr = SelectedSensor;

        switch(Csf_sensorAction)
        {
            case SENSOR_ACTION_SET_RPT_INT:
            {
                Collector_sendConfigRequest(
                        &SelectedSensorAddr, (CONFIG_FRAME_CONTROL),
                        reportInterval,
                        (CONFIG_POLLING_INTERVAL));
                break;
            }
            case SENSOR_ACTION_TOGGLE:
            {
                /* send Toggle if CUI */
#ifndef POWER_MEAS
                sendToggleAndUpdateUser(SelectedSensor);
#endif /* endif for POWER_MEAS */
                break;
            }
            case SENSOR_ACTION_DISASSOC:
            {
                if(Csf_removeDevice(SelectedSensorAddr.addr.shortAddr) == 0)
                {
                    Csf_deviceDisassocUpdate(SelectedSensorAddr.addr.shortAddr);
                }
                else
                {
#ifndef POWER_MEAS
                    CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "Disassociate Sensor Error - Addr=0x%04x not found",
                                         SelectedSensorAddr.addr.shortAddr);
#endif /* endif for POWER_MEAS */
                }
                break;
            }
#if defined(DEVICE_TYPE_MSG)
            case SENSOR_ACTION_DEVICE_TYPE_REQ:
            {
                Collector_sendDeviceTypeRequest(&SelectedSensorAddr);
                break;
            }
#endif /* endif for DEVICE_TYPE_MSG */
            default:
                break;
        }

        /* Clear the event */
        Util_clearEvent(&Csf_events, COLLECTOR_SENSOR_ACTION_EVT);
    }

#if defined(MT_CSF)
    MTCSF_displayStatistics();
#endif
}

/*!
 The application calls this function to retrieve the stored
 network information.

 Public function defined in csf.h
 */
bool Csf_getNetworkInformation(Llc_netInfo_t *pInfo)
{
    if((pNV != NULL) && (pNV->readItem != NULL) && (pInfo != NULL))
    {
        NVINTF_itemID_t id;

        /* Setup NV ID */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = CSF_NV_NETWORK_INFO_ID;
        id.subID = 0;

        /* Read Network Information from NV */
        if(pNV->readItem(id, 0, sizeof(Llc_netInfo_t), pInfo) == NVINTF_SUCCESS)
        {
            return(true);
        }
    }
    return(false);
}

/*!
 The application calls this function to indicate that it has
 started or restored the device in a network

 Public function defined in csf.h
 */
void Csf_networkUpdate(bool restored, Llc_netInfo_t *pNetworkInfo)
{
    /* check for valid structure pointer, ignore if not */
    if(pNetworkInfo != NULL)
    {
        if((pNV != NULL) && (pNV->writeItem != NULL))
        {
            NVINTF_itemID_t id;

            /* Setup NV ID */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = CSF_NV_NETWORK_INFO_ID;
            id.subID = 0;

            /* Write the NV item */
            pNV->writeItem(id, sizeof(Llc_netInfo_t), pNetworkInfo);
        }

        started = true;
#ifndef POWER_MEAS
        updateCollectorStatusLine(restored, pNetworkInfo);
        CUI_ledOn(csfCuiHndl, CONFIG_LED_RED, NULL);
#endif /* endif for POWER_MEAS */

#if defined(MT_CSF)
        MTCSF_networkUpdateIndCB();
#endif /* endif for MT_CSF */
    }
}


/*!
 The application calls this function to indicate that a device
 has joined the network.

 Public function defined in csf.h
 */
ApiMac_assocStatus_t Csf_deviceUpdate(ApiMac_deviceDescriptor_t *pDevInfo,
                                      ApiMac_capabilityInfo_t *pCapInfo)
{
    ApiMac_assocStatus_t status = ApiMac_assocStatus_success;

    /* flag which will be updated based on if the device joining is
     a new device or already existing one */
    bool newDevice;

    /* Save the device information */
    Llc_deviceListItem_t dev;

    memcpy(&dev.devInfo, pDevInfo, sizeof(ApiMac_deviceDescriptor_t));
    memcpy(&dev.capInfo, pCapInfo, sizeof(ApiMac_capabilityInfo_t));
    dev.rxFrameCounter = 0;

    if(addDeviceListItem(&dev, &newDevice) == false)
    {
#ifdef NV_RESTORE
        status = ApiMac_assocStatus_panAtCapacity;

#ifndef POWER_MEAS
        CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "Failed - 0x%04x ", pDevInfo->shortAddress);
#endif /* endif for POWER_MEAS */
#else
        status = ApiMac_assocStatus_success;
#ifndef POWER_MEAS
        CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "Joined - 0x%04x ", pDevInfo->shortAddress);
#endif /* endif for POWER_MEAS */
#endif
    }
#ifndef POWER_MEAS
    else if (true == newDevice) {
        CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "Joined - 0x%04x ", pDevInfo->shortAddress);
    }
    else {
        CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "Rejoined - 0x%04x ", pDevInfo->shortAddress);
    }
#endif /* endif for POWER_MEAS */

#ifndef POWER_MEAS
    CUI_statusLinePrintf(csfCuiHndl, numJoinDevStatusLine, "%x", getNumActiveDevices());
#endif
#if defined(MT_CSF)
    MTCSF_deviceUpdateIndCB(pDevInfo, pCapInfo);
#endif

    /* Return the status of the joining device */
    return (status);
}

/*!
 The application calls this function to indicate that a device
 is no longer active in the network.

 Public function defined in csf.h
 */
void Csf_deviceNotActiveUpdate(ApiMac_deviceDescriptor_t *pDevInfo,
bool timeout)
{
#ifndef POWER_MEAS
    CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "!Responding - 0x%04x ", pDevInfo->shortAddress);
#endif /* endif for POWER_MEAS */
#if defined(MT_CSF)
    MTCSF_deviceNotActiveIndCB(pDevInfo, timeout);
#endif /* endif for MT_CSF */
}

/*!
 The application calls this function to indicate that a device
 has responded to a Config Request.

 Public function defined in csf.h
 */
void Csf_deviceConfigUpdate(ApiMac_sAddr_t *pSrcAddr, int8_t rssi,
                            Smsgs_configRspMsg_t *pMsg)
{
#ifndef POWER_MEAS
    CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "ConfigRsp - 0x%04x ", pSrcAddr->addr.shortAddr);
    CUI_statusLinePrintf(csfCuiHndl, numJoinDevStatusLine, "%x", getNumActiveDevices());

#endif /* endif for POWER_MEAS */
#if defined(MT_CSF)
    MTCSF_configResponseIndCB(pSrcAddr, rssi, pMsg);
#endif /* endif for MT_CSF */
}




/*!
 The application calls this function to indicate that a device
 has reported sensor data.

 Public function defined in csf.h
 */
void Csf_deviceSensorDataUpdate(ApiMac_sAddr_t *pSrcAddr, int8_t rssi,
                                Smsgs_sensorMsg_t *pMsg)
{
    CUI_ledToggle(csfCuiHndl, CONFIG_LED_GREEN);
#ifndef POWER_MEAS
    CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "Sensor - Addr=0x%04x, Temp=%d, RSSI=%d",
                         pSrcAddr->addr.shortAddr, pMsg->tempSensor.ambienceTemp, rssi);
    CUI_statusLinePrintf(csfCuiHndl, numJoinDevStatusLine, "%x", getNumActiveDevices());

#endif /* endif for POWER_MEAS */

#if defined(MT_CSF)
    MTCSF_sensorUpdateIndCB(pSrcAddr, rssi, pMsg);
#endif /* endif for MT_CSF */
}

/*!
 The application calls this function to indicate that a device
 has been disassociated.

 Public function defined in csf.h
 */
void Csf_deviceDisassocUpdate(uint16_t shortAddr)
{
#ifndef POWER_MEAS
    CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "Disassociate Sensor - Addr=0x%04x", shortAddr);
    CUI_statusLinePrintf(csfCuiHndl, numJoinDevStatusLine, "%x", getNumActiveDevices());
#endif /* endif for POWER_MEAS */
}

#if defined(DEVICE_TYPE_MSG)
/*!
 * @brief       The application calls this function to print out the reported
 *              device type
 *
 * @param       deviceFamilyID - the integer ID of the device family
 * @param       deviceTypeID - the integer ID of the board/device
 *
 * Public function defined in csf.h
 */
void Csf_deviceSensorDeviceTypeResponseUpdate(uint8_t deviceFamilyID, uint8_t deviceTypeID)
{
    char* deviceStr;

    switch (deviceTypeID)
    {
        case DeviceType_ID_CC1310:
            deviceStr = "cc1310";
            break;
        case DeviceType_ID_CC1350:
            deviceStr = "cc1350";
            break;
        case DeviceType_ID_CC2640R2:
            deviceStr = "cc2640r2";
            break;
        case DeviceType_ID_CC1312R1:
            deviceStr = "cc1312r1";
            break;
        case DeviceType_ID_CC1352R1:
            deviceStr = "cc1352r1";
            break;
        case DeviceType_ID_CC1352P1:
            deviceStr = "cc1352p1";
            break;
        case DeviceType_ID_CC1352P_2:
            deviceStr = "cc1352p2";
            break;
        case DeviceType_ID_CC1352P_4:
            deviceStr = "cc1352p4";
            break;
        case DeviceType_ID_CC2642R1:
            deviceStr = "cc2642r1";
            break;
        case DeviceType_ID_CC2652R1:
            deviceStr = "cc2652r1";
            break;
        case DeviceType_ID_CC2652RB:
            deviceStr = "cc2652rb";
            break;
        default:
            deviceStr = "generic";
            break;
    }
#ifndef POWER_MEAS
    CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "Sensor - Device=%s, DeviceFamilyID=%i, DeviceTypeID=%i",
                         deviceStr, deviceFamilyID, deviceTypeID);
#endif /* endif for POWER_MEAS */
}

#endif /* DEVICE_TYPE_MSG */

/*!
 The application calls this function to toggle an LED.

 Public function defined in ssf.h
 */
void Csf_identifyLED(uint16_t identifyTime)
{
    CUI_ledBlink(csfCuiHndl, CONFIG_LED_GREEN, 3);

    /* Setup timer */
    Timer_setTimeout(identifyClkHandle, identifyTime);
    Timer_start(&identifyClkStruct);
}

/*!
 The application calls this function to indicate that a device
 set a Toggle LED Response message.

 Public function defined in csf.h
 */
void Csf_toggleResponseReceived(ApiMac_sAddr_t *pSrcAddr, bool ledState)
{
#if defined(MT_CSF)
    uint16_t shortAddr = 0xFFFF;

    if(pSrcAddr)
    {
        if(pSrcAddr->addrMode == ApiMac_addrType_short)
        {
            shortAddr = pSrcAddr->addr.shortAddr;
        }
        else
        {
            /* Convert extended to short addr */
            shortAddr = Csf_getDeviceShort(&pSrcAddr->addr.extAddr);
        }
    }
    MTCSF_deviceToggleIndCB(shortAddr, ledState);
#endif /* endif for MT_CSF */
}

/*!
 The application calls this function to indicate that the
 Coordinator's state has changed.

 Public function defined in csf.h
 */
void Csf_stateChangeUpdate(Cllc_states_t state)
{
    if(started == true)
    {
        /* always blink in FH mode because permit join is always on */
        if(state == Cllc_states_joiningAllowed || CONFIG_FH_ENABLE)
        {
            /* Flash LED1 while allowing joining */
            CUI_ledBlink(csfCuiHndl, CONFIG_LED_RED, CUI_BLINK_CONTINUOUS);
        }
        else if(state == Cllc_states_joiningNotAllowed)
        {
            /* Don't flash when not allowing joining */
            CUI_ledOn(csfCuiHndl, CONFIG_LED_RED, NULL);
        }
    }

    /* Save the state to be used later */
    savedCllcState = state;

#if defined(MT_CSF)
    MTCSF_stateChangeIndCB(state);
#endif /* endif for MT_CSF */
}

/*!
 Initialize the tracking clock.

 Public function defined in csf.h
 */
void Csf_initializeTrackingClock(void)
{
    /* Initialize the timers needed for this application */
    trackingClkHandle = Timer_construct(&trackingClkStruct,
                                        processTrackingTimeoutCallback,
                                        TRACKING_INIT_TIMEOUT_VALUE,
                                        0,
                                        false,
                                        0);
}

/*!
 Initialize the broadcast cmd clock.

 Public function defined in csf.h
 */
void Csf_initializeBroadcastClock(void)
{
    /* Initialize the timers needed for this application */
    broadcastClkHandle = Timer_construct(&broadcastClkStruct,
                                        processBroadcastTimeoutCallback,
                                        TRACKING_INIT_TIMEOUT_VALUE,
                                        0,
                                        false,
                                        0);
}

/*!
 Initialize the trickle clock.

 Public function defined in csf.h
 */
void Csf_initializeTrickleClock(void)
{
    /* Initialize trickle timer */
    tricklePAClkHandle = Timer_construct(&tricklePAClkStruct,
                                         processPATrickleTimeoutCallback,
                                         TRICKLE_TIMEOUT_VALUE,
                                         0,
                                         false,
                                         0);

    tricklePCClkHandle = Timer_construct(&tricklePCClkStruct,
                                         processPCTrickleTimeoutCallback,
                                         TRICKLE_TIMEOUT_VALUE,
                                         0,
                                         false,
                                         0);
}

/*!
 Initialize the clock for join permit attribute.

 Public function defined in csf.h
 */
void Csf_initializeJoinPermitClock(void)
{
    /* Initialize join permit timer */
    joinClkHandle = Timer_construct(&joinClkStruct,
                                    processJoinTimeoutCallback,
                                    JOIN_TIMEOUT_VALUE,
                                    0,
                                    false,
                                    0);
}

/*!
 Initialize the clock for config request delay

 Public function defined in csf.h
 */
void Csf_initializeConfigClock(void)
{
    /* Initialize join permit timer */
    configClkHandle = Timer_construct(&configClkStruct,
                                    processConfigTimeoutCallback,
                                    CONFIG_TIMEOUT_VALUE,
                                    0,
                                    false,
                                    0);
}

/*!
 Initialize the clock for identify timeout

 Public function defined in csf.h
 */
void Csf_initializeIdentifyClock(void)
{
    /* Initialize identify clock timer */
    identifyClkHandle = Timer_construct(&identifyClkStruct,
                                    processidentifyTimeoutCallback,
                                    10,
                                    0,
                                    false,
                                    0);
}


/*!
 Set the tracking clock.

 Public function defined in csf.h
 */
void Csf_setTrackingClock(uint32_t trackingTime)
{
    /* Stop the Tracking timer */
    if(Timer_isActive(&trackingClkStruct) == true)
    {
        Timer_stop(&trackingClkStruct);
    }

    if(trackingTime)
    {
        /* Setup timer */
        Timer_setTimeout(trackingClkHandle, trackingTime);
        Timer_start(&trackingClkStruct);
    }
}

/*!
 Set the broadcast clock.

 Public function defined in csf.h
 */
void Csf_setBroadcastClock(uint32_t broadcastTime)
{
    /* Stop the Tracking timer */
    if(Timer_isActive(&broadcastClkStruct) == true)
    {
        Timer_stop(&broadcastClkStruct);
    }

    if(broadcastTime)
    {
        /* Setup timer */
        Timer_setTimeout(broadcastClkHandle, broadcastTime);
        Timer_start(&broadcastClkStruct);
    }
}

/*!
 Set the trickle clock.

 Public function defined in csf.h
 */
void Csf_setTrickleClock(uint32_t trickleTime, uint8_t frameType)
{
    uint16_t randomNum = 0;
    uint16_t randomTime = 0;

    if(trickleTime > 0)
    {
        randomNum = ((ApiMac_randomByte() << 8) + ApiMac_randomByte());
        randomTime = (trickleTime >> 1) +
                      (randomNum % (trickleTime >> 1));
    }

    if(frameType == ApiMac_wisunAsyncFrame_advertisement)
    {
        /* Stop the PA trickle timer */
        if(Timer_isActive(&tricklePAClkStruct) == true)
        {
            Timer_stop(&tricklePAClkStruct);
        }

        if(trickleTime > 0)
        {
            /* Setup timer */
            Timer_setTimeout(tricklePAClkHandle, randomTime);
            Timer_start(&tricklePAClkStruct);
        }
    }
    else if(frameType == ApiMac_wisunAsyncFrame_config)
    {
        /* Stop the PC trickle timer */
        if(Timer_isActive(&tricklePCClkStruct) == true)
        {
            Timer_stop(&tricklePCClkStruct);
        }

        if(trickleTime > 0)
        {
            /* Setup timer */
            Timer_setTimeout(tricklePCClkHandle, randomTime);
            Timer_start(&tricklePCClkStruct);
        }
    }
}

/*!
 Set the clock join permit attribute.

 Public function defined in csf.h
 */
void Csf_setJoinPermitClock(uint32_t joinDuration)
{
    /* Stop the join timer */
    if(Timer_isActive(&joinClkStruct) == true)
    {
        Timer_stop(&joinClkStruct);
    }

    if(joinDuration != 0)
    {
        /* Setup timer */
        Timer_setTimeout(joinClkHandle, joinDuration);
        Timer_start(&joinClkStruct);
    }
}

/*!
 Set the clock config request delay.

 Public function defined in csf.h
 */
void Csf_setConfigClock(uint32_t delay)
{
    /* Stop the join timer */
    if(Timer_isActive(&configClkStruct) == true)
    {
        Timer_stop(&configClkStruct);
    }

    if(delay != 0)
    {
        /* Setup timer */
        Timer_setTimeout(configClkHandle, delay);
        Timer_start(&configClkStruct);
    }
}



/*!
 Read the number of device list items stored

 Public function defined in csf.h
 */
uint16_t Csf_getNumDeviceListEntries(void)
{
    uint16_t numEntries = 0;

    if(pNV != NULL)
    {
        NVINTF_itemID_t id;
        uint8_t stat;

        /* Setup NV ID for the number of entries in the device list */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = CSF_NV_DEVICELIST_ENTRIES_ID;
        id.subID = 0;

        /* Read the number of device list items from NV */
        stat = pNV->readItem(id, 0, sizeof(uint16_t), &numEntries);
        if(stat != NVINTF_SUCCESS)
        {
            numEntries = 0;
        }
    }
    return (numEntries);
}

/*!
 Find the short address from a given extended address

 Public function defined in csf.h
 */
uint16_t Csf_getDeviceShort(ApiMac_sAddrExt_t *pExtAddr)
{
    Llc_deviceListItem_t item;
    ApiMac_sAddr_t devAddr;
    uint16_t shortAddr = CSF_INVALID_SHORT_ADDR;

    devAddr.addrMode = ApiMac_addrType_extended;
    memcpy(&devAddr.addr.extAddr, pExtAddr, sizeof(ApiMac_sAddrExt_t));

    if(Csf_getDevice(&devAddr,&item))
    {
        shortAddr = item.devInfo.shortAddress;
    }

    return(shortAddr);
}

/*!
 Find entry in device list

 Public function defined in csf.h
 */
bool Csf_getDevice(ApiMac_sAddr_t *pDevAddr, Llc_deviceListItem_t *pItem)
{
    if((pNV != NULL) && (pItem != NULL))
    {
        uint16_t numEntries;

        numEntries = Csf_getNumDeviceListEntries();

        if(numEntries > 0)
        {
            NVINTF_itemID_t id;

            /* Setup NV ID for the device list records */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = CSF_NV_DEVICELIST_ID;
            id.subID = 0;
            /* Read Network Information from NV */
            if(pDevAddr->addrMode == ApiMac_addrType_short)
            {
                pNV->readContItem(id, 0, sizeof(Llc_deviceListItem_t), pItem,
                                      sizeof(uint16_t),
                                      (uint16_t)((uint32_t)&pItem->devInfo.shortAddress-(uint32_t)&pItem->devInfo.panID),
                                      &pDevAddr->addr.shortAddr, &id.subID);
            }
            else
            {
                pNV->readContItem(id, 0, sizeof(Llc_deviceListItem_t), pItem,
                                      APIMAC_SADDR_EXT_LEN,
                                      (uint16_t)((uint32_t)&pItem->devInfo.extAddress-(uint32_t)&pItem->devInfo.panID),
                                      &pDevAddr->addr.extAddr, &id.subID);
            }


            if(id.subID != CSF_INVALID_SUBID)
            {
                return(true);
            }
        }
    }
    return (false);
}

/*!
 Find entry in device list

 Public function defined in csf.h
 */
bool Csf_getDeviceItem(uint16_t devIndex, Llc_deviceListItem_t *pItem)
{
    if((pNV != NULL) && (pItem != NULL))
    {
        uint16_t numEntries;

        numEntries = Csf_getNumDeviceListEntries();

        if(numEntries > 0)
        {
            NVINTF_itemID_t id;
            uint8_t stat;
            int subId = 0;
            int readItems = 0;

            /* Setup NV ID for the device list records */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = CSF_NV_DEVICELIST_ID;

            while((readItems < numEntries) && (subId
                                               < CSF_MAX_DEVICELIST_IDS))
            {
                Llc_deviceListItem_t item;

                id.subID = (uint16_t)subId;

                /* Read Network Information from NV */
                stat = pNV->readItem(id, 0, sizeof(Llc_deviceListItem_t),
                                     &item);
                if(stat == NVINTF_SUCCESS)
                {
                    if(readItems == devIndex)
                    {
                        memcpy(pItem, &item, sizeof(Llc_deviceListItem_t));
                        return (true);
                    }
                    readItems++;
                }
                subId++;
            }
        }
    }

    return (false);
}

/*!
 Csf implementation for memory allocation

 Public function defined in csf.h
 */
void *Csf_malloc(uint16_t size)
{
#ifdef OSAL_PORT2TIRTOS
    return OsalPort_malloc(size);
#else
    return(ICall_malloc(size));
#endif /* endif for OSAL_PORT2TIRTOS */
}

/*!
 Csf implementation for memory de-allocation

 Public function defined in csf.h
 */
void Csf_free(void *ptr)
{
    if(ptr != NULL)
    {
#ifdef OSAL_PORT2TIRTOS
        OsalPort_free(ptr);
#else
        ICall_free(ptr);
#endif /* endif for OSAL_PORT2TIRTOS */
    }
}

/*!
 Update the Frame Counter

 Public function defined in csf.h
 */
void Csf_updateFrameCounter(ApiMac_sAddr_t *pDevAddr, uint32_t frameCntr)
{
    if((pNV != NULL) && (pNV->writeItem != NULL))
    {
        if(pDevAddr == NULL)
        {
            /* Update this device's frame counter */
            if((frameCntr >=
                (lastSavedCoordinatorFrameCounter + FRAME_COUNTER_SAVE_WINDOW)))
            {
                NVINTF_itemID_t id;

                /* Setup NV ID */
                id.systemID = NVINTF_SYSID_APP;
                id.itemID = CSF_NV_FRAMECOUNTER_ID;
                id.subID = 0;

                /* Write the NV item */
                if(pNV->writeItem(id, sizeof(uint32_t), &frameCntr)
                                == NVINTF_SUCCESS)
                {
                    lastSavedCoordinatorFrameCounter = frameCntr;
                }
            }
        }
        else
        {
            /* Child frame counter update */
            Llc_deviceListItem_t devItem;

            /* Is the device in our database? */
            if(Csf_getDevice(pDevAddr, &devItem))
            {
                /*
                 Don't save every update, only save if the new frame
                 counter falls outside the save window.
                 */
                if((devItem.rxFrameCounter + FRAME_COUNTER_SAVE_WINDOW)
                                <= frameCntr)
                {
                    /* Update the frame counter */
                    devItem.rxFrameCounter = frameCntr;
                    updateDeviceListItem(&devItem);
                }
            }
        }
    }
}

/*!
 Get the Frame Counter

 Public function defined in csf.h
 */
bool Csf_getFrameCounter(ApiMac_sAddr_t *pDevAddr, uint32_t *pFrameCntr)
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
                id.itemID = CSF_NV_FRAMECOUNTER_ID;
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
                    id.itemID = CSF_NV_FRAMECOUNTER_ID;
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
 Delete an entry from the device list

 Public function defined in csf.h
 */
void Csf_removeDeviceListItem(ApiMac_sAddrExt_t *pAddr)
{
    if((pNV != NULL) && (pNV->deleteItem != NULL))
    {
        int index;

        /* Does the item exist? */
        index = findDeviceListIndex(pAddr);
        if(index != DEVICE_INDEX_NOT_FOUND)
        {
            uint8_t stat;
            NVINTF_itemID_t id;

            /* Setup NV ID for the device list record */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = CSF_NV_DEVICELIST_ID;
            id.subID = (uint16_t)index;

            stat = pNV->deleteItem(id);
            if(stat == NVINTF_SUCCESS)
            {
                /* Update the number of entries */
                uint16_t numEntries = Csf_getNumDeviceListEntries();
                if(numEntries > 0)
                {
                    numEntries--;
                    saveNumDeviceListEntries(numEntries);
                }
            }
        }
    }
}

/*!
 Assert Indication

 Public function defined in csf.h
 */
void Csf_assertInd(uint8_t reason)
{
#if defined(MT_CSF)
    if((pNV != NULL) && (pNV->writeItem != NULL))
    {
        /* Attempt to save reason to read after reset */
        (void)pNV->writeItem(nvResetId, 1, &reason);
    }
#endif /* endif for MT_CSF */
}

/*!
 Clear all the NV Items

 Public function defined in csf.h
 */
void Csf_clearAllNVItems(void)
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
    if((pNV != NULL) && (pNV->deleteItem != NULL))
    {
        NVINTF_itemID_t id;
        uint16_t entries;

        /* Clear Network Information */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = CSF_NV_NETWORK_INFO_ID;
        id.subID = 0;
        pNV->deleteItem(id);

        /* Clear the device list entries number */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = CSF_NV_DEVICELIST_ENTRIES_ID;
        id.subID = 0;
        pNV->deleteItem(id);

        /*
         Clear the device list entries.  Brute force through
         every possible subID, if it doesn't exist that's fine,
         it will fail in deleteItem.
         */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = CSF_NV_DEVICELIST_ID;
        for(entries = 0; entries < CSF_MAX_DEVICELIST_IDS; entries++)
        {
            id.subID = entries;
            pNV->deleteItem(id);
        }

        /* Clear the device tx frame counter */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = CSF_NV_FRAMECOUNTER_ID;
        id.subID = 0;
        pNV->deleteItem(id);
    }
#endif /* end if for ONE_PAGE_NV */
}

/*!
 Check if config timer is active

 Public function defined in csf.h
 */
bool Csf_isConfigTimerActive(void)
{
    return(Timer_isActive(&configClkStruct));
}

/*!
 Check if tracking timer is active

 Public function defined in csf.h
 */
bool Csf_isTrackingTimerActive(void)
{
    return(Timer_isActive(&trackingClkStruct));
}

/*!
 The application calls this function to open the network.

 Public function defined in ssf.h
 */
void Csf_openNwk(void)
{
    permitJoining = true;
    Cllc_setJoinPermit(0xFFFFFFFF);
#ifndef POWER_MEAS
    updateCollectorStatusLine(false, NULL);
#endif /* endif for POWER_MEAS */
}

/*!
 The application calls this function to close the network.

 Public function defined in ssf.h
 */
void Csf_closeNwk(void)
{
    if(!CONFIG_FH_ENABLE)
    {
        permitJoining = false;
        Cllc_setJoinPermit(0);
#ifndef POWER_MEAS
        updateCollectorStatusLine(false, NULL);
#endif /* endif for POWER_MEAS */
    }
}

/*!
 * @brief       Removes a device from the network.
 *
 * @param        deviceShortAddr - device short address to remove.
 */
int Csf_removeDevice(uint16_t deviceShortAddr)
{
    int status = -1;

    if(pNV != NULL)
    {
        uint16_t numEntries;

        numEntries = Csf_getNumDeviceListEntries();

        if(numEntries > 0)
        {
            NVINTF_itemID_t id;
            uint16_t subId = 0;

            /* Setup NV ID for the device list records */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = CSF_NV_DEVICELIST_ID;

            while(subId < CSF_MAX_DEVICELIST_IDS)
            {
                Llc_deviceListItem_t item;
                uint8_t stat;

                id.subID = (uint16_t)subId;

                /* Read Network Information from NV */
                stat = pNV->readItem(id, 0, sizeof(Llc_deviceListItem_t),
                                     &item);

                if( (stat == NVINTF_SUCCESS) && (deviceShortAddr == item.devInfo.shortAddress))
                {
                    /* Send a disassociate to the device */
                    Cllc_sendDisassociationRequest(item.devInfo.shortAddress,
                                                   item.capInfo.rxOnWhenIdle);
                    /* remove device from the NV list */
                    Cllc_removeDevice(&item.devInfo.extAddress);

                    status = 0;
                    break;
                }
                subId++;
            }
        }
    }

    return status;
}

#ifdef FEATURE_SECURE_COMMISSIONING
/*!
 The application calls this function to get a passkey.

 Public function defined in ssf.h
 */
void Csf_SmPasskeyEntry(SM_passkeyEntry_t passkeyAction)
{
    static uint8_t smMenuUsed = 0;
    // if passkey is selected
    if(passkeyAction == SM_passkeyEntryReq)
    {
        // deregister main menu when you switch to SM menu
        CUI_deRegisterMenu(csfCuiHndl, &csfMainMenu);

        // request a menu if available
        CUI_registerMenu(csfCuiHndl, &smPassKeyMenu);
        smMenuUsed = 1;

        // Open the menu itself
        // there is only 1 item in smPassKeyMenu list.
        CUI_menuNav(csfCuiHndl, &smPassKeyMenu, 0);
    }
    else if(passkeyAction == SM_passkeyEntered)
    {
        // deregister the passkey menu
        CUI_deRegisterMenu(csfCuiHndl, &smPassKeyMenu);

        // Only re-enable the main menu if it was previously disabled
        if(smMenuUsed == 1)
        {
            // re-register the main menu again
            CUI_registerMenu(csfCuiHndl, &csfMainMenu);

        }
        // Go back to the help screen which is the last menu in the list.
        // third argument represents the index of the menu to travel to.
        CUI_menuNav(csfCuiHndl, &csfMainMenu, csfMainMenu.numItems - 1);
    }
}
#endif /* endif for FEATURE_SECURE_COMMISSIONING */

/******************************************************************************
 Local Functions
 *****************************************************************************/

/*!
 * @brief       Tracking timeout handler function.
 *
 * @param       a0 - ignored
 */
static void processTrackingTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Collector_events, COLLECTOR_TRACKING_TIMEOUT_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

/*!
 * @brief       Tracking timeout handler function.
 *
 * @param       a0 - ignored
 */
static void processBroadcastTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Collector_events, COLLECTOR_BROADCAST_TIMEOUT_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

/*!
 * @brief       Join permit timeout handler function.
 *
 * @param       a0 - ignored
 */
static void processJoinTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Cllc_events, CLLC_JOIN_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

/*!
 * @brief       Config delay timeout handler function.
 *
 * @param       a0 - ignored
 */
static void processConfigTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Collector_events, COLLECTOR_CONFIG_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

/*!
 * @brief       Identify timeout handler function.
 *
 * @param       a0 - ignored
 */
static void processidentifyTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    /* Stop LED blinking - we can write to GPIO in SWI */
    CUI_ledOff(csfCuiHndl, CONFIG_LED_GREEN);
}

/*!
 * @brief       Trickle timeout handler function for PA .
 *
 * @param       a0 - ignored
 */
static void processPATrickleTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Cllc_events, CLLC_PA_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

/*!
 * @brief       Trickle timeout handler function for PC.
 *
 * @param       a0 - ignored
 */
static void processPCTrickleTimeoutCallback(UArg a0)
{
    (void)a0; /* Parameter is not used */

    Util_setEvent(&Cllc_events, CLLC_PC_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

/*!
 * @brief       Key event handler function
 *
 * @param       keysPressed - Csf_keys that are pressed
 */
static void processKeyChangeCallback(uint32_t _btn, Button_EventMask _events)
{
    if (_events & Button_EV_CLICKED)
    {
        Csf_keys = _btn;
        Csf_events |= CSF_KEY_EVENT;
        /* Wake up the application thread when it waits for keys event */
        Semaphore_post(collectorSem);
    }
}

/*!
 * @brief       Add an entry into the device list
 *
 * @param       pItem - pointer to the device list entry
 * @param       pNewDevice - pointer to a flag which will be updated
 *              based on if the sensor joining is already assoc with
 *              the collector or freshly joining the network
 * @return      true if added or already existed, false if problem
 */
static bool addDeviceListItem(Llc_deviceListItem_t *pItem, bool *pNewDevice)
{
    bool retVal = false;
    int subId = DEVICE_INDEX_NOT_FOUND;
    /* By default, set this flag to true;
    will be updated - if device already found in the list*/
    *pNewDevice = true;

    if((pNV != NULL) && (pItem != NULL))
    {
        subId = findDeviceListIndex(&pItem->devInfo.extAddress);
        if(subId != DEVICE_INDEX_NOT_FOUND)
        {
            retVal = true;

            /* Not a new device; already exists */
            *pNewDevice = false;
        }
        else
        {
            uint8_t stat;
            NVINTF_itemID_t id;
            uint16_t numEntries = Csf_getNumDeviceListEntries();

            /* Check the maximum size */
            if(numEntries < CSF_MAX_DEVICELIST_ENTRIES)
            {
                /* Setup NV ID for the device list record */
                id.systemID = NVINTF_SYSID_APP;
                id.itemID = CSF_NV_DEVICELIST_ID;
                id.subID = (uint16_t)findUnusedDeviceListIndex();

                /* write the device list record */
                if(id.subID != CSF_INVALID_SUBID)
                {
                    stat = pNV->writeItem(id, sizeof(Llc_deviceListItem_t), pItem);
                    if(stat == NVINTF_SUCCESS)
                    {
                        /* Update the number of entries */
                        numEntries++;
                        saveNumDeviceListEntries(numEntries);
                        retVal = true;
                    }
                }
            }
        }
    }

    return (retVal);
}

/*!
 Read the number of currently active sensors

 * */
static uint16_t getNumActiveDevices(void)
{
    uint16_t activeSensors = 0;
    uint16_t x;
    uint16_t numEntries = Csf_getNumDeviceListEntries();
    for(x = 0; x < numEntries; x++)
        {
            if((Cllc_associatedDevList[x].shortAddr != CSF_INVALID_SHORT_ADDR)
               && (Cllc_associatedDevList[x].status & CLLC_ASSOC_STATUS_ALIVE))
            {
                activeSensors++;
            }


        }
    return (activeSensors);
}

/*!
 * @brief       Update an entry in the device list
 *
 * @param       pItem - pointer to the device list entry
 */
static void updateDeviceListItem(Llc_deviceListItem_t *pItem)
{
    if((pNV != NULL) && (pItem != NULL))
    {
        int idx;

        idx = findDeviceListIndex(&pItem->devInfo.extAddress);
        if(idx != DEVICE_INDEX_NOT_FOUND)
        {
            NVINTF_itemID_t id;

            /* Setup NV ID for the device list record */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = CSF_NV_DEVICELIST_ID;
            id.subID = (uint16_t)idx;

            /* write the device list record */
            pNV->writeItem(id, sizeof(Llc_deviceListItem_t), pItem);
        }
    }
}

/*!
 * @brief       Find entry in device list
 *
 * @param       pAddr - address to of device to find
 *
 * @return      sub index into the device list, -1 (DEVICE_INDEX_NOT_FOUND)
 *              if not found
 */
static int findDeviceListIndex(ApiMac_sAddrExt_t *pAddr)
{
    if((pNV != NULL) && (pAddr != NULL))
    {
        uint16_t numEntries;

        numEntries = Csf_getNumDeviceListEntries();

        if(numEntries > 0)
        {
            NVINTF_itemID_t id;
            Llc_deviceListItem_t item;

            /* Setup NV ID for the device list records */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = CSF_NV_DEVICELIST_ID;
            id.subID = 0;

            /* Read Network Information from NV */
            pNV->readContItem(id, 0, sizeof(Llc_deviceListItem_t), &item,
                                  APIMAC_SADDR_EXT_LEN,
                                  (uint16_t)((uint32_t)&item.devInfo.extAddress-(uint32_t)&item), pAddr, &id.subID);

            if(id.subID != CSF_INVALID_SUBID)
            {
                return(id.subID);
            }
        }
    }
    return (DEVICE_INDEX_NOT_FOUND);
}

/*!
 * @brief       Find an unused device list index
 *
 * @return      index that is not in use
 */
static int findUnusedDeviceListIndex(void)
{
    int x;

    for(x = 0; (x < CONFIG_MAX_DEVICES); x++)
    {
        /* Make sure the entry is valid. */
        if(CSF_INVALID_SHORT_ADDR == Cllc_associatedDevList[x].shortAddr)
        {
            return (x);
        }
    }
    return (CSF_INVALID_SUBID);
}

/*!
 * @brief       Read the number of device list items stored
 *
 * @param       numEntries - number of entries in the device list
 */
static void saveNumDeviceListEntries(uint16_t numEntries)
{
    if(pNV != NULL)
    {
        NVINTF_itemID_t id;

        /* Setup NV ID for the number of entries in the device list */
        id.systemID = NVINTF_SYSID_APP;
        id.itemID = CSF_NV_DEVICELIST_ENTRIES_ID;
        id.subID = 0;

        /* Read the number of device list items from NV */
        pNV->writeItem(id, sizeof(uint16_t), &numEntries);
    }
}

#if defined(TEST_REMOVE_DEVICE)
/*!
 * @brief       This is an example function on how to remove a device
 *              from this network.
 */
static void removeTheFirstDevice(void)
{
    if(pNV != NULL)
    {
        uint16_t numEntries;

        numEntries = Csf_getNumDeviceListEntries();

        if(numEntries > 0)
        {
            NVINTF_itemID_t id;
            uint16_t subId = 0;

            /* Setup NV ID for the device list records */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = CSF_NV_DEVICELIST_ID;

            while(subId < CSF_MAX_DEVICELIST_IDS)
            {
                Llc_deviceListItem_t item;
                uint8_t stat;

                id.subID = (uint16_t)subId;

                /* Read Network Information from NV */
                stat = pNV->readItem(id, 0, sizeof(Llc_deviceListItem_t),
                                     &item);

                if(stat == NVINTF_SUCCESS)
                {
                    /* Found the first device in the list */
                    ApiMac_sAddr_t addr;

                    /* Send a disassociate to the device */
                    Cllc_sendDisassociationRequest(item.devInfo.shortAddress,
                                                   item.capInfo.rxOnWhenIdle);
                    /* remove device from the NV list */
                    Cllc_removeDevice(&item.devInfo.extAddress);

                    /* Remove it from the Device list */
                    Csf_removeDeviceListItem(&item.devInfo.extAddress);

                    break;
                }
                subId++;
            }
        }
    }
}
#else

/*!
 * @brief       Retrieve the first device's short address
 *
 * @return      short address or 0xFFFF if not found
 */
static uint16_t getTheFirstDevice(void)
{
    uint16_t found = CSF_INVALID_SHORT_ADDR;
    if(pNV != NULL)
    {
        uint16_t numEntries;

        numEntries = Csf_getNumDeviceListEntries();

        if(numEntries > 0)
        {
            NVINTF_itemID_t id;

            /* Setup NV ID for the device list records */
            id.systemID = NVINTF_SYSID_APP;
            id.itemID = CSF_NV_DEVICELIST_ID;
            id.subID = 0;

            while(id.subID < CSF_MAX_DEVICELIST_IDS)
            {
                Llc_deviceListItem_t item;
                uint8_t stat;

                /* Read Network Information from NV */
                stat = pNV->readItem(id, 0, sizeof(Llc_deviceListItem_t),
                                     &item);
                if(stat == NVINTF_SUCCESS)
                {
                    found = item.devInfo.shortAddress;
                    break;
                }
                id.subID++;
            }
        }
    }
    return(found);
}
#endif /* endif for TEST_REMOVE_DEVICE */
/*!
 * @brief       Handles printing that the orphaned device joined back
 *
 * @return      none
 */
void Csf_IndicateOrphanReJoin(uint16_t shortAddr)
{
#ifndef POWER_MEAS
    CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "Orphaned Sensor Re-Joined - Addr=0x%04x",
                         shortAddr);
    CUI_statusLinePrintf(csfCuiHndl, numJoinDevStatusLine, "%x", getNumActiveDevices());

#endif /* endif for POWER_MEAS */
}

#ifdef USE_DMM

/*!
 Initialize the provisioning timeout clock.

 Public function defined in csf.h
 */
void Csf_initializeProvisioningClock(void)
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

 Public function defined in csf.h
 */
void Csf_setProvisioningClock(bool provision)
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

    Timer_setFunc(provisioningClkHandle, processProvisioningCallback, provision);
    Timer_start(&provisioningClkStruct);
}


static void processProvisioningCallback(UArg provision)
{
    static bool updateProvPolicy = true;

    //Arg used to select provision and associate to a network
    if (provision)
    {
        if (updateProvPolicy)
        {
            Csf_setProvisioningClock(true);

            /* Update policy */
            Util_setEvent(&Collector_events, COLLECTOR_PROV_EVT);

            updateProvPolicy = false;
        }
        else
        {
            Util_setEvent(&Collector_events, COLLECTOR_START_EVT);
            updateProvPolicy = true;
        }
    }

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}
#endif /* endif for USE_DMM */

/**
 *  @brief Updates the collector's status line
 *
 *  If pNetworkInfo is NULL, cached network information is printed to UART
 */
static void updateCollectorStatusLine(bool restored, Llc_netInfo_t *pNetworkInfo)
{
#if (CONFIG_MAC_BEACON_ORDER != NON_BEACON_ORDER)
    char macMode[4] = "BCN\0";
#elif (CONFIG_FH_ENABLE)
    char macMode[3] = "FH\0";
#else
    char macMode[5] = "NBCN\0";
#endif

    static bool restoredStatus;
    static Llc_netInfo_t networkInfo;
    static char* deviceStatus;
    char* permitJoinStatus;

    /* Network information is not always available when this function is invoked,
     * so cache information upon network startup */
    if(pNetworkInfo != NULL)
    {
        networkInfo.channel = pNetworkInfo->channel;
        networkInfo.devInfo.panID = pNetworkInfo->devInfo.panID;
        networkInfo.devInfo.shortAddress = pNetworkInfo->devInfo.shortAddress;
        networkInfo.fh = pNetworkInfo->fh;

        restoredStatus = restored;

        if(restoredStatus == false)
        {
            deviceStatus = "Started";
        }
        else
        {
            deviceStatus = "Restarted";
        }

    }

    /* Permit join is always on in FH Mode */
    if(permitJoining == false && !CONFIG_FH_ENABLE)
    {
        permitJoinStatus = "Off";
    }
    else
    {
        permitJoinStatus = "On";
    }
#ifndef POWER_MEAS
    CUI_statusLinePrintf(csfCuiHndl, numJoinDevStatusLine, "%x", getNumActiveDevices());

    /* Print out collector status information */
    if(networkInfo.fh == false)
    {

        CUI_statusLinePrintf(csfCuiHndl, collectorStatusLine,
                             "%s--Mode=%s, Addr=0x%04x, PanId=0x%04x, Ch=%d, PermitJoin=%s ",
                             deviceStatus, macMode, networkInfo.devInfo.shortAddress, networkInfo.devInfo.panID,
                             networkInfo.channel, permitJoinStatus);

    }
    else
    {

        CUI_statusLinePrintf(csfCuiHndl, collectorStatusLine,
                             "%s--Mode=%s, Addr=0x%04x, PanId=0x%04x, Ch=FH, PermitJoin=%s ",
                             deviceStatus, macMode, networkInfo.devInfo.shortAddress, networkInfo.devInfo.panID,
                             permitJoinStatus);

    }
#endif /* end for POWER_MEAS */
}

/*
 *  @brief Handles common network starting actions
 */
static void formNwkAndUpdateUser(void)
{
/* Network already formed if AUTO_START defined */
#ifndef AUTO_START
    if(started == false)
    {
#ifndef POWER_MEAS
        CUI_statusLinePrintf(csfCuiHndl, collectorStatusLine, "Starting...");
        CUI_statusLinePrintf(csfCuiHndl, numJoinDevStatusLine, "%x", getNumActiveDevices());
#endif
        /* Tell the collector to start */
        Util_setEvent(&Collector_events, COLLECTOR_START_EVT);
    }
#endif /* AUTO_START */
}

/*
 *  @brief Handles common network opening actions
 *
 *  @param  openNwkRequest - true to open network, false to close
 */
static void openCloseNwkAndUpdateUser(bool openNwkRequest)
{
#ifdef FEATURE_SECURE_COMMISSIONING
    /* Network permission changes not accepted during CM Process */
    if((fCommissionRequired == TRUE) || (SM_Current_State == SM_CM_InProgress))
    {
#ifndef POWER_MEAS
        CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine, "PermitJoin blocked during commissioning");
#endif /* endif for POWER_MEAS */
    }
    else
#endif /* FEATURE_SECURE_COMMISSIONING */
    {
        if(started)
        {
            if (openNwkRequest)
            {
                /* Set event to open network */
                Util_setEvent(&Collector_events, COLLECTOR_OPEN_NWK_EVT);
            }
            else
            {
                /* Set event to close network */
                Util_setEvent(&Collector_events, COLLECTOR_CLOSE_NWK_EVT);
            }
        }
    }
}

#ifndef POWER_MEAS

/*!
 * @brief       The application calls this function to toggle an LED request
 *              and update the user through the CUI.
 *
 * @param       shortAddr - address of the sensor
 *
 */
static void sendToggleAndUpdateUser(uint16_t shortAddr)
{
#ifdef FEATURE_SECURE_COMMISSIONING
   /* LED toggle not accepted during CM Process*/
   if(SM_Current_State != SM_CM_InProgress)
#endif /* endif for FEATURE_SECURE_COMMISSIONING */
   {
        ApiMac_sAddr_t toggleDev;
        toggleDev.addr.shortAddr = shortAddr;
        if(toggleDev.addr.shortAddr != CSF_INVALID_SHORT_ADDR)
        {
            toggleDev.addrMode = ApiMac_addrType_short;
            Collector_sendToggleLedRequest(&toggleDev);
            CUI_statusLinePrintf(csfCuiHndl, deviceStatusLine,
                                 "ToggleLEDRequest Sent - Addr=0x%04x",
                                 toggleDev.addr.shortAddr);
        }
   }
}

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




/**
 *  @brief Callback to be called when the UI sets PAN ID.
 */
static void setPanIdAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo)
{
  static CUI_cursorInfo_t cursor = {0, 6};
  static bool initPanId = true;
  static uint16_t panId;

  const char tmpInput[2] = {_input, '\0'};

  if(initPanId)
  {
      Cllc_getFormingPanId(&panId);
      initPanId = false;
  }

  switch (_input) {
      case CUI_ITEM_INTERCEPT_START:
      {
          Cllc_getFormingPanId(&panId);
          break;
      }
      // Submit the final modified value
      case CUI_ITEM_INTERCEPT_STOP:
      {
          Cllc_setFormingPanId(panId);
          // Reset the local cursor info
          cursor.col = 6;
          break;
      }
      // Get the correct value if there was a network previously.
      // previously initialized in the Waiting state.
      case CUI_ITEM_PREVIEW:
          Cllc_getFormingPanId(&panId);
          break;
      // Move the cursor to the left
      case CUI_INPUT_LEFT:
      {
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
          // get the position of the nibble to change
          uint8_t shift = 4 * (9 - cursor.col);

          // make sure you don't exceed the left boundary
          if (6 <= cursor.col)
          {
              // clear the desired nibble with F
              panId |= (uint32_t)(0x0F<<shift) ;
              cursor.col = moveCursorLeft(cursor.col, 6, 9, 0);
          }

          break;
      }
      case CUI_INPUT_EXECUTE:
          break;
      default:
      {
          // is it a number
          if(CUI_IS_INPUT_HEX(_input))
          {
              // multiply by 4 to use binary
              // get nibble position to change
              uint8_t shift = 4 * (9 - cursor.col);
              // convert from ascii to uint8
              uint8_t digit = strtol(tmpInput, NULL, 16);

              // clear the nibble
              panId &= ~((uint32_t)0xF << shift);
              // set the nibble to desired digit
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
        Cllc_getChanMask(channelMask);
        initChanMask = false;
    }

    switch (_input) {
        case CUI_ITEM_INTERCEPT_START:
        {
            // Reset the local cursor info
            cursor.col = 1;
            Cllc_getChanMask(channelMask);
            break;
        }
        // Submit the final modified value
        case CUI_ITEM_INTERCEPT_STOP:
        {
            Cllc_setChanMask(channelMask);

            // Reset the local cursor info
            cursor.col = 1;
            break;
        }
        // Show the value of this screen w/o making changes
        case CUI_ITEM_PREVIEW:
            Cllc_getChanMask(channelMask);
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
            // get the position of the entire hex byte based on the column you're at
            uint8_t nibbleIdx = (cursor.col-1) - ((cursor.col -1)/3);
            // get the position of the nibble that you'd like to change.
            uint8_t byteIdx = nibbleIdx / 2;

            // make sure you haven't exceeded the boundary
            if (1 <= cursor.col)
            {
                if(cursor.col % 3 == 2)
                {
                    channelMask[byteIdx] |= (uint32_t)(0x0F) ;
                    /* You are at the right side of the byte */
                }
                else if(cursor.col % 3 == 1)
                {
                    /* You are at the left side of the byte
                     * Set the value at the cursor to be F by default
                     * The shift by 4 is to modify only the left
                     * side of the byte. */
                    channelMask[byteIdx] |= (uint32_t)(0x0F<<4) ;
                }

                cursor.col = moveCursorLeft(cursor.col, 1, 50, 1);

            }
            break;
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

        // clear out the UART lines
        strcpy((_pLines[0] + chanMaskStrIdx), " ");
        chanMaskStrIdx += 1;
        strncat((_pLines[0] + chanMaskStrIdx), tmp, 2);
        chanMaskStrIdx += 2;
    }

    if (_input != CUI_ITEM_PREVIEW)
    {
        // copy the label in.
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
        Cllc_getAsyncChanMask(channelMask);
        initChanMask = false;
    }

    switch (_input) {
        case CUI_ITEM_INTERCEPT_START:
        {
            Cllc_getAsyncChanMask(channelMask);
            break;
        }
        // Submit the final modified value
        case CUI_ITEM_INTERCEPT_STOP:
        {
            Cllc_setAsyncChanMask(channelMask);

            // Reset the local cursor info
            cursor.col = 1;
            break;
        }
        // Show the value of this screen w/o making changes
        case CUI_ITEM_PREVIEW:
            Cllc_getAsyncChanMask(channelMask);
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
#endif /* endif for CONFIG_FH_ENABLE */

#ifdef FEATURE_MAC_SECURITY
/**
 *  @brief Callback to be called when the UI sets Default Network Key.
 *  The cursor represents where your keyboard is currently 'pointing' at.
 */
static void setNwkKeyAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo)
{
    static uint8_t defaultNwkKey[APIMAC_KEY_MAX_LEN];
    static bool initChanMask = true;
    /* The particular row the cursor is on */
    static CUI_cursorInfo_t cursor = {0, 1};

    uint8_t defaultNwkKeyByteIdx = 0;
    uint8_t defaultNwkKeyStrIdx = 0;

    const char tmpInput[2] = {_input, '\0'};

    if(initChanMask)
    {
        Cllc_getDefaultKey(defaultNwkKey);
        initChanMask = false;
    }

    switch (_input) {
        case CUI_ITEM_INTERCEPT_START:
        {
            Cllc_getDefaultKey(defaultNwkKey);
            break;
        }
        // Submit the final modified value
        case CUI_ITEM_INTERCEPT_STOP:
        {
            Cllc_setDefaultKey(defaultNwkKey);

            // Reset the local cursor info
            cursor.col = 1;
            break;
        }
        // Show the value of this screen w/o making changes
        case CUI_ITEM_PREVIEW:
            Cllc_getDefaultKey(defaultNwkKey);
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
            // get the byte index that needs to be changed
            uint8_t nibbleIdx = (cursor.col-1) - ((cursor.col -1)/3);
            // get the specific nibble location that needs to be set
            uint8_t byteIdx = nibbleIdx / 2;

            // make sure you don't exceed the boundary
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
            // grab the byte you need to modify
            uint8_t nibbleIdx = (cursor.col-1) - ((cursor.col -1)/3);
            // pick the nibble you need to modify
            uint8_t byteIdx = nibbleIdx / 2;

            // you are at the left side
            if(nibbleIdx % 2 == 0)
                {
                    // First, clear the left side, keep the right side
                    defaultNwkKey[byteIdx] &= (uint32_t)(0x0F);
                    // Next, shift the input left, and or it with the existing side.
                    defaultNwkKey[byteIdx] |= (uint32_t)(strtol(tmpInput, NULL, 16) << 4);
                }
            // you are the right side
            else
                {
                    // First, clear the right side, keep the left side
                    defaultNwkKey[byteIdx] &= (uint32_t)(0xF0);
                    // Next, use the input to or it with the existing side.
                    // strtol will convert from hex ascii to hex integers
                    defaultNwkKey[byteIdx] |= (uint32_t)(strtol(tmpInput, NULL, 16));
                }
            /* Move the cursor forward if the space is the next one */
            cursor.col = moveCursorRight(cursor.col, 1, 47, 1);
        }

        }

    }

    for(defaultNwkKeyByteIdx = 0; defaultNwkKeyByteIdx < APIMAC_KEY_MAX_LEN; defaultNwkKeyByteIdx++)
    {
        char tmp[2];

        uintToString( defaultNwkKey[defaultNwkKeyByteIdx], tmp, 16, 2, TRUE, FALSE);

        // clear the labels
        strcpy((_pLines[0] + defaultNwkKeyStrIdx), " ");
        defaultNwkKeyStrIdx += 1;
        strncat((_pLines[0] + defaultNwkKeyStrIdx), tmp, 2);
        defaultNwkKeyStrIdx += 2;
    }

    if (_input != CUI_ITEM_PREVIEW)
    {
        // set the label in the right place
        strcpy(_pLines[2], "     DEFAULT NWK KEY");
        _pCurInfo->row = 1;
        _pCurInfo->col = cursor.col+1;
    }
}
#endif /* FEATURE_MAC_SECURITY */

#ifdef FEATURE_SECURE_COMMISSIONING

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
#endif /* endif for FEATURE_SECURE_COMMISSIONING */

static void sensorSelectAction(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo)
{
    static CUI_cursorInfo_t cursor = {0, 6};
    static bool initSelectAddr = true;

    const char tmpInput[2] = {_input, '\0'};

    if(initSelectAddr)
    {
        SelectedSensor = 1;
        initSelectAddr = false;
    }

    switch (_input) {
        case CUI_ITEM_INTERCEPT_START:
        {
            break;
        }
        // Submit the final modified value
        case CUI_ITEM_INTERCEPT_STOP:
        {
            // Reset the local cursor info
            cursor.col = 6;
            break;
        }
        // Show the value of this screen w/o making changes
        case CUI_ITEM_PREVIEW:
            break;
        // Move the cursor to the left
        case CUI_INPUT_LEFT:
        {
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
            // multiply by 4 because we are working with binary
            // subtract the current column from the right boundary
            // in order to modify the correct nibble
            uint8_t shift = 4 * (9 - cursor.col);
            // don't exceed the boundary
            if (6 <= cursor.col)
            {
                // set the default of the nibble you want to change to F

                SelectedSensor &= ~((uint32_t)((0x0F<<shift)));
                cursor.col = moveCursorLeft(cursor.col, 6, 9, 0);

            }
            break;
        }
        case CUI_INPUT_EXECUTE:
            break;
        default:
        {
            //is it a hex number
            if(CUI_IS_INPUT_HEX(_input))
            {
                // multiply by 4 because we're working with binary
                // get the specific nibble position that you want to modify
                uint8_t shift = 4 * (9 - cursor.col);

                // Use strtol to convert from ASCII Hex to actual hex
                uint8_t digit = strtol(tmpInput, NULL, 16);

                // clear the nibble first
                SelectedSensor &= ~((uint32_t)0xF << shift);
                // set the nibble to the desired digit
                SelectedSensor |= (uint32_t)digit << shift;

                cursor.col = moveCursorRight(cursor.col, 6, 9, 0);

            }
        }
    }

    if(SelectedSensor == 0x00)
    {
        SelectedSensor = 0x01;
    }

    char tmp[4];
    uintToString( SelectedSensor, tmp, 16, 4, TRUE, FALSE);
    strcpy(_pLines[0], "    0x");
    strncat(_pLines[0], tmp, 4);

    if (_input != CUI_ITEM_PREVIEW) {
        strcpy(_pLines[2], "  SELECTED SENSOR");
        _pCurInfo->row = 1;
        _pCurInfo->col = cursor.col+1;
    }
}

static void sensorSetReportInterval(const char _input, char* _pLines[3], CUI_cursorInfo_t* _pCurInfo)
{
    static CUI_cursorInfo_t cursor = {0, 4};
    static char reportIntervalASCII[9] = "00090000";

    switch (_input) {
        case CUI_ITEM_INTERCEPT_START:
        {
            reportInterval = 0;
            cursor.col = 4;

            break;
        }
        // Submit the final modified value
        case CUI_ITEM_INTERCEPT_STOP:
        {
            Csf_events |= COLLECTOR_SENSOR_ACTION_EVT;
            Csf_sensorAction = SENSOR_ACTION_SET_RPT_INT;

            if(atoi(reportIntervalASCII) == 0x00)
            {
                memcpy(reportIntervalASCII, "00090000", 8);
            }

            reportInterval = atoi(reportIntervalASCII);

            // Wake up the application thread when it waits for clock event
            Semaphore_post(collectorSem);

            break;
        }
        // Show the value of this screen w/o making changes
        case CUI_ITEM_PREVIEW:
            break;
        // Move the cursor to the left
        case CUI_INPUT_LEFT:
            cursor.col = moveCursorLeft(cursor.col, 4, 11, 0);
            break;

        // Move the cursor to the right
        case CUI_INPUT_RIGHT:
            cursor.col = moveCursorRight(cursor.col, 4, 11, 0);
            break;

        case CUI_INPUT_UP:
            break;

        case CUI_INPUT_DOWN:
            break;

        case CUI_INPUT_EXECUTE:
            break;
        case CUI_INPUT_BACK:
            if (4 <= cursor.col)
            {
                // clear the ASCII directly with 0
                reportIntervalASCII[cursor.col-4] = '0' ;
                cursor.col = moveCursorLeft(cursor.col, 4, 11, 0);

            }
            break;
        default:
        {
            //is it a number
            //is it a number
            if(CUI_IS_INPUT_NUM(_input))
            {
                // directly set the ASCII value because the array
                // is ASCII And so is the input
                reportIntervalASCII[cursor.col-4] = _input;

                cursor.col = moveCursorRight(cursor.col, 4, 11, 0);
            }

        }
    }

    // clear the label.
    strcpy(_pLines[0], "    ");
    strncat(_pLines[0], reportIntervalASCII, 8);

    if (_input != CUI_ITEM_PREVIEW) {
        // set hte label in the right place.
        strcpy(_pLines[2], "  REPORT INTERVAL");
        _pCurInfo->row = 1;
        _pCurInfo->col = cursor.col+1;
    }
}

/**
 *  @brief Callback to be called when the UI selects toggle.
 */
static void sensorLedToggleAction(int32_t menuEntryInex)
{
    Csf_events |= COLLECTOR_SENSOR_ACTION_EVT;
    Csf_sensorAction = SENSOR_ACTION_TOGGLE;

    // Wake up the application thread when it waits for clock event
    Semaphore_post(collectorSem);
}

/**
 *  @brief Callback to be called when the UI selects toggle.
 */
static void sensorDisassocAction(int32_t menuEntryInex)
{
    Csf_events |= COLLECTOR_SENSOR_ACTION_EVT;
    Csf_sensorAction = SENSOR_ACTION_DISASSOC;

    // Wake up the application thread when it waits for clock event
    Semaphore_post(collectorSem);
}


#if defined(DEVICE_TYPE_MSG)
/**
 *  @brief Callback to be called when the UI selects toggle.
 */
static void sensorDeviceTypeRequestAction(int32_t menuEntryInex)
{
    Csf_events |= COLLECTOR_SENSOR_ACTION_EVT;
    Csf_sensorAction = SENSOR_ACTION_DEVICE_TYPE_REQ;

    // Wake up the application thread when it waits for clock event
    Semaphore_post(collectorSem);
}
#endif /* DEVICE_TYPE_MSG */

/**
 *  @brief Callback to be called when the UI forms network.
 */
static void formNwkAction(int32_t menuEntryInex)
{
    /* Tell the collector to start network */
    formNwkAndUpdateUser();

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

/**
 *  @brief Callback to be called when the UI opens network.
 */
static void openNwkAction(int32_t menuEntryInex)
{
    /* Open network */
    openCloseNwkAndUpdateUser(true);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

/**
 *  @brief Callback to be called when the UI closes network.
 */
static void closeNwkAction(int32_t menuEntryInex)
{
    /* Close network */
    openCloseNwkAndUpdateUser(false);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}


/**
 *  @brief Send process menu event.
 */
static void processMenuUpdate(void)
{
    Csf_events |= COLLECTOR_UI_INPUT_EVT;

    // Wake up the application thread when it waits for clock event
    Semaphore_post(collectorSem);
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
#endif /* POWER_MEAS */

