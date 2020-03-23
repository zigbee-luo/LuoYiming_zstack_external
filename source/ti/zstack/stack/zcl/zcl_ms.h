/**************************************************************************************************
  Filename:       zcl_ms.h
  Revised:        $Date: 2013-10-16 16:38:58 -0700 (Wed, 16 Oct 2013) $
  Revision:       $Revision: 35701 $

  Description:    This file contains the ZCL Measurement and Sensing Definitions


  Copyright (c) 2019, Texas Instruments Incorporated
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
**************************************************************************************************/

#ifndef ZCL_MS_H
#define ZCL_MS_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "zcl.h"

/*********************************************************************
 * CONSTANTS
 */


/*****************************************************************************/
/***    Illuminance Measurement Cluster Attributes                         ***/
/*****************************************************************************/
    // Illuminance Measurement Information attribute set
#define ATTRID_MS_ILLUMINANCE_MEASURED_VALUE                             0x0000
#define ATTRID_MS_ILLUMINANCE_MIN_MEASURED_VALUE                         0x0001
#define ATTRID_MS_ILLUMINANCE_MAX_MEASURED_VALUE                         0x0002
#define ATTRID_MS_ILLUMINANCE_TOLERANCE                                  0x0003
#define ATTRID_MS_ILLUMINANCE_LIGHT_SENSOR_TYPE                          0x0004

    // Illuminance Measurement Settings attribute set
// #define ATTRID_MS_ILLUMINANCE_MIN_PERCENT_CHANGE                         0x0100
// #define ATTRID_MS_ILLUMINANCE_MIN_ABSOLUTE_CHANGE                        0x0101

  /*** Light Sensor Type attribute values ***/
#define MS_ILLUMINANCE_LIGHT_SENSOR_PHOTODIODE                           0x00
#define MS_ILLUMINANCE_LIGHT_SENSOR_CMOS                                 0x01
#define MS_ILLUMINANCE_LIGHT_SENSOR_UNKNOWN                              0xFF

/*****************************************************************************/
/***    Illuminance Level Sensing Configuration Cluster Attributes         ***/
/*****************************************************************************/
    // Illuminance Level Sensing Information attribute set
#define ATTRID_MS_ILLUMINANCE_LEVEL_STATUS                               0x0000
#define ATTRID_MS_ILLUMINANCE_LEVEL_LIGHT_SENSOR_TYPE                    0x0001

/***  Level Status attribute values  ***/
#define MS_ILLUMINANCE_LEVEL_ON_TARGET                                   0x00
#define MS_ILLUMINANCE_LEVEL_BELOW_TARGET                                0x01
#define MS_ILLUMINANCE_LEVEL_ABOVE_TARGET                                0x02

/***  Light Sensor Type attribute values  ***/
#define MS_ILLUMINANCE_LEVEL_LIGHT_SENSOR_PHOTODIODE                     0x00
#define MS_ILLUMINANCE_LEVEL_LIGHT_SENSOR_CMOS                           0x01
#define MS_ILLUMINANCE_LEVEL_LIGHT_SENSOR_UNKNOWN                        0xFF

    // Illuminance Level Sensing Settings attribute set
#define ATTRID_MS_ILLUMINANCE_TARGET_LEVEL                               0x0010

/*****************************************************************************/
/***    Temperature Measurement Cluster Attributes                         ***/
/*****************************************************************************/
  // Temperature Measurement Information attributes set
#define ATTRID_MS_TEMPERATURE_MEASURED_VALUE                             0x0000 // M, R, int16_t
#define ATTRID_MS_TEMPERATURE_MIN_MEASURED_VALUE                         0x0001 // M, R, int16_t
#define ATTRID_MS_TEMPERATURE_MAX_MEASURED_VALUE                         0x0002 // M, R, int16_t
#define ATTRID_MS_TEMPERATURE_TOLERANCE                                  0x0003 // O, R, uint16_t

  // Temperature Measurement Settings attributes set
#define ATTRID_MS_TEMPERATURE_MIN_PERCENT_CHANGE                         0x0010
#define ATTRID_MS_TEMPERATURE_MIN_ABSOLUTE_CHANGE                        0x0011

/*****************************************************************************/
/***    Pressure Measurement Cluster Attributes                            ***/
/*****************************************************************************/
  // Pressure Measurement Information attribute set
#define ATTRID_MS_PRESSURE_MEASUREMENT_MEASURED_VALUE                    0x0000
#define ATTRID_MS_PRESSURE_MEASUREMENT_MIN_MEASURED_VALUE                0x0001
#define ATTRID_MS_PRESSURE_MEASUREMENT_MAX_MEASURED_VALUE                0x0002
#define ATTRID_MS_PRESSURE_MEASUREMENT_TOLERANCE                         0x0003
#define ATTRID_MS_PRESSURE_MEASUREMENT_SCALED_VALUE                      0x0010
#define ATTRID_MS_PRESSURE_MEASUREMENT_MIN_SCALED_VALUE                  0x0011
#define ATTRID_MS_PRESSURE_MEASUREMENT_MAX_SCALED_VALUE                  0x0012
#define ATTRID_MS_PRESSURE_MEASUREMENT_SCALED_TOLERANCE                  0x0013
#define ATTRID_MS_PRESSURE_MEASUREMENT_SCALE                             0x0014


  // Pressure Measurement Settings attribute set
