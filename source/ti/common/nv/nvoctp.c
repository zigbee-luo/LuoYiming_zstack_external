/******************************************************************************

 @file  nvoctp.c

 @brief NV driver for CC26x2 devices - On-Chip Two-Page Flash Memory

 Group: CMCU, LPC
 Target Device: cc13x2_26x2

 ******************************************************************************
 
 Copyright (c) 2018-2020, Texas Instruments Incorporated
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

//*****************************************************************************
// Design Overview
//*****************************************************************************
/*
This driver implements a non-volatile (NV) memory system that utilizes 2 pages
(consecutive) of on-chip Flash memory. After initialization, one page is ACTIVE
and the other page is available for "compaction" when the ACTIVE page does not
have enough empty space for data write operation. Compaction can occur 'just in
time' during a data write operation or 'on demand' by application request. The
compaction process is designed to survive a power cycle before it completes. It
will resume where it was interrupted and complete the process.

This driver makes the following assumptions and uses them to optimize the code
and data storage design: (1) Flash memory is addressable at individual, 1-byte
resolution so no padding or word-alignment is necessary (2) Flash has limited
number of writes per flash 'sector' between erases. To prevent going over this
limit, "small" items are written in one operation.

Each Flash page has a "page header" which indicates its current state (ERASED,
ACTIVE, or XFER), located at the first byte of the Flash page. The remainder of
the Flash page contains NV data items which are packed together following the
page header. Each NV data item has two parts, (1) a data block which is stored
first (lower memory address), (2) immediately followed by item header (higher
memory address). The item header contains information necessary to traverse the
packed data items, as well as, current status of each data item. Obsolete items
marked accordingly but a search for the newest instance of an item is sped up
by starting the search at the last entry in the page (higher memory address).

Each item is unique, addressed using three ID values (system ID, item ID, sub
ID). These three values are stored in the header along with a 'signature', a
CRC8 value, the length of the data block, and two status bits. The two status
bits indicate whether an item is still active and the health or validity of an
item. The signature byte is used by the driver to detect the presence of an
item, and is the same for all items as well as the page header. The CRC8 value
allows the driver to confirm the integrity of the items during compaction and
optionally when an item read operation is requested. The length of the data
block is used to jump from one item to the next. If this field is corrupted,
the driver is forced to search for items by signature and possibly compute
multiple CRC's to confirm it has found a valid item. Note that any corruption
event forces a compaction to recover.
*/
//*****************************************************************************
// Use / Configuration
//*****************************************************************************
/*
"nvintf.h" describes the generic NV interface which is used to access NVOCTP
after initialization. Initialization is done by passing a function pointer
struct to one of NVOCTPs pointer loader functions. Once this is done, the
pointer struct (which is part of the nvintf interface) should be used to call
the nvintf initialization function, which will initialize NVOCTP. At this point
NVOCTP is ready and loaded API functions can be called through the pointer
structure. Note that some pointers may be NULL depending on which NVOCTP
loader function was called. For example NVOCTP_loadApiPtrsMin() loads only
the essential functions to reduce code size at link time.
A sample code block is shown below:

NVINTF_nvFuncts_t nvFps;
NVOCTP_loadApiPtrs(&nvFps);

nvFps.initNV(NULL);
// Do some NV operations
nvFps.compactNV(NULL);
status = nvFps.readItem(id, 0, len, buf);

Note: Each item operation results in a traversal of the page starting at
the most recently written item. This makes 'finding' items by 'trying' item IDs
in order extremely inefficient. The doNext() API call allows the user to find,
read, or delete items in one page traversal. However, this call requires the
user to lock access to NV until the operation is complete so it should be used
carefully and sparingly.

Note: The compile flag NVDEBUG can be passed to enable ASSERT and ALERT
macros which provide assert and logging functionality. When this flag is used,
a printf function of the form void nvprint(char * str) MUST be
provided to link the driver. The function need not be functional, but it must
exist. NVDEBUG also exposes driver global variables for debug and testing.

Configuration:
NVOCTP_STATS - Places a protected item with driver stats
NVOCTP_CRCONREAD (on:1 off:0) - item crc is checked on read. Disabling this may
increase driver speed but safety is reduced.
NVOCTP_NVS_INDEX - The index of the NVS_Config structure which describes the
flash sector that NVOCTP should use. Default is 0.

Dependencies:
Requires NVS for NV access.
Requires TI-RTOS GateMutexPri to be enabled in configuration.
Requires API's in a crc.h to implement CRC functionality.
*/

//*****************************************************************************
// Includes
//*****************************************************************************

#include <string.h>
#include <ti/sysbios/gates/GateMutexPri.h>

#include "nvoctp.h"
#include "crc.h"

//*****************************************************************************
// Constants and Definitions
//*****************************************************************************

// Which NVS_config indice is used to initialize NVS.
#ifndef NVOCTP_NVS_INDEX
#define NVOCTP_NVS_INDEX    0
#endif // NVOCTP_NVS_INDEX

// Maximum ID parameters - must be coordinated with header format
#define NVOCTP_MAXSYSID     0x003F  //  6 bits
#define NVOCTP_MAXITEMID    0x03FF  // 10 bits
#define NVOCTP_MAXSUBID     0x03FF  // 10 bits
#define NVOCTP_MAXLEN       0x0FFF  // 12 bits

// Contents of an erased Flash memory locations
#define NVOCTP_ERASEDBYTE   0xFF
#define NVOCTP_ERASEDWORD   0xFFFFFFFF

// Size of byte
#define NVOCTP_ONEBYTE      1

// Invalid NV page - if 0xFF is ever used, change this definition
#define NVOCTP_NULLPAGE     0xFF

// Block size for Flash-Flash XFER (Bytes)
#define NVOCTP_XFERBLKMAX   32

// Size in bytes of biggest item size that will be concatenated
// in RAM before write, instead of header/data written separately
#define NVOCTP_SMALLITEM    12

#if defined (NVOCTP_STATS)
// NV item ID for driver diagnostics
static const NVINTF_itemID_t diagId = NVOCTP_NVID_DIAG;
#endif  // NVOCTP_STATS

// CRC options
// When not NULL, reads will result in a CRC check before returning
#define NVOCTP_CRCONREAD    1

// findItem search types
// Find any item, item of spec'd sysid, item of spec'd sys and item id
// or find the exact item specified
enum {NVOCTP_FINDANY = 0, NVOCTP_FINDSYSID, NVOCTP_FINDITMID,
    NVOCTP_FINDSTRICT};

//*****************************************************************************
// Macros
//*****************************************************************************

// Makes an NV Flash address (for 0x2000 page size)
#define NVOCTP_FLASHOFFSET(pg, ofs) ((uint32_t)(((pg) << PAGE_SIZE_LSHIFT) + (ofs)))

// Optional user provided function is called before writes/erases
// Intention is to check for sufficient voltage for operation
#define NVOCTP_FLASHACCESS(err) {if (NVOCTP_voltCheckFptr)\
    { if (!NVOCTP_voltCheckFptr()) { err = NVINTF_LOWPOWER;}}}

// Lock driver access via TI-RTOS gatemutex
#define NVOCTP_LOCK() IArg key = GateMutexPri_enter(NVOCTP_gMutexPri);

// Unlock driver access via TI-RTOS gatemutex and return error code
#define NVOCTP_UNLOCK(err) { \
    GateMutexPri_leave(NVOCTP_gMutexPri, key); return(err); }

// Generate a compressed NV ID (NOTE: bit31 must be zero)
#define NVOCTP_CMPRID(s,i,b) ((uint32_t)((((((s) & NVOCTP_MAXSYSID) << 12)   | \
                                            ((i) & NVOCTP_MAXITEMID)) << 12) | \
                                            ((b) & NVOCTP_MAXSUBID)))

// NVOCTP Unit Test Assert Macro/Function
#ifdef NVDEBUG
extern void nvprint(char * message);
extern void Main_assertHandler(uint8_t assertReason);
static void NVOCTP_assert(bool cond, char *message, bool fatal)
{
    if (!cond)
    {
        nvprint("NVDEBUG: ");
        nvprint(message);
        if (fatal)
        {
            Main_assertHandler(0);
        }
    }
}
#define NVOCTP_ASSERT(cond, message)    NVOCTP_assert((cond), (message), TRUE);
#define NVOCTP_ALERT(cond, message)     NVOCTP_assert((cond), (message), FALSE);
#else
#define NVOCTP_ASSERT(cond, message)
#define NVOCTP_ALERT(cond, message)
#endif // NVDEBUG

//*****************************************************************************
// Page and Header Definitions
//*****************************************************************************
// CC26x2 devices flash page size is (1 << 13) or 0x2000
#define PAGE_SIZE_LSHIFT 13
#if !defined (FLASH_PAGE_SIZE)
#define FLASH_PAGE_SIZE  (1 << PAGE_SIZE_LSHIFT)
#endif // FLASH_PAGE_SIZE

#if !defined (NVOCTP_VERSION)
// Version of NV page format (do not use 0xFF)
#define NVOCTP_VERSION  0x02
#endif // NVOCTP_VERSION

#if !defined (NVOCTP_SIGNATURE)
// Page header validation byte (do not use 0xFF)
#define NVOCTP_SIGNATURE  0x96
#endif // NVOCTP_SIGNATURE

// Page header structure
typedef struct
{
    uint8_t state;      // PGCLEAR, PGERASED, PGACTIVE, PGXFER
    uint8_t cycle;      // Rolling page compaction count (0x00, 0xFF not used)
    uint8_t version;    // Version of NV page format
    uint8_t signature;  // Signature for formatted NV page
} NVOCTP_pageHdr_t;

// Page header size (bytes)
#define NVOCTP_PGHDRLEN  (sizeof(NVOCTP_pageHdr_t))

// Page header offsets (from 1st byte of page)
#define NVOCTP_PGHDROFS  0
#define NVOCTP_PGHDRPST  0  // Page state
#define NVOCTP_PGHDRCYC  1  // Cycle count
#define NVOCTP_PGHDRVER  2  // Format version
#define NVOCTP_PGHDRSIG  3  // Page signature

