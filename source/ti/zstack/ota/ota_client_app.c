
/******************************************************************************
  Filename:       ota_client_app.c
  Revised:        $Date: 2015-04-17 16:14:00 -0700 (Fri, 17 Apr 2015) $
  Revision:       $Revision: 43459 $


  Description:   Over-the-Air Upgrade Cluster ( OTA ) client application


  Copyright 2015 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product. Other than for
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
******************************************************************************/

/******************************************************************************
 * INCLUDES
 */
#include <xdc/std.h>

#include "util_timer.h"
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/Power.h>
#include <string.h>

#include "zcomdef.h"
#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ota.h"
#include "ota_client_app.h"
#include "ota_common.h"

#include "ti_drivers_config.h"
#include "nvintf.h"
#include "zstack.h"
#include "zcl_port.h"
#include "board_key.h"
#include "board_led.h"
#include "rom_jt_154.h"
#include "zstackapi.h"
#include "zcl_port.h"
#include "nwk_util.h"

#include <ti/drivers/utils/Random.h>

#if defined ( INTER_PAN ) || defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
#include "stub_aps.h"
#endif

#if defined OTA_MMO_SIGN
#include "ota_signature.h"
#endif

#include "mac_util.h"

#include "flash_interface.h"
#include "oad_image_header.h"
#include "ext_flash_layout.h"
#include "oad_switch_agama.h"
#include "sys_ctrl.h"
#include "cui.h"

/*********************************************************************
 * MACROS
 */
#define OTA_NEW_IMAGE_QUERY_RATE    30000 // 30ms period. First Image Query request has + random jitter of 0 to 30 seconds

#define ZCL_OTA_MAX_ATTRIBUTES          11
#define OTA_INIT_TIMEOUT_VALUE          100

#define OTA_SERVER_DISCOVERY_TIMEOUT    5000

#define MINIMUM_IMAGE_BLOCK_REQUEST_TIMEOUT  300   //This is to avoid the device storming the network with many request, consider increasing this if the device is several hops away

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

/******************************************************************************
 * GLOBAL VARIABLES
 */
uint8_t zclOTA_ClientPdState;
uint32_t zclOTA_DownloadedImageSize;  // Downloaded image size



/******************************************************************************
 * LOCAL VARIABLES
 */

// Semaphore used to post events to the application thread
static Semaphore_Handle sem;
static uint16_t events = 0;
static uint8_t stAppID = 0xFF;
static bool reloadMatchDescTimer = false;

static bool reloadQueryServerTimer = false;


// Clock resources for ZCL_OTA_IMAGE_BLOCK_WAIT_EVT
static Clock_Struct imageBlockWaitClkStruct;
static Clock_Handle imageBlockWaitClkHandle;

// Clock resources for ZCL_OTA_UPGRADE_WAIT_EVT
static Clock_Struct upgradeWaitClkStruct;
static Clock_Handle upgradeWaitClkHandle;

// Clock resources for ZCL_OTA_QUERY_SERVER_EVT
static Clock_Struct queryServerClkStruct;
static Clock_Handle queryServerClkHandle;

// Clock resources for ZCL_OTA_BLOCK_RSP_TO_EVT
static Clock_Struct blockRspToClkStruct;
static Clock_Handle blockRspToClkHandle;

// Clock resources for ZCL_OTA_IMAGE_QUERY_TO_EVT
static Clock_Struct imageQueryToClkStruct;
static Clock_Handle imageQueryToClkHandle;

// Clock resources for ZCL_OTA_IMAGE_BLOCK_REQ_DELAY_EVT
static Clock_Struct imageBlockReqDelayClkStruct;
static Clock_Handle imageBlockReqDelayClkHandle;

// Clock resources for ZCL_OTA_SEND_MATCH_DESCRIPTOR_EVT
static Clock_Struct sendMatchDescriptorClkStruct;
static Clock_Handle sendMatchDescriptorClkHandle;



uint8_t zclOTA_AppTask = 0xFF;                            // Callback Task ID
// Task ID
//static uint8_t zclOTA_TaskID;

// Retry counters
static uint8_t zclOTA_BlockRetry;
static uint8_t zclOTA_UpgradeEndRetry;

static zclOTA_FileID_t zclOTA_CurrentDlFileId;
static cId_t otaClusters = ZCL_CLUSTER_ID_OTA;
static uint16_t zclOTA_UpdateDelay;
// OTA Service Discover Information:
static bool zclOta_OtaZDPTransSeq = false;

afAddrType_t zclOTA_serverAddr;                         // Server address

#define ZCL_OTA_MAX_INCLUSTERS        0
#define zclOTA_InClusterList          NULL

// Cluster ID list for match descriptor of the OTA Client.
#define ZCL_OTA_MAX_OUTCLUSTERS      1
const cId_t zclOTA_OutClusterList[ZCL_OTA_MAX_OUTCLUSTERS] =
{
  ZCL_CLUSTER_ID_OTA
};


// Image block command field control value
uint8_t zclOTA_ImageBlockFC = OTA_BLOCK_FC_REQ_DELAY_PRESENT; // set bitmask field control value(s) for device

CUI_clientHandle_t gOTAClientCuiHandle;
static uint32_t gOTAClientInfoLine;

uint8_t (*zclOTAClientUserMsgCB)(zclIncoming_t *pInMsg) = NULL;

/******************************************************************************
 * LOCAL FUNCTIONS
 */

static ZStatus_t zclOTA_ClientHdlIncoming ( zclIncoming_t *pInMsg );
static ZStatus_t zclOTA_ProcessImageNotify ( zclIncoming_t *pInMsg );
static ZStatus_t zclOTA_ProcessQueryNextImageRsp ( zclIncoming_t *pInMsg );
static ZStatus_t zclOTA_ProcessImageBlockRsp ( zclIncoming_t *pInMsg );
static ZStatus_t zclOTA_ProcessUpgradeEndRsp ( zclIncoming_t *pInMsg );
//static void zclOTA_ProcessZDOMsgs ( zdoIncomingMsg_t *pMsg );Modify so it can be called from ST App
static ZStatus_t zclOTA_ProcessQuerySpecificFileRsp ( zclIncoming_t *pInMsg );
static void zclOTA_UpgradeComplete ( uint8_t status );
static uint8_t zclOTA_CmpFileId ( zclOTA_FileID_t *f1, zclOTA_FileID_t *f2 );
static void zclOTA_ImageBlockWaitExpired ( void );
static ZStatus_t sendImageBlockReq ( afAddrType_t *dstAddr );
static void zclOTA_StartTimer ( Clock_Handle cHandle, Clock_Struct *cStruct,  uint32_t seconds);


static void OTA_StartTimeoutEvent( Clock_Handle cHandle, Clock_Struct *cStruct,  uint32_t mSeconds);

static void OTAApp_setPollRate(uint32_t newPollRate, bool rxOnIdle);
static uint16_t OTA_GetRand( void );


static void OTAClientApp_initializeClocks( void );
static void OTA_ProcessOTAMsgs( zclOTA_CallbackMsg_t* pMsg );

static uint8_t OTA_ProcessUnhandledFoundationZCLMsgs ( zclIncoming_t *pMsg );

static void OTA_processImageBlockWaitTimeoutCallback(UArg a0);
static void OTA_processUpgradeWaitTimeoutCallback(UArg a0);
static void OTA_processQueryServerClkStructTimeoutCallback(UArg a0);
static void OTA_processBlockRspToTimeoutCallback(UArg a0);
static void OTA_processImageQueryToTimeoutCallback(UArg a0);
static void OTA_processImageBlockReqDelayTimeoutCallback(UArg a0);
static void OTA_processSendMatchDescriptorTimeoutCallback(UArg a0);



//static void OTA_loadExtImage(void);
#ifdef FACTORY_IMAGE
static bool OTA_hasFactoryImage(void);
static bool OTA_saveFactoryImage(void);
#endif

static void OTA_UpdateStatusLine(void);


/******************************************************************************
 * OTA SIMPLE DESCRIPTOR
 */
SimpleDescriptionFormat_t zclOTA_SimpleDesc =
{
  ZCL_OTA_ENDPOINT,                 //  int Endpoint;
  ZCL_OTA_SAMPLE_PROFILE_ID,        //  uint16_t AppProfId[2];
  ZCL_OTA_SAMPLE_DEVICEID,          //  uint16_t AppDeviceId[2];
  ZCL_OTA_DEVICE_VERSION,           //  int   AppDevVer:4;
  ZCL_OTA_FLAGS,                    //  int   AppFlags:4;
  ZCL_OTA_MAX_INCLUSTERS,           //  byte  AppNumInClusters;
  ( cId_t * ) zclOTA_InClusterList, //  byte *pAppInClusterList;
  ZCL_OTA_MAX_OUTCLUSTERS,          //  byte  AppNumInClusters;
  ( cId_t * ) zclOTA_OutClusterList //  byte *pAppInClusterList;
};

