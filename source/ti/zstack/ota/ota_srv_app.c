/******************************************************************************
  Filename:       ota_srv_app.c
  Revised:        $Date: 2015-04-14 13:04:57 -0700 (Tue, 14 Apr 2015) $
  Revision:       $Revision: 43412 $

  Description:    Over-the-Air Upgrade Cluster ( OTA ) Server App


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
#include "zcomdef.h"
#include "rom_jt_154.h"
#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ota.h"
#include "ota_srv_app.h"
#include "ota_common.h"
#include "mt_ota.h"

#if defined ( INTER_PAN ) || defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
#include "stub_aps.h"
#endif

#if defined OTA_MMO_SIGN
#include "ota_signature.h"
#endif

#include "zcl_port.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

#define ZCL_OTA_MAX_ATTRIBUTES          11
 // Cluster ID list for match descriptor of the OTA Server.
#define ZCL_OTA_MAX_INCLUSTERS       1
/*********************************************************************
 * TYPEDEFS
 */
/******************************************************************************
 * GLOBAL VARIABLES
 */

/******************************************************************************
 * LOCAL VARIABLES
 */
static uint8_t stAppID = 0xFF;
static endPointDesc_t  zclOTA_Ep = {0};

// Table used to match transaction sequence numbers in ota responses
static otaSeqNumEntry_t SeqNumEntryTable[SEQ_NUM_ENTRY_MAX];

#define ZCL_OTA_MAX_ATTRIBUTES          11

CONST zclAttrRec_t zclOTA_Attrs[ZCL_OTA_MAX_ATTRIBUTES] =
{
  {
    ZCL_CLUSTER_ID_OTA,                     // Cluster IDs - defined in the foundation (ie. zcl.h)
    { // Attribute record
      ATTRID_UPGRADE_SERVER_ID,             // Attribute ID - Found in Cluster Library header (ie. zcl_general.h)
      ZCL_DATATYPE_IEEE_ADDR,               // Data Type - found in zcl.h
      ACCESS_CONTROL_READ | ACCESS_CLIENT,  // Variable access control - found in zcl.h
      ( void * ) &zclOTA_UpgradeServerID    // Pointer to attribute variable
    }
  },
  {
    ZCL_CLUSTER_ID_OTA,
    { // Attribute record
      ATTRID_FILE_OFFSET,
      ZCL_DATATYPE_UINT32,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      ( void * ) &zclOTA_FileOffset
    }
  },
  {
    ZCL_CLUSTER_ID_OTA,
    { // Attribute record
      ATTRID_CURRENT_FILE_VERSION,
      ZCL_DATATYPE_UINT32,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      ( void * ) &zclOTA_CurrentFileVersion
    }
  },
  {
    ZCL_CLUSTER_ID_OTA,
    { // Attribute record
      ATTRID_CURRENT_ZIGBEE_STACK_VERSION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      ( void * ) &zclOTA_CurrentZigBeeStackVersion
    }
  },
  {
    ZCL_CLUSTER_ID_OTA,
    { // Attribute record
      ATTRID_DOWNLOADED_FILE_VERSION,
      ZCL_DATATYPE_UINT32,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      ( void * ) &zclOTA_DownloadedFileVersion
    }
  },
  {
    ZCL_CLUSTER_ID_OTA,
    { // Attribute record
      ATTRID_DOWNLOADED_ZIGBEE_STACK_VERSION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      ( void * ) &zclOTA_DownloadedZigBeeStackVersion
    }
  },
  {
    ZCL_CLUSTER_ID_OTA,
    { // Attribute record
      ATTRID_IMAGE_UPGRADE_STATUS,
      ZCL_DATATYPE_ENUM8,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      ( void * ) &zclOTA_ImageUpgradeStatus
    }
  },
  {
    ZCL_CLUSTER_ID_OTA,
    { // Attribute record
      ATTRID_MANUFACTURER_ID,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      ( void * ) &zclOTA_ManufacturerID
    }
  },
  {
    ZCL_CLUSTER_ID_OTA,
    { // Attribute record
      ATTRID_MINIMUM_BLOCK_REQ_DELAY,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ | ACCESS_CLIENT,
      ( void * ) &zclOTA_MinBlockReqDelay
    }
  },
};

