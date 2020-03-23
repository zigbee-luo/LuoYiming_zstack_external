/**************************************************************************************************
  Filename:       zcl_generic_data.c
  Revised:        $Date: 2014-05-12 13:14:02 -0700 (Mon, 12 May 2014) $
  Revision:       $Revision: 38502 $


  Description:    Zigbee Cluster Library - sample device application.


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
 * INCLUDES
 */
#include "zcomdef.h"
#include "af.h"
#include "zd_config.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"

/* GENERIC_TODO: Include any of the header files below to access specific cluster data
#include "zcl_poll_control.h"
#include "zcl_electrical_measurement.h"
#include "zcl_diagnostic.h"
#include "zcl_meter_identification.h"
#include "zcl_appliance_identification.h"
#include "zcl_appliance_events_alerts.h"
#include "zcl_power_profile.h"
#include "zcl_appliance_control.h"
#include "zcl_appliance_statistics.h"
#include "zcl_hvac.h"
*/

#include "zcl_generic.h"

/*********************************************************************
 * CONSTANTS
 */

#define GENERIC_DEVICE_VERSION     0
#define GENERIC_FLAGS              0

#define GENERIC_HWVERSION          1
#define GENERIC_ZCLVERSION         BASIC_ZCL_VERSION

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

// Global attributes
const uint16_t Generic_basic_clusterRevision = 0x0002;
#ifdef ZCL_IDENTIFY
const uint16_t Generic_identify_clusterRevision = 0x0001;
#endif

// Basic Cluster
const uint8_t Generic_HWRevision = GENERIC_HWVERSION;
const uint8_t Generic_ZCLVersion = GENERIC_ZCLVERSION;
const uint8_t Generic_ManufacturerName[] = { 16, 'T','e','x','a','s','I','n','s','t','r','u','m','e','n','t','s' };
const uint8_t Generic_PowerSource = POWER_SOURCE_MAINS_1_PHASE;
uint8_t Generic_PhysicalEnvironment = PHY_UNSPECIFIED_ENV;

#ifdef ZCL_IDENTIFY
// Identify Cluster
uint16_t Generic_IdentifyTime;
#endif

/* GENERIC_TODO: declare attribute variables here. If its value can change,
 * initialize it in Generic_ResetAttributesToDefaultValues. If its
 * value will not change, initialize it here.
 */

#if ZCL_DISCOVER
CONST zclCommandRec_t Generic_Cmds[] =
{
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    COMMAND_BASIC_RESET_FACT_DEFAULT,
    CMD_DIR_SERVER_RECEIVED
  },

};

CONST uint8_t zclCmdsArraySize = ( sizeof(Generic_Cmds) / sizeof(Generic_Cmds[0]) );
#endif // ZCL_DISCOVER

/*********************************************************************
 * ATTRIBUTE DEFINITIONS - Uses REAL cluster IDs
 */
CONST zclAttrRec_t Generic_Attrs[] =
{
  // *** General Basic Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_GEN_BASIC,             // Cluster IDs - defined in the foundation (ie. zcl.h)
    {  // Attribute record
      ATTRID_BASIC_HW_VERSION,            // Attribute ID - Found in Cluster Library header (ie. zcl_general.h)
      ZCL_DATATYPE_UINT8,                 // Data Type - found in zcl.h
      ACCESS_CONTROL_READ,                // Variable access control - found in zcl.h
      (void *)&Generic_HWRevision  // Pointer to attribute variable
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_ZCL_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&Generic_ZCLVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_MANUFACTURER_NAME,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)Generic_ManufacturerName
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_POWER_SOURCE,
      ZCL_DATATYPE_ENUM8,
      ACCESS_CONTROL_READ,
      (void *)&Generic_PowerSource
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_PHYSICAL_ENV,
      ZCL_DATATYPE_ENUM8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&Generic_PhysicalEnvironment
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // Attribute record
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&Generic_basic_clusterRevision
    }
  },

#ifdef ZCL_IDENTIFY
  // *** Identify Cluster Attribute ***
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { // Attribute record
      ATTRID_IDENTIFY_TIME,
      ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&Generic_IdentifyTime
    }
  },

  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    {  // Attribute record
      ATTRID_CLUSTER_REVISION,
      ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ,
      (void *)&Generic_identify_clusterRevision
    }
  },
#endif
};

uint8_t CONST Generic_NumAttributes = ( sizeof(Generic_Attrs) / sizeof(Generic_Attrs[0]) );

/*********************************************************************
 * SIMPLE DESCRIPTOR
 */
// This is the Cluster ID List and should be filled with Application
// specific cluster IDs.
const cId_t Generic_InClusterList[] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,

  // GENERIC_TODO: Add application specific Input Clusters Here.
  //       See zcl.h for Cluster ID definitions

};
#define ZCLGENERIC_MAX_INCLUSTERS   (sizeof(Generic_InClusterList) / sizeof(Generic_InClusterList[0]))


const cId_t Generic_OutClusterList[] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,

  // GENERIC_TODO: Add application specific Output Clusters Here.
  //       See zcl.h for Cluster ID definitions
};
#define ZCLGENERIC_MAX_OUTCLUSTERS  (sizeof(Generic_OutClusterList) / sizeof(Generic_OutClusterList[0]))


SimpleDescriptionFormat_t Generic_SimpleDesc =
{
  GENERIC_ENDPOINT,                  //  int Endpoint;
  ZCL_HA_PROFILE_ID,                     //  uint16_t AppProfId;
  ZCL_HA_DEVICEID_TEST_DEVICE,          //  uint16_t AppDeviceId;
  // GENERIC_TODO: Replace ZCL_HA_DEVICEID_TEST_DEVICE with application specific device ID
  GENERIC_DEVICE_VERSION,            //  int   AppDevVer:4;
  GENERIC_FLAGS,                     //  int   AppFlags:4;
  ZCLGENERIC_MAX_INCLUSTERS,         //  byte  AppNumInClusters;
  (cId_t *)Generic_InClusterList, //  byte *pAppInClusterList;
  ZCLGENERIC_MAX_OUTCLUSTERS,        //  byte  AppNumInClusters;
  (cId_t *)Generic_OutClusterList //  byte *pAppInClusterList;
};

// Added to include Touchlink Target functionality
#if defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
bdbTLDeviceInfo_t tlGenericApp_DeviceInfo =
{
  GENERIC_ENDPOINT,                  //uint8_t endpoint;
  ZCL_HA_PROFILE_ID,                        //uint16_t profileID;
  // GENERIC_TODO: Replace ZCL_HA_DEVICEID_ON_OFF_LIGHT with application specific device ID
  ZCL_HA_DEVICEID_ON_OFF_LIGHT,          //uint16_t deviceID;
  GENERIC_DEVICE_VERSION,                    //uint8_t version;
  GENERIC_NUM_GRPS                   //uint8_t grpIdCnt;
};
#endif

/*********************************************************************
 * GLOBAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL FUNCTIONS
 */

void Generic_ResetAttributesToDefaultValues(void)
{

  Generic_PhysicalEnvironment = PHY_UNSPECIFIED_ENV;

#ifdef ZCL_IDENTIFY
  Generic_IdentifyTime = 0;
#endif

  /* GENERIC_TODO: initialize cluster attribute variables. */
}

/****************************************************************************
****************************************************************************/