// Page data size, offset into page
#define NVOCTP_PGDATAOFS  (NVOCTP_PGHDRLEN)
#define NVOCTP_PGDATAEND  (FLASH_PAGE_SIZE - 1)
#define NVOCTP_PGDATALEN  (FLASH_PAGE_SIZE - NVOCTP_PGHDRLEN)

// Page header state values - transitions change 1 bit in each nybble
#define NVOCTP_PGCLEAR   0xFF  // Erase started, not verified
#define NVOCTP_PGERASED  0xB7  // Erase verified, page ready for use
#define NVOCTP_PGACTIVE  0xA5  // Current active page
#define NVOCTP_PGXFER    0x24  // Active page being compacted

// Page compaction cycle count limits (0x00 and 0xFF not used)
#define NVOCTP_MINCYCLE  0x01  // Minimum cycle count (after rollover)
#define NVOCTP_MAXCYCLE  0xFE  // Maximum cycle count (before rollover)


//*****************************************************************************
// Item Header Definitions
//*****************************************************************************

// Item header structure
typedef struct
{
    uint32_t cmpid; // Compressed ID
    uint16_t subid; // Sub ID
    uint16_t itemid; // Item ID
    uint8_t  sysid; // System ID
    uint8_t  crc8;  // crc byte
    uint8_t  sig;   // signature byte
    uint8_t  stats; // Status 'marks'
    uint16_t hofs;  // Header offset
    uint16_t len;   // Data length
} NVOCTP_itemHdr_t;

// Length (bytes) of compressed header
#define NVOCTP_ITEMHDRLEN  7

// Offset from beginning (low address) of header to fields in the header
#define NVOCTP_HDRSIGOFS    6
#define NVOCTP_HDRVLDOFS    5

// Number of bytes in header to include in CRC calculation
#define NVOCTP_HDRCRCINC    5

// Compressed item header information <-- Lower Addr    Higher Addr-->
// Byte: [0]      [1]      [2]      [3]      [4]      [5]      [6]
// Item: SSSSSSII IIIIIIII SSSSSSSS SSLLLLLL LLLLLLCC CCCCCCAV SSSSSSSS
// LSB of field:         ^           ^            ^        ^          ^
// Bit:  0              15          25           37       45         55
//
// Bit(s)  Bit Field Description
// =============================
// 48-55:  Signature byte (NVOCTP_SIGNATURE)
//    47:  valid id mark (0=valid)
//    46:  active id mark (1=active)
// 38-45:  CRC8 value
// 26-37:  data length (0-4095)
// 16-25:  item sub id (0-1023)
//  6-15:  nv item id (0-1023)
//   0-5:  system id (0-63)

// Bit47 in compressed header - '1' indicates 'active' NV item
// A deleted item is 'inactive'
#define NVOCTP_ACTIVEIDBIT   0x2
// Bit46 in compressed header - '0' indicates 'valid' NV item
// A corrupted item is 'invalid'
#define NVOCTP_VALIDIDBIT    0x1
// This bit is NOT included in the NV item itself but is encoded
// the 'stats' field of the itemHdr_t struct when the item is read
#define NVOCTP_FOLLOWBIT     0x4

// Index of last item header byte
#define NVOCTP_ITEMHDREND (NVOCTP_ITEMHDRLEN - 1)

// Compressed item header byte array
typedef uint8_t cmpIH_t[NVOCTP_ITEMHDRLEN];

// Item write parameters
typedef struct
{
    NVOCTP_itemHdr_t *iHdr; // Ptr to item header
    uint16_t          dOfs; // Source data offset
    uint16_t          bOfs; // Buffer data offset
    uint16_t          len; // Buffer data length
    uint8_t          *pBuf; // Ptr to data buffer
} NVOCTP_itemWrp_t;

//*****************************************************************************
// Local variables
//*****************************************************************************

// NVS Objects
#ifdef NVDEBUG
// Expose these in debug mode
NVS_Handle NVOCTP_nvsHandle;
NVS_Attrs NVOCTP_nvsAttrs;
#else
static NVS_Handle NVOCTP_nvsHandle;
static NVS_Attrs NVOCTP_nvsAttrs;
#endif // NVDEBUG

// Flash Pages
static uint8_t NVOCTP_nvBegPage;
static uint8_t NVOCTP_nvEndPage;

#ifndef NVDEBUG
// Active page
static uint8_t NVOCTP_activePg;

// Active page offset to next available location to write an item
static uint16_t NVOCTP_pgOff;
#else
uint8_t NVOCTP_activePg;
uint16_t NVOCTP_pgOff;
#endif // NVDEBUG

// Active page compaction cycle count. Used to select the 'newest' active page
// at device reset, in the very unlikely scenario that both pages are active.
static uint8_t NVOCTP_pgCycle;

// Flag to indicate that a fatal error occurred while writing to or erasing the
// Flash memory. If flag is set, it's unsafe to attempt another write or erase.
// This flag locks writes to Flash until the next system reset.
static uint8_t NVOCTP_failF = NVINTF_NOTREADY;

// Flag to indicate that a non-fatal error occurred while writing to or erasing
// Flash memory. With flag set, it's still safe to attempt a write or erase.
// This flag is reset by any API calls that cause an erase/write to Flash.
static uint8_t NVOCTP_failW;

// TI-RTOS gateMutexPri for the NV driver API functions
static GateMutexPri_Handle NVOCTP_gMutexPri;

// Small NV Item Buffer, for item construction
static uint8_t NVOCTP_itemBuffer[NVOCTP_SMALLITEM];

// Function Pointer to an optional user provided voltage check function
static bool (*NVOCTP_voltCheckFptr)(void);

// Diagnostic counter for bad CRCs
#ifdef NVOCTP_STATS
static uint16_t NVOCTP_badCRCCount = 0;
#endif // NVOCTP_STATS

//*****************************************************************************
// NV API Function Prototypes
//*****************************************************************************

static uint8_t     NVOCTP_compactNvApi(uint16_t min);

static uint8_t     NVOCTP_initNvApi(void *param);

static IArg        NVOCTP_lockNvApi(void);

static void        NVOCTP_unlockNvApi(IArg);

static uint8_t     NVOCTP_createItemApi(NVINTF_itemID_t id,
                                        uint32_t len,
                                        void *buf);

static uint8_t     NVOCTP_deleteItemApi(NVINTF_itemID_t id);

static uint32_t    NVOCTP_getItemLenApi(NVINTF_itemID_t id);

static uint8_t     NVOCTP_readItemApi(NVINTF_itemID_t id,
                                      uint16_t ofs,
                                      uint16_t len,
                                      void *buf);

static uint8_t     NVOCTP_writeItemApi(NVINTF_itemID_t id,
                                       uint16_t len,
                                       void *buf);

static uint8_t     NVOCTP_doNextApi(NVINTF_nvProxy_t * prx);

//*****************************************************************************
// NV Local Function Prototypes
//*****************************************************************************

static uint8_t     NVOCTP_readByte(uint8_t pg,
                                   uint16_t ofs);

static void        NVOCTP_writeByte(uint8_t pg,
                                    uint16_t ofs,
                                    uint8_t bwv);

static uint8_t     NVOCTP_checkItem(NVINTF_itemID_t *id,
                                    uint16_t len,
                                    NVOCTP_itemHdr_t *iHdr,
                                    uint8_t flag);

static int16_t     NVOCTP_compactPage(uint8_t srcPg);

static void        NVOCTP_copyItem(uint8_t xPg,
                                   uint16_t sOfs,
                                   uint16_t dOfs,
                                   uint16_t len);

static int16_t     NVOCTP_findItem(uint8_t pg,
                                   uint16_t ofs,
                                   uint32_t cid,
                                   uint8_t flag);

static uint16_t    NVOCTP_findOffset(uint8_t pg,
                                     uint16_t ofs);

static uint8_t     NVOCTP_newItem(NVOCTP_itemHdr_t *iHdr,
                                  uint8_t *pBuf);

static inline void NVOCTP_read(uint8_t pg,
                               uint16_t off,
                               uint8_t *pBuf,
                               uint16_t len);

static uint8_t     NVOCTP_readItem(NVOCTP_itemHdr_t *iHdr,
                                  uint16_t ofs,
                                  uint16_t len,
                                  void *pBuf);

static void        NVOCTP_readHeader(uint8_t pg,
                                     uint16_t ofs,
                                     NVOCTP_itemHdr_t *iHdr);

static void        NVOCTP_setItemInactive(uint16_t hOfs);

static void        NVOCTP_setPageActive(uint8_t pg);

static void        NVOCTP_writeItem(NVOCTP_itemHdr_t *iHdr,
                                    uint8_t dstPg,
                                    uint8_t *pBuf);

static uint8_t     NVOCTP_doNVCRC(uint8_t pg,
                                  uint16_t ofs,
                                  uint16_t len,
                                  uint8_t crc);

static uint8_t     NVOCTP_doRAMCRC(uint8_t *input,
                                   uint16_t len,
                                   uint8_t crc);

static uint8_t     NVOCTP_verifyCRC(uint16_t iOfs,
                                    uint16_t len,
                                    uint8_t crc);

static uint8_t     NVOCTP_write(uint8_t dstPg,
                                uint16_t off,
                                uint8_t *pBuf,
                                uint16_t len);

static uint8_t     NVOCTP_erase(uint8_t dstPg);

//*****************************************************************************
// Load Pointer Functions (These are declared in nvoctp.h)
//*****************************************************************************

/**
 * @fn      NVOCTP_loadApiPtrs
 *
 * @brief   Global function to return function pointers for NV driver API that
 *          are supported by this module, NULL for functions not supported.
 *
 * @param   pfn - pointer to caller's structure of NV function pointers
 *
 * @return  none
 */
void NVOCTP_loadApiPtrs(NVINTF_nvFuncts_t *pfn)
{
    // Load caller's structure with pointers to the NV API functions
    pfn->initNV      = &NVOCTP_initNvApi;
    pfn->compactNV   = &NVOCTP_compactNvApi;
    pfn->createItem  = &NVOCTP_createItemApi;
    pfn->deleteItem  = &NVOCTP_deleteItemApi;
    pfn->readItem    = &NVOCTP_readItemApi;
    pfn->writeItem   = &NVOCTP_writeItemApi;
    pfn->getItemLen  = &NVOCTP_getItemLenApi;
    pfn->lockNV      = NULL;
    pfn->unlockNV    = NULL;
    pfn->doNext      = NULL;
}

