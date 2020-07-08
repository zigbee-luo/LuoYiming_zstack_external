/**************************************************************************************************
  Filename:       ota_server.c
  Revised:        $Date: 2014-10-24 16:04:46 -0700 (Fri, 24 Oct 2014) $
  Revision:       $Revision: 40796 $


  Description:    Zigbee Cluster Library - sample light application.


  Copyright 2006-2014 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
**************************************************************************************************/

/*********************************************************************
  This application implements a ZigBee Light, based on Z-Stack 3.0. It can be configured as an
  On/Off light or as a dimmable light, by undefining or defining ZCL_LEVEL_CTRL, respectively.

  This application is based on the common sample-application user interface. Please see the main
  comment in zcl_sampleapp_ui.c. The rest of this comment describes only the content specific for
  this sample application.

  Application-specific UI peripherals being used:

  - LEDs:
    LED1 reflect the current light state (On / Off accordingly).

  Application-specific menu system:

    <TOGGLE LIGHT> Toggle the local light and display its status and level
      Press OK to toggle the local light on and off.
      This screen shows the following information
        Line1: (only populated if ZCL_LEVEL_CTRL is defined)
          LEVEL XXX - xxx is the current level of the light if the light state is ON, or the target level
            of the light when the light state is off. The target level is the level that the light will be
            set to when it is switched from off to on using the on or the toggle commands.
        Line2:
          LIGHT OFF / ON: shows the current state of the light.
      Note when ZCL_LEVEL_CTRL is enabled:
        - If the light state is ON and the light level is X, and then the light receives the OFF or TOGGLE
          commands: The level will decrease gradually until it reaches 1, and only then the light state will
          be changed to OFF. The level then will be restored to X, with the state staying OFF. At this stage
          the light is not lighting, and the level represent the target level for the next ON or TOGGLE
          commands.
        - If the light state is OFF and the light level is X, and then the light receives the ON or TOGGLE
          commands; The level will be set to 1, the light state will be set to ON, and then the level will
          increase gradually until it reaches level X.
        - Any level-setting command will affect the level directly, and may also affect the on/off state,
          depending on the command's arguments.

*********************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "zcomdef.h"
#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"
#include "zcl_ota.h"
#include "zcl_diagnostic.h"

#include "ota_server.h"
#include <string.h>
#include "zcl_sampleapps_ui.h"
#include "bdb_interface.h"
#include "ota_common.h"
#include "ota_srv_app.h"

#include "ti_drivers_config.h"
#include <ti/drivers/apps/Button.h>
#include "nvintf.h"
#include "zstackmsg.h"
#include "zcl_port.h"

#include <ti/sysbios/knl/Semaphore.h>
#include "zstackapi.h"
#include "util_timer.h"
#include "nl_mede.h"
#include "npi_data.h"

#include "npi_data.h"
#include "npi_task.h"

#include "mt.h"
#include "mt_af.h"
#include "mt_rpc.h"
#include "mt_sys.h"

#include "mt_util.h"
#include "mt_zdo.h"
#include "mt_nwk.h"
#include "mt_app.h"
#include "mt_ota.h"
#include "string.h"

/*********************************************************************
 * MACROS
 */
#define APP_TITLE "OTA Server"

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
uint8_t OTA_Dongle_SeqNo;
devStates_t OTA_Dongle_devState;

/*********************************************************************
 * GLOBAL FUNCTIONS
 */

uint8_t appTask_getServiceTaskID(void);

/*********************************************************************
 * LOCAL VARIABLES
 */
// Semaphore used to post events to the application thread
static Semaphore_Handle appSemHandle;
static Semaphore_Struct appSem;
/* App service ID used for messaging with stack service task */
static uint8_t  appServiceTaskId;
/* App service task events, set by the stack service task when sending a message */
static uint32_t appServiceTaskEvents;
static endPointDesc_t  zclOtaEpDesc = {0};
static endPointDesc_t  zclSysAppEpDesc = {0};

#if ZG_BUILD_ENDDEVICE_TYPE
static Clock_Handle EndDeviceRejoinClkHandle;
static Clock_Struct EndDeviceRejoinClkStruct;
#endif
static Clock_Handle OtaServerNotifyClkHandle;
static Clock_Struct OtaServerNotifyClkStruct;

// Passed in function pointers to the NV driver
static NVINTF_nvFuncts_t *pfnZdlNV = NULL;

// Key press parameters
static Button_Handle keys = NULL;

afAddrType_t otaServer_DstAddr;

static Button_Handle gRightButtonHandle;
static Button_Handle gLeftButtonHandle;
/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void otaServer_initialization(void);
static void otaServer_process_loop(void);
static void otaServer_initParameters(void);
static void otaServer_processZStackMsgs(zstackmsg_genericReq_t *pMsg);
static void SetupZStackCallbacks(void);
static void otaServer_processAfIncomingMsgInd(zstack_afIncomingMsgInd_t *pInMsg);
static void otaServer_initializeClocks(void);
#if ZG_BUILD_ENDDEVICE_TYPE
static void otaServer_processEndDeviceRejoinTimeoutCallback(UArg a0);
#endif
static void otaServer_sendNotifylTimeoutCallback(UArg a0);
static void otaServer_changeKeyCallback(Button_Handle _btn, Button_EventMask _buttonEvents);
static void otaServer_processKey(Button_Handle keysPressed);
static void otaServer_Init( void );

static void otaServer_BasicResetCB( void );

static void OTA_ProcSysAppMsg(zstack_sysAppMsg_t *pMsg);
static void OTA_ProcessSysApp_ImageNotifyReq(uint8_t *pData);
static void OTA_ProcessSysApp_ReadAttrReq(uint8_t *pData);
static void OTA_ProcessSysApp_DiscoveryReq(uint8_t *pData);
static void OTA_ProcessSysApp_JoinReq(uint8_t *pData);

static void OTA_Send_DeviceInd(uint16_t shortAddr);
static void OTA_Send_JoinInd(void);
static void OTA_Send_ReadAttrInd(uint16_t cluster, uint16_t shortAddr, zclReadRspStatus_t *pAttr);
static void OTA_Send_EndpointInd(uint16_t addr, uint8_t endpoint);
static void OTA_Send_DongleInd(void);

static void otaServer_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg);

// Functions to process ZCL Foundation incoming Command/Response messages
static uint8_t otaServer_ProcessIncomingMsg( zclIncoming_t *msg );
#ifdef ZCL_READ
static uint8_t otaServer_ProcessInReadRspCmd( zclIncoming_t *pInMsg );
#endif
#ifdef ZCL_WRITE
static uint8_t otaServer_ProcessInWriteRspCmd( zclIncoming_t *pInMsg );
#endif
static uint8_t otaServer_ProcessInDefaultRspCmd( zclIncoming_t *pInMsg );
#ifdef ZCL_DISCOVER
static uint8_t otaServer_ProcessInDiscCmdsRspCmd( zclIncoming_t *pInMsg );
static uint8_t otaServer_ProcessInDiscAttrsRspCmd( zclIncoming_t *pInMsg );
static uint8_t otaServer_ProcessInDiscAttrsExtRspCmd( zclIncoming_t *pInMsg );
#endif