#ifdef OTA_STANDALONE
// Endpoint for OTA Cluster
static endPointDesc_t zclOTA_Ep =
{
  ZCL_OTA_ENDPOINT,
#ifndef ZCL_STANDALONE
  &zcl_TaskID,
#else
  &zclOTA_TaskID,
#endif
  ( SimpleDescriptionFormat_t * ) &zclOTA_SimpleDesc,
  ( afNetworkLatencyReq_t ) 0
};


CONST zclAttrRec_t zclOTA_Attrs[ZCL_OTA_MAX_ATTRIBUTES] =
{
  OTA_ATTR_LIST
};
#endif


/******************************************************************************
 * @fn      OTAClientApp_initializeClocks
 *
 * @brief   Initialize Clocks
 *
 * @param   none
 *
 * @return  none
 */
static void OTAClientApp_initializeClocks(void)
{


  imageBlockWaitClkHandle = Timer_construct(
    &imageBlockWaitClkStruct,
    OTA_processImageBlockWaitTimeoutCallback,
    OTA_INIT_TIMEOUT_VALUE,
    0,
    false,
    0);


  upgradeWaitClkHandle = Timer_construct(
    &upgradeWaitClkStruct,
    OTA_processUpgradeWaitTimeoutCallback,
    OTA_INIT_TIMEOUT_VALUE,
    0,
    false,
    0);

  queryServerClkHandle = Timer_construct(
    &queryServerClkStruct,
    OTA_processQueryServerClkStructTimeoutCallback,
    OTA_INIT_TIMEOUT_VALUE,
    0,
    false,
    0);


  blockRspToClkHandle = Timer_construct(
    &blockRspToClkStruct,
    OTA_processBlockRspToTimeoutCallback,
    OTA_INIT_TIMEOUT_VALUE,
    0,
    false,
    0);

  imageQueryToClkHandle = Timer_construct(
    &imageQueryToClkStruct,
    OTA_processImageQueryToTimeoutCallback,
    OTA_INIT_TIMEOUT_VALUE,
    0,
    false,
    0);


  imageBlockReqDelayClkHandle = Timer_construct(
    &imageBlockReqDelayClkStruct,
    OTA_processImageBlockReqDelayTimeoutCallback,
    OTA_INIT_TIMEOUT_VALUE,
    0,
    false,
    0);

 sendMatchDescriptorClkHandle = Timer_construct(
    &sendMatchDescriptorClkStruct,
    OTA_processSendMatchDescriptorTimeoutCallback,
    OTA_SERVER_DISCOVERY_TIMEOUT,
    0,
    false,
    0);
}



/******************************************************************************
 * @fn      OTA_Client_Init
 *
 * @brief   Call to initialize the OTA Client Task
 *
 * @param   task_id
 *
 * @return  none
 */
void OTA_Client_Init ( Semaphore_Handle appSem, uint8_t stEnt, CUI_clientHandle_t cuiHandle)
{
  //zclOTA_TaskID = task_id;
  sem = appSem;
  stAppID = stEnt;
  gOTAClientCuiHandle = cuiHandle;
  // Register as a ZCL Plugin
  zcl_registerPlugin ( ZCL_CLUSTER_ID_OTA,
                       ZCL_CLUSTER_ID_OTA,
                       zclOTA_HdlIncoming );
  // Initialize attribute variables
  zclOTA_CurrentZigBeeStackVersion = OTA_STACK_VER_PRO;
  zclOTA_ImageUpgradeStatus = OTA_STATUS_NORMAL;

#ifdef OTA_STANDALONE
  // Register for the cluster endpoint
  afRegister ( &zclOTA_Ep );

  // Register attribute list
  zcl_registerAttrList ( ZCL_OTA_ENDPOINT,
                         ZCL_OTA_MAX_ATTRIBUTES,
                         zclOTA_Attrs );
#ifndef OTA_HA
  // Register the application's cluster option list
  zcl_registerClusterOptionList ( ZCL_OTA_ENDPOINT, ZCL_OTA_MAX_OPTIONS, zclOta_Options );
#endif // OTA_HA

#endif

  OTAClientApp_initializeClocks();

  OTAClient_InitializeSettings ();
  OTAClient_DiscoverServer();

  flash_init();

  CUI_statusLineResourceRequest(gOTAClientCuiHandle, "   OTA Info", &gOTAClientInfoLine);
  OTA_UpdateStatusLine();
}

static void OTA_UpdateStatusLine(void)
{
    char lineFormat[MAX_STATUS_LINE_VALUE_LEN] = {'\0'};

    strcat(lineFormat, "["CUI_COLOR_YELLOW"Current File Version"CUI_COLOR_RESET"] 0x%08x");

    CUI_statusLinePrintf(gOTAClientCuiHandle, gOTAClientInfoLine, lineFormat, zclOTA_CurrentFileVersion);
}


/******************************************************************************
 * @fn      zclOTA_StartTimer
 *
 * @brief   Start a ZCL OTA timer.
 *
 * @param   eventId - OSAL event set on timer expiration
 * @param   seconds - timeout in seconds
 *
 * @return  None
 */
static void zclOTA_StartTimer ( Clock_Handle cHandle, Clock_Struct *cStruct,  uint32_t seconds )
{
  // Record the number of whole minutes to wait
  zclOTA_UpdateDelay = ( seconds / 60 );

  if(Timer_isActive(cStruct) == true)
  {
      Timer_stop(cStruct);
  }

  // Set a timer for the remaining seconds to wait.zclOTA_HdlIncoming
  //OsalPortTimers_startTimer ( zclOTA_TaskID, eventId, ( seconds % 60 ) * 1000 );
  Timer_setTimeout(cHandle, ( seconds % 60 ) * 1000);
  Timer_start(cStruct);
}

/******************************************************************************
 * @fn      zclOTA_HdlIncoming
 *
 * @brief   Callback from ZCL to process incoming Commands specific
 *          to this cluster library or Profile commands for attributes
 *          that aren't in the attribute list
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
 ZStatus_t  zclOTA_HdlIncoming( zclIncoming_t *pInMsg )
{
  ZStatus_t stat = ZSuccess;
  uint8_t endpointStatus= FALSE;

  endpointStatus = OTAClient_SetEndpoint( zcl_getRawAFMsg()->endPoint, NULL );

  if( endpointStatus == TRUE )
  {
    if ( pInMsg->msg->endPoint == currentOtaEndpoint )
    {
#if defined (OTA_STANDALONE) || defined (SWITCH_OTA)
      zclOTA_Permit = TRUE;
#endif

      if ( zcl_ClusterCmd ( pInMsg->hdr.fc.type ) )
      {
        // Is this a manufacturer specific command?
        if ( pInMsg->hdr.fc.manuSpecific == 0 )
        {
          // Is command for server?
          if ( zcl_ServerCmd ( pInMsg->hdr.fc.direction ) )
          {
            stat = ZCL_STATUS_UNSUP_CLUSTER_COMMAND;
          }
          else // Else command is for client
          {
            stat = zclOTA_ClientHdlIncoming ( pInMsg );
          }
        }
        else
        {
          // We don't support any manufacturer specific command.
          stat = ZCL_STATUS_UNSUP_MANU_CLUSTER_COMMAND;
        }
      }
    }
  }
  else
  {
    // Handle all the normal (Read, Write...) commands -- should never get here
    stat = ZFailure;
  }

 return ( stat );
}

/******************************************************************************
 * @fn      zclOTA_ClientHdlIncoming
 *
 * @brief   Handle incoming client commands.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclOTA_ClientHdlIncoming ( zclIncoming_t *pInMsg )
{
  switch ( pInMsg->hdr.commandID )
  {
    case COMMAND_IMAGE_NOTIFY:
      return zclOTA_ProcessImageNotify ( pInMsg );

    case COMMAND_QUERY_NEXT_IMAGE_RSP:
      return zclOTA_ProcessQueryNextImageRsp ( pInMsg );

    case COMMAND_IMAGE_BLOCK_RSP:
      return zclOTA_ProcessImageBlockRsp ( pInMsg );

    case COMMAND_UPGRADE_END_RSP:
      return zclOTA_ProcessUpgradeEndRsp ( pInMsg );

    case COMMAND_QUERY_SPECIFIC_FILE_RSP:
      return zclOTA_ProcessQuerySpecificFileRsp ( pInMsg );

    default:
      return ZFailure;
  }
}


/******************************************************************************
 * @fn          zclOTA_event_loop
 *
 * @brief       Event Loop Processor for OTA Client task.
 *
 * @param       task_id - TaskId
 *              events - events
 *
 * @return      Unprocessed event bits
 */