zclOTA_QueryImageRspParams_t queryResponse;             // Global variable for sent query response

const cId_t zclOTA_InClusterList[ZCL_OTA_MAX_INCLUSTERS] =
{
  ZCL_CLUSTER_ID_OTA
};

#define ZCL_OTA_MAX_OUTCLUSTERS       0
#define zclOTA_OutClusterList         NULL

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



/******************************************************************************
 * LOCAL FUNCTIONS
 */
ZStatus_t zclOTA_ServerHdlIncoming ( zclIncoming_t *pInMsg );
static ZStatus_t zclOTA_HdlIncoming ( zclIncoming_t *pInMsg );
static ZStatus_t zclOTA_Srv_QueryNextImageReq ( afAddrType_t *pSrcAddr, zclOTA_QueryNextImageReqParams_t *pParam, uint8_t transSeqNum );
static ZStatus_t zclOTA_Srv_ImageBlockReq ( afAddrType_t *pSrcAddr, zclOTA_ImageBlockReqParams_t *pParam, uint8_t transSeqNum );
static ZStatus_t zclOTA_Srv_ImagePageReq ( afAddrType_t *pSrcAddr, zclOTA_ImagePageReqParams_t *pParam, uint8_t transSeqNum );
static ZStatus_t zclOTA_Srv_UpgradeEndReq ( afAddrType_t *pSrcAddr, zclOTA_UpgradeEndReqParams_t *pParam, uint8_t transSeqNum );
static ZStatus_t zclOTA_Srv_QuerySpecificFileReq ( afAddrType_t *pSrcAddr, zclOTA_QuerySpecificFileReqParams_t *pParam, uint8_t transSeqNum );
static ZStatus_t zclOTA_ProcessQuerySpecificFileReq ( zclIncoming_t *pInMsg );
static void zclOTA_ProcessNextImgRsp ( uint8_t* pMSGpkt, zclOTA_FileID_t *pFileId, afAddrType_t *pAddr );
static void zclOTA_ProcessFileReadRsp ( uint8_t* pMSGpkt, zclOTA_FileID_t *pFileId, afAddrType_t *pAddr );

static ZStatus_t zclOTA_ProcessImageBlockReq ( zclIncoming_t *pInMsg );
static ZStatus_t zclOTA_ProcessQueryNextImageReq ( zclIncoming_t *pInMsg );
static ZStatus_t zclOTA_ProcessImagePageReq ( zclIncoming_t *pInMsg );
static ZStatus_t zclOTA_ProcessUpgradeEndReq ( zclIncoming_t *pInMsg );

static void zclOTA_AddSeqNumEntry(uint8_t transSeqNum, uint16_t shortAddr);
static uint16_t zclOTA_FindSeqNumEntry(uint16_t shortAddr);



  /******************************************************************************
 * @fn      OTA_Server_Init
 *
 * @brief   Call to initialize the OTA Server Task
 *
 * @param   task_id
 *
 * @return  none
 */
void OTA_Server_Init( uint8_t stEnt )
{
  uint16_t i;
  stAppID = stEnt;
  zclOTA_Permit = TRUE;

  // Register as a ZCL Plugin
  zcl_registerPlugin ( ZCL_CLUSTER_ID_OTA,
                       ZCL_CLUSTER_ID_OTA,
                       zclOTA_HdlIncoming );


  zclOTA_Ep.endPoint = ZCL_OTA_ENDPOINT;
  zclOTA_Ep.simpleDesc = &zclOTA_SimpleDesc;
  zclport_registerEndpoint(stAppID, &zclOTA_Ep);

  // Initialize attribute variables
  zclOTA_CurrentZigBeeStackVersion = OTA_STACK_VER_PRO;
  zclOTA_ImageUpgradeStatus = OTA_STATUS_NORMAL;

  // Register attribute list
  zcl_registerAttrList ( ZCL_OTA_ENDPOINT,
                        ZCL_OTA_MAX_ATTRIBUTES,
                        zclOTA_Attrs );


  // The default upgradeServerID is FF:FF:FF:FF:FF:FF:FF:FF
  memset ( zclOTA_UpgradeServerID, 0xFF, sizeof ( zclOTA_UpgradeServerID ) );

  // Initialize SeqNumEntryTable
  for(i = 0; i < SEQ_NUM_ENTRY_MAX; i++)
  {
    memset(&SeqNumEntryTable[i], 0xFF, sizeof (otaSeqNumEntry_t));
  }
}


 /******************************************************************************
 * @fn      zclOTA_AddSeqNumEntry
 *
 * @brief   Add an entry to SeqNumEntryTable
 *
 * @param   transSeqNum
 * @param   shortAddr
 *
 * @return  none
 */