static void Initialize_UI(void);

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * REFERENCED EXTERNALS
 */
extern int16_t zdpExternalStateTaskID;

/*********************************************************************
 * ZCL General Profile Callback table
 */
static zclGeneral_AppCallbacks_t otaServer_CmdCallbacks =
{
  otaServer_BasicResetCB,                 // Basic Cluster Reset command
  NULL,                                   // Identfiy cmd
  NULL,                                   // Identify Query command
  NULL,                                   // Identify Query Response command
  NULL,                                   // Identify Trigger Effect command
#ifdef ZCL_ON_OFF
  NULL,                                   // On/Off cluster commands
  NULL,                                   // On/Off cluster enhanced command Off with Effect
  NULL,                                   // On/Off cluster enhanced command On with Recall Global Scene
  NULL,                                   // On/Off cluster enhanced command On with Timed Off
#endif
#ifdef ZCL_GROUPS
  NULL,                                   // Group Response commands
#endif
  NULL,                                  // RSSI Location command
  NULL                                   // RSSI Location Response command
};


/*******************************************************************************
 * @fn          sampleApp_task
 *
 * @brief       Application task entry point for the Z-Stack
 *              Sample Application
 *
 * @param       pfnNV - pointer to the NV functions
 *
 * @return      none
 */
void sampleApp_task(NVINTF_nvFuncts_t *pfnNV)
{
  // Save and register the function pointers to the NV drivers
  pfnZdlNV = pfnNV;
  zclport_registerNV(pfnZdlNV, ZCL_PORT_SCENE_TABLE_NV_ID);

  // Initialize application
  otaServer_initialization();

  // No return from task process
  otaServer_process_loop();
}



/*******************************************************************************
 * @fn          otaServer_initialization
 *
 * @brief       Initialize the application
 *
 * @param       none
 *
 * @return      none
 */
static void otaServer_initialization(void)
{

    /* Initialize user clocks */
    otaServer_initializeClocks();

    Initialize_UI();

    /* create semaphores for messages / events
     */
    Semaphore_Params semParam;
    Semaphore_Params_init(&semParam);
    semParam.mode = ti_sysbios_knl_Semaphore_Mode_COUNTING;
    Semaphore_construct(&appSem, 0, &semParam);
    appSemHandle = Semaphore_handle(&appSem);

    appServiceTaskId = OsalPort_registerTask(Task_self(), appSemHandle, &appServiceTaskEvents);

    //Initialize stack
    otaServer_Init();

    OTA_Server_Init(appServiceTaskId);
}



/*******************************************************************************
 * @fn      SetupZStackCallbacks
 *
 * @brief   Setup the Zstack Callbacks wanted
 *
 * @param   none
 *
 * @return  none
 */
static void SetupZStackCallbacks(void)
{
    zstack_devZDOCBReq_t zdoCBReq = {0};

    // Register for Callbacks, turn on:
    //  Device State Change,
    //  ZDO Match Descriptor Response,
    zdoCBReq.has_devStateChange = true;
    zdoCBReq.devStateChange = true;

    zdoCBReq.has_deviceAnnounce = true;
    zdoCBReq.deviceAnnounce = true;

    zdoCBReq.has_matchDescRsp = true;
    zdoCBReq.matchDescRsp = true;


    (void)Zstackapi_DevZDOCBReq(appServiceTaskId, &zdoCBReq);
}



/*********************************************************************
 * @fn          otaServer_Init
 *
 * @brief       Initialization function for the zclGeneral layer.
 *
 * @param       none
 *
 * @return      none
 */
static void otaServer_Init( void )
{

  // Set destination address to indirect
  otaServer_DstAddr.addrMode = (afAddrMode_t)AddrNotPresent;
  otaServer_DstAddr.endPoint = 0;
  otaServer_DstAddr.addr.shortAddr = 0;

  //Register Endpoint
  zclOtaEpDesc.endPoint = OTA_DONGLE_ENDPOINT;
  zclOtaEpDesc.simpleDesc = &otaServerEpDesc;
  zclport_registerEndpoint(appServiceTaskId, &zclOtaEpDesc);

  //Register Endpoint
  zclSysAppEpDesc.endPoint = OTA_SYSAPP_ENDPOINT;
  zclSysAppEpDesc.simpleDesc = &otaServerSysAppEpDesc;
  zclport_registerEndpoint(appServiceTaskId, &zclSysAppEpDesc);



  // Register the ZCL General Cluster Library callback functions
  zclGeneral_RegisterCmdCallbacks( OTA_DONGLE_ENDPOINT, &otaServer_CmdCallbacks );

  // Register the application's attribute list and reset to default values
  zcl_registerAttrList( OTA_DONGLE_ENDPOINT, otaServer_NumAttributes, otaServer_Attrs );

  // Register the Application to receive the unprocessed Foundation command/response messages
  zclport_registerZclHandleExternal( OTA_DONGLE_ENDPOINT, otaServer_ProcessIncomingMsg );

  //Write the bdb initialization parameters
  otaServer_initParameters();

  //Setup ZDO callbacks
  SetupZStackCallbacks();

#ifdef ZCL_DISCOVER
  // Register the application's command list
  zcl_registerCmdList( OTA_DONGLE_ENDPOINT, zclCmdsArraySize, otaServer_Cmds );
#endif


  Timer_setTimeout( OtaServerNotifyClkHandle, OTA_SEND_NOTIFY_TIMEOUT );
  Timer_start(&OtaServerNotifyClkStruct);
}