uint16_t zclOTA_event_loop ( void )
{

  if ( events & ZCL_OTA_IMAGE_BLOCK_WAIT_EVT )
  {
    // If the time has expired, perform the required action
    if ( zclOTA_UpdateDelay == 0 )
    {
      zclOTA_ImageBlockWaitExpired();
    }
    else
    {
      // Decrement the number of minutes to wait and reset the timer
      zclOTA_UpdateDelay--;
      //OsalPortTimers_startTimer ( zclOTA_TaskID, ZCL_OTA_IMAGE_BLOCK_WAIT_EVT, 60000 );
      OTA_StartTimeoutEvent( imageBlockWaitClkHandle, &imageBlockWaitClkStruct,  60000);
    }

    events &= ( ~ZCL_OTA_IMAGE_BLOCK_WAIT_EVT );
  }

  if ( events & ZCL_OTA_UPGRADE_WAIT_EVT )
  {
    // If the time has expired, perform the required action
    if ( zclOTA_UpdateDelay == 0 )
    {
      if ( zclOTA_ImageUpgradeStatus == OTA_STATUS_COUNTDOWN )
      {
        zclOTA_UpgradeComplete ( ZSuccess );
      }
      else if ( zclOTA_ImageUpgradeStatus == OTA_STATUS_UPGRADE_WAIT )
      {
        if ( ++zclOTA_UpgradeEndRetry > OTA_MAX_END_REQ_RETRIES )
        {
          // If we have not heard from the server for N retries, perform the upgrade
          zclOTA_UpgradeComplete ( ZSuccess );
        }
        else
        {
          // Send another update end request
          zclOTA_UpgradeEndReqParams_t  req;

          req.status = ZSuccess;
          //OsalPort_memcpy ( &req.fileId, &zclOTA_CurrentDlFileId, sizeof ( zclOTA_FileID_t ) );
          OsalPort_memcpy(&req.fileId, &zclOTA_CurrentDlFileId, sizeof ( zclOTA_FileID_t ));
          zclOTA_SendUpgradeEndReq (currentOtaEndpoint, &zclOTA_serverAddr, &req );

          // Restart the timer for another hour
          zclOTA_StartTimer (upgradeWaitClkHandle, &upgradeWaitClkStruct, 3600 );
        }
      }
    }
    else
    {
      // Decrement the number of minutes to wait and reset the timer
      zclOTA_UpdateDelay--;
      //OsalPortTimers_startTimer ( zclOTA_TaskID, ZCL_OTA_UPGRADE_WAIT_EVT, 60000 );
      OTA_StartTimeoutEvent( upgradeWaitClkHandle, &upgradeWaitClkStruct,  60000);
    }

    events &= ( ~ZCL_OTA_UPGRADE_WAIT_EVT );
  }

  if ( events & ZCL_OTA_IMAGE_QUERY_TO_EVT )
  {
    if ( zclOTA_ImageUpgradeStatus == OTA_STATUS_NORMAL )// rewrite this and move the application part to here
    {
      // Notify the application task of the timeout waiting for the download to start
      zclOTA_CallbackMsg_t *pMsg;

      //pMsg = ( zclOTA_CallbackMsg_t* ) OsalPort_msgAllocate ( sizeof ( zclOTA_CallbackMsg_t ) );
      pMsg = ( zclOTA_CallbackMsg_t* ) OsalPort_malloc (sizeof ( zclOTA_CallbackMsg_t ));

      if ( pMsg )
      {
        pMsg->hdr.event = ZCL_OTA_CALLBACK_IND;
        pMsg->hdr.status = ZFailure;
        pMsg->ota_event = ZCL_OTA_START_CALLBACK;


        //OsalPort_msgSend ( zclOTA_AppTask, ( uint8_t* ) pMsg );
        OTA_ProcessOTAMsgs ( pMsg );
      }
    }

    events &= ( ~ZCL_OTA_IMAGE_QUERY_TO_EVT );
  }

  if ( events & ZCL_OTA_BLOCK_RSP_TO_EVT )
  {
    // We timed out waiting for a Block Response
    if ( ++zclOTA_BlockRetry > OTA_MAX_BLOCK_RETRIES )
    {
      // Send a failed update end request
      zclOTA_UpgradeEndReqParams_t  req;

      req.status = ZOtaAbort;
      //OsalPort_memcpy ( &req.fileId, &zclOTA_CurrentDlFileId, sizeof ( zclOTA_FileID_t ) );
      OsalPort_memcpy ( &req.fileId, &zclOTA_CurrentDlFileId, sizeof ( zclOTA_FileID_t ) );

      zclOTA_SendUpgradeEndReq (currentOtaEndpoint, &zclOTA_serverAddr, &req );

      zclOTA_UpgradeComplete ( ZOtaAbort );
    }
    else
    {
      // Send another block request
      sendImageBlockReq(&zclOTA_serverAddr);
    }

    events &= ( ~ZCL_OTA_BLOCK_RSP_TO_EVT );
  }

  if ( events & ZCL_OTA_QUERY_SERVER_EVT )
  {
    if (zclOTA_ImageUpgradeStatus == OTA_STATUS_NORMAL)
    {

      zclOTA_QueryNextImageReqParams_t queryParams;

      queryParams.fieldControl = 0;
      queryParams.fileId.manufacturer = OTA_MANUFACTURER_ID;
      queryParams.fileId.type = OTA_TYPE_ID;
      queryParams.fileId.version = zclOTA_CurrentFileVersion;

      zclOTA_SendQueryNextImageReq (currentOtaEndpoint, &zclOTA_serverAddr, &queryParams );

      // Set a timer to wait for the response
      //OsalPortTimers_startTimer ( zclOTA_TaskID, ZCL_OTA_IMAGE_QUERY_TO_EVT, 10000 );
      OTA_StartTimeoutEvent( imageQueryToClkHandle, &imageQueryToClkStruct, 10000);

    }
    events &= ( ~ZCL_OTA_QUERY_SERVER_EVT );
  }

  if ( events & ZCL_OTA_IMAGE_BLOCK_REQ_DELAY_EVT )
  {

    sendImageBlockReq ( &zclOTA_serverAddr );

    events &= (~ ZCL_OTA_IMAGE_BLOCK_REQ_DELAY_EVT );
  }

  if ( events & ZCL_OTA_SEND_MATCH_DESCRIPTOR_EVT )
  {
    // Look for OTA Server
    zclOta_OtaZDPTransSeq = true; // to keep track if the message was initiated by OTA or APP

    zstack_zdoMatchDescReq_t req;

    req.dstAddr = 0xFFFF;
    req.nwkAddrOfInterest = 0xFFFF;
    req.profileID = ZCL_HA_PROFILE_ID;
    req.n_inputClusters = 1;
    req.pInputClusters = &otaClusters;
    req.n_outputClusters = 0;
    req.pOutputClusters = NULL;

    Zstackapi_ZdoMatchDescReq(stAppID, &req);

    events &= ( ~ZCL_OTA_SEND_MATCH_DESCRIPTOR_EVT );
  }

  // Discard unknown events
  return 0;
}


