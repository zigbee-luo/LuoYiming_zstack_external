/******************************************************************************

 @file  mac_security_pib.c

 @brief This module contains procedures for the Security-related MAC PIB.

 Group: WCS, LPC
 Target Device: cc13x2_26x2

 ******************************************************************************
 
 Copyright (c) 2010-2019, Texas Instruments Incorporated
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
#ifndef LIBRARY
#include "ti_154stack_features.h"
#endif

#ifdef FEATURE_MAC_SECURITY
/* ------------------------------------------------------------------------------------------------
 *                                          Includes
 * ------------------------------------------------------------------------------------------------
 */
#include "hal_mcu.h"
#include "mac_api.h"
#include "mac_spec.h"
#include "mac_low_level.h"
#include "mac_main.h"
#include "mac_security_pib.h"
#include "mac_pib.h"
#include "mac_assert.h"
#include <stddef.h>

/******************************************************************************
  ROM jump table will support
  1. TIMAC ROM image build
  2. TIMAC stack build without ROM image
  3. TIMAC stack with calling ROM image
 *****************************************************************************/
#include "rom_jt_154.h"

/* ------------------------------------------------------------------------------------------------
 *                                           Constants
 * ------------------------------------------------------------------------------------------------
 */

/* Attribute index constants, based on attribute ID values from spec */
#define MAC_ATTR_SECURITY_SET1_START       0x81
#define MAC_ATTR_SECURITY_SET1_END         0x8B
#define MAC_ATTR_SECURITY_SET1_OFFSET      0
#define MAC_ATTR_SECURITY_SET2_START       0xD0
#define MAC_ATTR_SECURITY_SET2_END         0xD5




uint8 MAC_MlmeGetSecurityReqSize( uint8 pibAttribute );

#if defined( FEATURE_MAC_PIB_PTR )

/**************************************************************************************************
 * @fn          MAC_MlmeSetActiveSecurityPib
 *
 * @brief       This direct execute function sets the active MAC security PIB.
 *
 * input parameters
 *
 * @param       pPib - pointer to the PIB structure.
 *
 * output parameters
 *
 * @return      None.
 *
 **************************************************************************************************
 */
void MAC_MlmeSetActiveSecurityPib( void* pSecPib)
{
  pMacSecurityPib = (macSecurityPib_t *)pSecPib;
}
#endif /* FEATURE_MAC_PIB_PTR */

/**************************************************************************************************
 * @fn          macSecurityPibReset
 *
 * @brief       This function initializes the security related PIB.
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
MAC_INTERNAL_API void macSecurityPibReset(void)
{
  uint8 kk;

  /* copy security related PIB defaults */
#if defined( FEATURE_MAC_PIB_PTR )
  *pMacSecurityPib = macSecurityPibDefaults;
#else

#ifdef STATIC_MAC_SECURITY_DEFAULTS
  macSecurityPib = macSecurityPibDefaults;

  /* Fix up arrays that can change size */

  /* Fix up for MAX_DEVICE_TABLE_ENTRIES */
  {
      uint16 x;
      for ( x = 0; x < macCfg.maxKeyTableEntries; x++ )
      {
          uint16 y;
          for ( y = 0; y < macCfg.maxDeviceTableEntries; y++ )
          {
              macSecurityPib.macKeyDeviceList[x][y].deviceDescriptorHandle = 0;
              macSecurityPib.macKeyDeviceList[x][y].uniqueDevice = false;
              macSecurityPib.macKeyDeviceList[x][y].blackListed = false;
          }
      }

      for ( x = 0; x < macCfg.maxDeviceTableEntries; x++ )
      {
          MAP_osal_memcpy( &macSecurityPib.macDeviceTable[x],
                       &macSecurityPibDefaults.macDeviceTable[0],
                       sizeof(deviceDescriptor_t));
      }
  }