static void otaServer_initParameters(void)
{
    zstack_bdbSetAttributesReq_t zstack_bdbSetAttrReq;

    zstack_bdbSetAttrReq.bdbCommissioningGroupID              = BDB_DEFAULT_COMMISSIONING_GROUP_ID;
    zstack_bdbSetAttrReq.bdbPrimaryChannelSet                 = BDB_DEFAULT_PRIMARY_CHANNEL_SET;
    zstack_bdbSetAttrReq.bdbScanDuration                      = BDB_DEFAULT_SCAN_DURATION;
    zstack_bdbSetAttrReq.bdbSecondaryChannelSet               = BDB_DEFAULT_SECONDARY_CHANNEL_SET;
    zstack_bdbSetAttrReq.has_bdbCommissioningGroupID          = TRUE;
    zstack_bdbSetAttrReq.has_bdbPrimaryChannelSet             = TRUE;
    zstack_bdbSetAttrReq.has_bdbScanDuration                  = TRUE;
    zstack_bdbSetAttrReq.has_bdbSecondaryChannelSet           = TRUE;
#if (ZG_BUILD_COORDINATOR_TYPE)
    zstack_bdbSetAttrReq.has_bdbJoinUsesInstallCodeKey        = TRUE;
    zstack_bdbSetAttrReq.has_bdbTrustCenterNodeJoinTimeout    = TRUE;
    zstack_bdbSetAttrReq.has_bdbTrustCenterRequireKeyExchange = TRUE;
    zstack_bdbSetAttrReq.bdbJoinUsesInstallCodeKey            = BDB_DEFAULT_JOIN_USES_INSTALL_CODE_KEY;
    zstack_bdbSetAttrReq.bdbTrustCenterNodeJoinTimeout        = BDB_DEFAULT_TC_NODE_JOIN_TIMEOUT;
    zstack_bdbSetAttrReq.bdbTrustCenterRequireKeyExchange     = BDB_DEFAULT_TC_REQUIRE_KEY_EXCHANGE;
#endif
#if (ZG_BUILD_JOINING_TYPE)
    zstack_bdbSetAttrReq.has_bdbTCLinkKeyExchangeAttemptsMax  = TRUE;
    zstack_bdbSetAttrReq.has_bdbTCLinkKeyExchangeMethod       = TRUE;
    zstack_bdbSetAttrReq.bdbTCLinkKeyExchangeAttemptsMax      = BDB_DEFAULT_TC_LINK_KEY_EXCHANGE_ATTEMPS_MAX;
    zstack_bdbSetAttrReq.bdbTCLinkKeyExchangeMethod           = BDB_DEFAULT_TC_LINK_KEY_EXCHANGE_METHOD;
#endif

    Zstackapi_bdbSetAttributesReq(appServiceTaskId, &zstack_bdbSetAttrReq);

  // Call BDB initialization. Should be called once from application at startup to restore
  // previous network configuration, if applicable.
  zstack_bdbStartCommissioningReq_t zstack_bdbStartCommissioningReq;
  zstack_bdbStartCommissioningReq.commissioning_mode = 0;
  Zstackapi_bdbStartCommissioningReq(appServiceTaskId,&zstack_bdbStartCommissioningReq);
}

/*******************************************************************************
 * @fn      otaServer_initializeClocks
 *
 * @brief   Initialize Clocks
 *
 * @param   none
 *
 * @return  none
 */
static void otaServer_initializeClocks(void)
{
#if ZG_BUILD_ENDDEVICE_TYPE
    // Initialize the timers needed for this application
    EndDeviceRejoinClkHandle = Timer_construct(
    &EndDeviceRejoinClkStruct,
    otaServer_processEndDeviceRejoinTimeoutCallback,
    SAMPLEAPP_END_DEVICE_REJOIN_DELAY,
    0, false, 0);
#endif
    OtaServerNotifyClkHandle = Timer_construct(
    &OtaServerNotifyClkStruct,
    otaServer_sendNotifylTimeoutCallback,
    OTA_SEND_NOTIFY_TIMEOUT,
    0, false, 0);
}

#if ZG_BUILD_ENDDEVICE_TYPE
/*******************************************************************************
 * @fn      otaServer_processEndDeviceRejoinTimeoutCallback
 *
 * @brief   Timeout handler function
 *
 * @param   a0 - ignored
 *
 * @return  none
 */
static void otaServer_processEndDeviceRejoinTimeoutCallback(UArg a0)
{
    (void)a0; // Parameter is not used

    appServiceTaskEvents |= SAMPLEAPP_END_DEVICE_REJOIN_EVT;

    // Wake up the application thread when it waits for clock event
    Semaphore_post(sem);
}
#endif

/*******************************************************************************
 * @fn      otaServer_sendNotifylTimeoutCallback
 *
 * @brief   Timeout handler function
 *
 * @param   a0 - ignored
 *
 * @return  none
 */
static void otaServer_sendNotifylTimeoutCallback(UArg a0)
{
    (void)a0; // Parameter is not used

    appServiceTaskEvents |= SAMPLEAPP_OTA_SERVER_NOTIFY_EVT;

    // Wake up the application thread when it waits for clock event
    Semaphore_post(appSemHandle);
}

/*******************************************************************************
 * @fn      otaServer_process_loop
 *
 * @brief   Application task processing start.
 *
 * @param   none
 *
 * @return  void
 */
static void otaServer_process_loop(void)
{
    /* Forever loop */
    for(;;)
    {
        zstackmsg_genericReq_t *pMsg = NULL;
        bool msgProcessed = FALSE;

        /* Wait for response message */
        if(Semaphore_pend(appSemHandle, BIOS_WAIT_FOREVER ))
        {
            /* Retrieve the response message */
            if( (pMsg = (zstackmsg_genericReq_t*) OsalPort_msgReceive( appServiceTaskId )) != NULL)
            {
                otaServer_processZStackMsgs(pMsg);

                // Free any separately allocated memory
                msgProcessed = Zstackapi_freeIndMsg(pMsg);
            }

            if((msgProcessed == FALSE) && (pMsg != NULL))
            {
                OsalPort_msgDeallocate((uint8_t*)pMsg);
            }


            if(appServiceTaskEvents & SAMPLEAPP_KEY_EVT)
            {
                // Process Key Presses
                otaServer_processKey(keys);
                keys = NULL;
                appServiceTaskEvents &= ~SAMPLEAPP_KEY_EVT;
            }


#if ZG_BUILD_ENDDEVICE_TYPE
            if ( appServiceTaskEvents & SAMPLEAPP_END_DEVICE_REJOIN_EVT )
            {
              zstack_bdbZedAttemptRecoverNwkRsp_t zstack_bdbZedAttemptRecoverNwkRsp;

              Zstackapi_bdbZedAttemptRecoverNwkReq(appServiceTaskId,&zstack_bdbZedAttemptRecoverNwkRsp);

              appServiceTaskEvents &= ~SAMPLEAPP_END_DEVICE_REJOIN_EVT;
            }
#endif
            if ( appServiceTaskEvents & SAMPLEAPP_OTA_SERVER_NOTIFY_EVT )
            {
               OTA_Send_DongleInd();
              Timer_setTimeout( OtaServerNotifyClkHandle, OTA_SEND_NOTIFY_TIMEOUT );
              Timer_start(&OtaServerNotifyClkStruct);

              appServiceTaskEvents &= ~SAMPLEAPP_OTA_SERVER_NOTIFY_EVT;
            }
        }
    }
}



/*******************************************************************************
 * @fn      otaServer_processZStackMsgs
 *
 * @brief   Process event from Stack
 *
 * @param   pMsg - pointer to incoming ZStack message to process
 *
 * @return  void
 */