/******************************************************************************
 * @fn      zclOTA_ProcessImageNotify
 *
 * @brief   Process received Image Notify command.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclOTA_ProcessImageNotify ( zclIncoming_t *pInMsg )
{
  zclOTA_ImageNotifyParams_t  param;
  zclOTA_QueryNextImageReqParams_t req;
  uint8_t *pData;

  // verify message length
  if ( ( pInMsg->pDataLen > PAYLOAD_MAX_LEN_IMAGE_NOTIFY ) ||
       ( pInMsg->pDataLen < PAYLOAD_MIN_LEN_IMAGE_NOTIFY ) )
  {
    // no further processing if invalid
    return ZCL_STATUS_MALFORMED_COMMAND;
  }

  // verify  in 'normal' state
  if ( ( zclOTA_Permit == FALSE ) ||
       ( zclOTA_ImageUpgradeStatus != OTA_STATUS_NORMAL ) )
  {
    return ZFailure;
  }

  // parse message
  pData = pInMsg->pData;
  param.payloadType = *pData++;
  param.queryJitter = *pData++;
  param.fileId.manufacturer = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.type = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.version = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );

  // if message is broadcast
  if ( pInMsg->msg->wasBroadcast )
  {
    // verify manufacturer
    if ( ( param.payloadType >= NOTIFY_PAYLOAD_JITTER_MFG ) &&
         ( param.fileId.manufacturer != zclOTA_ManufacturerID ) )
    {
      return ZSuccess;
    }

    // verify image type
    if ( ( param.payloadType >= NOTIFY_PAYLOAD_JITTER_MFG_TYPE ) &&
         ( param.fileId.type != zclOTA_ImageType ) )
    {
      return ZSuccess;
    }

    // verify version; if version matches ignore
    if ( ( param.payloadType >= NOTIFY_PAYLOAD_JITTER_MFG_TYPE_VERS ) &&
         ( param.fileId.version == zclOTA_CurrentFileVersion ) )
    {
      return ZSuccess;
    }

    // get random value and compare to query jitter
    if ( ( ( uint8_t ) OTA_GetRand() % 100 ) > param.queryJitter )
    {
      // if greater than query jitter ignore;
      return ZSuccess;
    }
  }

  // if unicast message, or broadcast and still made it here, send query next image
  req.fieldControl = 0;
  req.fileId.manufacturer = zclOTA_ManufacturerID;
  req.fileId.type = zclOTA_ImageType;
  req.fileId.version = zclOTA_CurrentFileVersion;
  zclOTA_SendQueryNextImageReq (currentOtaEndpoint, & ( pInMsg->msg->srcAddr ), &req );

  return ZSuccess;
}

/******************************************************************************
 * @fn      zclOTA_ProcessQueryNextImageRsp
 *
 * @brief   Process received Query Next Image Response.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclOTA_ProcessQueryNextImageRsp ( zclIncoming_t *pInMsg )
{
  zclOTA_QueryImageRspParams_t  param;
  uint8_t *pData;
  uint8_t status = ZFailure;

  // verify message length
  if ( ( pInMsg->pDataLen != PAYLOAD_MAX_LEN_QUERY_NEXT_IMAGE_RSP ) &&
       ( pInMsg->pDataLen != PAYLOAD_MIN_LEN_QUERY_NEXT_IMAGE_RSP ) )
  {
    // no further processing if invalid
    return ZCL_STATUS_MALFORMED_COMMAND;
  }

  // ignore message if in 'download in progress' state
  if ( zclOTA_ImageUpgradeStatus == OTA_STATUS_IN_PROGRESS )
  {
    return ZSuccess;
  }

  // get status
  pData = pInMsg->pData;
  param.status = *pData++;

#if defined (OTA_STANDALONE) || defined (SWITCH_OTA)
  // if status is success
  if ( param.status == ZCL_STATUS_SUCCESS )
  {
    // parse message
    param.fileId.manufacturer = BUILD_UINT16 ( pData[0], pData[1] );
    pData += 2;
    param.fileId.type = BUILD_UINT16 ( pData[0], pData[1] );
    pData += 2;
    param.fileId.version = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );
    pData += 4;
    param.imageSize = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );

    // verify manufacturer id and image type
    if ( ( param.fileId.type == zclOTA_ImageType ) &&
         ( param.fileId.manufacturer == zclOTA_ManufacturerID ) )
    {
      // store file version and image size
      zclOTA_DownloadedFileVersion = param.fileId.version;
      zclOTA_DownloadedImageSize = param.imageSize;

      // initialize other variables
      zclOTA_FileOffset = 0;
      zclOTA_ClientPdState = ZCL_OTA_PD_MAGIC_0_STATE;

      // set state to 'in progress'
      zclOTA_ImageUpgradeStatus = OTA_STATUS_IN_PROGRESS;

      // store server address
      zclOTA_serverAddr = pInMsg->msg->srcAddr;

      // Store the file ID
      //OsalPort_memcpy ( &zclOTA_CurrentDlFileId, &param.fileId, sizeof ( zclOTA_FileID_t ) );
      OsalPort_memcpy ( &zclOTA_CurrentDlFileId, &param.fileId, sizeof ( zclOTA_FileID_t ) );


      // send image block request
      //OsalPortTimers_startTimer ( zclOTA_TaskID, ZCL_OTA_IMAGE_BLOCK_REQ_DELAY_EVT, zclOTA_MinBlockReqDelay );
      OTA_StartTimeoutEvent( imageBlockReqDelayClkHandle, &imageBlockReqDelayClkStruct, zclOTA_MinBlockReqDelay);

      status = ZCL_STATUS_CMD_HAS_RSP;

      // Request the IEEE address of the server to put into the
      // ATTRID_UPGRADE_SERVER_ID attribute

      zstack_zdoIeeeAddrReq_t pReq;
      pReq.nwkAddr = pInMsg->msg->srcAddr.addr.shortAddr;
      pReq.type = zstack_NwkAddrReqType_SINGLE_DEVICE;
      pReq.startIndex = 0;



      Zstackapi_ZdoIeeeAddrReq( stAppID, &pReq);

      //OsalPortTimers_stopTimer ( zclOTA_TaskID, ZCL_OTA_IMAGE_QUERY_TO_EVT );
      if(Timer_isActive(&imageQueryToClkStruct) == true)
      {
          Timer_stop(&imageQueryToClkStruct);
      }
    }
  }
#endif

  // Notify the application task of the failure
  zclOTA_CallbackMsg_t *pMsg;

  //pMsg = ( zclOTA_CallbackMsg_t* ) OsalPort_msgAllocate ( sizeof ( zclOTA_CallbackMsg_t ) );//allocate normal message no need to keep track of msgs for osal
  pMsg = ( zclOTA_CallbackMsg_t* )OsalPort_malloc( sizeof ( zclOTA_CallbackMsg_t ) );
  if ( pMsg )
  {
    pMsg->hdr.event = ZCL_OTA_CALLBACK_IND;
    pMsg->hdr.status = param.status;
    pMsg->ota_event = ZCL_OTA_START_CALLBACK;

    //OsalPort_msgSend ( zclOTA_AppTask, ( uint8_t* ) pMsg );
    OTA_ProcessOTAMsgs ( pMsg );
  }


  return status;
}

/******************************************************************************
 * @fn      zclOTA_ProcessImageBlockRsp
 *
 * @brief   Process received Image Block Response.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclOTA_ProcessImageBlockRsp ( zclIncoming_t *pInMsg )
{
  zclOTA_ImageBlockRspParams_t  param;
  zclOTA_UpgradeEndReqParams_t  req;
  uint8_t *pData;
  uint8_t status = ZSuccess;

  // verify in 'in progress' state
  if ( zclOTA_ImageUpgradeStatus != OTA_STATUS_IN_PROGRESS )
  {
    return ZSuccess;
  }

  // get status
  pData = pInMsg->pData;
  param.status = *pData++;

  // if status is success
  if ( param.status == ZCL_STATUS_SUCCESS )
  {
    // verify message length
    if ( pInMsg->pDataLen < PAYLOAD_MAX_LEN_IMAGE_BLOCK_RSP )
    {
      // no further processing if invalid
      return ZCL_STATUS_MALFORMED_COMMAND;
    }

    // parse message
    param.rsp.success.fileId.manufacturer = BUILD_UINT16 ( pData[0], pData[1] );
    pData += 2;
    param.rsp.success.fileId.type = BUILD_UINT16 ( pData[0], pData[1] );
    pData += 2;
    param.rsp.success.fileId.version = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );
    pData += 4;
    param.rsp.success.fileOffset = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );
    pData += 4;
    param.rsp.success.dataSize = *pData++;
    param.rsp.success.pData = pData;

    // verify manufacturer, image type, file version, file offset
    if ( ( param.rsp.success.fileId.type != zclOTA_ImageType ) ||
         ( param.rsp.success.fileId.manufacturer != zclOTA_ManufacturerID ) ||
         ( param.rsp.success.fileId.version != zclOTA_DownloadedFileVersion ) )
    {
      status = ZCL_STATUS_INVALID_IMAGE;
    }
    else
    {
      // Drop duplicate packets (retries)
      if ( param.rsp.success.fileOffset != zclOTA_FileOffset )
      {
        return ZSuccess;
      }

      Timer_stop(&blockRspToClkStruct);
      events &= (~ZCL_OTA_BLOCK_RSP_TO_EVT);
      // Stop the timer and clear the retry count
      zclOTA_BlockRetry = 0;

      status = zclOTA_ProcessImageData ( param.rsp.success.pData, param.rsp.success.dataSize );


      if ( status == ZSuccess )
      {
        if ( zclOTA_ImageUpgradeStatus == OTA_STATUS_COMPLETE )
        {
          // send upgrade end req with success status
          //OsalPort_memcpy ( &req.fileId, &param.rsp.success.fileId, sizeof ( zclOTA_FileID_t ) );
          OsalPort_memcpy ( &req.fileId, &param.rsp.success.fileId, sizeof ( zclOTA_FileID_t ) );

          req.status = ZSuccess;
          zclOTA_SendUpgradeEndReq (currentOtaEndpoint, & ( pInMsg->msg->srcAddr ), &req );
        }
        else
        {
          // send image block request using rate limiting
          //OsalPortTimers_startTimer ( zclOTA_TaskID, ZCL_OTA_IMAGE_BLOCK_REQ_DELAY_EVT, zclOTA_MinBlockReqDelay );
          OTA_StartTimeoutEvent( imageBlockReqDelayClkHandle, &imageBlockReqDelayClkStruct, zclOTA_MinBlockReqDelay);
        }
      }
    }
  }
  // else if status is 'wait for data'
  else if ( param.status == ZCL_STATUS_WAIT_FOR_DATA )
  {
    // verify message length
    if ( pInMsg->pDataLen != PAYLOAD_MIN_LEN_IMAGE_BLOCK_WAIT )
    {
      // no further processing if invalid
      return ZCL_STATUS_MALFORMED_COMMAND;
    }

    // parse message
    param.rsp.wait.currentTime = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );
    pData += 4;
    param.rsp.wait.requestTime = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );
    pData += 4;
    param.rsp.wait.blockReqDelay = BUILD_UINT16 ( pData[0], pData[1] );

    // check to see if device supports blockReqDelay rate limiting
    if ( ( zclOTA_ImageBlockFC & OTA_BLOCK_FC_REQ_DELAY_PRESENT ) != 0 )
    {
      if ( ( param.rsp.wait.requestTime - param.rsp.wait.currentTime ) > 0 )
      {
        // Stop the timer and clear the retry count
        zclOTA_BlockRetry = 0;
        //OsalPortTimers_stopTimer ( zclOTA_TaskID, ZCL_OTA_BLOCK_RSP_TO_EVT );
        if(Timer_isActive(&blockRspToClkStruct) == true)
        {
          Timer_stop(&blockRspToClkStruct);
        }

        // set timer for next image block req
        zclOTA_StartTimer (imageBlockWaitClkHandle,&imageBlockWaitClkStruct,
                            ( param.rsp.wait.requestTime - param.rsp.wait.currentTime ) );
      }
      else
      {
        // if wait timer delta is 0, then update device with blockReqDelay value and use rate limiting
        zclOTA_MinBlockReqDelay = param.rsp.wait.blockReqDelay;

        //OsalPortTimers_startTimer ( zclOTA_TaskID, ZCL_OTA_IMAGE_BLOCK_REQ_DELAY_EVT, zclOTA_MinBlockReqDelay );
        OTA_StartTimeoutEvent( imageBlockReqDelayClkHandle, &imageBlockReqDelayClkStruct, zclOTA_MinBlockReqDelay + MINIMUM_IMAGE_BLOCK_REQUEST_TIMEOUT);
      }
    }
    else
    {
      // Stop the timer and clear the retry count
      zclOTA_BlockRetry = 0;
      //OsalPortTimers_stopTimer ( zclOTA_TaskID, ZCL_OTA_BLOCK_RSP_TO_EVT );
      if(Timer_isActive(&blockRspToClkStruct) == true)
      {
        Timer_stop(&blockRspToClkStruct);
      }


      // set timer for next image block req
      zclOTA_StartTimer (imageBlockWaitClkHandle,&imageBlockWaitClkStruct,
                            ( param.rsp.wait.requestTime - param.rsp.wait.currentTime ) );
    }
  }
  else if ( param.status == ZCL_STATUS_ABORT )
  {
    // download aborted; set state to 'normal' state
    zclOTA_ImageUpgradeStatus = OTA_STATUS_NORMAL;
    //reset endpoint
    OTAClient_SetEndpoint(0, NULL);
    // Stop the timer and clear the retry count
    zclOTA_BlockRetry = 0;
    //OsalPortTimers_stopTimer ( zclOTA_TaskID, ZCL_OTA_BLOCK_RSP_TO_EVT );
    if(Timer_isActive(&blockRspToClkStruct) == true)
    {
       Timer_stop(&blockRspToClkStruct);
    }

    zclOTA_UpgradeComplete ( ZOtaAbort );

    return ZSuccess;
  }
  else
  {
    return ZCL_STATUS_MALFORMED_COMMAND;
  }

  if ( status != ZSuccess )
  {
    // download failed; set state to 'normal'
    zclOTA_ImageUpgradeStatus = OTA_STATUS_NORMAL;

    OTAClient_SetEndpoint(0, NULL);

    // send upgrade end req with failure status
    //OsalPort_memcpy ( &req.fileId, &param.rsp.success.fileId, sizeof ( zclOTA_FileID_t ) );
    OsalPort_memcpy ( &req.fileId, &param.rsp.success.fileId, sizeof ( zclOTA_FileID_t ) );

    req.status = status;
    zclOTA_SendUpgradeEndReq (currentOtaEndpoint, & ( pInMsg->msg->srcAddr ), &req );
  }

  return ZSuccess;
}



void OTA_ProcessIEEEAddrRsp( ZDO_NwkIEEEAddrResp_t *pMsg )
{
    // If this is from the OTA server, record the server's IEEE address

    if ( pMsg->nwkAddr == zclOTA_serverAddr.addr.shortAddr )
    {
      OsalPort_memcpy ( &zclOTA_UpgradeServerID, pMsg->extAddr, Z_EXTADDR_LEN );
    }

}

bool OTA_ProcessMatchDescRsp ( ZDO_MatchDescRsp_t *pMsg )
{
  if ( zclOta_OtaZDPTransSeq == true ) // cant check this modify to use something else
  {
    if ( pMsg->status == ZSuccess && pMsg->cnt )
    {
      zclOTA_serverAddr.addrMode = ( afAddrMode_t ) Addr16Bit;
      zclOTA_serverAddr.addr.shortAddr = pMsg->nwkAddr;
      // Take the first endpoint, Can be changed to search through endpoints
      zclOTA_serverAddr.endPoint = pMsg->epList[0];
      //OsalPortTimers_stopTimer ( zclOTA_TaskID, ZCL_OTA_SEND_MATCH_DESCRIPTOR_EVT );
      if(Timer_isActive(&sendMatchDescriptorClkStruct) == true)
      {
        Timer_stop(&sendMatchDescriptorClkStruct);
        reloadMatchDescTimer = false;
      }
      zclOta_OtaZDPTransSeq = false;
    }
    return true;
  }
  return false;
}

/******************************************************************************
 * @fn      zclOTA_ProcessUpgradeEndRsp
 *
 * @brief   Process received Upgrade End Response.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclOTA_ProcessUpgradeEndRsp ( zclIncoming_t *pInMsg )
{
  zclOTA_UpgradeEndRspParams_t  param;
  zclOTA_FileID_t currentFileId = {zclOTA_ManufacturerID, zclOTA_ImageType, zclOTA_DownloadedFileVersion};
  uint8_t *pData;


  // Clear the Upgrade End Request transaction sequence number.  At this stage
  // it's no longer needed.
  zclOta_OtaUpgradeEndReqTransSeq = 0;

  // verify message length
  if ( pInMsg->pDataLen != PAYLOAD_MAX_LEN_UPGRADE_END_RSP )
  {
    // no further processing if invalid
    return ZCL_STATUS_MALFORMED_COMMAND;
  }

  // parse message
  pData = pInMsg->pData;
  param.fileId.manufacturer = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.type = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.version = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );
  pData += 4;
  param.currentTime = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );
  pData += 4;
  param.upgradeTime = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );

  // verify in 'download complete'  or 'waiting for upgrade' state
  if ( ( zclOTA_ImageUpgradeStatus == OTA_STATUS_COMPLETE ) ||
       ( ( zclOTA_ImageUpgradeStatus == OTA_STATUS_UPGRADE_WAIT ) && ( param.upgradeTime!=OTA_UPGRADE_TIME_WAIT ) ) )
  {
    // verify manufacturer, image type
    if ( zclOTA_CmpFileId ( &param.fileId, &currentFileId ) == FALSE )
    {
      return ZSuccess;
    }

    // check upgrade time
    if ( param.upgradeTime != OTA_UPGRADE_TIME_WAIT )
    {
      uint32_t notifyDelay = 0;

      if ( param.upgradeTime > param.currentTime )
      {
        // time to wait before notification
        notifyDelay = param.upgradeTime - param.currentTime;
      }

      // set state to 'countdown'
      zclOTA_ImageUpgradeStatus = OTA_STATUS_COUNTDOWN;
      // set timer for upgrade complete notification
      zclOTA_StartTimer (upgradeWaitClkHandle, &upgradeWaitClkStruct, notifyDelay );
    }
    else
    {
      // Wait for another upgrade end response
      zclOTA_ImageUpgradeStatus = OTA_STATUS_UPGRADE_WAIT;
      // Set a timer for 60 minutes to send another Upgrade End Rsp
      zclOTA_StartTimer (upgradeWaitClkHandle, &upgradeWaitClkStruct, 3600 );
      zclOTA_UpgradeEndRetry = 0;
    }
  }

  return ZSuccess;
}

/******************************************************************************
 * @fn      zclOTA_ProcessQuerySpecificFileRsp
 *
 * @brief   Process received Query Specific File Response.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclOTA_ProcessQuerySpecificFileRsp ( zclIncoming_t *pInMsg )
{
  zclOTA_QueryImageRspParams_t  param;
  uint8_t *pData;

  // verify message length
  if ( ( pInMsg->pDataLen != PAYLOAD_MAX_LEN_QUERY_SPECIFIC_FILE_RSP ) &&
       ( pInMsg->pDataLen != PAYLOAD_MIN_LEN_QUERY_SPECIFIC_FILE_RSP ) )
  {
    // no further processing if invalid
    return ZCL_STATUS_MALFORMED_COMMAND;
  }

  // ignore message if in 'download in progress' state
  if ( zclOTA_ImageUpgradeStatus == OTA_STATUS_IN_PROGRESS )
  {
    return ZSuccess;
  }

  // get status
  pData = pInMsg->pData;
  param.status = *pData++;

  // if status is success
  if ( param.status == ZCL_STATUS_SUCCESS )
  {
    // parse message
    param.fileId.manufacturer = BUILD_UINT16 ( pData[0], pData[1] );
    pData += 2;
    param.fileId.type = BUILD_UINT16 ( pData[0], pData[1] );
    pData += 2;
    param.fileId.version = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );
    pData += 4;
    param.imageSize = BUILD_UINT32( pData[0], pData[1], pData[2], pData[3] );//OsalPort_buildUint32 ( pData, 4 );

    // verify manufacturer id and image type
    if ( ( param.fileId.type == zclOTA_ImageType ) &&
         ( param.fileId.manufacturer == zclOTA_ManufacturerID ) )
    {
      // store file version and image size
      zclOTA_DownloadedFileVersion = param.fileId.version;
      zclOTA_DownloadedImageSize = param.imageSize;

      // initialize other variables
      zclOTA_FileOffset = 0;

      // set state to 'in progress'
      zclOTA_ImageUpgradeStatus = OTA_STATUS_IN_PROGRESS;

      // send image block request
      sendImageBlockReq ( & ( pInMsg->msg->srcAddr ) );
    }
  }

  return ZSuccess;
}
/******************************************************************************
 * @fn      zclOTA_UpgradeComplete
 *
 * @brief   Notify the application task that an upgrade has completed.
 *
 * @param   status - The status of the upgrade
 *
 * @return  none
 */