static void zclOTA_AddSeqNumEntry(uint8_t transSeqNum, uint16_t shortAddr)
{
  uint16_t i;
  for(i = 0; i < SEQ_NUM_ENTRY_MAX; i++)
  {
    if((SeqNumEntryTable[i].shortAddr == 0xFFFF && SeqNumEntryTable[i].transSeqNum == 0xFF) ||
      (SeqNumEntryTable[i].shortAddr == shortAddr))
    {
      SeqNumEntryTable[i].shortAddr = shortAddr;
      SeqNumEntryTable[i].transSeqNum = transSeqNum;
      break;
    }
  }
}


 /******************************************************************************
 * @fn      zclOTA_FindSeqNumEntry
 *
 * @brief   Find an entry to SeqNumEntryTable with matching shortAddr
 *
 * @param   transSeqNum - Transaction sequence number
 * @param   shortAddr - Short address
 *
 * @return  uint16_t
 */
static uint16_t zclOTA_FindSeqNumEntry(uint16_t shortAddr)
{
  uint16_t i;
  uint8_t transSeqNum;
  bool found = FALSE;
  for(i = 0; i < SEQ_NUM_ENTRY_MAX; i++)
  {
    if(SeqNumEntryTable[i].shortAddr == shortAddr)
    {
      transSeqNum = SeqNumEntryTable[i].transSeqNum;
      SeqNumEntryTable[i].shortAddr = 0xFFFF;
      SeqNumEntryTable[i].transSeqNum = 0xFF;
      found = TRUE;
      break;
    }
  }

  if(found){
    return transSeqNum;
  }
  // Return internal frame counter if no match is found
  return zclOTA_getSeqNo();
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
static ZStatus_t zclOTA_HdlIncoming ( zclIncoming_t *pInMsg )
{
  ZStatus_t stat = ZSuccess;

  if ( zcl_ClusterCmd ( pInMsg->hdr.fc.type ) )
  {
    // Is this a manufacturer specific command?
    if ( pInMsg->hdr.fc.manuSpecific == 0 )
    {
      // Is command for server?
      if ( zcl_ServerCmd ( pInMsg->hdr.fc.direction ) )
      {
        stat = zclOTA_ServerHdlIncoming ( pInMsg );
      }
      else // Else command is for client
      {
        stat = ZCL_STATUS_UNSUP_CLUSTER_COMMAND;
      }
    }
    else
    {
      // We don't support any manufacturer specific command.
      stat = ZCL_STATUS_UNSUP_MANU_CLUSTER_COMMAND;
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
 * @fn      zclOTA_ServerHdlIncoming
 *
 * @brief   Handle incoming server commands.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
ZStatus_t zclOTA_ServerHdlIncoming ( zclIncoming_t *pInMsg )
{
  switch ( pInMsg->hdr.commandID )
  {
    case COMMAND_QUERY_NEXT_IMAGE_REQ:
      return zclOTA_ProcessQueryNextImageReq ( pInMsg );

    case COMMAND_IMAGE_BLOCK_REQ:
      return zclOTA_ProcessImageBlockReq ( pInMsg );

    case COMMAND_IMAGE_PAGE_REQ:
      return zclOTA_ProcessImagePageReq ( pInMsg );

    case COMMAND_UPGRADE_END_REQ:
      return zclOTA_ProcessUpgradeEndReq ( pInMsg );

    case COMMAND_QUERY_SPECIFIC_FILE_REQ:
      return zclOTA_ProcessQuerySpecificFileReq ( pInMsg );

    default:
      return ZFailure;
  }
}


/******************************************************************************
 * @fn      zclOTA_Srv_ImageBlockReq
 *
 * @brief   Handle an Image Block Request.
 *
 * @param   pSrcAddr - The source of the message
 *          pParam - message parameters
 *          transSeqNum - Transaction Sequence Number
 *
 * @return  ZStatus_t
 */
ZStatus_t zclOTA_Srv_ImageBlockReq ( afAddrType_t *pSrcAddr, zclOTA_ImageBlockReqParams_t *pParam,
    uint8_t transSeqNum )
{
  uint8_t status = ZFailure;

  if ( ( pParam != NULL ) && (pParam->fileId.version != queryResponse.fileId.version) )
  {
    status = ZCL_STATUS_NO_IMAGE_AVAILABLE;
  }
  else
  {

    if ( zclOTA_Permit && ( pParam != NULL ) )
    {
      uint8_t len = pParam->maxDataSize;

      if ( len > OTA_MAX_MTU )
      {
        len = OTA_MAX_MTU;
      }

      // check if client supports rate limiting feature, and if client rate needs to be set
      if ( ( ( pParam->fieldControl & OTA_BLOCK_FC_REQ_DELAY_PRESENT ) != 0 ) &&
           ( pParam->blockReqDelay != zclOTA_MinBlockReqDelay ) )
      {
        zclOTA_ImageBlockRspParams_t blockRsp;

        // Fill in the response parameters
        blockRsp.status = ZOtaWaitForData;
        OsalPort_memcpy ( &blockRsp.rsp.success.fileId, &pParam->fileId, sizeof ( zclOTA_FileID_t ) );
        blockRsp.rsp.wait.currentTime = 0;
        blockRsp.rsp.wait.requestTime = 0;
        blockRsp.rsp.wait.blockReqDelay = zclOTA_MinBlockReqDelay;

        // Send a wait response with updated rate limit timing
        zclOTA_SendImageBlockRsp ( ZCL_OTA_ENDPOINT, pSrcAddr, &blockRsp, transSeqNum );
      }
      else
      {
        // Read the data from the OTA Console
        status = MT_OtaFileReadReq ( pSrcAddr, &pParam->fileId, len, pParam->fileOffset );

        // Send a wait response to the client
        if ( status != ZSuccess )
        {
          zclOTA_ImageBlockRspParams_t blockRsp;

          // Fill in the response parameters
          blockRsp.status = ZOtaWaitForData;
          OsalPort_memcpy ( &blockRsp.rsp.success.fileId, &pParam->fileId, sizeof ( zclOTA_FileID_t ) );
          blockRsp.rsp.wait.currentTime = 0;
          blockRsp.rsp.wait.requestTime = OTA_SEND_BLOCK_WAIT;
          blockRsp.rsp.wait.blockReqDelay = zclOTA_MinBlockReqDelay;

          // Send the block to the peer
          zclOTA_SendImageBlockRsp ( ZCL_OTA_ENDPOINT, pSrcAddr, &blockRsp, transSeqNum );
        }
        else
        {
          // Add transSeqNum to SeqNumEntryTable for use in response
          zclOTA_AddSeqNumEntry( transSeqNum, pSrcAddr->addr.shortAddr );
        }
      }

      status = ZCL_STATUS_CMD_HAS_RSP;

    }
  }

  return status;
}

/******************************************************************************
 * @fn      zclOTA_ProcessQueryNextImageReq
 *
 * @brief   Process received Query Next Image Request.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclOTA_ProcessQueryNextImageReq ( zclIncoming_t *pInMsg )
{
  zclOTA_QueryNextImageReqParams_t  param;
  uint8_t *pData;
  uint8_t transSeqNum = pInMsg->hdr.transSeqNum;

  /* verify message length */
  if ( ( pInMsg->pDataLen != PAYLOAD_MAX_LEN_QUERY_NEXT_IMAGE_REQ ) &&
       ( pInMsg->pDataLen != PAYLOAD_MIN_LEN_QUERY_NEXT_IMAGE_REQ ) )
  {
    /* no further processing if invalid */
    return ZCL_STATUS_MALFORMED_COMMAND;
  }

  /* parse message parameters */
  pData = pInMsg->pData;
  param.fieldControl = *pData++;
  param.fileId.manufacturer = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.type = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.version = OsalPort_buildUint32 ( pData, 4 );
  pData += 4;
  if ( ( param.fieldControl & 0x01 ) != 0 )
  {
    param.hardwareVersion = BUILD_UINT16 ( pData[0], pData[1] );
  }

  /* call callback */
  return zclOTA_Srv_QueryNextImageReq ( &pInMsg->msg->srcAddr, &param, transSeqNum );
}

/******************************************************************************
 * @fn      zclOTA_ProcessImageBlockReq
 *
 * @brief   Process received Image Block Request.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclOTA_ProcessImageBlockReq ( zclIncoming_t *pInMsg )
{
  zclOTA_ImageBlockReqParams_t  param;
  uint8_t *pData;
  uint8_t transSeqNum = pInMsg->hdr.transSeqNum;

  /* verify message length */
  if ( ( pInMsg->pDataLen > PAYLOAD_MAX_LEN_IMAGE_BLOCK_REQ ) &&
       ( pInMsg->pDataLen < PAYLOAD_MIN_LEN_IMAGE_BLOCK_REQ ) )
  {
    /* no further processing if invalid */
    return ZCL_STATUS_MALFORMED_COMMAND;
  }

  /* parse message parameters */
  pData = pInMsg->pData;
  param.fieldControl = *pData++;
  param.fileId.manufacturer = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.type = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.version = OsalPort_buildUint32 ( pData, 4 );
  pData += 4;
  param.fileOffset = OsalPort_buildUint32 ( pData, 4 );
  pData += 4;
  param.maxDataSize = *pData++;
  if ( ( param.fieldControl & OTA_BLOCK_FC_NODES_IEEE_PRESENT ) != 0 )
  {
    osal_cpyExtAddr ( param.nodeAddr, pData );
    pData += 8;
  }
  if ( ( param.fieldControl & OTA_BLOCK_FC_REQ_DELAY_PRESENT ) != 0 )
  {
    param.blockReqDelay = BUILD_UINT16 ( pData[0], pData[1] );
  }

  /* call callback */
  return zclOTA_Srv_ImageBlockReq ( &pInMsg->msg->srcAddr, &param, transSeqNum );
}

/******************************************************************************
 * @fn      zclOTA_ProcessImagePageReq
 *
 * @brief   Process received Image Page Request.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclOTA_ProcessImagePageReq ( zclIncoming_t *pInMsg )
{
  zclOTA_ImagePageReqParams_t  param;
  uint8_t *pData;
  uint8_t transSeqNum = pInMsg->hdr.transSeqNum;

  /* verify message length */
  if ( ( pInMsg->pDataLen != PAYLOAD_MAX_LEN_IMAGE_PAGE_REQ ) &&
       ( pInMsg->pDataLen != PAYLOAD_MIN_LEN_IMAGE_PAGE_REQ ) )
  {
    /* no further processing if invalid */
    return ZCL_STATUS_MALFORMED_COMMAND;
  }

  /* parse message parameters */
  pData = pInMsg->pData;
  param.fieldControl = *pData++;
  param.fileId.manufacturer = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.type = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.version = OsalPort_buildUint32 ( pData, 4 );
  pData += 4;
  param.fileOffset = OsalPort_buildUint32 ( pData, 4 );
  pData += 4;
  param.maxDataSize = *pData++;
  param.pageSize = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.responseSpacing = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  if ( ( param.fieldControl & 0x01 ) != 0 )
  {
    osal_cpyExtAddr ( param.nodeAddr, pData );
  }

  /* call callback */
  return zclOTA_Srv_ImagePageReq ( &pInMsg->msg->srcAddr, &param, transSeqNum );
}

/******************************************************************************
 * @fn      zclOTA_Srv_QueryNextImageReq
 *
 * @brief   Handle a Query Next Image Request.
 *
 * @param   pSrcAddr - The source of the message
 * @param   pParam - message parameters
 * @param   transSeqNum - Transaction Sequence Number
 *
 * @return  ZStatus_t
 *
 * @note    On a query next image, we must request a file listing
 *          from the File Server.  Then open a file if
 */
ZStatus_t zclOTA_Srv_QueryNextImageReq ( afAddrType_t *pSrcAddr, zclOTA_QueryNextImageReqParams_t *pParam,
    uint8_t transSeqNum )
{
  uint8_t options = 0;
  uint8_t status;

  if ( zclOTA_Permit )
  {
    if ( pParam->fieldControl )
    {
      options |= MT_OTA_HW_VER_PRESENT_OPTION;
    }

    // Request the next image for this device from the console via the MT File System
    status = MT_OtaGetImage ( pSrcAddr, &pParam->fileId, pParam->hardwareVersion, NULL, options );
  }
  else
  {
    status = ZOtaNoImageAvailable;
  }

  if ( status != ZSuccess )
  {
    zclOTA_QueryImageRspParams_t queryRsp;

    // Fill in the response parameters
    OsalPort_memcpy ( &queryRsp.fileId, &pParam->fileId, sizeof ( zclOTA_FileID_t ) );
    queryRsp.status = ZOtaNoImageAvailable;
    queryRsp.imageSize = 0;

    // Send a failure response to the client
    zclOTA_SendQueryNextImageRsp ( ZCL_OTA_ENDPOINT, pSrcAddr, &queryRsp, transSeqNum );
  }
  else
  {
    // Add transSeqNum to SeqNumEntryTable for use in response
    zclOTA_AddSeqNumEntry( transSeqNum, pSrcAddr->addr.shortAddr );
  }

  return ZCL_STATUS_CMD_HAS_RSP;
}

/******************************************************************************
 * @fn      zclOTA_Srv_ImagePageReq
 *
 * @brief   Handle an Image Page Request.  Note: Not currently supported.
 *
 * @param   pSrcAddr - The source of the message
 * @param   pParam - message parameters
 * @param   transSeqNum - Transaction Sequence Number
 *
 * @return  ZStatus_t
 */
ZStatus_t zclOTA_Srv_ImagePageReq ( afAddrType_t *pSrcAddr, zclOTA_ImagePageReqParams_t *pParam,
    uint8_t transSeqNum )
{
  // Send not supported resposne
  return ZUnsupClusterCmd;
}

/******************************************************************************
 * @fn      zclOTA_ProcessUpgradeEndReq
 *
 * @brief   Process received Upgrade End Request.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclOTA_ProcessUpgradeEndReq ( zclIncoming_t *pInMsg )
{
  zclOTA_UpgradeEndReqParams_t  param;
  uint8_t *pData;
  uint8_t transSeqNum = pInMsg->hdr.transSeqNum;

  /* verify message length */
  if ( ( pInMsg->pDataLen != PAYLOAD_MAX_LEN_UPGRADE_END_REQ ) &&
       ( pInMsg->pDataLen != PAYLOAD_MIN_LEN_UPGRADE_END_REQ ) )
  {
    /* no further processing if invalid */
    return ZCL_STATUS_MALFORMED_COMMAND;
  }

  /* parse message parameters */
  pData = pInMsg->pData;
  param.status = *pData++;
  if ( param.status == ZCL_STATUS_SUCCESS )
  {
    param.fileId.manufacturer = BUILD_UINT16 ( pData[0], pData[1] );
    pData += 2;
    param.fileId.type = BUILD_UINT16 ( pData[0], pData[1] );
    pData += 2;
    param.fileId.version = OsalPort_buildUint32 ( pData, 4 );
  }

  /* call callback */
  return zclOTA_Srv_UpgradeEndReq ( &pInMsg->msg->srcAddr, &param, transSeqNum );
}