static void otaServer_processZStackMsgs(zstackmsg_genericReq_t *pMsg)
{
    switch(pMsg->hdr.event)
    {
        case zstackmsg_CmdIDs_BDB_NOTIFICATION:
            {
                zstackmsg_bdbNotificationInd_t *pInd;
                pInd = (zstackmsg_bdbNotificationInd_t*)pMsg;
                otaServer_ProcessCommissioningStatus(&(pInd->Req));
            }
            break;

        case zstackmsg_CmdIDs_BDB_IDENTIFY_TIME_CB:

            break;

        case zstackmsg_CmdIDs_BDB_BIND_NOTIFICATION_CB:

            break;

        case zstackmsg_CmdIDs_AF_INCOMING_MSG_IND:
            {
                // Process incoming data messages
                zstackmsg_afIncomingMsgInd_t *pInd;
                pInd = (zstackmsg_afIncomingMsgInd_t *)pMsg;
                otaServer_processAfIncomingMsgInd( &(pInd->req) );
            }
            break;

        case zstackmsg_CmdIDs_DEV_PERMIT_JOIN_IND:
            break;


#if (ZG_BUILD_JOINING_TYPE)
        case zstackmsg_CmdIDs_BDB_CBKE_TC_LINK_KEY_EXCHANGE_IND:
        {
          zstack_bdbCBKETCLinkKeyExchangeAttemptReq_t zstack_bdbCBKETCLinkKeyExchangeAttemptReq;
          /* Z3.0 has not defined CBKE yet, so lets attempt default TC Link Key exchange procedure
           * by reporting CBKE failure.
           */

          zstack_bdbCBKETCLinkKeyExchangeAttemptReq.didSuccess = FALSE;

          Zstackapi_bdbCBKETCLinkKeyExchangeAttemptReq(appServiceTaskId,
                                                       &zstack_bdbCBKETCLinkKeyExchangeAttemptReq);
        }
        break;

        case zstackmsg_CmdIDs_BDB_FILTER_NWK_DESCRIPTOR_IND:

         /*   User logic to remove networks that do not want to join
          *   Networks to be removed can be released with Zstackapi_bdbNwkDescFreeReq
          */

          Zstackapi_bdbFilterNwkDescComplete(appServiceTaskId);
        break;

#endif
        case zstackmsg_CmdIDs_DEV_STATE_CHANGE_IND:
        {
            // The ZStack Thread is indicating a State change
            zstackmsg_devStateChangeInd_t *pInd;
            pInd = (zstackmsg_devStateChangeInd_t *)pMsg;

            OTA_Dongle_devState = (devStates_t)pInd->req.state;

            if ((OTA_Dongle_devState == DEV_END_DEVICE) || (OTA_Dongle_devState == DEV_ROUTER) || (OTA_Dongle_devState == DEV_ZB_COORD))
            {
              OTA_Send_JoinInd();
            }
        }
        break;

        case zstackmsg_CmdIDs_ZDO_DEVICE_ANNOUNCE:
        {
          zstackmsg_zdoDeviceAnnounceInd_t *pInd;
          pInd = (zstackmsg_zdoDeviceAnnounceInd_t *)pMsg;

          OTA_Send_DeviceInd(pInd->req.devAddr);
        }
        break;

        case zstackmsg_CmdIDs_ZDO_MATCH_DESC_RSP:
        {
          zstackmsg_zdoMatchDescRspInd_t *pInd;
          pInd = (zstackmsg_zdoMatchDescRspInd_t *)pMsg;

          OTA_Send_EndpointInd(pInd->rsp.nwkAddrOfInterest, pInd->rsp.pMatchList[0]);
        }
        break;

        case zstackmsg_CmdIDs_SYS_APP_MSG_REQ:
        {
          zstackmsg_sysAppMsg_t *pInd;
          pInd = (zstackmsg_sysAppMsg_t*)pMsg;

          OTA_ProcSysAppMsg(&pInd->Req);
        }
        break;

        case zstackmsg_CmdIDs_SYS_OTA_MSG_REQ:
        {
          zstackmsg_sysOtaMsg_t *pInd;
          pInd = (zstackmsg_sysOtaMsg_t*)pMsg;

          zclOTA_ServerHandleFileSysCb((uint8_t*)pInd->Req.pData);   //pData also includes cmdId at the beggining of the buffer

        }
        break;

        /*
         * These are messages/indications from ZStack that this
         * application doesn't process.  These message can be
         * processed by your application, remove from this list and
         * process them here in this switch statement.
         */

        case zstackmsg_CmdIDs_BDB_TC_LINK_KEY_EXCHANGE_NOTIFICATION_IND:
        case zstackmsg_CmdIDs_AF_DATA_CONFIRM_IND:
        case zstackmsg_CmdIDs_ZDO_NWK_ADDR_RSP:
        case zstackmsg_CmdIDs_ZDO_IEEE_ADDR_RSP:
        case zstackmsg_CmdIDs_ZDO_NODE_DESC_RSP:
        case zstackmsg_CmdIDs_ZDO_POWER_DESC_RSP:
        case zstackmsg_CmdIDs_ZDO_SIMPLE_DESC_RSP:
        case zstackmsg_CmdIDs_ZDO_ACTIVE_EP_RSP:
        case zstackmsg_CmdIDs_ZDO_COMPLEX_DESC_RSP:
        case zstackmsg_CmdIDs_ZDO_USER_DESC_RSP:
        case zstackmsg_CmdIDs_ZDO_USER_DESC_SET_RSP:
        case zstackmsg_CmdIDs_ZDO_SERVER_DISC_RSP:
        case zstackmsg_CmdIDs_ZDO_END_DEVICE_BIND_RSP:
        case zstackmsg_CmdIDs_ZDO_BIND_RSP:
        case zstackmsg_CmdIDs_ZDO_UNBIND_RSP:
        case zstackmsg_CmdIDs_ZDO_MGMT_NWK_DISC_RSP:
        case zstackmsg_CmdIDs_ZDO_MGMT_LQI_RSP:
        case zstackmsg_CmdIDs_ZDO_MGMT_RTG_RSP:
        case zstackmsg_CmdIDs_ZDO_MGMT_BIND_RSP:
        case zstackmsg_CmdIDs_ZDO_MGMT_LEAVE_RSP:
        case zstackmsg_CmdIDs_ZDO_MGMT_DIRECT_JOIN_RSP:
        case zstackmsg_CmdIDs_ZDO_MGMT_PERMIT_JOIN_RSP:
        case zstackmsg_CmdIDs_ZDO_MGMT_NWK_UPDATE_NOTIFY:
        case zstackmsg_CmdIDs_ZDO_SRC_RTG_IND:
        case zstackmsg_CmdIDs_ZDO_CONCENTRATOR_IND:
        case zstackmsg_CmdIDs_ZDO_LEAVE_CNF:
        case zstackmsg_CmdIDs_ZDO_LEAVE_IND:
        case zstackmsg_CmdIDs_SYS_RESET_IND:
        case zstackmsg_CmdIDs_AF_REFLECT_ERROR_IND:
        case zstackmsg_CmdIDs_ZDO_TC_DEVICE_IND:
            break;

        default:
            break;
    }
}