static void zclOTA_UpgradeComplete ( uint8_t status )
{
  // Go back to the normal state
  zclOTA_ImageUpgradeStatus = OTA_STATUS_NORMAL;

  if ( ( zclOTA_DownloadedImageSize == OTA_HEADER_LEN_MIN_ECDSA ) ||
       ( zclOTA_DownloadedImageSize == OTA_HEADER_LEN_MIN ) )
  {
    status = ZFailure;
  }

  //if ( zclOTA_AppTask != 0xFF )
  {
    // Notify the application task the upgrade stopped
    zclOTA_CallbackMsg_t *pMsg;

    //pMsg = ( zclOTA_CallbackMsg_t* ) OsalPort_msgAllocate ( sizeof ( zclOTA_CallbackMsg_t ) );
    pMsg = ( zclOTA_CallbackMsg_t* )OsalPort_malloc( sizeof ( zclOTA_CallbackMsg_t ) );



    if ( pMsg )
    {
      pMsg->hdr.event = ZCL_OTA_CALLBACK_IND;
      pMsg->hdr.status = status;
      pMsg->ota_event = ZCL_OTA_DL_COMPLETE_CALLBACK;

      //OsalPort_msgSend ( zclOTA_AppTask, ( uint8_t* ) pMsg );
      OTA_ProcessOTAMsgs ( pMsg );
    }
  }
  /*else
  {
    // We may have got here due to an aborted download.  Obviously, we should
    // only proceed with the next steps if the downlowd was successful.
    if ( status == ZSuccess )
    {
      // Reset the CRC Shadow and reboot.  The bootloader will see the
      // CRC shadow has been cleared and switch to the new image
      //HalOTAInvRC();
      //OTA_SystemReset();
      OTA_loadExtImage(ST_FULL_IMAGE);

    }
  }*/
}

