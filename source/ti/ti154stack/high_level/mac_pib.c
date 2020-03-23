/******************************************************************************

 @file  mac_pib.c

 @brief This module contains procedures for the MAC PIB.

 Group: WCS, LPC
 Target Device: cc13x2_26x2

 ******************************************************************************
 
 Copyright (c) 2006-2019, Texas Instruments Incorporated
 All rights reserved.

 IMPORTANT: Your use of this Software is limited to those specific rights
 granted under the terms of a software license agreement between the user
 who downloaded the software, his/her employer (which must be your employer)
 and Texas Instruments Incorporated (the "License"). You may not use this
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

 ******************************************************************************
 
 
 *****************************************************************************/

/* ------------------------------------------------------------------------------------------------
 *                                          Includes
 * ------------------------------------------------------------------------------------------------
 */

#ifndef LIBRARY
#include "ti_154stack_features.h"
#endif

#include "hal_mcu.h"
#include "mac_api.h"
#include "mac_spec.h"
#include "mac_radio.h"
#include "mac_low_level.h"
#include "mac_radio_defs.h"
#include "mac_main.h"
#include "mac_pib.h"
#include "mac_mgmt.h"
#include "mac_assert.h"
#include "mac_hl_patch.h"
#include <stddef.h>

/******************************************************************************
  ROM jump table will support
  1. TIMAC ROM image build
  2. TIMAC stack build without ROM image
  3. TIMAC stack with calling ROM image
 *****************************************************************************/
#include "rom_jt_154.h"


/* ------------------------------------------------------------------------------------------------
 *                                           Local Variables
 * ------------------------------------------------------------------------------------------------
 */


extern uint8 MAC_MlmeGetReqSize( uint8 pibAttribute );

#if defined( FEATURE_MAC_PIB_PTR )

/* Pointer to the MAC PIB */
macPib_t* pMacPib = &macPib;

/**************************************************************************************************
 * @fn          MAC_MlmeSetActivePib
 *
 * @brief       This function initializes the PIB.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * None.
 *
 * @return      None.
 **************************************************************************************************
 */
void MAC_MlmeSetActivePib( void* pPib )
{
  halIntState_t intState;
  HAL_ENTER_CRITICAL_SECTION(intState);
  pMacPib = (macPib_t *)pPib;
  HAL_EXIT_CRITICAL_SECTION(intState);
}
#endif /* FEATURE_MAC_PIB_PTR */

/**************************************************************************************************
 * @fn          macPibReset
 *
 * @brief       This function initializes the PIB.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * None.
 *
 * @return      None.
 **************************************************************************************************
 */
MAC_INTERNAL_API void macPibReset(void)
{
  /* copy PIB defaults */
#if defined( FEATURE_MAC_PIB_PTR )
  *pMacPib = macPibDefaults;
#else
  //macPib = macPibDefaults;
  MAP_osal_memcpy((void *)&macPib, (const void *)&macPibDefaults, sizeof(macPib));
#endif /* FEATURE_MAC_PIB_PTR */

  /* initialize random sequence numbers */
  pMacPib->dsn = MAP_macRadioRandomByte();
  pMacPib->bsn = MAP_macRadioRandomByte();
}

/**************************************************************************************************
 * @fn          MAC_MlmeGetReq
 *
 * @brief       This direct execute function retrieves an attribute value
 *              from the MAC PIB.
 *
 * input parameters
 *
 * @param       pibAttribute - The attribute identifier.
 * @param       pValue - pointer to the attribute value.
 *
 * output parameters
 *
 * @param       pValue - pointer to the attribute value.
 *
 * @return      The status of the request, as follows:
 *              MAC_SUCCESS Operation successful.
 *              MAC_UNSUPPORTED_ATTRIBUTE Attribute not found.
 *
 **************************************************************************************************
 */
uint8 MAC_MlmeGetReq(uint8 pibAttribute, void *pValue)
{
  uint8         i;
  halIntState_t intState;

  if ((i = MAP_macPibIndex(pibAttribute)) == MAC_PIB_INVALID)
  {
    return MAC_UNSUPPORTED_ATTRIBUTE;
  }

  HAL_ENTER_CRITICAL_SECTION(intState);
  MAP_osal_memcpy(pValue, (uint8 *) pMacPib + macPibTbl[i].offset, macPibTbl[i].len);
  HAL_EXIT_CRITICAL_SECTION(intState);
  return MAC_SUCCESS;
}

/**************************************************************************************************
 * @fn          MAC_MlmeGetReqSize
 *
 * @brief       This direct execute function gets the MAC PIB attribute size
 *
 * input parameters
 *
 * @param       pibAttribute - The attribute identifier.
 *
 * output parameters
 *
 * None.
 *
 * @return      size in bytes
 *
 **************************************************************************************************
 */