/**
 * @fn      NVOCTP_loadApiPtrsMin
 *
 * @brief   Global function to return function pointers for NV driver API that
 *          are supported by this module, NULL for functions not supported.
 *          This function loads the minimum necessary API functions.
 *          This should allow smaller code size.
 *
 * @param   pfn - pointer to caller's structure of NV function pointers
 *
 * @return  none
 */
void NVOCTP_loadApiPtrsMin(NVINTF_nvFuncts_t *pfn)
{
    // Load caller's structure with pointers to the NV API functions
    pfn->initNV      = &NVOCTP_initNvApi;
    pfn->compactNV   = &NVOCTP_compactNvApi;
    pfn->createItem  = NULL;
    pfn->deleteItem  = NULL;
    pfn->readItem    = &NVOCTP_readItemApi;
    pfn->writeItem   = &NVOCTP_writeItemApi;
    pfn->getItemLen  = NULL;
    pfn->lockNV      = NULL;
    pfn->unlockNV    = NULL;
    pfn->doNext      = NULL;
}

/**
 * @fn      NVOCTP_loadApiPtrsExt
 *
 * @brief   Global function to return function pointers for NV driver API that
 *          are supported by this module, NULL for functions not supported.
 *          This function also loads the 'extended' API function pointers.
 *
 * @param   pfn - pointer to caller's structure of NV function pointers
 *
 * @return  none
 */
void NVOCTP_loadApiPtrsExt(NVINTF_nvFuncts_t *pfn)
{
    // Load caller's structure with pointers to the NV API functions
    pfn->initNV      = &NVOCTP_initNvApi;
    pfn->compactNV   = &NVOCTP_compactNvApi;
    pfn->createItem  = &NVOCTP_createItemApi;
    pfn->deleteItem  = &NVOCTP_deleteItemApi;
    pfn->readItem    = &NVOCTP_readItemApi;
    pfn->writeItem   = &NVOCTP_writeItemApi;
    pfn->getItemLen  = &NVOCTP_getItemLenApi;
    pfn->lockNV      = &NVOCTP_lockNvApi;
    pfn->unlockNV    = &NVOCTP_unlockNvApi;
    pfn->doNext      = &NVOCTP_doNextApi;
}

/**
 * @fn      NVOCTP_setCheckVoltage
 *
 * @brief   Global function to allow user to provide a voltage check function
 *          for the driver to use. If a pointer is provided, the driver will
 *          call the provided function before flash erases and writes. The
 *          provided function should return true when the battery voltage is
 *          sufficient and vice versa. The user can withdraw their function
 *          by passing a NULL pointer to this function.
 *
 * @param   funcPtr - pointer to a function which returns a bool.
 *
 * @return  none
 */
extern void NVOCTP_setCheckVoltage(void *funcPtr)
{
    NVOCTP_voltCheckFptr = (bool (*)()) funcPtr;
}

/******************************************************************************
 * @fn      NVOCTP_initNvApi
 *
 * @brief   API function to initialize the specified NV Flash pages
 *
 * @param   param - pointer to caller's structure of NV init parameters
 *
 * @return  NVINTF_SUCCESS or specific failure code
 */
static uint8_t NVOCTP_initNvApi(void *param)
{
    NVOCTP_ALERT(FALSE, "NVOCTP Init. Called!")
    NVOCTP_failW = NVOCTP_failF;

    if(NVOCTP_failF == NVINTF_NOTREADY)
    {
        uint8_t pg;
        uint8_t xferPg;
        GateMutexPri_Params gateParams;

        // Only one init per device reset
        NVOCTP_failF = NVINTF_SUCCESS;
        NVOCTP_failW = NVINTF_SUCCESS;

        // Create a priority gate mutex for the NV driver
        GateMutexPri_Params_init(&gateParams);
        NVOCTP_gMutexPri = GateMutexPri_create(&gateParams, NULL);

        xferPg = NVOCTP_NULLPAGE;
        NVOCTP_activePg = NVOCTP_NULLPAGE;

        // Initialize NVS objects
        NVS_init();

        // Use default NVS_Params to open this flash region
        NVOCTP_nvsHandle = NVS_open(NVOCTP_NVS_INDEX, param);

        // Get NV hardware attributes
        NVS_getAttrs(NVOCTP_nvsHandle,&NVOCTP_nvsAttrs);

        // Get page number from hardware attributes
        NVOCTP_nvBegPage = 0;
        NVOCTP_nvEndPage = NVOCTP_nvBegPage + 1;

        // Confirm NV region has expected characteristics
        if (FLASH_PAGE_SIZE != NVOCTP_nvsAttrs.sectorSize ||
                (2 * FLASH_PAGE_SIZE > NVOCTP_nvsAttrs.regionSize))
        {
            NVOCTP_failF = NVINTF_FAILURE;
            NVOCTP_EXCEPTION(pg, NVINTF_FAILURE)
            return(NVOCTP_failF);
        }

        // Confirm that the NVS region opened properly
        if (NVOCTP_nvsHandle == NULL)
        {
            NVOCTP_failF = NVINTF_FAILURE;
            NVOCTP_ASSERT(FALSE,"NVS HANDLE IS NULL")
            NVOCTP_EXCEPTION(pg, NVINTF_NOTREADY);
            return(NVOCTP_failF);
        }

        // Look for active page and clean up the other if necessary
        for(pg = NVOCTP_nvBegPage; pg <= NVOCTP_nvEndPage; pg++)
        {
            NVOCTP_pageHdr_t pHdr;

            // Get page header
            NVOCTP_read(pg,NVOCTP_PGHDROFS, (uint8_t *)&pHdr, NVOCTP_PGHDRLEN);

            if((pHdr.state == NVOCTP_PGACTIVE) &&
               (pHdr.cycle <= NVOCTP_MAXCYCLE) &&
               (pHdr.signature == NVOCTP_SIGNATURE))
            {
                // Looks like a valid page header
                if(pHdr.version != NVOCTP_VERSION)
                {
                    // NV page and NV driver versions are different
                    NVOCTP_ASSERT(FALSE, "Version mismatch.")
                    NVOCTP_EXCEPTION(pg, NVINTF_BADVERSION);
                }
                if(NVOCTP_activePg == NVOCTP_NULLPAGE)
                {
                    // First active page found
                    NVOCTP_activePg = pg;
                    NVOCTP_pgCycle  = pHdr.cycle;
                }
                else
                {
                    // Both pages are in the active state, pick one...
                    if(((NVOCTP_pgCycle > pHdr.cycle) &&
                       ((NVOCTP_pgCycle != NVOCTP_MAXCYCLE) &&
                        (pHdr.cycle != NVOCTP_MINCYCLE))) ||
                       ((NVOCTP_pgCycle < pHdr.cycle) &&
                       ((NVOCTP_pgCycle == NVOCTP_MINCYCLE) &&
                        (pHdr.cycle == NVOCTP_MAXCYCLE))))
                    {
                        // First page found is newer, erase the second
                        NVOCTP_failW = NVOCTP_erase(pg);
                    }
                    else
                    {
                        // Second page found is newer, erase the first
                        NVOCTP_failW    = NVOCTP_erase(NVOCTP_activePg);
                        NVOCTP_activePg = pg;
                        NVOCTP_pgCycle  = pHdr.cycle;
                    }
                }
            }
            else if(pHdr.state == NVOCTP_PGXFER)
            {
                xferPg = pg;
                if(NVOCTP_pgCycle == 0)
                {
                    // Restore interrupted compaction cycle count
                    NVOCTP_pgCycle = pHdr.cycle;
                }
            }
            else if(pHdr.state != NVOCTP_PGERASED)
            {
                // Ensure that interrupted compaction page erase gets finished
                NVOCTP_failW = NVOCTP_erase(pg);
            }
        }

        if(NVOCTP_activePg == NVOCTP_NULLPAGE)
        {
            if(xferPg == NVOCTP_NULLPAGE)
            {
                // Both pages are erased. Initial state, select an active page
                NVOCTP_setPageActive(NVOCTP_nvBegPage);
                NVOCTP_pgOff = NVOCTP_PGDATAOFS;
            }
            else
            {
                // Compacting interrupted in previous power cycle - do it now
                NVOCTP_ALERT(FALSE, "NVOCTP Init., recovering from compaction"
                        " interruption.")
                NVOCTP_activePg = xferPg;
                NVOCTP_pgOff    = NVOCTP_findOffset(xferPg, FLASH_PAGE_SIZE);
                (void)NVOCTP_compactPage(xferPg);
            }
        }
        else
        {
            if(xferPg != NVOCTP_NULLPAGE)
            {
                // Compacting completed except for last step to erase xferPage
                NVOCTP_failW = NVOCTP_erase(xferPg);
            }
            // Find the active page offset for next NV item write
            NVOCTP_pgOff = NVOCTP_findOffset(NVOCTP_activePg, FLASH_PAGE_SIZE);
        }

#if defined (NVOCTP_STATS)
        {
            uint8_t err;
            NVOCTP_diag_t diags;

            // Look for a copy of diagnostic info
            err = NVOCTP_readItemApi(diagId, 0, sizeof(diags), &diags);
            if(err == NVINTF_NOTFOUND)
            {
                // Assume this is the first time,
                memset(&diags, 0, sizeof(diags));
                // Space available for everything else
                diags.available = FLASH_PAGE_SIZE - (NVOCTP_pgOff +
                        NVOCTP_ITEMHDRLEN + sizeof(diags));
            }
            // Remember this reset
            diags.resets += 1;
            // Create/Update the diagnostic NV item
            NVOCTP_writeItemApi(diagId, sizeof(diags), &diags);
        }
#endif // NVOCTP_STATS
    }

    return(NVOCTP_failW);
}

/******************************************************************************
 * @fn      NVOCTP_compactNvApi
 *
 * @brief   API function to force NV active page compaction
 *
 * @param   minAvail - threshold size of available bytes on Flash page to do
 *                     compaction: 0 = always, >0 = minimum remaining bytes
 *
 * @return  NVINTF_SUCCESS or specific failure code
 */