/*******************************************************************************
 *
 * @fn          otaServer_processAfIncomingMsgInd
 *
 * @brief       Process AF Incoming Message Indication message
 *
 * @param       pInMsg - pointer to incoming message
 *
 * @return      none
 *
 */
static void otaServer_processAfIncomingMsgInd(zstack_afIncomingMsgInd_t *pInMsg)
{
    afIncomingMSGPacket_t afMsg;

    /*
     * All incoming messages are passed to the ZCL message processor,
     * first convert to a structure that ZCL can process.
     */
    afMsg.groupId = pInMsg->groupID;
    afMsg.clusterId = pInMsg->clusterId;
    afMsg.srcAddr.endPoint = pInMsg->srcAddr.endpoint;
    afMsg.srcAddr.panId = pInMsg->srcAddr.panID;
    afMsg.srcAddr.addrMode = (afAddrMode_t)pInMsg->srcAddr.addrMode;
    if( (afMsg.srcAddr.addrMode == afAddr16Bit)
        || (afMsg.srcAddr.addrMode == afAddrGroup)
        || (afMsg.srcAddr.addrMode == afAddrBroadcast) )
    {
        afMsg.srcAddr.addr.shortAddr = pInMsg->srcAddr.addr.shortAddr;
    }
    else if(afMsg.srcAddr.addrMode == afAddr64Bit)
    {
        OsalPort_memcpy(afMsg.srcAddr.addr.extAddr, &(pInMsg->srcAddr.addr.extAddr), 8);
    }
    afMsg.macDestAddr = pInMsg->macDestAddr;
    afMsg.endPoint = pInMsg->endpoint;
    afMsg.wasBroadcast = pInMsg->wasBroadcast;
    afMsg.LinkQuality = pInMsg->linkQuality;
    afMsg.correlation = pInMsg->correlation;
    afMsg.rssi = pInMsg->rssi;
    afMsg.SecurityUse = pInMsg->securityUse;
    afMsg.timestamp = pInMsg->timestamp;
    afMsg.nwkSeqNum = pInMsg->nwkSeqNum;
    afMsg.macSrcAddr = pInMsg->macSrcAddr;
    afMsg.radius = pInMsg->radius;
    afMsg.cmd.DataLength = pInMsg->n_payload;
    afMsg.cmd.Data = pInMsg->pPayload;

    zcl_ProcessMessageMSG(&afMsg);
}


/*********************************************************************
 * @fn      otaServer_ProcessCommissioningStatus
 *
 * @brief   Callback in which the status of the commissioning process are reported
 *
 * @param   bdbCommissioningModeMsg - Context message of the status of a commissioning process
 *
 * @return  none
 */
static void otaServer_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg)
{
  switch(bdbCommissioningModeMsg->bdbCommissioningMode)
  {
    case BDB_COMMISSIONING_FORMATION:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //YOUR JOB:
      }
      else
      {
        //Want to try other channels?
        //try with bdb_setChannelAttribute
      }
    break;
    case BDB_COMMISSIONING_NWK_STEERING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //YOUR JOB:
        //We are on the nwk, what now?
      }
      else
      {
        //See the possible errors for nwk steering procedure
        //No suitable networks found
        //Want to try other channels?
        //try with bdb_setChannelAttribute
      }
    break;
    case BDB_COMMISSIONING_FINDING_BINDING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //YOUR JOB:
      }
      else
      {
        //YOUR JOB:
        //retry?, wait for user interaction?
      }
    break;
    case BDB_COMMISSIONING_INITIALIZATION:
      //Initialization notification can only be successful. Failure on initialization
      //only happens for ZED and is notified as BDB_COMMISSIONING_PARENT_LOST notification

      //YOUR JOB:
      //We are on a network, what now?

    break;
#if ZG_BUILD_ENDDEVICE_TYPE
    case BDB_COMMISSIONING_PARENT_LOST:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_NETWORK_RESTORED)
      {
        //We did recover from losing parent
      }
      else
      {
        //Parent not found, attempt to rejoin again after a fixed delay
        Timer_setTimeout( EndDeviceRejoinClkHandle, SAMPLEAPP_END_DEVICE_REJOIN_DELAY );
        Timer_start(&EndDeviceRejoinClkStruct);
      }
    break;
#endif
  }
}



/*********************************************************************
 * @fn      otaServer_BasicResetCB
 *
 * @brief   Callback from the ZCL General Cluster Library
 *          to set all the Basic Cluster attributes to default values.
 *
 * @param   none
 *
 * @return  none
 */
static void otaServer_BasicResetCB( void )
{
  //Reset every attribute in all supported cluster to their default value.

  otaServer_ResetAttributesToDefaultValues();

}

/*********************************************************************
 * @fn      OTA_ProcSysAppMsg
 *
 * @brief   Handles sys app messages from the server application.
 *
 * @param   pMsg - The message from the server.
 *
 * @return  none
 */
void OTA_ProcSysAppMsg(zstack_sysAppMsg_t *pMsg)
{
  uint8_t cmd;

  if (pMsg == NULL)
    return;

  cmd = *pMsg->pAppData++;

  switch(cmd)
  {
  case OTA_APP_READ_ATTRIBUTE_REQ:
    OTA_ProcessSysApp_ReadAttrReq(pMsg->pAppData);
    break;
  case OTA_APP_IMAGE_NOTIFY_REQ:
    OTA_ProcessSysApp_ImageNotifyReq(pMsg->pAppData);
    break;
  case OTA_APP_DISCOVERY_REQ:
    OTA_ProcessSysApp_DiscoveryReq(pMsg->pAppData);
    break;
  case OTA_APP_JOIN_REQ:
    OTA_ProcessSysApp_JoinReq(pMsg->pAppData);
    break;
  case OTA_APP_LEAVE_REQ:
    // Simulate a leave by rebooting the dongle
    //SystemReset();
  default:
    break;
  }
}

/*********************************************************************
 * @fn      OTA_ProcessSysApp_ImageNotifyReq
 *
 * @brief   Handles app messages from the console application.
 *
 * @param   pData - The data from the server.
 *
 * @return  none
 */
void OTA_ProcessSysApp_ImageNotifyReq(uint8_t *pData)
{
  zclOTA_ImageNotifyParams_t imgNotifyParams;
  afAddrType_t dstAddr;

  // Setup the destination address
  dstAddr.addr.shortAddr = BUILD_UINT16(pData[0], pData[1]);
  dstAddr.endPoint = pData[2];
  dstAddr.panId = _NIB.nwkPanId;

  if(NWK_BROADCAST_SHORTADDR_DEVZCZR <= dstAddr.addr.shortAddr)
  {
    dstAddr.addrMode = afAddrBroadcast;
  }
  else
  {
    dstAddr.addrMode = afAddr16Bit;
  }

  // Fill the Send Image Notify Parameters
  imgNotifyParams.payloadType = pData[3];
  imgNotifyParams.queryJitter = pData[4];
  imgNotifyParams.fileId.manufacturer = BUILD_UINT16(pData[5], pData[6]);
  imgNotifyParams.fileId.type = BUILD_UINT16(pData[7], pData[8]);
  imgNotifyParams.fileId.version = BUILD_UINT32(pData[9], pData[10], pData[11], pData[12]);

  // Send the command
  zclOTA_SendImageNotify(ZCL_OTA_ENDPOINT, &dstAddr, &imgNotifyParams);
}