// #define ATTRID_MS_PRESSURE_MEASUREMENT_MIN_PERCENT_CHANGE                0x0100
// #define ATTRID_MS_PRESSURE_MEASUREMENT_MIN_ABSOLUTE_CHANGE               0x0101

/*****************************************************************************/
/***        Flow Measurement Cluster Attributes                            ***/
/*****************************************************************************/
  // Flow Measurement Information attribute set
#define ATTRID_MS_FLOW_MEASUREMENT_MEASURED_VALUE                        0x0000
#define ATTRID_MS_FLOW_MEASUREMENT_MIN_MEASURED_VALUE                    0x0001
#define ATTRID_MS_FLOW_MEASUREMENT_MAX_MEASURED_VALUE                    0x0002
#define ATTRID_MS_FLOW_MEASUREMENT_TOLERANCE                             0x0003

  // Flow Measurement Settings attribute set
// #define ATTRID_MS_FLOW_MEASUREMENT_MIN_PERCENT_CHANGE                    0x0100
// #define ATTRID_MS_FLOW_MEASUREMENT_MIN_ABSOLUTE_CHANGE                   0x0101

/*****************************************************************************/
/***        Relative Humidity Cluster Attributes                           ***/
/*****************************************************************************/
  // Relative Humidity Information attribute set
#define ATTRID_MS_RELATIVE_HUMIDITY_MEASURED_VALUE                       0x0000
#define ATTRID_MS_RELATIVE_HUMIDITY_MIN_MEASURED_VALUE                   0x0001
#define ATTRID_MS_RELATIVE_HUMIDITY_MAX_MEASURED_VALUE                   0x0002
#define ATTRID_MS_RELATIVE_HUMIDITY_TOLERANCE                            0x0003

/*****************************************************************************/
/***         Occupancy Sensing Cluster Attributes                          ***/
/*****************************************************************************/
    // Occupancy Sensor Configuration attribute set
#define ATTRID_MS_OCCUPANCY_SENSING_CONFIG_OCCUPANCY                     0x0000 // M, R, BITMAP8
#define ATTRID_MS_OCCUPANCY_SENSING_CONFIG_OCCUPANCY_SENSOR_TYPE         0x0001 // M, R, ENUM8

/*** Occupancy Sensor Type Attribute values ***/
#define MS_OCCUPANCY_SENSOR_TYPE_PIR                                     0x00
#define MS_OCCUPANCY_SENSOR_TYPE_ULTRASONIC                              0x01
#define MS_OCCUPANCY_SENSOR_TYPE_PIR_AND_ULTRASONIC                      0x02

    // PIR Configuration attribute set
#define ATTRID_MS_OCCUPANCY_SENSING_CONFIG_PIR_O_TO_U_DELAY              0x0010 // O, R/W, uint16_t
#define ATTRID_MS_OCCUPANCY_SENSING_CONFIG_PIR_U_TO_O_DELAY              0x0011 // O, R/W, uint16_t
#define ATTRID_MS_OCCUPANCY_SENSING_CONFIG_PIR_U_TO_O_THRESH             0x0012 // O, R/W, uint8_t

    // Ultrasonic Configuration attribute set
#define ATTRID_MS_OCCUPANCY_SENSING_CONFIG_ULTRASONIC_O_TO_U_DELAY       0x0020 // O, R/W, uint16_t
#define ATTRID_MS_OCCUPANCY_SENSING_CONFIG_ULTRASONIC_U_TO_O_DELAY       0x0021 // O, R/W, uint16_t
#define ATTRID_MS_OCCUPANCY_SENSING_CONFIG_ULTRASONIC_U_TO_O_THRESH      0x0022 // O, R/W, uint8_t

/************************************************************************************
 * MACROS
 */


/****************************************************************************
 * TYPEDEFS
 */

typedef void (*zclMS_PlaceHolder_t)( void );

// Register Callbacks table entry - enter function pointers for callbacks that
// the application would like to receive
typedef struct
{
  zclMS_PlaceHolder_t               pfnMSPlaceHolder; // Place Holder
//  NULL
} zclMS_AppCallbacks_t;


/****************************************************************************
 * VARIABLES
 */


/****************************************************************************
 * FUNCTIONS
 */

 /*
  * Register for callbacks from this cluster library
  */
extern ZStatus_t zclMS_RegisterCmdCallbacks( uint8_t endpoint, zclMS_AppCallbacks_t *callbacks );


#ifdef __cplusplus
}
#endif

#endif /* ZCL_MS_H */