static uint8_t NVOCTP_compactNvApi(uint16_t minAvail)
{
    uint8_t err;

    // Prevent RTOS thread contention
    NVOCTP_LOCK();
    NVOCTP_ALERT(FALSE, "API Compaction Request.")
    err = NVOCTP_failF;
    // Check for a fatal error
    if(err == NVINTF_SUCCESS)
    {
        int16_t left;

        // Number of bytes left on active page
        left = FLASH_PAGE_SIZE - NVOCTP_pgOff;

        // Time to do a compaction?
        if((left < minAvail) || (minAvail == 0))
        {
            // Transfer all items to non-ACTIVE page
            (void)NVOCTP_compactPage(NVOCTP_activePg);
            // 'failW' indicates compaction status
            err = NVOCTP_failW;
        }
        else
        {
            // Indicate "bad" minAvail value
            err = NVINTF_BADPARAM;
        }
    }
    NVOCTP_UNLOCK(err);
}

//*****************************************************************************
// API Functions - NV Data Items
//*****************************************************************************

/******************************************************************************
 * @fn      NVOCTP_createItemApi
 *
 * @brief   API function to create a new NV item in Flash memory. This function
 *          will return an error if the specified item already exists.
 *
 * @param   id - NV item type identifier
 * @param   len - length of NV data
 * @param   pBuf - pointer to caller's data buffer  (NULL is illegal)
 *
 * @return  NVINTF_SUCCESS or specific failure code
 */
static uint8_t NVOCTP_createItemApi(NVINTF_itemID_t id,
                                    uint32_t len,
                                    void *pBuf)
{
    uint8_t err;
    NVOCTP_itemHdr_t iHdr;

    // Parameter Sanity Check
    if (pBuf == NULL || len == 0)
    {
        return(NVINTF_BADPARAM);
    }

    // Prevent RTOS thread contention
    NVOCTP_LOCK();

    err = NVOCTP_checkItem(&id, len, &iHdr, NVOCTP_FINDSTRICT);
    if(err == NVINTF_NOTFOUND)
    {
        // Create the new item
        err = NVOCTP_newItem(&iHdr, pBuf);
    }
    else
    {
        if(err == NVINTF_SUCCESS)
        {
            // Item already exists
            NVOCTP_ALERT(FALSE, "createItem called for existing itemID.")
            err = NVINTF_FAILURE;
        }
    }

    NVOCTP_UNLOCK(err);
}

/******************************************************************************
 * @fn      NVOCTP_deleteItemApi
 *
 * @brief   API function to delete an existing NV item from Flash memory. Note,
 *          it is inefficient to use this function to delete a range of items.
 *          The doNext call is recommended for that use case.
 *
 * @param   id - NV item type identifier
 *
 * @return  NVINTF_SUCCESS or specific failure code
 */
static uint8_t NVOCTP_deleteItemApi(NVINTF_itemID_t id)
{
    uint8_t err;
    NVOCTP_itemHdr_t iHdr;

#if defined (NVOCTP_STATS)
    if(!memcmp(&id, &diagId, sizeof(NVINTF_itemID_t)))
    {
        // Protect NV driver item(s)
        return(NVINTF_BADSYSID);
    }
#endif // NVOCTP_STATS

    // Prevent RTOS thread contention
    NVOCTP_LOCK();

    err = NVOCTP_checkItem(&id, 0, &iHdr, NVOCTP_FINDSTRICT);
    if(err == NVINTF_SUCCESS)
    {
        int16_t hOfs;

        // Mark this item as inactive
        NVOCTP_setItemInactive(iHdr.hofs);

        // Verify that item has been removed
        hOfs = NVOCTP_findItem(NVOCTP_activePg, NVOCTP_pgOff,
                               iHdr.cmpid, NVOCTP_FINDSTRICT);

        // If item did get deleted, report 'failW' status
        err = (hOfs <= 0) ? NVOCTP_failW : NVINTF_FAILURE;
        NVOCTP_ALERT(err == NVOCTP_failW, "Item delete failed.")
    }

    NVOCTP_UNLOCK(err);
}

/******************************************************************************
 * @fn      NVOCTP_getItemLenApi
 *
 * @brief   API function to return the length of an NV data item
 *
 * @param   id - NV item type identifier
 *
 * @return  NV item length or 0 if item not found
 */
static uint32_t NVOCTP_getItemLenApi(NVINTF_itemID_t id)
{
    uint8_t err;
    uint32_t len;
    NVOCTP_itemHdr_t iHdr;

    // Prevent RTOS thread contention
    NVOCTP_LOCK();

    err = NVOCTP_checkItem(&id, 0, &iHdr, NVOCTP_FINDSTRICT);

    // If there was any error, report zero length
    len = (err != NVINTF_SUCCESS) ? 0 : iHdr.len;

    NVOCTP_UNLOCK(len);
}

/******************************************************************************
 * @fn      NVOCTP_readItemApi
 *
 * @brief   API function to read data from an NV item
 *
 * @param   id   - NV item type identifier
 * @param   ofs - offset into NV data
 * @param   len - length of NV data to return (0 is illegal)
 * @param   pBuf - pointer to caller's read data buffer  (NULL is illegal)
 *
 * @return  NVINTF_SUCCESS or specific failure code
 */
static uint8_t NVOCTP_readItemApi(NVINTF_itemID_t id,
                                  uint16_t ofs,
                                  uint16_t len,
                                  void *pBuf)
{
    uint8_t err;
    NVOCTP_itemHdr_t iHdr;

    // Parameter Sanity Check
    if (pBuf == NULL || len == 0)
    {
        return(NVINTF_BADPARAM);
    }

    // Prevent RTOS thread contention
    NVOCTP_LOCK();

    err = NVOCTP_checkItem(&id, len, &iHdr, NVOCTP_FINDSTRICT);

    // Read Item
    if (NVINTF_SUCCESS == err)
    {
        err = NVOCTP_readItem(&iHdr, ofs, len, pBuf);
    }

    NVOCTP_UNLOCK(err);
}

/******************************************************************************
 * @fn      NVOCTP_writeItemApi
 *
 * @brief   API function to write data to item, creates item if needed.
 *          Note that when writing to an existing item, data is not
 *          checked for redundancy. Data passed to this function will be
 *          written to NV. NOTE: It is not recommended to write items with
 *          SYSID 0 as this is reserved for the driver. NVOCTP will not
 *          delete items with this SYSID.
 *
 * @param   id   - NV item type identifier
 * @param   len - data buffer length to write into NV block  (0 is illegal)
 * @param   pBuf - pointer to caller's data buffer to write  (NULL is illegal)
 *
 * @return  NVINTF_SUCCESS or specific failure code
 */
static uint8_t NVOCTP_writeItemApi(NVINTF_itemID_t id,
                                   uint16_t len,
                                   void *pBuf)
{
    uint8_t err;
    NVOCTP_itemHdr_t iHdr;

    // Parameter Sanity Check
    if (pBuf == NULL || len == 0)
    {
        return(NVINTF_BADPARAM);
    }

    // Prevent RTOS thread contention
    NVOCTP_LOCK();


    err  = NVOCTP_checkItem(&id, len, &iHdr, NVOCTP_FINDSTRICT);

    if(err == NVINTF_SUCCESS)
    {
        // Found old version of item
        // Our new item may have different length
        iHdr.len = len;

        // Trigger item creation
        err = NVINTF_NOTFOUND;
    }

    // Check voltage if possible
    NVOCTP_FLASHACCESS(err)

    if (err == NVINTF_NOTFOUND)
    {
        // Create a new item
        err = NVOCTP_newItem(&iHdr, pBuf);
        if((err == NVINTF_SUCCESS) && (iHdr.hofs > 0))
        {
            // Mark old item as inactive
            NVOCTP_setItemInactive(iHdr.hofs);
            err = NVOCTP_failW;
        }
    }

    NVOCTP_UNLOCK(err);
}

//*****************************************************************************
// Extended API Functions
//*****************************************************************************

/**
 * @fn      NVOCTP_lockNvApi
 *
 * @brief   Global function to lock the NV priority gate mutex
 *
 * @return  Key value needed to unlock the gate
 */
static IArg NVOCTP_lockNvApi(void)
{
    return(GateMutexPri_enter(NVOCTP_gMutexPri));
}

/**
 * @fn      NVOCTP_unlockNvApi
 *
 * @brief   Global function to unlock the NV priority gate mutex
 *
 * @return  none
 */
static void NVOCTP_unlockNvApi(IArg key)
{
    GateMutexPri_leave(NVOCTP_gMutexPri,key);
}

/******************************************************************************
 * @fn      NVOCTP_doNextApi
 *
 * @brief   API function which allows operations on batches of NV items. This
 * function provides a faster way of finding, reading, or deleting multiple
 * NV items. However, the user must first lock access to NV with lockNV() to
 * ensure consistent results. The user must take care to minimize the time NV
 * is locked if NV access is shared. User must also remember to unlock NV when
 * done with unlockNV().
 *
 * Usage Details:
 * doNext is controlled through the nvProxy item pointed to by prx
 * User will set flag bit NVINTF_DOSTART and then other flags based on the
 * desired operation. For example to find all items in system NVINTF_SYS_BLE,
 * the user would set flag bit NVINTF_DOFIND and set prx->sysid = NVINTF_SYS_BLE.
 * Then every call to doNextApi() returns with a status code and with the proxy
 * item populated with the found item if there was one. NVINTF_SUCCESS is
 * returned on a successful item operation, NVINTF_NOTFOUND is returned when a
 * matching item is not found, and other error codes can be returned.
 * Sample Code:
 *
 * // Use doNext to delete items of sysid
 * NVINTF_nvFuncts_t nvFps;
 * NVINTF_nvProxy_t nvProxy;
 * NVOCTP_loadApiPtrsExt(&nvFps);
 * nvFps.initNV(NULL);
 * nvProxy.sysid = sysid;
 * nvProxy.flag = NVINTF_DOSTART | NVINTF_DOSYSID | NVINTF_DODELETE;
 *
 * key = nvFps.lockNV();
 * while(!status)
 * {
 *     status |= nvFps.doNext(&nvProxy);
 * }
 * nvFps.unlockNV(key);
 *
 * Notes:
 * -User changes to the proxy struct will have no effect until a new search is
 * started by setting NVINTF_DOSTART
 * -On read operations, the user will supply a buffer and length into the proxy
 * -Items with system id NVINTF_SYSID_NVDRVR cannot be deleted with this API,
 * deleteItemApi must be used one an individual item basis
 *
 * @param   prx - pointer to nvProxy item which contains user inputs
 *
 * @return  NVINTF_SUCCESS or specific failure code
 */