/*********************************************************************
 * @fn      OTA_ProcessSysApp_ReadAttrReq
 *
 * @brief   Handles app messages from the console application.
 *
 * @param   pData - The data from the server.
 *
 * @return  none
 */
void OTA_ProcessSysApp_ReadAttrReq(uint8_t *pData)
{
  uint8_t readCmd[sizeof(zclReadCmd_t) + sizeof(uint16_t) * OTA_APP_MAX_ATTRIBUTES];
  zclReadCmd_t *pReadCmd = (zclReadCmd_t*) readCmd;
  afAddrType_t dstAddr;
  uint16_t cluster;
  int8_t i;

  // Setup the destination address
  dstAddr.addr.shortAddr = BUILD_UINT16(pData[0], pData[1]);
  dstAddr.endPoint = pData[2];
  dstAddr.addrMode = afAddr16Bit;
  dstAddr.panId = _NIB.nwkPanId;

  // Fill the Send Image Notify Parameters
  cluster = BUILD_UINT16(pData[3], pData[4]);
  pReadCmd->numAttr = pData[5];

  if (pReadCmd->numAttr > OTA_APP_MAX_ATTRIBUTES)
    pReadCmd->numAttr = OTA_APP_MAX_ATTRIBUTES;

  pData += 6;
  for (i=0; i<pReadCmd->numAttr; i++)
  {
    pReadCmd->attrID[i] = BUILD_UINT16(pData[i*2], pData[i*2+1]);
  }

  // Send the command
  zcl_SendRead(OTA_DONGLE_ENDPOINT, &dstAddr, cluster, pReadCmd,
               ZCL_FRAME_SERVER_CLIENT_DIR, TRUE, 0, OTA_Dongle_SeqNo++);
}

/*********************************************************************
 * @fn      OTA_ProcessSysApp_DiscoveryReq
 *
 * @brief   Handles app messages from the console application.
 *
 * @param   pData - The data from the server.
 *
 * @return  none
 */
void OTA_ProcessSysApp_DiscoveryReq(uint8_t *pData)
{
  cId_t otaClusters = ZCL_CLUSTER_ID_OTA;
  zstack_zdoMatchDescReq_t Req;

  Req.dstAddr = 0xFFFF;
  Req.nwkAddrOfInterest = 0xFFFF;
  Req.profileID = ZCL_HA_PROFILE_ID;
  Req.n_inputClusters = 0;
  Req.pInputClusters = NULL;
  Req.n_outputClusters = 1;
  Req.pOutputClusters = &otaClusters;

  Zstackapi_ZdoMatchDescReq(appServiceTaskId, &Req);

}

/*********************************************************************
 * @fn      OTA_ProcessSysApp_JoinReq
 *
 * @brief   Handles app messages from the console application.
 *
 * @param   pData - The data from the server.
 *
 * @return  none
 */
void OTA_ProcessSysApp_JoinReq(uint8_t *pData)
{
  zstack_bdbStartCommissioningReq_t zstack_bdbStartCommissioningReq;
  zstack_bdbSetAttributesReq_t Req;

  memset(&Req,0,sizeof(zstack_bdbSetAttributesReq_t));

  // Setup Z-Stack global configuration
  zgConfigPANID = BUILD_UINT16(pData[0], pData[1]);
  zgDefaultChannelList = 0x00000800;
  zgDefaultChannelList <<= (pData[2] - 11);

  Req.bdbPrimaryChannelSet = zgDefaultChannelList;
  Req.has_bdbPrimaryChannelSet = TRUE;
  Req.bdbSecondaryChannelSet = 0;
  Req.has_bdbSecondaryChannelSet = FALSE;


  Zstackapi_bdbSetAttributesReq(appServiceTaskId, &Req);

  if(ZG_BUILD_COORDINATOR_TYPE && ZG_DEVICE_COORDINATOR_TYPE)
  {
      zstack_bdbStartCommissioningReq.commissioning_mode = BDB_COMMISSIONING_MODE_NWK_FORMATION | BDB_COMMISSIONING_MODE_NWK_STEERING | BDB_COMMISSIONING_MODE_FINDING_BINDING;
      Zstackapi_bdbStartCommissioningReq(appServiceTaskId,&zstack_bdbStartCommissioningReq);
  }
  else if (ZG_BUILD_JOINING_TYPE && ZG_DEVICE_JOINING_TYPE)
  {
      zstack_bdbStartCommissioningReq.commissioning_mode = BDB_COMMISSIONING_MODE_NWK_STEERING | BDB_COMMISSIONING_MODE_FINDING_BINDING;
      Zstackapi_bdbStartCommissioningReq(appServiceTaskId,&zstack_bdbStartCommissioningReq);
  }
}

/*********************************************************************
 * @fn      OTA_Send_DeviceInd
 *
 * @brief   Notifies the console about the existance of a device on the network.
 *
 * @param   none.
 *
 * @return  none
 */
void OTA_Send_DeviceInd(uint16_t shortAddr)
{
  uint8_t buffer[OTA_APP_DEVICE_IND_LEN];
  uint8_t *pBuf = buffer;
  uint16_t pan = _NIB.nwkPanId;

  *pBuf++ = OTA_SYSAPP_ENDPOINT;
  *pBuf++ = OTA_APP_DEVICE_IND;

  *pBuf++ = LO_UINT16(pan);
  *pBuf++ = HI_UINT16(pan);

  *pBuf++ = LO_UINT16(shortAddr);
  *pBuf++ = HI_UINT16(shortAddr);

  // Send the indication
  MT_BuildAndSendZToolResponse(MT_RPC_SYS_APP, MT_APP_MSG, OTA_APP_DEVICE_IND_LEN, buffer);
}

/*********************************************************************
 * @fn      OTA_Send_ReadAttrInd
 *
 * @brief   Notifies the console about attribute values for a device.
 *
 * @param   none.
 *
 * @return  none
 */
