/******************************************************************************
  Filename:       ota_srv_app.h
  Revised:        $Date: 2015-04-14 13:04:57 -0700 (Tue, 14 Apr 2015) $
  Revision:       $Revision: 43412 $

  Description:    Over-the-Air Upgrade Cluster server definitions.


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

#ifndef OTA_SRV_APP_H
#define OTA_SRV_APP_H

#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************************************
 * INCLUDES
 */
#include "zcl.h"
#include "ota_common.h"

/*********************************************************************
 * TYPEDEFS
 */

// Defines the max number of OTA clients performing an upgrade simultaneously
#define SEQ_NUM_ENTRY_MAX 3

typedef struct
{
  uint8_t transSeqNum;
  uint16_t      shortAddr;
} otaSeqNumEntry_t;


/******************************************************************************
 * FUNCTIONS
 */

/******************************************************************************
 * @fn      OTA_Server_Init
 *
 * @brief   Call to initialize the OTA Server Task
 *
 * @param   Application Task ID
 *
 * @return  none
 */
extern void OTA_Server_Init( uint8_t stEnt );

/******************************************************************************
 * @fn      OTA_HandleFileSysCb
 *
 * @brief   Handles File Server Callbacks.
 *
 * @param   pMSGpkt - The data from the server.
 *
 * @return  none
 */
extern void zclOTA_ServerHandleFileSysCb ( uint8_t* pMSGpkt );

#endif