uint8 MAC_MlmeGetReqSize( uint8 pibAttribute )
{
  uint8 index;

  if ((index = MAP_macPibIndex(pibAttribute)) == MAC_PIB_INVALID)
  {
    return 0;
  }

  return ( macPibTbl[index].len );
}

/**************************************************************************************************
 * @fn          MAC_MlmeSetReq
 *
 * @brief       This direct execute function sets an attribute value
 *              in the MAC PIB.
 *
 * input parameters
 *
 * @param       pibAttribute - The attribute identifier.
 * @param       pValue - pointer to the attribute value.
 *
 * output parameters
 *
 * None.
 *
 * @return      The status of the request, as follows:
 *              MAC_SUCCESS Operation successful.
 *              MAC_UNSUPPORTED_ATTRIBUTE Attribute not found.
 *
 **************************************************************************************************
 */
uint8 MAC_MlmeSetReq(uint8 pibAttribute, void *pValue)
{
  uint8         i;
  halIntState_t intState;

  if (MAP_macPibCheckByPatch(pibAttribute, pValue) == MAC_PIB_PATCH_DONE)
  {
      return MAC_SUCCESS;
  }

  if (pibAttribute == MAC_BEACON_PAYLOAD)
#if defined (FEATURE_BEACON_MODE) || defined (FEATURE_NON_BEACON_MODE)	  
  {
    /* Store the payload locally */
    MAP_osal_memcpy((void *)macBeaconPayload, pValue, MAC_PIB_MAX_BEACON_PAYLOAD_LEN);
    pMacPib->pBeaconPayload = (uint8 *)macBeaconPayload;

    return MAC_SUCCESS;
  }
#else
  {
    return MAC_UNSUPPORTED_ATTRIBUTE;  
  }
#endif

  /* look up attribute in PIB table */
  if ((i = MAP_macPibIndex(pibAttribute)) == MAC_PIB_INVALID)
  {
    return MAC_UNSUPPORTED_ATTRIBUTE;
  }

#if defined(COMBO_MAC)
  if (pMacPib->rfFreq == MAC_RF_FREQ_2_4G)
  {
      if ((pibAttribute == MAC_PHY_CURRENT_DESCRIPTOR_ID) ||
         (pibAttribute == MAC_CHANNEL_PAGE))
      {
        return MAC_SUCCESS;
      }
  }
#else
#if defined(FREQ_2_4G)
  if ((pibAttribute == MAC_PHY_CURRENT_DESCRIPTOR_ID) ||
     (pibAttribute == MAC_CHANNEL_PAGE))
  {
    return MAC_SUCCESS;
  }
#endif
#endif

  /* do range check; no range check if min and max are zero */
  if ((macPibTbl[i].min != 0) || (macPibTbl[i].max != 0))
  {
    /* if min == max, this is a read-only attribute */
    if (macPibTbl[i].min == macPibTbl[i].max)
    {
      return MAC_READ_ONLY;
    }

    /* check for special cases */
    if (pibAttribute == MAC_MAX_FRAME_TOTAL_WAIT_TIME)
    {
      if ((*((uint16 *) pValue) < MAC_MAX_FRAME_RESPONSE_MIN) ||
          (*((uint16 *) pValue) > MAC_MAX_FRAME_RESPONSE_MAX))
      {
        return MAC_INVALID_PARAMETER;
      }
    }
    else if (pibAttribute == MAC_EBEACON_ORDER_NBPAN)
    {
      if ((*((uint16 *) pValue) < MAC_EBEACON_ORDER_NBPAN_MIN) ||
          (*((uint16 *) pValue) > MAC_EBEACON_ORDER_NBPAN_MAX))
      {
        return MAC_INVALID_PARAMETER;
      }
    }
    else if (pibAttribute == MAC_PHY_TRANSMIT_POWER_SIGNED)
    {
      if (MAP_macGetRadioTxPowerReg(*((int8*) pValue)) == MAC_RADIO_TX_POWER_INVALID)
      {
        return MAC_INVALID_PARAMETER;
      }
    }
    else if (pibAttribute == MAC_PHY_CURRENT_DESCRIPTOR_ID)
    {
      /* Only if the PhyID changes, reconfigure radio per PhyID */
      if (pMacPib->curPhyID == *((uint8*) pValue))
      {
        MAP_macSetDefaultsByPhyID();
        return MAC_SUCCESS;
      }
    }
    /* range check for general case */
    else if ((*((uint8 *) pValue) < macPibTbl[i].min) || (*((uint8 *) pValue) > macPibTbl[i].max))
    {
      return MAC_INVALID_PARAMETER;
    }

  }

  /* set value in PIB */
  HAL_ENTER_CRITICAL_SECTION(intState);
  MAP_osal_memcpy((uint8 *) pMacPib + macPibTbl[i].offset, pValue, macPibTbl[i].len);
  HAL_EXIT_CRITICAL_SECTION(intState);

  /* handle special cases */
  switch (pibAttribute)
  {
    case MAC_PAN_ID:
      /* set pan id in radio */
      MAP_macRadioSetPanID(pMacPib->panId);
#if defined(COMBO_MAC)
      if (pMacPib->rfFreq == MAC_RF_FREQ_2_4G)
      {
          if (macSrcMatchIsEnabled == FALSE)
          {
              MAP_MAC_SrcMatchEnable();
          }
      }
#else
#if defined(FREQ_2_4G)
      if (macSrcMatchIsEnabled == FALSE)
      {
          MAP_MAC_SrcMatchEnable();
      }
#endif
#endif
      break;

    case MAC_SHORT_ADDRESS:
      /* set short address in radio */
      MAP_macRadioSetShortAddr(pMacPib->shortAddress);
#if defined(COMBO_MAC)
      if (pMacPib->rfFreq == MAC_RF_FREQ_2_4G)
      {
          if (macSrcMatchIsEnabled == FALSE)
          {
              MAP_MAC_SrcMatchEnable();
          }
      }
#else
#if defined(FREQ_2_4G)
      if (macSrcMatchIsEnabled == FALSE)
      {
          MAP_MAC_SrcMatchEnable();
      }
#endif
#endif
      break;

    case MAC_RX_ON_WHEN_IDLE:
      /* turn rx on or off */
      if (pMacPib->rxOnWhenIdle)
      {
        MAP_macRxEnable(MAC_RX_WHEN_IDLE);
      }
      else
      {
        MAP_macRxDisable(MAC_RX_WHEN_IDLE);
      }
      break;

    case MAC_LOGICAL_CHANNEL:
      if (TRUE != MAP_macRadioSetChannel(pMacPib->logicalChannel))
      {
        return MAC_NO_RESOURCES;
      }
      break;

    case MAC_EXTENDED_ADDRESS:
      /* set ext address in radio */
      MAP_macRadioSetIEEEAddr(pMacPib->extendedAddress.addr.extAddr);
#if defined(COMBO_MAC)
      if (pMacPib->rfFreq == MAC_RF_FREQ_2_4G)
      {
          if (macSrcMatchIsEnabled == FALSE)
          {
            MAP_MAC_SrcMatchEnable();
          }
      }
#else
#if defined(FREQ_2_4G)
      if (macSrcMatchIsEnabled == FALSE)
      {
          MAP_MAC_SrcMatchEnable();
      }
#endif
#endif
      break;

    case MAC_PHY_TRANSMIT_POWER_SIGNED:
      MAP_macRadioSetTxPower(pMacPib->phyTransmitPower);
      break;

    case MAC_PHY_CURRENT_DESCRIPTOR_ID:
      MAP_macSetDefaultsByPhyID();
      MAP_MAC_ResumeReq();
      break;

    case MAC_RANGE_EXTENDER:
      MAP_macSetDefaultsByRE();
      MAP_macRadioSetRE(pMacPib->rangeExt);
      if (pMacPib->rangeExt)
      {
        MAP_MAC_ResumeReq();
      }
      break;

    default:
      MAP_macSetDefaultsByPatch(pibAttribute);
      break;
  }

  return MAC_SUCCESS;
}