static uint8_t NVOCTP_doNextApi(NVINTF_nvProxy_t * prx)
{
    static enum {doFind, doRead, doDelete} op = doFind;
    NVOCTP_itemHdr_t hdr;
    static uint32_t cid;
    static uint8_t search;
    static int16_t fOfs    = FLASH_PAGE_SIZE;
    static uint16_t bufLen = 0;
    uint8_t status         = NVINTF_SUCCESS;
    int16_t iOfs           = NVOCTP_pgOff;

    // Sanitize inputs
    if (NULL == prx)
    {
        return(NVINTF_BADPARAM);
    }
    else if (0 == prx->flag)
    {
        return(NVINTF_BADPARAM);
    }

    // Locks NV
    NVOCTP_LOCK();

    // New search if start flag set
    if (prx->flag & NVINTF_DOSTART)
    {
        // Remove start flag
        prx->flag &= ~NVINTF_DOSTART;
        // Start at latest item
        fOfs = NVOCTP_pgOff;
        // Read in buffer len
        bufLen = prx->len;
        // Make cid
        cid = NVOCTP_CMPRID(prx->sysid,prx->itemid,prx->subid);

        // Decode flag
        if (prx->flag & NVINTF_DOSYSID)
        {
            search = NVOCTP_FINDSYSID;
        }
        else if (prx->flag & NVINTF_DOITMID)
        {
            search = NVOCTP_FINDITMID;
        }
        else if (prx->flag & NVINTF_DOANYID)
        {
            search = NVOCTP_FINDANY;
        }
        if (prx->flag & NVINTF_DOFIND)
        {
            op = doFind;
        }
        else if (prx->flag & NVINTF_DOREAD)
        {
            op = doRead;
        }
        else if (prx->flag & NVINTF_DODELETE)
        {
            op = doDelete;
        }
    }

    // Look for item
    iOfs = NVOCTP_findItem(NVOCTP_activePg, fOfs, cid, search);

    if (iOfs > 0 && iOfs < FLASH_PAGE_SIZE)
    {
        // Found an item, gets its header
        NVOCTP_readHeader(NVOCTP_activePg, (uint16_t)iOfs, &hdr);
        // store its attributes
        prx->sysid  = hdr.sysid;
        prx->itemid = hdr.itemid;
        prx->subid  = hdr.subid;
        prx->len    = hdr.len;
        // start from this item on next findItem()
        fOfs = iOfs - hdr.len;

        // Do operation based on flag
        switch (op)
        {
        case doFind:
            // nothing, we already stored its info
            break;
        case doRead:
            // read item into user supplied buffer
            if (prx->buffer != NULL && hdr.len <= bufLen)
            {
                status = NVOCTP_readItem(&hdr, 0, hdr.len, prx->buffer);
            }
            break;
        case doDelete:
            if (prx->sysid != NVINTF_SYSID_NVDRVR)
            {
                NVOCTP_setItemInactive(iOfs);
            }
            break;
        default:
            NVOCTP_ALERT(FALSE, "doNext flag is invalid.")
            status = NVINTF_BADPARAM;
        }
    }
    else
    {
        // No more items match, done.
        status = NVINTF_NOTFOUND;
    }

    // Unlocks NV
    NVOCTP_UNLOCK(status);
}

//*****************************************************************************
// Local NV Driver Utility Functions
//*****************************************************************************

/******************************************************************************
 * @fn      NVOCTP_checkItem
 *
 * @brief   Local function to check parameters and locate existing item
 *
 * @param   id   - NV item type identifier
 * @param   len - NV item data length
 * @param   pHdr - pointer to header buffer
 * @param   flag - flag for item search
 *
 * @return  NVINTF_SUCCESS or specific failure code
 */
static uint8_t NVOCTP_checkItem(NVINTF_itemID_t *id,
                                uint16_t len,
                                NVOCTP_itemHdr_t *pHdr,
                                uint8_t flag)
{
    int16_t ofs;
    uint32_t cid;

    if(len > NVOCTP_MAXLEN)
    {
        // Item data is too long
        NVOCTP_ALERT(FALSE, "Item data too large.")
        return(NVINTF_BADLENGTH);
    }
    if(id->systemID > NVOCTP_MAXSYSID)
    {
        // Too large for compressed header
        NVOCTP_ALERT(FALSE, "Item sysid too large.")
        return(NVINTF_BADSYSID);
    }
    if(id->itemID > NVOCTP_MAXITEMID)
    {
        // Too large for compressed header
        NVOCTP_ALERT(FALSE, "Item itemid too large.")
        return(NVINTF_BADITEMID);
    }
    if(id->subID > NVOCTP_MAXSUBID)
    {
        // Too large for compressed header
        NVOCTP_ALERT(FALSE, "Item subid too large.")
        return(NVINTF_BADSUBID);
    }

    if(NVOCTP_failF == NVINTF_NOTREADY)
    {
        // NV driver has not been initialized
        NVOCTP_ASSERT(FALSE, "Driver uninitialized.")
        return(NVINTF_NOTREADY);
    }

    // Reset erase/write fail indicator for current transaction
    NVOCTP_failW = NVINTF_SUCCESS;
    cid          = NVOCTP_CMPRID(id->systemID, id->itemID, id->subID);
    ofs          = NVOCTP_findItem(NVOCTP_activePg, NVOCTP_pgOff, cid, flag);

    if(ofs <= 0)
    {
        // Item does not exist yet
        pHdr->len    = len;
        pHdr->hofs   = 0;
        pHdr->cmpid  = cid;
        pHdr->subid  = id->subID;
        pHdr->itemid = id->itemID;
        pHdr->sysid  = id->systemID;
        pHdr->sig    = NVOCTP_SIGNATURE;
        return(NVINTF_NOTFOUND);
    }

    // Read and decompress item header
    NVOCTP_readHeader(NVOCTP_activePg, (uint16_t)ofs, pHdr);

    return(NVOCTP_failW);
}
/******************************************************************************
 * @fn      NVOCTP_newItem
 *
 * @brief   Local function to check for adequate space and create a new item
 *
 * @param   iHdr - pointer to header buffer
 *
 * @return  NVINTF_SUCCESS or specific failure code
 */
static uint8_t NVOCTP_newItem(NVOCTP_itemHdr_t *iHdr,
                              uint8_t *pBuf)
{
    uint8_t err;
    uint16_t iLen;
    uint32_t cid;

    iLen = NVOCTP_ITEMHDRLEN + iHdr->len;
    if((NVOCTP_pgOff + iLen) > FLASH_PAGE_SIZE)
    {
        // Won't fit on the active page, compact and check again
        if(NVOCTP_compactPage(NVOCTP_activePg) < iLen)
        {
            // Failure means there's no place to put this item
            NVOCTP_ALERT(FALSE, "Out of NV.")
            err = (NVOCTP_failW != NVINTF_SUCCESS) ?
                    NVOCTP_failW : NVINTF_BADLENGTH;
            return(err);
        }
        else   /*Compaction is a success */
        {
            if(NVOCTP_failW == NVINTF_SUCCESS)
            {
                cid          = NVOCTP_CMPRID(iHdr->sysid, iHdr->itemid, iHdr->subid);
                /* get item offset after compaction */
                iHdr->hofs   = NVOCTP_findItem(NVOCTP_activePg, NVOCTP_pgOff, cid, NVOCTP_FINDSTRICT);
            }
        }
    }

    // Create the new NV item
    NVOCTP_writeItem(iHdr, NVOCTP_activePg, pBuf);

    // Status of writing/erasing Flash
    return(NVOCTP_failW);
}

/******************************************************************************
 * @fn      NVOCTP_read
 *
 * @brief   Writes to a flash buffer from RAM
 *
 * @param   pg  - Flash page to write from
 * @param   off - Offset in destination page to read from
 * @param   pBuf  - Pointer to write the results into
 * @param   len - Number of bytes to write into
 *
 * @return  NVS_STATUS_SUCCESS or other NVS status code
 */
static inline void NVOCTP_read(uint8_t pg,
                               uint16_t off,
                               uint8_t *pBuf,
                               uint16_t len)
{
    NVS_read(NVOCTP_nvsHandle, NVOCTP_FLASHOFFSET(pg, off),
             (uint8_t *)pBuf, len);
}

/******************************************************************************
 * @fn      NVOCTP_write
 *
 * @brief   Writes to a flash buffer from RAM
 *
 * @param   dstPg  - Flash page to write to
 * @param   off - offset in destination page to write to
 * @param   pBuf  - Pointer to caller's buffer to write & verify
 * @param   len - number of bytes to write from pBuf
 *
 * @return  NVS_STATUS_SUCCESS or other NVS status code
 */
static uint8_t NVOCTP_write(uint8_t dstPg,
                            uint16_t off,
                            uint8_t *pBuf,
                            uint16_t len)
{
    uint8_t err = NVINTF_SUCCESS;
    int_fast16_t nvsRes = 0;

    // check voltage if possible
    NVOCTP_FLASHACCESS(err)

    if (NVINTF_SUCCESS == err)
    {
        nvsRes = NVS_write(NVOCTP_nvsHandle, NVOCTP_FLASHOFFSET(dstPg, off),
                           pBuf, len, NVS_WRITE_POST_VERIFY);
    }
    else
    {
        err = NVINTF_LOWPOWER;
    }

    if (nvsRes < 0)
    {
        err = NVINTF_FAILURE;
    }

    NVOCTP_ALERT(NVINTF_LOWPOWER != err, "Voltage check failed.")
    if (NVINTF_FAILURE == err)
    {
        NVOCTP_ALERT(NVINTF_FAILURE != err, "NVS write failure.")
    }

    return(err);
}