/******************************************************************************
 * @fn      zclOTA_CmpFileId
 *
 * @brief   Called to compare two file IDs
 *
 * @param   f1, f2 - Pointers to the two file IDs to compare
 *
 * @return  TRUE if the file IDs are the same, else FALSE
 */
static uint8_t zclOTA_CmpFileId ( zclOTA_FileID_t *f1, zclOTA_FileID_t *f2 )
{
  if ( ( f1->manufacturer == 0xFFFF ) ||
       ( f2->manufacturer == 0xFFFF ) ||
       ( f1->manufacturer == f2->manufacturer ) )
  {
    if ( ( f1->type == 0xFFFF ) ||
         ( f2->type == 0xFFFF ) ||
         ( f1->type == f2->type ) )
    {
      if ( ( f1->version == 0xFFFFFFFF ) ||
           ( f2->version == 0xFFFFFFFF ) ||
           ( f1->version == f2->version ) )
      {
        return TRUE;
      }
    }
  }

  return FALSE;
}

/******************************************************************************
 * @fn      zclOTA_ImageBlockWaitExpired
 *
 * @brief   Perform action on image block wait timer expiration.
 *
 * @param   none
 *
 * @return  none
 */
static void zclOTA_ImageBlockWaitExpired ( void )
{
  // verify in 'in progress' state
  if ( zclOTA_ImageUpgradeStatus == OTA_STATUS_IN_PROGRESS )
  {
    // request next block
    sendImageBlockReq ( &zclOTA_serverAddr );
  }
}

/******************************************************************************
 * @fn      sendImageBlockReq
 *
 * @brief   Send an Image Block Request.
 *
 * @param   dstAddr - where you want the message to go
 *
 * @return  ZStatus_t
 */
static ZStatus_t sendImageBlockReq ( afAddrType_t *dstAddr )
{
  zclOTA_ImageBlockReqParams_t req;

  req.fieldControl = zclOTA_ImageBlockFC; // Image block command field control value
  req.fileId.manufacturer = zclOTA_ManufacturerID;
  req.fileId.type = zclOTA_ImageType;
  req.fileId.version = zclOTA_DownloadedFileVersion;
  req.fileOffset = zclOTA_FileOffset;

  if ( zclOTA_DownloadedImageSize - zclOTA_FileOffset < OTA_MAX_MTU )
  {
    req.maxDataSize = zclOTA_DownloadedImageSize - zclOTA_FileOffset;
  }
  else
  {
    req.maxDataSize = OTA_MAX_MTU;
  }

  req.blockReqDelay = zclOTA_MinBlockReqDelay;

  // Start a timer waiting for a response
  //OsalPortTimers_startTimer ( zclOTA_TaskID, ZCL_OTA_BLOCK_RSP_TO_EVT, OTA_MAX_BLOCK_RSP_WAIT_TIME );
  OTA_StartTimeoutEvent( blockRspToClkHandle, &blockRspToClkStruct,  OTA_MAX_BLOCK_RSP_WAIT_TIME);


  return zclOTA_SendImageBlockReq (currentOtaEndpoint, dstAddr, &req );
}
/******************************************************************************
 * @fn      zclOTA_RequestNextUpdate
 *
 * @brief   Called by an application after discovery of the OTA server
 *          to initiate the query next image of the OTA server.
 *
 * @param   srvAddr - Short address of the server
 * @param   srvEndPoint - Endpoint on the server
 *
 * @return  none
 */