#else
  /* init the macPib to zero */
  MAP_osal_memset(&macSecurityPib, 0x00, sizeof(macSecurityPib));

  /* create the macDeviceTable dynamically */
  {
      uint16 x;

      /* Alloc macKeyTable[macCfg.maxKeyTableEntries] */
      pMacSecurityPib->macKeyTable = MAP_osal_mem_alloc(macCfg.maxKeyTableEntries * sizeof (keyDescriptor_t));
      MAC_ASSERT( pMacSecurityPib->macKeyTable != NULL);

      pMacSecurityPib->macKeyIdLookupList = MAP_osal_mem_alloc(macCfg.maxKeyTableEntries * macCfg.maxKeyIdLookUpEntries * sizeof (keyIdLookupDescriptor_t));
      MAC_ASSERT(pMacSecurityPib->macKeyIdLookupList != NULL );

      /*Alloc macKeyDeviceList[macCfg.maxKeyTableEntries][macCfg.maxKeyDeviceTableEntries] */

      pMacSecurityPib->macKeyDeviceList = MAP_osal_mem_alloc( macCfg.maxKeyTableEntries * macCfg.maxKeyDeviceTableEntries * sizeof (keyDeviceDescriptor_t));
      MAC_ASSERT(pMacSecurityPib->macKeyDeviceList != NULL );

      /*Alloc macKeyUsageList[macCfg.maxKeyTableEntries][MAX_KEY_USAGE_TABLE_ENTRIES] */

      pMacSecurityPib->macKeyUsageList = MAP_osal_mem_alloc(macCfg.maxKeyTableEntries * MAX_KEY_USAGE_TABLE_ENTRIES * sizeof (keyUsageDescriptor_t));
      MAC_ASSERT(pMacSecurityPib->macKeyUsageList != NULL );

      for (x = 0; x < macCfg.maxKeyTableEntries; x++)
      {
          pMacSecurityPib->macKeyTable[x].keyUsageListEntries = 0;
          pMacSecurityPib->macKeyTable[x].keyIdLookupEntries = 0;
          pMacSecurityPib->macKeyTable[x].keyDeviceListEntries = 0;
      }

      /* ALloc macDeviceTable[macCfg.maxDeviceTableEntries] */
      pMacSecurityPib->macDeviceTable = MAP_osal_mem_alloc(macCfg.maxDeviceTableEntries * sizeof (deviceDescriptor_t));
      MAC_ASSERT( pMacSecurityPib->macDeviceTable != NULL);

      for (x = 0; x < macCfg.maxDeviceTableEntries; x++)
      {
          pMacSecurityPib->macDeviceTable[x].frameCtr = MAP_osal_mem_alloc(macCfg.maxNodeKeyTableEntries * sizeof (frameCntr_t));
          MAC_ASSERT(pMacSecurityPib->macDeviceTable[x].frameCtr != NULL );
          for (kk = 0; kk < macCfg.maxNodeKeyTableEntries; kk++)
          {
              pMacSecurityPib->macDeviceTable[x].frameCtr[kk].frameCounter = 0;
              pMacSecurityPib->macDeviceTable[x].frameCtr[kk].frameCntrIdx = 0xffff;
          }
      }
  }

  {
      uint16_t x;

      /* auto request key source
       * {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  autoRequestKeySource
       */
      MAP_osal_memset(&macSecurityPib.autoRequestKeySource[0], 0xFF,
                  sizeof(macSecurityPib.autoRequestKeySource));
      macSecurityPib.autoRequestKeyIndex = 0xFF;
      MAP_osal_memset(&macSecurityPib.defaultKeySource[0], 0xFF,
                  sizeof(macSecurityPib.defaultKeySource));

      macSecurityPib.panCoordExtendedAddress.addrMode = SADDR_MODE_EXT;
      macSecurityPib.panCoordShortAddress = MAC_SHORT_ADDR_NONE;

      /* macKeyusageList */
      for (x = 0; x < macCfg.maxKeyTableEntries; x++)
      {
          macSecurityPib.macKeyUsageList[x * MAX_KEY_USAGE_TABLE_ENTRIES + 0].frameType = MAC_FRAME_TYPE_DATA;
          macSecurityPib.macKeyUsageList[x * MAX_KEY_USAGE_TABLE_ENTRIES + 0].cmdFrameId = MAC_DATA_REQ_FRAME;

          macSecurityPib.macKeyUsageList[x * MAX_KEY_USAGE_TABLE_ENTRIES + 1].frameType = MAC_FRAME_TYPE_COMMAND;
          macSecurityPib.macKeyUsageList[x * MAX_KEY_USAGE_TABLE_ENTRIES + 1].cmdFrameId = MAC_DATA_REQ_FRAME;
      }

      /* key secuirty level table */
      for (x =0 ; x < MAX_SECURITY_LEVEL_TABLE_ENTRIES; x++)
      {
          macSecurityPib.macSecurityLevelTable[x].frameType = MAC_FRAME_TYPE_DATA;
          macSecurityPib.macSecurityLevelTable[x].commandFrameIdentifier = MAC_DATA_REQ_FRAME;
          macSecurityPib.macSecurityLevelTable[x].securityMinimum = MAC_SEC_LEVEL_ENC_MIC_32;
          macSecurityPib.macSecurityLevelTable[x].securityOverrideSecurityMinimum = TRUE;
      }

      macSecurityPib.macSecurityLevelTable[1].frameType = MAC_FRAME_TYPE_COMMAND;

      /* device short address to 0xFFFF */
      for (x = 0; x < macCfg.maxDeviceTableEntries; x++)
      {
          macSecurityPib.macDeviceTable[x].shortAddress = 0xFFFF;
      }

  }