/******************************************************************************
 * @fn      zclOTA_Srv_UpgradeEndReq
 *
 * @brief   Handle an Upgrade End Request.
 *
 * @param   pSrcAddr - The source of the message
 *          pParam - message parameters
 *          transSeqNum - Transaction Sequence Number
 *
 * @return  ZStatus_t
 */
ZStatus_t zclOTA_Srv_UpgradeEndReq ( afAddrType_t *pSrcAddr, zclOTA_UpgradeEndReqParams_t *pParam,
    uint8_t transSeqNum )
{
  uint8_t status = ZFailure;

  if ( zclOTA_Permit && ( pParam != NULL ) )
  {
    zclOTA_UpgradeEndRspParams_t rspParms;

    if ( pParam->status == ZSuccess )
    {
      OsalPort_memcpy ( &rspParms.fileId, &pParam->fileId, sizeof ( zclOTA_FileID_t ) );
      rspParms.currentTime = MAP_osal_GetSystemClock();
      rspParms.upgradeTime = rspParms.currentTime + OTA_UPGRADE_DELAY;

      // Send the response to the peer
      zclOTA_SendUpgradeEndRsp ( ZCL_OTA_ENDPOINT, pSrcAddr, &rspParms, transSeqNum );

      status = ZCL_STATUS_CMD_HAS_RSP;
    }
    else
    {
      // When non success status is received, send default rsp with success status
      status = ZSuccess;
    }

    // Notify the Console Tool
    MT_OtaSendStatus ( pSrcAddr->addr.shortAddr, MT_OTA_DL_COMPLETE, pParam->status, 0 );
  }

  return status;
}

