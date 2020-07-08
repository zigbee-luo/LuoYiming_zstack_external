
/******************************************************************************
  Filename:       ota_client_app.h
  Revised:        $Date: 2015-04-14 21:59:34 -0700 (Tue, 14 Apr 2015) $
  Revision:       $Revision: 43420 $

  Description:    Over-the-Air Upgrade Cluster client App definitions.


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

#ifndef OTA_CLIENT_APP_H
#define OTA_CLIENT_APP_H

#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************************************
 * INCLUDES
 */
#include "zcl.h"
#include "ota_common.h"
#include "zcl_ota.h"
#include "zd_object.h"
#include "cui.h"
#include "zcl_port.h" // fixed by luoyiming 2020-06-23

/******************************************************************************
 * CONSTANTS
 */

// Callback events to application from OTA
#define ZCL_OTA_START_CALLBACK                        0
#define ZCL_OTA_DL_COMPLETE_CALLBACK                  1

#define SAMPLEAPP_BL_OFFSET                           0x1F001

#define ST_FULL_IMAGE                                 0x01
#define ST_APP_ONLY_IMAGE                             0x02
#define ST_STACK_ONLY_IMAGE                           0x03
#define ST_FULL_FACTORY_IMAGE                         0x04

// OTA_Storage_Status_t status codes
typedef enum {
    OTA_Storage_Status_Success, ///< Success
    OTA_Storage_Failed,         ///< Fail
    OTA_Storage_CrcError,       ///< Acknowledgment or Response Timed out
    OTA_Storage_FlashError,     ///< flash access error
    OTA_Storage_Aborted,        ///< Canceled by application
    OTA_Storage_Rejected,       ///< OAD request rejected by application
} OTA_Storage_Status_t;

// OTA attribute macro definitions

#define OTA_ATTR_UPGRADE_SERVERID  { ZCL_CLUSTER_ID_OTA, \
 { \
   ATTRID_UPGRADE_SERVER_ID, \
   ZCL_DATATYPE_IEEE_ADDR ,\
   ACCESS_CONTROL_READ | ACCESS_CLIENT, \
   ( void * ) &zclOTA_UpgradeServerID \
 } \
}

#define OTA_ATTR_FILE_OFFSET       { ZCL_CLUSTER_ID_OTA,  \
  { \
      ATTRID_FILE_OFFSET, \
      ZCL_DATATYPE_UINT32, \
      ACCESS_CONTROL_READ | ACCESS_CLIENT,\
      ( void * ) &zclOTA_FileOffset \
    } \
  }

#define OTA_ATTR_CURR_FILE_VERSION  { ZCL_CLUSTER_ID_OTA,   \
    { \
      ATTRID_CURRENT_FILE_VERSION,\
      ZCL_DATATYPE_UINT32, \
      ACCESS_CONTROL_READ | ACCESS_CLIENT, \
      ( void * ) &zclOTA_CurrentFileVersion \
    } \
  }

#define OTA_ATTR_CURR_ZIGBEE_STACK_VERSION { ZCL_CLUSTER_ID_OTA, \
    { \
      ATTRID_CURRENT_ZIGBEE_STACK_VERSION,\
      ZCL_DATATYPE_UINT16,\
      ACCESS_CONTROL_READ | ACCESS_CLIENT,\
      ( void * ) &zclOTA_CurrentZigBeeStackVersion \
    } \
  }

#define OTA_ATTR_DOWNLOADED_FILE_VERSION   { ZCL_CLUSTER_ID_OTA, \
    { \
      ATTRID_DOWNLOADED_FILE_VERSION, \
      ZCL_DATATYPE_UINT32, \
      ACCESS_CONTROL_READ | ACCESS_CLIENT, \
      ( void * ) &zclOTA_DownloadedFileVersion \
    }\
   }

#define OTA_ATTR_DOWNLOADED_ZIGBEESTACK_VERSION  { ZCL_CLUSTER_ID_OTA,\
    { \
      ATTRID_DOWNLOADED_ZIGBEE_STACK_VERSION, \
      ZCL_DATATYPE_UINT16, \
      ACCESS_CONTROL_READ | ACCESS_CLIENT, \
      ( void * ) &zclOTA_DownloadedZigBeeStackVersion \
    } \
  }

#define OTA_ATTR_IMAGE_UPGRADE_STATUS  { ZCL_CLUSTER_ID_OTA, \
    { \
      ATTRID_IMAGE_UPGRADE_STATUS, \
      ZCL_DATATYPE_ENUM8, \
      ACCESS_CONTROL_READ | ACCESS_CLIENT, \
      ( void * ) &zclOTA_ImageUpgradeStatus \
    } \
  }

#define OTA_ATTR_MANUFACTURE_ID        { ZCL_CLUSTER_ID_OTA,  \
    { \
      ATTRID_MANUFACTURER_ID, \
      ZCL_DATATYPE_UINT16, \
      ACCESS_CONTROL_READ | ACCESS_CLIENT, \
      ( void * ) &zclOTA_ManufacturerID \
    } \
  }