void zclOTA_RequestNextUpdate ( uint16_t srvAddr, uint8_t srvEndPoint )
{
  // Record the server address
  zclOTA_serverAddr.addrMode = afAddr16Bit;
  zclOTA_serverAddr.endPoint = srvEndPoint;
  zclOTA_serverAddr.addr.shortAddr = srvAddr;

  // Set an event to query the server
  //OsalPort_setEvent ( zclOTA_TaskID, ZCL_OTA_QUERY_SERVER_EVT );
  events |= ZCL_OTA_QUERY_SERVER_EVT;
}

/******************************************************************************
 * @fn      OTAClient_DiscoverServer
 *
 * @brief   Call to discover the OTA Client
 *
 * @param   task_id
 *
 * @return  none
 */

void OTAClient_DiscoverServer( void )//uint8_t task_id )
{
  // Per section 6.1 of ZigBee Over-the-Air Upgrading Cluster spec, we should
  // periodically query the server. It does not specify the rate.  For example's
  // sake, here we query the server periodically between 30-60s.
  uint32_t queryImgJitter = ( ( uint32_t ) OTA_GetRand() % OTA_NEW_IMAGE_QUERY_RATE ) + ( uint32_t ) OTA_NEW_IMAGE_QUERY_RATE;
  reloadQueryServerTimer = true;
  OTA_StartTimeoutEvent( queryServerClkHandle, &queryServerClkStruct, queryImgJitter);
  //OsalPortTimers_startReloadTimer (  ZCL_OTA_QUERY_SERVER_EVT, queryImgJitter );

  // Wake up in 5 seconds and do some service discovery for an OTA Server
  queryImgJitter = ( ( uint32_t ) OTA_SERVER_DISCOVERY_TIMEOUT );
  //OsalPortTimers_startReloadTimer ( ZCL_OTA_SEND_MATCH_DESCRIPTOR_EVT, queryImgJitter );
  reloadMatchDescTimer = true;
  OTA_StartTimeoutEvent( sendMatchDescriptorClkHandle, &sendMatchDescriptorClkStruct, queryImgJitter);

  // Initiliaze OTA Update End Request Transaction Seq Number
  zclOta_OtaUpgradeEndReqTransSeq = 0;

}

/******************************************************************************
 * @fn      OTAClient_InitializeSettings
 *
 * @brief   Call to initialize the OTA Client
 *
 * @param   task_id
 *
 * @return  none
 */

void OTAClient_InitializeSettings( void )
{

  //memset ( zclOTA_UpgradeServerID, 0xFF, sizeof ( zclOTA_UpgradeServerID ) );
  memset ( zclOTA_UpgradeServerID, 0xFF, sizeof ( zclOTA_UpgradeServerID ) );


  zclOTA_ManufacturerID = OTA_MANUFACTURER_ID;

  zclOTA_ImageType = OTA_TYPE_ID;

  zclOTA_CurrentFileVersion = OTA_APP_VERSION;

}

/******************************************************************************
 * @fn      OTAClient_SetEndpoint
 *
 * @brief   Set OTA endpoint.
 *
 * @param   endpoint - endpoint ID from which OTA functions can be accessed
 * @param   pfnCB - Register the Application to receive the unprocessed Foundation command/response messages
 *
 * @return  TRUE if endpoint set, else FALSE
 */