/******************************************************************************
 * @fn      OTA_HandleFileSysCb
 *
 * @brief   Handles File Server Callbacks.
 *
 * @param   pMSGpkt - The data from the server.
 *
 * @return  none
 */
void zclOTA_ServerHandleFileSysCb ( uint8_t* pMSGpkt )
{
  zclOTA_FileID_t pFileId;
  afAddrType_t pAddr;
  uint8_t *pMsg;

  if ( pMSGpkt != NULL )
  {
    // Get the File ID and AF Address
    pMsg = ((OTA_MtMsg_t*)pMSGpkt)->data;
    pMsg = OTA_StreamToFileId ( &pFileId, pMsg );
    pMsg = OTA_StreamToAfAddr ( &pAddr, pMsg );

    switch ( ((OTA_MtMsg_t*)pMSGpkt)->cmd )
    {
      case MT_OTA_NEXT_IMG_RSP:
        zclOTA_ProcessNextImgRsp ( pMsg, &pFileId, &pAddr );
        break;

      case MT_OTA_FILE_READ_RSP:
        zclOTA_ProcessFileReadRsp ( pMsg, &pFileId, &pAddr );
        break;

      default:
        break;
    }
  }
}
#if defined (OTA_SERVER) && (OTA_SERVER == TRUE)
/******************************************************************************
 * @fn      zclOTA_ProcessNextImgRsp
 *
 * @brief   Handles a response to a MT_OTA_NEXT_IMG_RSP.
 *
 * @param   pMsg - The data from the server.
 * @param   pFileId - The ID of the OTA File.
 * @param   pAddr - The source of the message.
 *
 * @return  none
 */