#endif

#endif /* FEATURE_MAC_PIB_PTR */

  pMacSecurityPib->securityLevelTableEntries = MAX_SECURITY_LEVEL_TABLE_ENTRIES;

}

/**************************************************************************************************
 * @fn          macSecurityPibIndex
 *
 * @brief       This function takes an security related PIB attribute and returns the index in to
 *              macSecurityPibTbl for the attribute.
 *
 * input parameters
 *
 * @param       pibAttribute - Security related PIB attribute to look up.
 *
 * output parameters
 *
 * None.
 *
 * @return      Index in to macSecurityPibTbl for the attribute or MAC_SECURITY_PIB_INVALID.
 **************************************************************************************************
 */
MAC_INTERNAL_API uint8 macSecurityPibIndex(uint8 pibAttribute)
{
  if ((pibAttribute >= MAC_ATTR_SECURITY_SET1_START) && (pibAttribute <= MAC_ATTR_SECURITY_SET1_END))
  {
    return (pibAttribute - MAC_ATTR_SECURITY_SET1_START + MAC_ATTR_SECURITY_SET1_OFFSET);
  }
  else
  {
    return MAC_SECURITY_PIB_INVALID;
  }
}


/**************************************************************************************************
 * @fn          MAC_MlmeGetSecurityReq
 *
 * @brief       This direct execute function retrieves an attribute value
 *              from the MAC security related PIB.
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
uint8 MAC_MlmeGetSecurityReq(uint8 pibAttribute, void *pValue)
{
  uint16        i, keyIndex;
  uint16        device_entry;
  uint8         entry;
  halIntState_t intState;

  /* Special handling for proprietary Security PIB Get and Set Attributes */
  switch (pibAttribute)
  {
    case MAC_KEY_TABLE:
    case MAC_DEVICE_TABLE:
    case MAC_SECURITY_LEVEL_TABLE:
      /* It does not make sense to return a target pointer. */
      return MAC_INVALID_PARAMETER;

    case MAC_FRAME_COUNTER:
      return MAC_UNSUPPORTED_ATTRIBUTE;

    case MAC_KEY_ID_LOOKUP_ENTRY:
      keyIndex = ((macSecurityPibKeyIdLookupEntry_t *)pValue)->key_index;
      entry    = ((macSecurityPibKeyIdLookupEntry_t *)pValue)->key_id_lookup_index;
      if (keyIndex >= macCfg.maxKeyTableEntries || entry >= pMacSecurityPib->macKeyTable[keyIndex].keyIdLookupEntries)
      {
        return MAC_INVALID_PARAMETER;
      }

      MAP_osal_memcpy(&((macSecurityPibKeyIdLookupEntry_t *)pValue)->macKeyIdLookupEntry, \
                      &pMacSecurityPib->macKeyIdLookupList[keyIndex * macCfg.maxKeyIdLookUpEntries + entry], sizeof(keyIdLookupDescriptor_t));

      return MAC_SUCCESS;

    case MAC_KEY_DEVICE_ENTRY:
      keyIndex = ((macSecurityPibKeyDeviceEntry_t *)pValue)->key_index;
      device_entry    = ((macSecurityPibKeyDeviceEntry_t *)pValue)->key_device_index;
      if (keyIndex >= macCfg.maxKeyTableEntries || device_entry >= pMacSecurityPib->macKeyTable[keyIndex].keyDeviceListEntries)
      {
        return MAC_INVALID_PARAMETER;
      }

      MAP_osal_memcpy(&((macSecurityPibKeyDeviceEntry_t *)pValue)->macKeyDeviceEntry, \
                      &pMacSecurityPib->macKeyDeviceList[keyIndex * macCfg.maxKeyDeviceTableEntries + device_entry], sizeof(keyDeviceDescriptor_t));

      return MAC_SUCCESS;

    case MAC_KEY_USAGE_ENTRY:
      keyIndex = ((macSecurityPibKeyUsageEntry_t *)pValue)->key_index;
      entry    = ((macSecurityPibKeyUsageEntry_t *)pValue)->key_key_usage_index;
      if (keyIndex >= macCfg.maxKeyTableEntries || entry >= pMacSecurityPib->macKeyTable[keyIndex].keyUsageListEntries)
      {
        return MAC_INVALID_PARAMETER;
      }

      MAP_osal_memcpy(&((macSecurityPibKeyUsageEntry_t *)pValue)->macKeyUsageEntry, \
                      &pMacSecurityPib->macKeyUsageList[keyIndex * MAX_KEY_USAGE_TABLE_ENTRIES + entry], sizeof(keyUsageDescriptor_t));

      return MAC_SUCCESS;

    case MAC_KEY_ENTRY:
      keyIndex = ((macSecurityPibKeyEntry_t *)pValue)->key_index;
      if (keyIndex >= macCfg.maxKeyTableEntries)
      {
        return MAC_INVALID_PARAMETER;
      }
      MAP_osal_memcpy(((macSecurityPibKeyEntry_t *)pValue)->keyEntry, pMacSecurityPib->macKeyTable[keyIndex].key, MAC_KEY_MAX_LEN);
      ((macSecurityPibKeyEntry_t *)pValue)->frameCounter = pMacSecurityPib->macKeyTable[keyIndex].frameCounter;
      return MAC_SUCCESS;

    case MAC_DEVICE_ENTRY:
      device_entry = ((macSecurityPibDeviceEntry_t *)pValue)->device_index;
      if (device_entry >= macCfg.maxDeviceTableEntries)
      {
        return MAC_INVALID_PARAMETER;
      }
      //MAP_osal_memcpy(&((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry, &pMacSecurityPib->macDeviceTable[device_entry], sizeof(deviceDescriptor_t));
      ((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry.panID = pMacSecurityPib->macDeviceTable[device_entry].panID;
      ((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry.shortAddress = pMacSecurityPib->macDeviceTable[device_entry].shortAddress;
      MAP_osal_memcpy(&((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry.extAddress,
                      &pMacSecurityPib->macDeviceTable[device_entry].extAddress,
                      sizeof(sAddrExt_t));
      MAP_osal_memcpy(((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry.frameCtr,
                      pMacSecurityPib->macDeviceTable[device_entry].frameCtr,
                      (macCfg.maxNodeKeyTableEntries*sizeof(frameCntr_t)));
      ((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry.exempt = pMacSecurityPib->macDeviceTable[device_entry].exempt;
      return MAC_SUCCESS;

    case MAC_SECURITY_LEVEL_ENTRY:
      entry = ((macSecurityPibSecurityLevelEntry_t *)pValue)->security_level_index;
      if (entry >= MAX_SECURITY_LEVEL_TABLE_ENTRIES)
      {
        return MAC_INVALID_PARAMETER;
      }
      MAP_osal_memcpy(&((macSecurityPibSecurityLevelEntry_t *)pValue)->macSecurityLevelEntry, &pMacSecurityPib->macSecurityLevelTable[entry], sizeof(securityLevelDescriptor_t));
      return MAC_SUCCESS;
  }

  if ((i = MAP_macSecurityPibIndex(pibAttribute)) == MAC_SECURITY_PIB_INVALID)
  {
    return MAC_UNSUPPORTED_ATTRIBUTE;
  }

  HAL_ENTER_CRITICAL_SECTION(intState);
  MAP_osal_memcpy(pValue, (uint8 *) pMacSecurityPib + macSecurityPibTbl[i].offset, macSecurityPibTbl[i].len);
  HAL_EXIT_CRITICAL_SECTION(intState);
  return MAC_SUCCESS;
}


/**************************************************************************************************
 * @fn          MAC_MlmeGetPointerSecurityReq
 *
 * @brief       This direct execute function gets the MAC security PIB attribute pointers
 *
 * input parameters
 *
 * @param       pibAttribute - The attribute identifier.
 *
 * output parameters
 *
 * @param       pValue will contain a pointer to the attribute in macPib
 *
 * @return      MAC_SUCCESS
 *
 **************************************************************************************************
 */
MAC_INTERNAL_API uint8 MAC_MlmeGetPointerSecurityReq(uint8 pibAttribute, void **pValue)
{
  switch (pibAttribute)
  {
  case MAC_KEY_TABLE:
    *pValue = pMacSecurityPib->macKeyTable;
    return MAC_SUCCESS;
  case MAC_DEVICE_TABLE:
    *pValue =  pMacSecurityPib->macDeviceTable;
    return MAC_SUCCESS;
  case MAC_SECURITY_LEVEL_TABLE:
    *pValue = pMacSecurityPib->macSecurityLevelTable;
    return MAC_SUCCESS;
  default:
    return MAC_INVALID_PARAMETER;
  }
}


/**************************************************************************************************
 * @fn          MAC_MlmeGetSecurityReqSize
 *
 * @brief       This direct execute function gets the MAC security PIB attribute size
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
uint8 MAC_MlmeGetSecurityReqSize( uint8 pibAttribute )
{
  uint8 len = 0;

  /* Special handling for proprietary Security PIB Get and Set Attributes */
  switch (pibAttribute)
  {
    case MAC_KEY_ID_LOOKUP_ENTRY:
      len = (uint8)(sizeof(macSecurityPibKeyIdLookupEntry_t));
      break;

    case MAC_KEY_DEVICE_ENTRY:
      len = (uint8)(sizeof(macSecurityPibKeyDeviceEntry_t));
      break;

    case MAC_KEY_USAGE_ENTRY:
      len = (uint8)(sizeof(macSecurityPibKeyUsageEntry_t));
      break;

    case MAC_KEY_ENTRY:
      len = (uint8)(sizeof(macSecurityPibKeyEntry_t));
      break;

    case MAC_DEVICE_ENTRY:
      len = (uint8)(sizeof(macSecurityPibDeviceEntry_t));
      break;

    case MAC_SECURITY_LEVEL_ENTRY:
      len = (uint8)(sizeof(macSecurityPibSecurityLevelEntry_t));
      break;

    default:
      {
        uint8 i;
        if ((i = MAP_macSecurityPibIndex(pibAttribute)) != MAC_SECURITY_PIB_INVALID)
        {
          len = macSecurityPibTbl[i].len;
        }
      }
      break;
  }

  return ( len );
}


/**************************************************************************************************
 * @fn          MAC_MlmeSetSecurityReq
 *
 * @brief       This direct execute function sets an attribute value
 *              in the MAC security related PIB.
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
uint8 MAC_MlmeSetSecurityReq(uint8 pibAttribute, void *pValue)
{
  uint16        i, keyIndex;
  uint8         entry;
  uint16        device_entry;
  halIntState_t intState;

  /* Special handling for proprietary Security PIB Get and Set Attributes */
  switch (pibAttribute)
  {
    case MAC_FRAME_COUNTER:
      return MAC_UNSUPPORTED_ATTRIBUTE;

    case MAC_KEY_TABLE:
      if (pValue == NULL)
      {
        /* If the pValue is null, the PIB entries are already created by the
         * proprietary security PIB set requests. This call simply builds the table.
         */
        for(keyIndex = 0; keyIndex < macCfg.maxKeyTableEntries; keyIndex++)
        {
          /* Build the key table by assigning the corresponding pointers. Note that the number
           * of entries are filled when the individual entry is received. This also assume the list
           * must be received in sequential order from small to large.
           */

          pMacSecurityPib->macKeyTable[keyIndex].keyIdLookupList = &pMacSecurityPib->macKeyIdLookupList[keyIndex * macCfg.maxKeyIdLookUpEntries];
          pMacSecurityPib->macKeyTable[keyIndex].keyDeviceList   = &pMacSecurityPib->macKeyDeviceList[keyIndex * macCfg.maxKeyDeviceTableEntries];
          pMacSecurityPib->macKeyTable[keyIndex].keyUsageList    = &pMacSecurityPib->macKeyUsageList[keyIndex * MAX_KEY_USAGE_TABLE_ENTRIES];

        }
      }
      else
      {
        MAP_osal_memcpy(&pMacSecurityPib->macKeyTable, pValue, sizeof(pMacSecurityPib->macKeyTable));
      }

      return MAC_SUCCESS;

    case MAC_KEY_ID_LOOKUP_ENTRY:
      keyIndex = ((macSecurityPibKeyIdLookupEntry_t *)pValue)->key_index;
      entry    = ((macSecurityPibKeyIdLookupEntry_t *)pValue)->key_id_lookup_index;
      if (keyIndex >= macCfg.maxKeyTableEntries || entry >= macCfg.maxKeyIdLookUpEntries)
      {
        return MAC_INVALID_PARAMETER;
      }
      pMacSecurityPib->macKeyTable[keyIndex].keyIdLookupEntries = entry+1;

      MAP_osal_memcpy(&pMacSecurityPib->macKeyIdLookupList[keyIndex * macCfg.maxKeyIdLookUpEntries + entry], \
                      &((macSecurityPibKeyIdLookupEntry_t *)pValue)->macKeyIdLookupEntry, sizeof(keyIdLookupDescriptor_t));

      return MAC_SUCCESS;

    case MAC_KEY_DEVICE_ENTRY:
      keyIndex = ((macSecurityPibKeyDeviceEntry_t *)pValue)->key_index;
      device_entry    = ((macSecurityPibKeyDeviceEntry_t *)pValue)->key_device_index;
      if (keyIndex >= macCfg.maxKeyTableEntries || device_entry >= macCfg.maxKeyDeviceTableEntries)
      {
        return MAC_INVALID_PARAMETER;
      }
      /* Key device entry may be overwritten individually and hence table size must not be
       * determined based on last entry written.
       * However the following variable is still used just to ensure that only initialized entries
       * are accessed. */
      if (pMacSecurityPib->macKeyTable[keyIndex].keyDeviceListEntries <= device_entry)
      {
        pMacSecurityPib->macKeyTable[keyIndex].keyDeviceListEntries = device_entry+1;
      }

      MAP_osal_memcpy(&pMacSecurityPib->macKeyDeviceList[keyIndex * macCfg.maxKeyDeviceTableEntries + device_entry], \
                      &((macSecurityPibKeyDeviceEntry_t *)pValue)->macKeyDeviceEntry, sizeof(keyDeviceDescriptor_t));

      return MAC_SUCCESS;

    case MAC_KEY_USAGE_ENTRY:
      keyIndex = ((macSecurityPibKeyUsageEntry_t *)pValue)->key_index;
      entry    = ((macSecurityPibKeyUsageEntry_t *)pValue)->key_key_usage_index;
      if (keyIndex >= macCfg.maxKeyTableEntries || entry >= MAX_KEY_USAGE_TABLE_ENTRIES)
      {
        return MAC_INVALID_PARAMETER;
      }
      pMacSecurityPib->macKeyTable[keyIndex].keyUsageListEntries = entry+1;

      MAP_osal_memcpy(&pMacSecurityPib->macKeyUsageList[keyIndex*MAX_KEY_USAGE_TABLE_ENTRIES + entry], &((macSecurityPibKeyUsageEntry_t *)pValue)->macKeyUsageEntry, sizeof(keyUsageDescriptor_t));

      return MAC_SUCCESS;

    case MAC_KEY_ENTRY:
      keyIndex = ((macSecurityPibKeyEntry_t *)pValue)->key_index;
      if (keyIndex >= macCfg.maxKeyTableEntries)
      {
        return MAC_INVALID_PARAMETER;
      }
      MAP_osal_memcpy(pMacSecurityPib->macKeyTable[keyIndex].key, ((macSecurityPibKeyEntry_t *)pValue)->keyEntry, MAC_KEY_MAX_LEN);
      pMacSecurityPib->macKeyTable[keyIndex].frameCounter = ((macSecurityPibKeyEntry_t *)pValue)->frameCounter;

      return MAC_SUCCESS;

    case MAC_DEVICE_TABLE:
      if (pValue != NULL)
      {
        /* If the pValue is null, the PIB entries are already created by the
         * proprietary security PIB set requests. This call simply builds the table.
         */
        MAP_osal_memcpy(&pMacSecurityPib->macDeviceTable, pValue, sizeof(pMacSecurityPib->macDeviceTable));
      }
      return MAC_SUCCESS;

    case MAC_SECURITY_LEVEL_TABLE:
      if (pValue != NULL)
      {
        /* If the pValue is null, the PIB entries are already created by the
         * proprietary security PIB set requests. This call simply builds the table.
         */
        MAP_osal_memcpy(&pMacSecurityPib->macSecurityLevelTable, pValue, sizeof(pMacSecurityPib->macSecurityLevelTable));
      }
      return MAC_SUCCESS;

    case MAC_DEVICE_ENTRY:
      device_entry = ((macSecurityPibDeviceEntry_t *)pValue)->device_index;
      if (device_entry >= macCfg.maxDeviceTableEntries)
      {
        return MAC_INVALID_PARAMETER;
      }
      //MAP_osal_memcpy(&pMacSecurityPib->macDeviceTable[device_entry], &((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry, sizeof(deviceDescriptor_t));
      pMacSecurityPib->macDeviceTable[device_entry].panID = ((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry.panID;
      pMacSecurityPib->macDeviceTable[device_entry].shortAddress = ((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry.shortAddress;
      MAP_osal_memcpy(&pMacSecurityPib->macDeviceTable[device_entry].extAddress,
                      &((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry.extAddress,
                      sizeof(sAddrExt_t));
      MAP_osal_memcpy(pMacSecurityPib->macDeviceTable[device_entry].frameCtr,
                      ((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry.frameCtr,
                      (macCfg.maxNodeKeyTableEntries*sizeof(frameCntr_t)));
      pMacSecurityPib->macDeviceTable[device_entry].exempt = ((macSecurityPibDeviceEntry_t *)pValue)->macDeviceEntry.exempt;
      return MAC_SUCCESS;

    case MAC_SECURITY_LEVEL_ENTRY:
      entry = ((macSecurityPibSecurityLevelEntry_t *)pValue)->security_level_index;
      if (entry >= MAX_SECURITY_LEVEL_TABLE_ENTRIES)
      {
        return MAC_INVALID_PARAMETER;
      }
      MAP_osal_memcpy(&pMacSecurityPib->macSecurityLevelTable[entry], &((macSecurityPibSecurityLevelEntry_t *)pValue)->macSecurityLevelEntry, sizeof(securityLevelDescriptor_t));
      return MAC_SUCCESS;

  }

  /* look up attribute in security related PIB table */
  if ((i = MAP_macSecurityPibIndex(pibAttribute)) == MAC_SECURITY_PIB_INVALID)
  {
    return MAC_UNSUPPORTED_ATTRIBUTE;
  }

  /* do range check; no range check if min and max are zero */
  if ((macSecurityPibTbl[i].min != 0) || (macSecurityPibTbl[i].max != 0))
  {
    /* if min == max, this is a read-only attribute */
    if (macSecurityPibTbl[i].min == macSecurityPibTbl[i].max)
    {
      return MAC_READ_ONLY;
    }

    /* range check for general case */
    if ((*((uint8 *) pValue) < macSecurityPibTbl[i].min) || (*((uint8 *) pValue) > macSecurityPibTbl[i].max))
    {
      return MAC_INVALID_PARAMETER;
    }
  }

  /* set value in security related PIB */
  HAL_ENTER_CRITICAL_SECTION(intState);
  MAP_osal_memcpy((uint8 *) pMacSecurityPib + macSecurityPibTbl[i].offset, pValue, macSecurityPibTbl[i].len);
  HAL_EXIT_CRITICAL_SECTION(intState);

  return MAC_SUCCESS;
}

#endif