void OTA_Send_ReadAttrInd(uint16_t cluster, uint16_t shortAddr, zclReadRspStatus_t *pAttr)
{
  uint8_t buffer[OTA_APP_READ_ATTRIBUTE_IND_LEN];
  uint8_t *pBuf = buffer;
  uint8_t len;

  *pBuf++ = OTA_SYSAPP_ENDPOINT;
  *pBuf++ = OTA_APP_READ_ATTRIBUTE_IND;

  *pBuf++ = LO_UINT16(_NIB.nwkPanId);
  *pBuf++ = HI_UINT16(_NIB.nwkPanId);

  *pBuf++ = LO_UINT16(cluster);
  *pBuf++ = HI_UINT16(cluster);

  *pBuf++ = LO_UINT16(shortAddr);
  *pBuf++ = HI_UINT16(shortAddr);

  *pBuf++ = LO_UINT16(pAttr->attrID);
  *pBuf++ = HI_UINT16(pAttr->attrID);
  *pBuf++ = pAttr->status;
  *pBuf++ = pAttr->dataType;

  len = zclGetDataTypeLength(pAttr->dataType);

  // We should not be reading attributes greater than 8 bytes in length
  if (len <= 8)
  {
  *pBuf++ = len;

  if (len)
  {
    uint8_t *pStr;

    switch ( pAttr->dataType )
    {
      case ZCL_DATATYPE_UINT8:
        *pBuf = *((uint8_t *)pAttr->data);
        break;

      case ZCL_DATATYPE_UINT16:
        *pBuf++ = LO_UINT16( *((uint16_t*)pAttr->data) );
        *pBuf++ = HI_UINT16( *((uint16_t*)pAttr->data) );
        break;

      case ZCL_DATATYPE_UINT32:
        pBuf = OsalPort_bufferUint32( pBuf, *((uint32_t*)pAttr->data) );
        break;

      case ZCL_DATATYPE_IEEE_ADDR:
        pStr = (uint8_t*)pAttr->data;
        OsalPort_memcpy( pBuf, pStr, 8 );
        break;

      default:
        break;
    }
   }
  }
  else
    *pBuf = 0;

  // Send the indication
  MT_BuildAndSendZToolResponse(MT_RPC_SYS_APP, MT_APP_MSG, OTA_APP_READ_ATTRIBUTE_IND_LEN, buffer);
}

/*********************************************************************
 * @fn      OTA_ProcessSysApp_JoinReq
 *
 * @brief   Notifies the console that the dognle has joined a network.
 *
 * @param   none.
 *
 * @return  none
 */
void OTA_Send_JoinInd()
{
  uint8_t buffer[OTA_APP_JOIN_IND_LEN];
  uint8_t *pBuf = buffer;
  uint16_t pan = _NIB.nwkPanId;

  *pBuf++ = OTA_SYSAPP_ENDPOINT;
  *pBuf++ = OTA_APP_JOIN_IND;

  *pBuf++ = LO_UINT16(pan);
  *pBuf = HI_UINT16(pan);

  // Send the indication
  MT_BuildAndSendZToolResponse(MT_RPC_SYS_APP, MT_APP_MSG, OTA_APP_JOIN_IND_LEN, buffer);
}

/*********************************************************************
 * @fn      OTA_ProcessSysApp_JoinReq
 *
 * @brief   Notifies the console about the OTA endpoint on a device.
 *
 * @param   none.
 *
 * @return  none
 */
void OTA_Send_EndpointInd(uint16_t addr, uint8_t endpoint)
{
  uint8_t buffer[OTA_APP_ENDPOINT_IND_LEN];
  uint8_t *pBuf = buffer;

  *pBuf++ = OTA_SYSAPP_ENDPOINT;
  *pBuf++ = OTA_APP_ENDPOINT_IND;

  *pBuf++ = LO_UINT16(_NIB.nwkPanId);
  *pBuf++ = HI_UINT16(_NIB.nwkPanId);

  *pBuf++ = LO_UINT16(addr);
  *pBuf++ = HI_UINT16(addr);

  *pBuf = endpoint;

  // Send the indication
  MT_BuildAndSendZToolResponse(MT_RPC_SYS_APP, MT_APP_MSG, OTA_APP_ENDPOINT_IND_LEN, buffer);
}

/*********************************************************************
 * @fn      OTA_Send_DongleInd
 *
 * @brief   Notifies the console about the Dongle.
 *
 * @param   none.
 *
 * @return  none
 */
static void OTA_Send_DongleInd(void)
{

  uint8_t buffer[OTA_APP_DONGLE_IND_LEN];
  uint8_t *pBuf = buffer;

  *pBuf++ = OTA_SYSAPP_ENDPOINT;
  *pBuf++ = OTA_APP_DONGLE_IND;

  *pBuf++ = zgDeviceLogicalType;

  *pBuf++ = LO_UINT16(_NIB.nwkPanId);
  *pBuf++ = HI_UINT16(_NIB.nwkPanId);

  *pBuf++ = LO_UINT16(_NIB.nwkDevAddress);
  *pBuf++ = HI_UINT16(_NIB.nwkDevAddress);

  *pBuf++ = ZCL_OTA_ENDPOINT;

  *pBuf++ = _NIB.nwkLogicalChannel;

  *pBuf = OTA_Dongle_devState;

  // Send the indication
  MT_BuildAndSendZToolResponse(MT_RPC_SYS_APP, MT_APP_MSG, OTA_APP_DONGLE_IND_LEN, buffer);

}


/******************************************************************************
 *
 *  Functions for processing ZCL Foundation incoming Command/Response messages
 *
 *****************************************************************************/

/*********************************************************************
 * @fn      otaServer_ProcessIncomingMsg
 *
 * @brief   Process ZCL Foundation incoming message
 *
 * @param   pInMsg - pointer to the received message
 *
 * @return  uint8_t - TRUE if got handled
 */
static uint8_t otaServer_ProcessIncomingMsg( zclIncoming_t *pInMsg )
{
  uint8_t handled = FALSE;

  switch ( pInMsg->hdr.commandID )
  {
#ifdef ZCL_READ
    case ZCL_CMD_READ_RSP:
      otaServer_ProcessInReadRspCmd( pInMsg );
      handled = TRUE;
      break;
#endif
#ifdef ZCL_WRITE
    case ZCL_CMD_WRITE_RSP:
      otaServer_ProcessInWriteRspCmd( pInMsg );
      handled = TRUE;
      break;
#endif
    case ZCL_CMD_CONFIG_REPORT:
    case ZCL_CMD_CONFIG_REPORT_RSP:
    case ZCL_CMD_READ_REPORT_CFG:
    case ZCL_CMD_READ_REPORT_CFG_RSP:
    case ZCL_CMD_REPORT:
      //bdb_ProcessIncomingReportingMsg( pInMsg );
      break;

    case ZCL_CMD_DEFAULT_RSP:
      otaServer_ProcessInDefaultRspCmd( pInMsg );
      handled = TRUE;
      break;
#ifdef ZCL_DISCOVER
    case ZCL_CMD_DISCOVER_CMDS_RECEIVED_RSP:
      otaServer_ProcessInDiscCmdsRspCmd( pInMsg );
      handled = TRUE;
      break;

    case ZCL_CMD_DISCOVER_CMDS_GEN_RSP:
      otaServer_ProcessInDiscCmdsRspCmd( pInMsg );
      handled = TRUE;
      break;

    case ZCL_CMD_DISCOVER_ATTRS_RSP:
      otaServer_ProcessInDiscAttrsRspCmd( pInMsg );
      handled = TRUE;
      break;

    case ZCL_CMD_DISCOVER_ATTRS_EXT_RSP:
      otaServer_ProcessInDiscAttrsExtRspCmd( pInMsg );
      handled = TRUE;
      break;
#endif
    default:
      break;
  }

  return handled;
}