#define OTA_MIN_BLOCK_REQ_DELAY        {  ZCL_CLUSTER_ID_OTA,  \
    { \
      ATTRID_MINIMUM_BLOCK_REQ_DELAY, \
      ZCL_DATATYPE_UINT16, \
      ACCESS_CONTROL_READ | ACCESS_CLIENT, \
      ( void * ) &zclOTA_MinBlockReqDelay \
    } \
  }


#define OTA_CLUSTER_REVISION          {  ZCL_CLUSTER_ID_OTA, \
    { \
      ATTRID_CLUSTER_REVISION,\
      ZCL_DATATYPE_UINT16,\
      ACCESS_CONTROL_READ | ACCESS_CLIENT, \
      ( void * ) &zclOTA_clusterRevision \
    } \
  }


#define OTA_ATTR_LIST  OTA_ATTR_UPGRADE_SERVERID ,\
  OTA_ATTR_FILE_OFFSET      , \
  OTA_ATTR_CURR_FILE_VERSION , \
  OTA_ATTR_CURR_ZIGBEE_STACK_VERSION, \
  OTA_ATTR_DOWNLOADED_FILE_VERSION, \
  OTA_ATTR_DOWNLOADED_ZIGBEESTACK_VERSION , \
  OTA_ATTR_IMAGE_UPGRADE_STATUS , \
  OTA_ATTR_MANUFACTURE_ID , \
  OTA_MIN_BLOCK_REQ_DELAY , \
  OTA_CLUSTER_REVISION , \

/******************************************************************************
 * TYPEDEFS
 */

/******************************************************************************
 * GLOBAL VARIABLES
 */

extern uint8_t zclOTA_ClientPdState;
extern uint32_t zclOTA_DownloadedImageSize;  // Downloaded image size


/******************************************************************************
 * FUNCTIONS
 */

/******************************************************************************
 * @fn      OTA_Client_Init
 *
 * @brief   Call to initialize the OTA Client Task
 *
 * @param   task_id
 *
 * @return  none
 */
extern void OTA_Client_Init ( Semaphore_Handle appSem, uint8_t stEnt, CUI_clientHandle_t cuiHandle);

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
extern uint16_t zclOTA_event_loop( void );


extern void OTA_ProcessIEEEAddrRsp( ZDO_NwkIEEEAddrResp_t *pMsg );

extern bool OTA_ProcessMatchDescRsp ( ZDO_MatchDescRsp_t *pMsg );

/******************************************************************************
 * @fn      zclOTA_RequestNextUpdate
 *
 * @brief   Called by an application after discovery of the OTA server
 *          to initiate the query next image of the OTA server.
 *
 * @param   srvAddr - Short address of the server
 * @param   srvEndPoint - Endpoint on the server
 *
 * @return  ZStatus_t
 */
extern void zclOTA_RequestNextUpdate(uint16_t srvAddr, uint8_t srvEndPoint);

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
extern  ZStatus_t zclOTA_HdlIncoming ( zclIncoming_t *pInMsg );

/******************************************************************************
 * @fn      OTAClient_DiscoverServer
 *
 * @brief   Call to discover the OTA Client
 *
 * @param   task_id
 *
 * @return  none
 */
extern void OTAClient_DiscoverServer( void );//uint8_t task_id );

/******************************************************************************
 * @fn      OTAClient_InitializeSettings
 *
 * @brief   Call to initialize the OTA Client
 *
 * @param   task_id
 *
 * @return  none
 */

extern void OTAClient_InitializeSettings( void );//uint8_t task_id);

/******************************************************************************
 * @fn      OTAClient_SetEndpoint
 *
 * @brief   Set OTA endpoint.
 *
 * @param   endpoint - endpoint ID from which OTA functions can be accessed
 * @param   pfnCB - Register the Application to receive the unprocessed Foundation command/response messages
 *
 * @return  true if endpoint set, else false
 */
extern bool OTAClient_SetEndpoint(  uint8_t endpoint, zclport_pFnZclHandleExternal pfnCB );


/******************************************************************************
 * @fn          SensorTagApp_applyFactoryImage
 *
 * @brief       Load the factory image for the SensorTag from external flash
 *              and reboot.
 *
 * @param       none
 *
 * @return      none
 */
extern void OTA_loadExtImage(uint8_t imageSelect);


/*********************************************************************
 * @fn          OTA_UpdateStatusLine
 *
 * @brief       Generate part of the OTA Info string
 *
 * @param       none
 *
 * @return      none
 */
extern void OTA_UpdateStatusLine(void);



#ifdef FACTORY_IMAGE
/******************************************************************************
 * @fn          OTA_hasFactoryImage
 *
 * @brief   This function check if the valid factory image exists on external
 *          flash
 *
 * @param   None
 *
 * @return  TRUE If factory image exists on external flash, else FALSE
 *
 */
extern bool OTA_hasFactoryImage(void);


/*******************************************************************************
 * @fn      OTA_saveFactoryImage
 *
 * @brief   This function creates factory image backup of current running image
 *
 * @return  rtn  OTA_Storage_Status_Success/OTA_Storage_FlashError
 */
extern uint8_t OTA_saveFactoryImage(void);
#endif

#endif /*OTA_CLIENT_APP_H*/