/**************************************************************************************************
 * @fn          MAC_GetPHYParamReq
 *
 * @brief       This direct execute function retrieves an attribute value
 *              from the PHY descriptor related entries
 *
 * @param       phyAttribute - The attribute identifier.
 * @param       phyID - phyID corresponding to the attribute
 * @param       pValue - pointer to the attribute value.
 *
 * @return      The status of the request, as follows:
 *              MAC_SUCCESS Operation successful.
 *              MAC_UNSUPPORTED_ATTRIBUTE Attribute not found.
 *
 **************************************************************************************************
 */
uint8 MAC_GetPHYParamReq(uint8 phyAttribute, uint8 phyID, void *pValue)
{
  /* phyAttribute can only be MAC_PHY_DESCRIPTOR */
  macPHYDesc_t *pPhyDesc = NULL;
  uint8 status = MAC_SUCCESS;

  MAC_PARAM_STATUS( pValue != NULL, status );

  if ( MAC_SUCCESS != status )
  {
    return status;
  }

  switch (phyAttribute)
  {
    case MAC_PHY_DESCRIPTOR:
#if defined(COMBO_MAC) || !defined(FREQ_2_4G)
      if (phyID)
      {
          pPhyDesc = MAP_macMRFSKGetPhyDesc(phyID);
      }
#endif
#if defined(COMBO_MAC) || defined(FREQ_2_4G)
      if (!phyID)
      {
          pPhyDesc = MAP_macIEEEGetPhyDesc(phyID);
      }
#endif

      if ( pPhyDesc != NULL )
      {
        MAP_osal_memcpy(((macPHYDesc_t *)pValue), pPhyDesc, sizeof(macPHYDesc_t) );
      }
      else
      {
        return MAC_INVALID_PARAMETER;
      }

      break;

    default:
      return MAC_UNSUPPORTED_ATTRIBUTE;
  }
  return status;
}