bool OTAClient_SetEndpoint( uint8_t endpoint, zclport_pFnZclHandleExternal pfnCB )
{
  if( currentOtaEndpoint == OTA_UNUSED_ENDPOINT )
  {
    currentOtaEndpoint = endpoint;
    // Register for all OTA End Point, unhandled, ZCL foundation commands

    //zcl_registerForMsgExt( zclOTA_TaskID, currentOtaEndpoint ); // use zclport_registerZclHandleExternal
    zclport_registerZclHandleExternal( endpoint, OTA_ProcessUnhandledFoundationZCLMsgs );
    zclOTAClientUserMsgCB = pfnCB;
    return TRUE;
  }
  else if( endpoint == currentOtaEndpoint )
  {
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}



/******************************************************************************
 * @fn      OTA_processImageBlockWaitTimeoutCallback
 *
 * @brief   Timeout handler function
 *
 * @param   a0 - ignored
 *
 * @return  none
 */
static void OTA_processImageBlockWaitTimeoutCallback(UArg a0)
{
  (void) a0;  // Parameter is not used

  events |= ZCL_OTA_IMAGE_BLOCK_WAIT_EVT;

  // Wake up the application thread when it waits for clock event
  Semaphore_post(sem);
}
/******************************************************************************
 * @fn      OTA_processUpgradeWaitTimeoutCallback
 *
 * @brief   Timeout handler function
 *
 * @param   a0 - ignored
 *
 * @return  none
 */
static void OTA_processUpgradeWaitTimeoutCallback(UArg a0)
{
  (void) a0;  // Parameter is not used

  events |= ZCL_OTA_UPGRADE_WAIT_EVT;

  // Wake up the application thread when it waits for clock event
  Semaphore_post(sem);
}
/******************************************************************************
 * @fn      OTA_processQueryServerClkStructTimeoutCallback
 *
 * @brief   Timeout handler function
 *
 * @param   a0 - ignored
 *
 * @return  none
 */
static void OTA_processQueryServerClkStructTimeoutCallback(UArg a0)
{
  (void) a0;  // Parameter is not used

  events |= ZCL_OTA_QUERY_SERVER_EVT;
  if ( reloadQueryServerTimer )
  {
      uint32_t queryImgJitter = OTA_NEW_IMAGE_QUERY_RATE;
      Timer_setTimeout(queryServerClkHandle, queryImgJitter);

      Timer_start(&queryServerClkStruct);
  }

  // Wake up the application thread when it waits for clock event
  Semaphore_post(sem);
}
/******************************************************************************
 * @fn      OTA_processBlockRspToTimeoutCallback
 *
 * @brief   Timeout handler function
 *
 * @param   a0 - ignored
 *
 * @return  none
 */
static void OTA_processBlockRspToTimeoutCallback(UArg a0)
{
  (void) a0;  // Parameter is not used

  events |= ZCL_OTA_BLOCK_RSP_TO_EVT;

  // Wake up the application thread when it waits for clock event
  Semaphore_post(sem);
}
/******************************************************************************
 * @fn      OTA_processImageQueryToTimeoutCallback
 *
 * @brief   Timeout handler function
 *
 * @param   a0 - ignored
 *
 * @return  none
 */
static void OTA_processImageQueryToTimeoutCallback(UArg a0)
{
  (void) a0;  // Parameter is not used

  events |= ZCL_OTA_IMAGE_QUERY_TO_EVT;

  // Wake up the application thread when it waits for clock event
  Semaphore_post(sem);
}
/******************************************************************************
 * @fn      OTA_processImageBlockReqDelayTimeoutCallback
 *
 * @brief   Timeout handler function
 *
 * @param   a0 - ignored
 *
 * @return  none
 */
static void OTA_processImageBlockReqDelayTimeoutCallback(UArg a0)
{
  (void) a0;  // Parameter is not used

  events |= ZCL_OTA_IMAGE_BLOCK_REQ_DELAY_EVT;

  // Wake up the application thread when it waits for clock event
  Semaphore_post(sem);
}
/******************************************************************************
 * @fn      OTA_processSendMatchDescriptorTimeoutCallback
 *
 * @brief   Timeout handler function
 *
 * @param   a0 - ignored
 *
 * @return  none
 */
static void OTA_processSendMatchDescriptorTimeoutCallback(UArg a0)
{
  (void) a0;  // Parameter is not used

  events |= ZCL_OTA_SEND_MATCH_DESCRIPTOR_EVT;

  if ( reloadMatchDescTimer )
  {
      Timer_setTimeout(sendMatchDescriptorClkHandle, OTA_SERVER_DISCOVERY_TIMEOUT);
      Timer_start(&sendMatchDescriptorClkStruct);
  }

  // Wake up the application thread when it waits for clock event
  Semaphore_post(sem);
}


static void OTA_StartTimeoutEvent ( Clock_Handle cHandle, Clock_Struct *cStruct,  uint32_t mSeconds)
{
  if(Timer_isActive(cStruct) == true)
  {
    Timer_stop(cStruct);
  }

  Timer_setTimeout(cHandle, mSeconds);

  Timer_start(cStruct);

}

/*********************************************************************
 * @fn      OTA_ProcessOTAMsgs
 *
 * @brief   Called to process callbacks from the ZCL OTA.
 *
 * @param   none
 *
 * @return  none
 */
static void OTA_ProcessOTAMsgs( zclOTA_CallbackMsg_t* pMsg )
{
  bool RxOnIdle;

  switch(pMsg->ota_event)
  {
  case ZCL_OTA_START_CALLBACK:
    if (pMsg->hdr.status == ZSuccess)
    {
      // Speed up the poll rate
      RxOnIdle = true;
      OTAApp_setPollRate(2000, RxOnIdle);
    }
    break;

  case ZCL_OTA_DL_COMPLETE_CALLBACK:
    if (pMsg->hdr.status == ZSuccess)
    {

      OTA_loadExtImage(ST_FULL_IMAGE);
    }
    else
    {
#if (ZG_BUILD_ENDDEVICE_TYPE)
      // slow the poll rate back down.
      RxOnIdle = false;
      OTAApp_setPollRate(POLL_RATE_MAX, RxOnIdle);
#endif
    }
    break;

  default:
    break;
  }

  OsalPort_free(pMsg);
}

/******************************************************************************
 * @fn      zclOTA_ProcessZCLMsgs
 *
 * @brief   Process unhandled foundation ZCL messages for the OTA End Point.
 *
 * @param   pMsg - a Pointer to the ZCL message
 *
 * @return  none
 */
static uint8_t OTA_ProcessUnhandledFoundationZCLMsgs ( zclIncoming_t *pMsg )
{
  zclIncomingMsg_t *pCmd;

  if( zclOTAClientUserMsgCB )
  {
    zclOTAClientUserMsgCB( pMsg );
  }

  pCmd = (zclIncomingMsg_t *)OsalPort_msgAllocate( sizeof ( zclIncomingMsg_t ) );
  if ( pCmd != NULL )
  {
    // fill in the message
    pCmd->hdr.event = ZCL_INCOMING_MSG;
    pCmd->zclHdr    = pMsg->hdr;
    pCmd->clusterId = pMsg->msg->clusterId;
    pCmd->srcAddr   = pMsg->msg->srcAddr;
    pCmd->endPoint  = pMsg->msg->endPoint;
    pCmd->attrCmd   = pMsg->attrCmd;


    switch ( pCmd->zclHdr.commandID )
    {
      case ZCL_CMD_DEFAULT_RSP:
        zclOTA_ProcessInDefaultRspCmd( pCmd );
        break;
      default :
        break;
    }
    OsalPort_msgDeallocate((uint8_t *)pCmd);
  }

  if ( pMsg->attrCmd )
  {
    OsalPort_free(pMsg->attrCmd);
    pMsg->attrCmd = NULL;
  }

  return 0;
}
/*static void OTA_SystemReset( void )
{
  zstack_sysResetReq_t restReq;
  restReq.newNwkState = false;
  (void)Zstackapi_sysResetReq(stAppID, &restReq);
}*/

/******************************************************************************
 * @fn      OTAApp_setPollRate
 *
 * @brief   Set the ZStack Thread Poll Rate
 *
 * @param   newPollRate - new poll rate in milliseconds
 *
 * @return  none
 */
static void OTAApp_setPollRate(uint32_t newPollRate, bool rxOnIdle )
{
  zstack_sysConfigWriteReq_t  writeReq = { 0 };

  // Set RX on when idle
  writeReq.has_macRxOnIdle = true;
  writeReq.macRxOnIdle = rxOnIdle;
  // Set the new poll rate
  writeReq.has_pollRate = true;
  writeReq.pollRate = newPollRate;
  writeReq.pollRateType = POLL_RATE_TYPE_APP_1;

  (void) Zstackapi_sysConfigWriteReq(stAppID, &writeReq);
}

/*********************************************************************
 * @fn        OTA_GetRand
 *
 * @brief    Random number generator
 *
 * @param   none
 *
 * @return  uint16_t - new random number
 *
 *********************************************************************/
static uint16_t OTA_GetRand( void )
{
  uint16_t randNum;

  randNum = (uint8_t)(Random_getNumber()&0xFF);
  randNum += (((uint8_t)(Random_getNumber()&0xFF)) << 8);
  return ( randNum );
}

/******************************************************************************
 * @fn          SampleApp_applyFactoryImage
 *
 * @brief       Load the factory image for the SampleApp from external flash
 *              and reboot.
 *
 * @param       none
 *
 * @return      none
 */
void OTA_loadExtImage(uint8_t imageSelect)
{

  /* Zigbee OAD assumptions:
   * Factory New Metadata and binary image exist in external flash.
   * Zigbee OAD will always take the next slot available after FN header and binary
   */

  if(imageSelect == ST_FULL_IMAGE)
  {
      if(flash_open() != 0)
      {
          // Copy the metadata to the meta page
          ExtImageInfo_t storedImgHdr;
          readFlashPg(EFL_FACT_IMG_META_PG + 1, 0, (uint8_t *)&storedImgHdr,
                      sizeof(ExtImageInfo_t));

          // Populate ext imge info struct
          ExtImageInfo_t extFlMetaHdr;

          // Copy the Metadata header
          OsalPort_memcpy((uint8_t *)&extFlMetaHdr, (uint8_t *)&storedImgHdr,
                  sizeof(ExtImageInfo_t));

          extFlMetaHdr.fixedHdr.imgCpStat = NEED_COPY;

          // Store the metadata
          writeFlashPg(EFL_FACT_IMG_META_PG + 1, 0, (uint8_t *)&extFlMetaHdr, sizeof(ExtImageInfo_t));

          flash_close();

          /* press the virtual reset button */
          SysCtrlSystemReset();
      }

  }
  else if(imageSelect == ST_FULL_FACTORY_IMAGE)
  {
      OAD_markSwitch();
  }

}
#ifdef FACTORY_IMAGE
/******************************************************************************
 * @fn          SampleApp_hasFactoryImage
 *
 * @brief       Is the factory image available?
 *
 * @param       none
 *
 * @return      none
 */
static bool OTA_hasFactoryImage(void)
{
#if defined(EXTERNAL_IMAGE_CHECK)
  bool  valid;
  valid = extFlashOpen();

  if(valid)
  {
    uint16_t  buffer[2];

    // 1. Check reset vector
    valid = extFlashRead(EFL_ADDR_RECOVERY, sizeof(buffer),
                         (uint8_t *) buffer);
    if(valid)
    {
      valid = (buffer[0] != 0xFFFF && buffer[1] != 0xFFFF) &&
              (buffer[0] != 0x0000 && buffer[1] != 0x0000);
    }

    extFlashClose();
  }

  return valid;
#else
  return (true);
#endif
}


/*******************************************************************************
 * @fn      SampleAppSaveFactoryImage
 *
 * @brief   Save the current image to external flash as a factory image
 *
 * @return  none
 */
static bool OTA_saveFactoryImage(void)
{
  bool success;

  success = extFlashOpen();

  if (success)
  {
    uint32_t address;

    // Erase external flash
    for (address= 0; address<EFL_FLASH_SIZE; address+=EFL_PAGE_SIZE)
    {
      extFlashErase(address,EFL_PAGE_SIZE);
    }

    // Install factory image
    for (address=0; address<EFL_SIZE_RECOVERY && success; address+=EFL_PAGE_SIZE)
    {
      success = extFlashErase(EFL_ADDR_RECOVERY+address, EFL_PAGE_SIZE);
      if (success)
      {
        size_t offset;
        static uint8_t buf[256];  // RAM storage needed due to SPI/DMA limitation

        for (offset=0; offset<EFL_PAGE_SIZE; offset+=sizeof(buf))
        {
          const uint8_t *pIntFlash;

          // Copy from internal to external flash
          pIntFlash = (const uint8_t*)address + offset;
          OsalPort_memcpy(buf,pIntFlash,sizeof(buf));
          success = extFlashWrite(EFL_ADDR_RECOVERY+address+offset, sizeof(buf), buf);

          // Verify first few bytes
          if (success)
          {
            extFlashRead(EFL_ADDR_RECOVERY+address+offset, sizeof(buf), buf);
            success = buf[2] == pIntFlash[2] && buf[3] == pIntFlash[3];
          }
        }
      }


    }

    extFlashClose();
  }

  return success;
}
#endif