void zclOTA_ProcessNextImgRsp ( uint8_t* pMsg, zclOTA_FileID_t *pFileId,
                                afAddrType_t *pAddr )
{
  zclOTA_QueryImageRspParams_t queryRsp;
  uint8_t options;
  uint8_t status;
  uint8_t transSeqNum = zclOTA_FindSeqNumEntry(pAddr->addr.shortAddr);

  // Get the status of the operation
  status = *pMsg++;

  // Get the options
  options = *pMsg++;

  // Copy the file ID
  OsalPort_memcpy ( &queryRsp.fileId, pFileId, sizeof ( zclOTA_FileID_t ) );

  // Set the image size
  if ( status == ZSuccess )
  {
    queryRsp.status = ZSuccess;
    queryRsp.imageSize = BUILD_UINT32 ( pMsg[0], pMsg[1], pMsg[2], pMsg[3] );
  }
  else
  {
    queryRsp.status = ZOtaNoImageAvailable;
    queryRsp.imageSize = 0;
  }

  queryResponse = queryRsp; // save global variable for query image response. Used later in image block request check

  // Send a response to the client
  if ( options & MT_OTA_QUERY_SPECIFIC_OPTION )
  {
    zclOTA_SendQuerySpecificFileRsp ( ZCL_OTA_ENDPOINT, pAddr, &queryRsp, transSeqNum );
  }
  else
  {
    zclOTA_SendQueryNextImageRsp ( ZCL_OTA_ENDPOINT, pAddr, &queryRsp, transSeqNum );
  }
}