/**************************************************************************************************
 * @fn          MAC_SetPHYParamReq
 *
 * @brief       This direct execute function sets an attribute value
 *              from the PHY descriptor related entries
 *              Note: this function will cause a radio restart.
 *
 * input parameters
 *
 * @param       phyAttribute - The attribute identifier.
*  @param       phyID corresponding to the attribute
 * @param       pValue - pointer to the attribute value.
 *
 * @return      The status of the request, as follows:
 *              MAC_SUCCESS Operation successful.
 *              MAC_UNSUPPORTED_ATTRIBUTE Attribute not found.
 *
 **************************************************************************************************
 */
uint8 MAC_SetPHYParamReq(uint8 phyAttribute, uint8 phyID, void *pValue)
{
  uint8 status = MAC_SUCCESS;
  MAC_PARAM_STATUS( pValue != NULL, status );

  if ( MAC_SUCCESS != status )
  {
    return status;
  }
#if defined(COMBO_MAC) || !defined(FREQ_2_4G)
  /* phyAttribute can only be MAC_PHY_DESCRIPTOR */
  switch (phyAttribute)
  {
    case MAC_PHY_DESCRIPTOR:

       if ( ( phyID >= MAC_MRFSK_GENERIC_PHY_ID_BEGIN ) &&
            ( phyID <= MAC_MRFSK_GENERIC_PHY_ID_END) )
       {
         MAP_osal_memcpy(&macMRFSKGenPhyTable[phyID - MAC_MRFSK_GENERIC_PHY_ID_BEGIN] , pValue,
                     sizeof(macPHYDesc_t));

         /* Reconfigure radio per phyID */
       }
       else
       {
         /* TBD if standard PHY descriptors can be overwritten and/or need to
          * to set valid flag for them */
         return MAC_INVALID_PARAMETER;
       }

       break;

    default:
      return MAC_UNSUPPORTED_ATTRIBUTE;
  }
#endif
  return status;
}
#if defined(COMBO_MAC) || !defined(FREQ_2_4G)
/**************************************************************************************************
 * @fn          macMRFSKGetPhyDesc
 *
 * @brief       This function selects the indexed MR-FSK PHY descriptor
 *
 * input parameters
 *
 * @param       phyID - index of the PHY descriptor
 *
 * @return      pValue - pointer to the PHY descriptor
 *
 **************************************************************************************************
 */
MAC_INTERNAL_API macMRFSKPHYDesc_t* macMRFSKGetPhyDesc(uint8 phyID)
{
  macMRFSKPHYDesc_t *pMRFSKDesc = NULL;

  if (MAP_macCheckPhyMode(phyID) == PHY_MODE_STD)
  {
    pMRFSKDesc = (macMRFSKPHYDesc_t*) &macMRFSKStdPhyTable[phyID - MAC_MRFSK_STD_PHY_ID_BEGIN];
  }
  else if (MAP_macCheckPhyMode(phyID) == PHY_MODE_GEN)
  {
    pMRFSKDesc = (macMRFSKPHYDesc_t*) &macMRFSKGenPhyTable[phyID - MAC_MRFSK_GENERIC_PHY_ID_BEGIN];
  }
  else
  {
    MAC_ASSERT_FORCED();
  }
  return pMRFSKDesc;
}
#endif
#if defined(COMBO_MAC) || defined(FREQ_2_4G)
/**************************************************************************************************
 * @fn          macIEEEGetPhyDesc
 *
 * @brief       This function selects the IEEE PHY descriptor
 *
 * input parameters
 *
 * @param       phyID - index of the PHY descriptor
 *
 * @return      pValue - pointer to the PHY descriptor
 *
 **************************************************************************************************
 */
MAC_INTERNAL_API macIEEEPHYDesc_t* macIEEEGetPhyDesc(uint8 phyID)
{
  (void) phyID;

  return (macIEEEPHYDesc_t*)&macIEEEPhyTable;
}
#endif