/******************************************************************************
 * @fn      NVOCTP_erase
 *
 * @brief   Erases a flash page
 *
 * @param   dstPg - Flash page to write to
 *
 * @return  NVINT_SUCCESS or other NVINTF status code
 */
static uint8_t NVOCTP_erase(uint8_t dstPg)
{
    uint8_t err = NVINTF_SUCCESS;
    int_fast16_t nvsRes = 0;

    // Check voltage if possible
    NVOCTP_FLASHACCESS(err)

    if (NVINTF_SUCCESS == err)
    {
        nvsRes = NVS_erase(NVOCTP_nvsHandle, NVOCTP_FLASHOFFSET(dstPg, 0),
                           NVOCTP_nvsAttrs.sectorSize);
    }
    else
    {
        err = NVINTF_LOWPOWER;
    }

    if (nvsRes < 0)
    {
        err = NVINTF_FAILURE;
    }

    NVOCTP_ALERT(NVINTF_LOWPOWER != err, "Voltage check failed.")
    NVOCTP_ALERT(NVINTF_FAILURE != err, "NVS erase failure.")

    return(err);
}

/******************************************************************************
 * @fn      NVOCTP_writeItem
 *
 * @brief   Write entire NV item to new location on active Flash page.
 *          Each call to NVS_write() does a read-back to verify. If an
 *          error is detected, the 'failW' flag is set to inhibit further
 *          flash write attempts until the next NV transaction.
 *
 * @param   pHdr  - Pointer to caller's item header buffer
 * @param   dstPg - Destination NV Flash page
 * @param   pBuf  - Points to buffer which will be written to item
 *
 * @return  none
 */
static void NVOCTP_writeItem(NVOCTP_itemHdr_t *pHdr,
                             uint8_t dstPg,
                             uint8_t *pBuf)
{
    uint16_t iLen;

    // Total length of this item
    iLen = NVOCTP_ITEMHDRLEN + pHdr->len;

    if((NVOCTP_pgOff + iLen) <= FLASH_PAGE_SIZE)
    {
        cmpIH_t cHdr;
        uint16_t hOfs, dLen;
        uint8_t newCRC;

        // Compressed item header information <-- Lower Addr    Higher Addr-->
        // Byte: [0]      [1]      [2]      [3]      [4]      [5]      [6]
        // Item: SSSSSSII IIIIIIII SSSSSSSS SSLLLLLL LLLLLLCC CCCCCCAV SSSSSSSS
        // LSB of field:         ^           ^            ^        ^          ^
        cHdr[0] = ((pHdr->sysid << 2) | ((pHdr->itemid >> 8) & 0x3));
        cHdr[1] = (pHdr->itemid & 0xFF);
        cHdr[2] = ((pHdr->subid >> 2) & 0xFF);
        cHdr[3] = ((pHdr->subid & 0x3) << 6) | ((pHdr->len >> 6) & 0x3F);
        cHdr[4] = ((pHdr->len & 0x3F) << 2);

        // Header is located after the item data
        dLen = pHdr->len;
        hOfs = NVOCTP_pgOff + dLen;

        if (iLen <= NVOCTP_SMALLITEM)
        {
            // Construct item in one buffer
            // Put data into buffer
            memcpy(NVOCTP_itemBuffer, (const void *)pBuf, dLen);
            // Put most of header into buffer
            memcpy(NVOCTP_itemBuffer + dLen, (const void *)cHdr,
                   NVOCTP_HDRCRCINC);
            // Calculate CRC
            newCRC = NVOCTP_doRAMCRC(NVOCTP_itemBuffer, dLen +
                                     NVOCTP_HDRCRCINC, 0);
            // Insert CRC and last bytes
            cHdr[4] |= ((newCRC >> 6) & 0x3);
            // Note NVOCTP_VALIDIDBIT set implicitly zero
            cHdr[5] = ((newCRC & 0x3F) << 2) | NVOCTP_ACTIVEIDBIT;
            cHdr[6] = NVOCTP_SIGNATURE;
            memcpy(NVOCTP_itemBuffer + dLen, (const void *)cHdr,
                   NVOCTP_ITEMHDRLEN);
            // NVS_write
            NVOCTP_failW = NVOCTP_write(dstPg, NVOCTP_pgOff,
                                        NVOCTP_itemBuffer, iLen);
            // Advance to next location
            NVOCTP_pgOff += iLen;
        }
        else
        {
            // Write header/item separately
            // Calculate CRC on data portion
            newCRC = NVOCTP_doRAMCRC(pBuf, dLen, 0);
            // Finish CRC using header portion
            newCRC = NVOCTP_doRAMCRC(cHdr, NVOCTP_HDRCRCINC, newCRC);
            // Complete Header with CRC, bits, and sig
            cHdr[4] |= ((newCRC >> 6) & 0x3);
            // Note NVOCTP_VALIDIDBIT set implicitly zero
            cHdr[5] = ((newCRC & 0x3F) << 2) | NVOCTP_ACTIVEIDBIT;
            cHdr[6] = NVOCTP_SIGNATURE;
            // Write data
            NVOCTP_failW = NVOCTP_write(dstPg, NVOCTP_pgOff, pBuf, dLen);
            // Write header
            NVOCTP_failW |= NVOCTP_write(dstPg, hOfs, cHdr, NVOCTP_ITEMHDRLEN);
            // Advance to next location
            NVOCTP_ASSERT(NVOCTP_pgOff < (NVOCTP_pgOff + iLen),
                          "Page offset overflow!")
            if(!NVOCTP_failW)
            {
                NVOCTP_pgOff += iLen;
            }
            else
            {
                return;
            }
        }
        // If there was a write failure, delete item
        NVOCTP_ALERT(!NVOCTP_failW, "Driver write failure. Item deleted.")
        if (NVOCTP_failW)
        {
            NVOCTP_setItemInactive(hOfs);
        }
    }
    else
    {
        // Not enough room on page
        NVOCTP_failW = NVINTF_BADLENGTH;
    }
}


/******************************************************************************
 * @fn      NVOCTP_readHeader
 *
 * @brief   Read header block from NV and expand into caller's buffer
 *
 * @param   pg   - A valid NV Flash page
 * @param   ofs  - A valid offset into the page
 * @param   pHdr - Pointer to caller's item header buffer
 *
 * @return  none
 */
static void NVOCTP_readHeader(uint8_t pg,
                              uint16_t ofs,
                              NVOCTP_itemHdr_t *pHdr)
{
    uint8_t sigOfItemBehind;
    cmpIH_t cHdr;

    // Get item header from Flash
    NVOCTP_read(pg, ofs, (uint8_t *)cHdr, NVOCTP_ITEMHDRLEN);

    // Offset to compressed header
    pHdr->hofs = ofs;

    // Compressed item header information <-- Lower Addr    Higher Addr-->
    // Byte: [0]      [1]      [2]      [3]      [4]      [5]      [6]
    // Item: SSSSSSII IIIIIIII SSSSSSSS SSLLLLLL LLLLLLCC CCCCCCAV SSSSSSSS
    // LSB of field:         ^           ^            ^        ^          ^
    pHdr->sysid  = (cHdr[0] >> 2) & 0x3F;
    pHdr->itemid = ((cHdr[0] & 0x3) << 8) | cHdr[1];
    pHdr->subid  = (cHdr[2] << 2) | ((cHdr[3] >> 6) & 0x3);
    pHdr->len    = ((cHdr[3] & 0x3F) << 6) | ((cHdr[4] >> 2) & 0x3F);
    pHdr->crc8   = ((cHdr[4] & 0x3) << 6) | ((cHdr[5] >> 2) & 0x3F);
    pHdr->stats  = cHdr[5] & (NVOCTP_VALIDIDBIT | NVOCTP_ACTIVEIDBIT);
    pHdr->sig    = cHdr[6];
    pHdr->cmpid  = NVOCTP_CMPRID(pHdr->sysid, pHdr->itemid, pHdr->subid);

    // Our item has correct signature?
    if (pHdr->sig != NVOCTP_SIGNATURE)
    {
        // Indicate item is invalid
        NVOCTP_ALERT(FALSE, "Invalid signature detected! Item corrupted.")
        pHdr->stats |=  NVOCTP_VALIDIDBIT;
    }
    else
    {
        // Check for item behind us
        uint16_t nextItemSigOfs = ofs - (pHdr->len + 1);
        if (nextItemSigOfs < ofs)
        {
            NVOCTP_read(pg, nextItemSigOfs, &sigOfItemBehind, NVOCTP_ONEBYTE);
            if (sigOfItemBehind == NVOCTP_SIGNATURE)
            {
                (pHdr->stats) |= NVOCTP_FOLLOWBIT;
            }
        }
        NVOCTP_ALERT(pHdr->stats & NVOCTP_FOLLOWBIT,
                     "Item gap detected. Item not followed.")
    }
}

/******************************************************************************
 * @fn      NVOCTP_readItem
 *
 * @brief   Function to read an item described by iHdr into pBuf
 *
 * @param   iHdr - pointer to an item header struct
 * @param   bOfs - offset into NV data block
 * @param   len - length of NV data to return (0 is illegal)
 * @param   pBuf - pointer to caller's read data buffer  (NULL is illegal)
 *
 * @return  NVINTF_SUCCESS or specific failure code
 */
static uint8_t NVOCTP_readItem(NVOCTP_itemHdr_t *iHdr,
                               uint16_t ofs,
                               uint16_t len,
                               void *pBuf)
{
    uint8_t err = NVINTF_SUCCESS;
    uint16_t dOfs, iOfs;
    iOfs = (iHdr->hofs - iHdr->len);

    // Optional CRC integrity check
    if (NVOCTP_CRCONREAD)
    {
        err = NVOCTP_verifyCRC(iOfs, iHdr->len, iHdr->crc8);
    }
    if(err == NVINTF_SUCCESS)
    {
        // Offset to start of item data
        dOfs = iOfs + ofs;
        if((dOfs + len) <= iHdr->hofs)
        {
            // Copy NV data block to caller's buffer
            NVOCTP_read(NVOCTP_activePg, dOfs, (uint8_t *)pBuf, len);
        }
        else
        {
            // Bad length or offset
            err = (len > iHdr->len) ? NVINTF_BADLENGTH : NVINTF_BADOFFSET;
        }
    }

    return(err);
}