/******************************************************************************
 * @fn      zclOTA_ProcessFileReadRsp
 *
 * @brief   Handles a response to a MT_OTA_FILE_READ_RSP.
 *
 * @param   pMsg - The data from the server.
 *          pFileId - The ID of the OTA File.
 *          pAddr - The source of the message.
 *
 * @return  none
 */
void zclOTA_ProcessFileReadRsp ( uint8_t* pMsg, zclOTA_FileID_t *pFileId,
                                 afAddrType_t *pAddr )
{
  zclOTA_ImageBlockRspParams_t blockRsp;
  uint8_t transSeqNum = zclOTA_FindSeqNumEntry(pAddr->addr.shortAddr);

  // Set the status
  blockRsp.status = *pMsg++;

  // Check the status of the file read
  if ( blockRsp.status == ZSuccess )
  {
    // Fill in the response parameters
    OsalPort_memcpy ( &blockRsp.rsp.success.fileId, pFileId, sizeof ( zclOTA_FileID_t ) );
    blockRsp.rsp.success.fileOffset = BUILD_UINT32 ( pMsg[0], pMsg[1], pMsg[2], pMsg[3] );
    pMsg += 4;
    blockRsp.rsp.success.dataSize = *pMsg++;
    blockRsp.rsp.success.pData = pMsg;
  }
  else
  {
    blockRsp.status = ZOtaAbort;
  }

  // Send the block response to the peer
  zclOTA_SendImageBlockRsp ( ZCL_OTA_ENDPOINT, pAddr, &blockRsp, transSeqNum );
}