#ifdef ZCL_READ
/*********************************************************************
 * @fn      otaServer_ProcessInReadRspCmd
 *
 * @brief   Process the "Profile" Read Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8_t otaServer_ProcessInReadRspCmd( zclIncoming_t *pInMsg )
{
  zclReadRspCmd_t *readRspCmd;
  uint8_t i;

  readRspCmd = (zclReadRspCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < readRspCmd->numAttr; i++)
  {
      OTA_Send_ReadAttrInd(pInMsg->msg->clusterId, pInMsg->msg->srcAddr.addr.shortAddr, &readRspCmd->attrList[i]);
  }

  return ( TRUE );
}
#endif // ZCL_READ

#ifdef ZCL_WRITE
/*********************************************************************
 * @fn      otaServer_ProcessInWriteRspCmd
 *
 * @brief   Process the "Profile" Write Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8_t otaServer_ProcessInWriteRspCmd( zclIncoming_t *pInMsg )
{
  zclWriteRspCmd_t *writeRspCmd;
  uint8_t i;

  writeRspCmd = (zclWriteRspCmd_t *)pInMsg->attrCmd;
  for ( i = 0; i < writeRspCmd->numAttr; i++ )
  {
    // Notify the device of the results of the its original write attributes
    // command.
  }

  return ( TRUE );
}
#endif // ZCL_WRITE

/*********************************************************************
 * @fn      otaServer_ProcessInDefaultRspCmd
 *
 * @brief   Process the "Profile" Default Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8_t otaServer_ProcessInDefaultRspCmd( zclIncoming_t *pInMsg )
{
  // zclDefaultRspCmd_t *defaultRspCmd = (zclDefaultRspCmd_t *)pInMsg->attrCmd;

  // Device is notified of the Default Response command.
  (void)pInMsg;

  return ( TRUE );
}

#ifdef ZCL_DISCOVER
/*********************************************************************
 * @fn      otaServer_ProcessInDiscCmdsRspCmd
 *
 * @brief   Process the Discover Commands Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8_t otaServer_ProcessInDiscCmdsRspCmd( zclIncoming_t *pInMsg )
{
  zclDiscoverCmdsCmdRsp_t *discoverRspCmd;
  uint8_t i;

  discoverRspCmd = (zclDiscoverCmdsCmdRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numCmd; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}

/*********************************************************************
 * @fn      otaServer_ProcessInDiscAttrsRspCmd
 *
 * @brief   Process the "Profile" Discover Attributes Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8_t otaServer_ProcessInDiscAttrsRspCmd( zclIncoming_t *pInMsg )
{
  zclDiscoverAttrsRspCmd_t *discoverRspCmd;
  uint8_t i;

  discoverRspCmd = (zclDiscoverAttrsRspCmd_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}

/*********************************************************************
 * @fn      otaServer_ProcessInDiscAttrsExtRspCmd
 *
 * @brief   Process the "Profile" Discover Attributes Extended Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8_t otaServer_ProcessInDiscAttrsExtRspCmd( zclIncoming_t *pInMsg )
{
  zclDiscoverAttrsExtRsp_t *discoverRspCmd;
  uint8_t i;

  discoverRspCmd = (zclDiscoverAttrsExtRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}
#endif // ZCL_DISCOVER



/*******************************************************************************
 * @fn          Initialize_UI
 *
 * @brief       Initialize the Common User Interface
 *
 * @param       none
 *
 * @return      none
 */
static void Initialize_UI(void)
{
    /* Initialize btns */
    Button_Params bparams;
    Button_Params_init(&bparams);
    gLeftButtonHandle = Button_open(CONFIG_BTN_LEFT, otaServer_changeKeyCallback, &bparams);
    // Open Right button without appCallBack
    gRightButtonHandle = Button_open(CONFIG_BTN_RIGHT, NULL, &bparams);

    if (!GPIO_read(((Button_HWAttrs*)gRightButtonHandle->hwAttrs)->gpioIndex))
    {
        Zstackapi_bdbResetLocalActionReq(appServiceTaskId);
    }

    // Set button callback
    Button_setCallback(gRightButtonHandle, otaServer_changeKeyCallback);
}


/****************************************************************************
****************************************************************************/

/*********************************************************************
 * @fn      otaServer_changeKeyCallback
 *
 * @brief   Key event handler function
 *
 * @param   keysPressed - keys to be process in application context
 *
 * @return  none
 */
static void otaServer_changeKeyCallback(Button_Handle _btn, Button_EventMask _buttonEvents)
{
    if (_buttonEvents & Button_EV_CLICKED)
    {
        keys = _btn;

        appServiceTaskEvents |= SAMPLEAPP_KEY_EVT;

        // Wake up the application thread when it waits for clock event
        Semaphore_post(appSemHandle);
    }
}

/*********************************************************************
 * @fn      otaServer_processKey
 *
 * @brief   Key event handler function
 *
 * @param   keysPressed - keys to be process in application context
 *
 * @return  none
 */
static void otaServer_processKey(Button_Handle keysPressed)
{
    zstack_bdbStartCommissioningReq_t zstack_bdbStartCommissioningReq;
    //Button 1
    if(keysPressed == gLeftButtonHandle)
    {
        if(ZG_BUILD_COORDINATOR_TYPE && ZG_DEVICE_COORDINATOR_TYPE)
        {

            zstack_bdbStartCommissioningReq.commissioning_mode = BDB_COMMISSIONING_MODE_NWK_FORMATION | BDB_COMMISSIONING_MODE_NWK_STEERING | BDB_COMMISSIONING_MODE_FINDING_BINDING;
            Zstackapi_bdbStartCommissioningReq(appServiceTaskId,&zstack_bdbStartCommissioningReq);
        }
        else if (ZG_BUILD_JOINING_TYPE && ZG_DEVICE_JOINING_TYPE)
        {
            zstack_bdbStartCommissioningReq.commissioning_mode = BDB_COMMISSIONING_MODE_NWK_STEERING | BDB_COMMISSIONING_MODE_FINDING_BINDING;
            Zstackapi_bdbStartCommissioningReq(appServiceTaskId,&zstack_bdbStartCommissioningReq);
        }

    }
    //Button 2
    if(keysPressed == gRightButtonHandle)
    {

    }

}

uint8_t appTask_getServiceTaskID(void)
{
    return appServiceTaskId;
}