/******************************************************************************
 * @fn      NVOCTP_setItemInactive
 *
 * @brief   Mark an item as inactive
 *
 * @param   iOfs - Offset to item header (lowest address) in active page
 *
 * @return  none
 */
static void NVOCTP_setItemInactive(uint16_t iOfs)
{
    uint8_t tmp;

    // Get byte with validity bit
    tmp = NVOCTP_readByte(NVOCTP_activePg, iOfs + NVOCTP_HDRVLDOFS);

    // Remove ACTIVE_IDS_MARK
    tmp &= ~NVOCTP_ACTIVEIDBIT;

    // Mark the item as inactive
    NVOCTP_writeByte(NVOCTP_activePg, iOfs + NVOCTP_HDRVLDOFS, tmp);
}

/******************************************************************************
 * @fn      NVOCTP_setPageActive
 *
 * @brief   Set specified NV page to active state
 *
 * @param   pg - NV page to activate
 *
 * @return  none
 */
static void NVOCTP_setPageActive(uint8_t pg)
{
    uint8_t cycle;
    uint8_t headerBuff[NVOCTP_PGHDRLEN];

    // Bump the compaction cycle counter, wrap-around if at maximum
    cycle = (NVOCTP_pgCycle < NVOCTP_MAXCYCLE) ?
            (NVOCTP_pgCycle + 1) : NVOCTP_MINCYCLE;

    // Load header
    headerBuff[NVOCTP_PGHDRSIG] = (uint8_t)NVOCTP_SIGNATURE;
    headerBuff[NVOCTP_PGHDRVER] = (uint8_t)NVOCTP_VERSION;
    headerBuff[NVOCTP_PGHDRCYC] = (uint8_t)cycle;
    headerBuff[NVOCTP_PGHDRPST] = (uint8_t)NVOCTP_PGACTIVE;

    // Write to page
    NVOCTP_failW = NVOCTP_write(pg, 0, headerBuff, NVOCTP_PGHDRLEN);

    if(NVOCTP_failW == NVINTF_SUCCESS)
    {
        // No errors, switch active page
        NVOCTP_activePg = pg;
        NVOCTP_pgCycle = cycle;
    }
}

/******************************************************************************
 * @fn      NVOCTP_findOffset
 *
 * @brief   Find the offset to next available empty space in specified page
 *
 * @param   pg   - Valid NV page on which to find offset to next available data
 * @param   ofs - Beginning offset to start search
 *
 * @return  Number of bytes from start of page to next available item location
 */
static uint16_t NVOCTP_findOffset(uint8_t pg,
                                  uint16_t ofs)
{
    uint8_t i,j;
    uint32_t tmp = 0;

    // Find first non-erased 4-byte location
    while(ofs >= sizeof(tmp))
    {
        ofs -= sizeof(tmp);
        NVOCTP_read(pg, ofs, (uint8_t *)&tmp, sizeof(tmp));
        if(tmp != NVOCTP_ERASEDWORD)
        {
            break;
        }
    }

    // Starting with LSB, look for non-erased byte
    for(i = j = 1; i <= 4; i++)
    {
        if((tmp & 0xFF) != NVOCTP_ERASEDBYTE)
        {
            // Last non-erased byte so far
            j = i;
        }
        tmp >>= 8;
    }

    return(ofs + j);
}

/******************************************************************************
 * @fn      NVOCTP_findItem
 *
 * @brief   Find a valid item from designated page and offset
 *
 * @param   pg  - Valid NV page
 * @param   ofs - Offset in NV page from where to start search
 * @param   cid - Compressed NV item ID to search for
 * @param   flag - specifies type of search
 *
 * @return  When >0, offset to the item header for found item
 *          When <=0, -number of items searched when item not found
 *
 */
static int16_t NVOCTP_findItem(uint8_t pg,
                               uint16_t ofs,
                               uint32_t cid,
                               uint8_t flag)
{
    bool found = FALSE;

    while(ofs >= (NVOCTP_PGHDRLEN + NVOCTP_ITEMHDRLEN))
    {
        NVOCTP_itemHdr_t iHdr;

        // Align to start of item header
        ofs -= NVOCTP_ITEMHDRLEN;

        // Read and decompress item header
        NVOCTP_readHeader(pg, ofs, &iHdr);

        if((iHdr.stats & NVOCTP_ACTIVEIDBIT) &&
          !(iHdr.stats & NVOCTP_VALIDIDBIT))
        {
            uint32_t sysid = ((cid >> 24) & 0x3F);
            uint32_t itemid = ((cid >> 12) & 0x3FF);

            switch (flag)
            {
            case NVOCTP_FINDANY:
                found = TRUE;
                break;
            case NVOCTP_FINDSTRICT:
                // Return first cid match
                if (cid == iHdr.cmpid)
                {
                    found = TRUE;
                }
                break;
            case NVOCTP_FINDSYSID:
                // return first sysid match
                if (sysid == iHdr.sysid)
                {
                    found = TRUE;
                }
                break;
            case NVOCTP_FINDITMID:
                // return first sysid AND itemid match
                if (sysid == iHdr.sysid && itemid == iHdr.itemid)
                {
                    found = TRUE;
                }
                break;
            default:
                // Should not get here
                NVOCTP_EXCEPTION(pg, NVINTF_BADPARAM);
                NVOCTP_ASSERT(FALSE, "Unhandled case in findItem().")
                return(0);
            }
            // Item found - return offset of item header
            if (found)
            {
                return(ofs);
            }
        }
        // Try to jump to next item
        if(iHdr.stats & NVOCTP_FOLLOWBIT)
        {
            // Appears to be an item there, check bounds
            if(iHdr.len < ofs)
            {
                // Adjust offset for next try
                ofs -= iHdr.len;
            }
            else
            {
                // Length is corrupt, mark item invalid and compact
                NVOCTP_ALERT(FALSE, "Item length corrupted. Deleting item.")
                NVOCTP_setItemInactive(ofs);
                NVOCTP_compactPage(NVOCTP_activePg);
            }
        }
        else
        {
            // Something is corrupted, compact to fix
            NVOCTP_ALERT(FALSE, "No item following current item, "
                    "compaction needed.")
            NVOCTP_compactPage(NVOCTP_activePg);
        }
    }

    // Item not found (negate number of items searched)
    // or nth not found, return last found
    return(0);
}

/******************************************************************************
 * @fn      NVOCTP_compactPage
 *
 * @brief   Compact specified page by copying active items to other page
 *
 *          Compaction occurs under three circumstances: (1) 'maintenance'
 *          activity which is triggered by a user call to compactNvApi(),
 *          (2) 'update' activity where an NV page is packed to make room
 *          for an item being written. The 'update' mode is performed by
 *          writing the item after the rest of the page has been compacted,
 *          and (3) when corruption is detected in the NV page. The compaction
 *          operation will move all active&valid items to the other page.
 *
 * @param   srcPg - Valid NV page to compact from
 *
 * @return  Number of available bytes on compacted page, -1 if error
 */