/******************************************************************************
 * @fn      zclOTA_ProcessQuerySpecificFileReq
 *
 * @brief   Process received Image Page Request.
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclOTA_ProcessQuerySpecificFileReq ( zclIncoming_t *pInMsg )
{
  zclOTA_QuerySpecificFileReqParams_t  param;
  uint8_t *pData;
  uint8_t transSeqNum = pInMsg->hdr.transSeqNum;

  /* verify message length */
  if ( pInMsg->pDataLen != PAYLOAD_MAX_LEN_QUERY_SPECIFIC_FILE_REQ )
  {
    /* no further processing if invalid */
    return ZCL_STATUS_MALFORMED_COMMAND;
  }

  /* parse message parameters */
  pData = pInMsg->pData;
  osal_cpyExtAddr ( param.nodeAddr, pData );
  pData += Z_EXTADDR_LEN;
  param.fileId.manufacturer = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.type = BUILD_UINT16 ( pData[0], pData[1] );
  pData += 2;
  param.fileId.version = OsalPort_buildUint32 ( pData, 4 );
  pData += 4;
  param.stackVersion = BUILD_UINT16 ( pData[0], pData[1] );

  /* call callback */
  return zclOTA_Srv_QuerySpecificFileReq ( &pInMsg->msg->srcAddr, &param, transSeqNum );
}

/******************************************************************************
 * @fn      zclOTA_Srv_QuerySpecificFileReq
 *
 * @brief   Handles a Query Specific File Request.
 *
 * @param   pSrcAddr - The source of the message
 * @param   pParam - message parameters
 * @param   transSeqNum - Transaction Sequence Number
 *
 * @return  ZStatus_t
 */
ZStatus_t zclOTA_Srv_QuerySpecificFileReq ( afAddrType_t *pSrcAddr, zclOTA_QuerySpecificFileReqParams_t *pParam,
    uint8_t transSeqNum )
{
  uint8_t status;

  // Request the image from the console
  if ( zclOTA_Permit )
  {
    status = MT_OtaGetImage ( pSrcAddr, &pParam->fileId, 0,  pParam->nodeAddr, MT_OTA_QUERY_SPECIFIC_OPTION );
  }
  else
  {
    status = ZOtaNoImageAvailable;
  }

  if ( status != ZSuccess )
  {
    zclOTA_QueryImageRspParams_t queryRsp;

    // Fill in the response parameters
    OsalPort_memcpy ( &queryRsp.fileId, &pParam->fileId, sizeof ( zclOTA_FileID_t ) );
    queryRsp.status = ZOtaNoImageAvailable;
    queryRsp.imageSize = 0;

    // Send a failure response to the client
    zclOTA_SendQuerySpecificFileRsp ( ZCL_OTA_ENDPOINT, pSrcAddr, &queryRsp, transSeqNum );
  }
  else
  {
    // Add transSeqNum to SeqNumEntryTable for use in response
    zclOTA_AddSeqNumEntry( transSeqNum, pSrcAddr->addr.shortAddr );
  }

  return ZCL_STATUS_CMD_HAS_RSP;
}


#endif //OTA_SERVER