static int16_t NVOCTP_compactPage(uint8_t srcPg)
{
    bool needScan = FALSE;
    uint8_t dstPg;
    uint16_t dstOff;
    uint16_t endOff;
    uint16_t srcOff;
#if defined (NVOCTP_STATS)
    uint32_t nvcid;
    uint16_t aitems = 0;
    uint16_t ditems = 0;
#endif // NVOCTP_STATS

    // Reset Flash erase/write fail indicator
    NVOCTP_failW = NVINTF_SUCCESS;

    // Select the destination page
    dstPg = (srcPg == NVOCTP_nvBegPage) ? NVOCTP_nvEndPage : NVOCTP_nvBegPage;

    // Ensure that destination page is ready
    NVOCTP_failW = NVOCTP_erase(dstPg);

    // Mark the specified page to be in XFER state
    NVOCTP_writeByte(srcPg, NVOCTP_PGHDROFS, (uint8_t)NVOCTP_PGXFER);

    // Destination items start right after page header
    dstOff = NVOCTP_PGDATAOFS;
#if defined (NVOCTP_STATS)
    // Create a compressed item ID for NV diagnostic
    nvcid = NVOCTP_CMPRID(NVINTF_SYSID_NVDRVR, 1, 0);
    // Reserved space for NV driver diagnostic item
    dstOff += NVOCTP_ITEMHDRLEN + sizeof(NVOCTP_diag_t);
#endif // NVOCTP_STATS
    // Stop looking when we get to this offset
    endOff = NVOCTP_PGHDRLEN + NVOCTP_ITEMHDRLEN - 1;

    // Source items start with the last written item
    srcOff = NVOCTP_pgOff;

    NVOCTP_ALERT(FALSE, "Compaction triggered.")
    while(srcOff > endOff && dstOff < FLASH_PAGE_SIZE)
    {
        if(NVOCTP_failW == NVINTF_SUCCESS)
        {
            NVOCTP_itemHdr_t srcHdr;
            uint16_t dataLen;
            uint16_t itemSize;

            // Align to start of item header
            srcOff -= NVOCTP_ITEMHDRLEN;

            // Read and decompress item header
            NVOCTP_readHeader(srcPg, srcOff, &srcHdr);
            dataLen  = srcHdr.len;
            itemSize = NVOCTP_ITEMHDRLEN + dataLen;
            
            // Check if length is safe
            if (srcOff < (dataLen + NVOCTP_PGHDRLEN) ||
                    (NVOCTP_SIGNATURE != srcHdr.sig))
            {
                needScan = TRUE;
            }
            else
            {
                needScan = FALSE;
            }

            // Is item valid?
            if(!(srcHdr.stats & NVOCTP_VALIDIDBIT) &&
                    srcHdr.stats & NVOCTP_ACTIVEIDBIT &&
                    !needScan)
            {
                uint16_t crcOff;
                uint8_t crcStatus = NVINTF_FAILURE;

                NVOCTP_ALERT(srcOff >= (dataLen + NVOCTP_PGHDRLEN),
                             "Item header corrupted, data length too long.")
                crcOff    = srcOff - dataLen;
                crcStatus = NVOCTP_verifyCRC(crcOff,dataLen,srcHdr.crc8);

                if (!crcStatus)
                {
                    // Copy valid/non-diag item to XFER page
                    // Unless its the diagnostic item
#if defined (NVOCTP_STATS)
                    if (srcHdr.cmpid != nvcid)
                    {
#endif // NVOCTP_STATS
                        NVOCTP_copyItem(dstPg, crcOff, dstOff, itemSize);
                        dstOff += itemSize;
                        if (dstOff > FLASH_PAGE_SIZE)
                        {
                            // Somehow ran out of space on new page
                            NVOCTP_ALERT(FALSE, "Offset overflow: dstOff")
                            NVOCTP_failW = NVINTF_BADLENGTH;
                        }
#if defined (NVOCTP_STATS)
                        aitems += 1;
                    }
#endif // NVOCTP_STATS
                }
                else
                {
                    // Invalid CRC, corruption
                    NVOCTP_ALERT(FALSE, "Item CRC incorrect!")
                    needScan = TRUE;
                    srcOff--;
                }
            }
#ifdef NVOCTP_STATS
            else
            {
                ditems++;
            }
#endif // NVOCTP_STATS
            // Move to next item if no issues
            if (!needScan)
            {
                NVOCTP_ALERT(srcOff > dataLen, "Offset overflow: srcOff")
                srcOff -= dataLen;
            }
            else
            {
                // Detected a problem, find next header (scan for signature)
                bool foundSig = FALSE;
                NVOCTP_ALERT(FALSE, "Attempting to find signature...")
                while(!foundSig && srcOff > endOff)
                {
                    // read in NVOCTP_XFERBLKMAX bytes at a time for signature
                    uint16_t i, rdLen;
                    uint8_t readBuffer[NVOCTP_XFERBLKMAX];

                    // Check read bounds
                    rdLen   = (srcOff - NVOCTP_XFERBLKMAX > endOff) ?
                            NVOCTP_XFERBLKMAX : srcOff - endOff;
                    srcOff -= rdLen;
                    NVOCTP_read(srcPg, srcOff, readBuffer, rdLen);
                    for(i = rdLen; i > 0; i--)
                    {
                        if (NVOCTP_SIGNATURE == readBuffer[i])
                        {
                            // Found possible header, resume normal operation
                            NVOCTP_ALERT(FALSE, "Found possible signature.")
                            foundSig = TRUE;
                            // srcOff is address of [first byte of item]
                            // after our [found sig]
                            srcOff += (i + 1);
                            break;
                        }
                    }
                }
                // If we get here and foundSig is false, we never found another
                // item in the page
                NVOCTP_ALERT(foundSig, "Attempt to find signature failed.")
            }
        }
        else
        {
            // Failure during item xfer makes next findItem() unreliable
            NVOCTP_ASSERT(FALSE, "COMPACTION FAILURE")
            return(-1);
        }
    }

#if defined (NVOCTP_STATS)
    NVOCTP_diag_t diags;
    NVOCTP_itemHdr_t dHdr;
    // Get the diagnostic info
    NVOCTP_read(srcPg, NVOCTP_PGHDRLEN, (uint8_t *)&diags, sizeof(diags));
    // One more erase/compaction is complete
    diags.compacts += 1;
    // Number of items copied
    diags.active = aitems;
    // Number of items left behind
    diags.deleted = ditems;
    // Number of bad CRCs found
    diags.badCRC += NVOCTP_badCRCCount;
    NVOCTP_badCRCCount = 0;
    // Available space after this item update
    diags.available = (FLASH_PAGE_SIZE - dstOff);
    // Make Diag Header Object
    dHdr.len     = sizeof(diags);
    dHdr.hofs    = 0;
    dHdr.cmpid   = nvcid;
    dHdr.subid   = diagId.subID;
    dHdr.itemid  = diagId.itemID;
    dHdr.sysid   = diagId.systemID;
    dHdr.sig     = NVOCTP_SIGNATURE;
    NVOCTP_pgOff = NVOCTP_PGHDRLEN;
    NVOCTP_writeItem(&dHdr, dstPg, (uint8_t *)&diags);
#endif // NVOCTP_STATS

    // All items have been copied - activate the new page
    NVOCTP_setPageActive(dstPg);

    if(NVOCTP_failW != NVINTF_SUCCESS)
    {
        // Something bad happened when trying to compact the page
        NVOCTP_ASSERT(FALSE, "COMPACTION FAILURE")
        return(-1);
    }

    // Next item offset for activePg
    NVOCTP_pgOff = dstOff;
    // Erase the previous active page
    NVOCTP_failW = NVOCTP_erase(srcPg);

    // Tell caller how much room is left on the active page
    return(FLASH_PAGE_SIZE - dstOff);
}

/******************************************************************************
 * @fn      NVOCTP_copyItem
 *
 * @brief   Copy an NV item from active page to specified destination page
 *
 * @param   dstPg - Destination page
 * @param   sOfs  - Source page offset of original data in active page
 * @param   dOfs  - Destination page offset to transferred copy of the item
 * @param   len   - Length of data to copy
 *
 * @return  none.
 */
static void NVOCTP_copyItem(uint8_t dstPg,
                            uint16_t sOfs,
                            uint16_t dOfs,
                            uint16_t len)
{
    uint16_t num;
    uint8_t tmp[NVOCTP_XFERBLKMAX];

    // Copy over the data: Flash to RAM, then RAM to Flash
    while(len > 0 && !NVOCTP_failW)
    {
        // Number of bytes to transfer in this block
        num = (len < NVOCTP_XFERBLKMAX) ? len : NVOCTP_XFERBLKMAX;

        // Get block of bytes from source page
        NVOCTP_read(NVOCTP_activePg, sOfs, (uint8_t *)&tmp, num);

        // Write block to destination page
        NVOCTP_failW = NVOCTP_write(dstPg, dOfs, (uint8_t *)&tmp, num);

        dOfs += num;
        sOfs += num;
        len  -= num;
    }
}

/******************************************************************************
 * @fn      NVOCTP_readByte
 *
 * @brief   Read one byte from Flash memory
 *
 * @param   pg  - NV Flash page
 * @param   ofs - Offset into the page
 *
 * @return  byte read from flash memory
 */
static uint8_t NVOCTP_readByte(uint8_t pg,
                               uint16_t ofs)
{
    uint8_t byteVal;
    NVOCTP_read(pg, ofs, &byteVal, NVOCTP_ONEBYTE);

    return(byteVal);
}


/******************************************************************************
 * @fn      NVOCTP_writeByte
 *
 * @brief   Write one byte to Flash and read back to verify
 *
 * @param   pg  - NV Flash page
 * @param   ofs - offset into the page
 * @param   bwv - byte to write & verify
 *
 * @return  none ('failF' or 'failW' will be set if write fails)
 */
static void NVOCTP_writeByte(uint8_t pg,
                             uint16_t ofs,
                             uint8_t bwv)
{
    NVOCTP_failW = NVOCTP_write(pg, ofs, &bwv, 1);
}


/******************************************************************************
 * @fn      NVOCTP_doNVCRC
 *
 * @brief   Computes the CRC8 on the NV buffer indicated
 *          CRC code external, API in crc.h
 *
 * @param   pg - Flash page to check
 * @param   ofs - Flash page offset to lowest address item byte
 * @param   len - Item data length
 * @param   crc - value to start with, should be NULL if new calculation
 *
 * @return  crc byte
 */
static uint8_t NVOCTP_doNVCRC(uint8_t pg,
                              uint16_t ofs,
                              uint16_t len,
                              uint8_t crc)
{
    uint16_t rdLen = 0;
    uint8_t tmp[NVOCTP_XFERBLKMAX];
    crc_t newCRC = (crc_t)crc;

    // Read flash and compute CRC in blocks
    while(len > 0)
    {
        rdLen  = (len < NVOCTP_XFERBLKMAX ? len : NVOCTP_XFERBLKMAX);
        NVOCTP_read(pg, ofs, tmp, rdLen);
        newCRC = crc_update(newCRC,tmp,rdLen);
        len   -= rdLen;
        ofs   += rdLen;
    }

    return(newCRC);
}

/******************************************************************************
 * @fn      NVOCTP_doRAMCRC
 *
 * @brief   Calculates CRC8 given a buffer and length
 *          CRC code external, API in crc.h
 *
 * @param   input - pointer to data buffer
 * @param   len - length of data in buffer
 * @param   crc - value to start with, should be NULL if new calculation
 *
 * @return  CRC8 byte
 */
static uint8_t NVOCTP_doRAMCRC(uint8_t *input,
                               uint16_t len, uint8_t crc)
{
    crc_t newCRC = crc_update((crc_t)crc, input, len);

    return(uint8_t)newCRC;
}

/******************************************************************************
 * @fn      NVOCTP_verifyCRC
 *
 * @brief   Helper function to validate item crc from NV
 *
 * @param   iOfs - offset to item data
 * @param   len - length of item data
 * @param   crc - crc to compare against
 *
 * @return  status byte
 */
static uint8_t NVOCTP_verifyCRC(uint16_t iOfs,
                                uint16_t len, uint8_t crc)
{
    uint8_t newCRC;
    uint16_t crcLen   = len + NVOCTP_HDRCRCINC - 1;
    uint8_t finalByte = (len & 0x3F) << 2;

    // CRC calculations stop at the length field of header
    // So the last byte must be done separately
    newCRC = NVOCTP_doNVCRC(NVOCTP_activePg, iOfs, crcLen, 0);
    newCRC = NVOCTP_doRAMCRC(&finalByte,sizeof(finalByte),newCRC);
    NVOCTP_ALERT(newCRC == crc, "Invalid CRC detected.")
#ifdef NVOCTP_STATS
    if (newCRC != crc)
    {
        NVOCTP_badCRCCount++;
    }
#endif // NVOCTP_STATS
    return(newCRC == crc ? NVINTF_SUCCESS : NVINTF_CORRUPT);
}

//*****************************************************************************
