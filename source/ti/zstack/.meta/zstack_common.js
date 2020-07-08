/*
 * Copyright (c) 2018, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== zstack_common.js ========
 */

"use strict";

const TI154Common = system.getScript("/ti/ti154stack/ti154stack_common.js");

/* Min poll period (ms) */
const POLL_PERIOD_MIN = 100;

/* Max poll period (ms) */
const POLL_PERIOD_MAX = 0xFFFFFFFF;

/* PAN ID length (bytes) */
const PAN_ID_LEN = 2;

/* Extended PAN ID length (bytes) */
const EPID_LEN = 8;

/* Network Key length (bytes) */
const NWK_KEY_LEN = 16;

/* GPD Type length (bytes) */
const GPD_TYPE_LEN = 1;

/* GPD ID length (bytes) */
const GPD_ID_LEN = 4;

/* GPDF Security Key length (bytes) */
const GPDF_SEC_KEY_LEN = 16;

/* Helper function for range validation (making sure a configurable is
 * between a minimum and maximum value */
function validateRange(inst, validation, configVal, configName, configDisplay,
    min, max)
{
    if(min != null && configVal < min)
    {
        validation.logError(
            configDisplay + " must be greater than or equal to " + min,
            inst, configName
        );
    }
    else if(max != null && configVal > max)
    {
        validation.logError(
            configDisplay + " must be less than or equal to " + max,
            inst, configName
        );
    }
}

/* Function that converts the channel array from the RF channel configurables
 * to a bitmask in hex string format */
function chanArrToBitmask(chanArr)
{
    let i;
    let bitmask = 0;
    for(i = 0; i < chanArr.length; i++)
    {
        const channel = chanArr[i];

        // Shift the channel bit depending on what channel is selected
        const bit = 0x1 << (channel);

        // Add channel bit to the bitmask
        bitmask |= bit;
    }
    return("0x" + bitmask.toString(16).padStart(8, "0"));
}

// Dictionary mapping a device name to default LaunchPad; used to discover the
// appropriate RF settings when a device is being used without a LaunchPad
const deviceToBoard = {
    CC1352R: "CC1352R1_LAUNCHXL",
    CC1352P: "CC1352P1_LAUNCHXL",
    CC2652R1: "CC26X2R1_LAUNCHXL",
    CC2652RB: "CC2652RB_LAUNCHXL"
};

/*!
 *  ======== getDeviceOrLaunchPadName ========
 *  Get the name of the board (or device)
 *
 *  @param convertToBoard - Boolean. When true, return the associated LaunchPad
 *                          name if a device is being used without a LaunchPad
 *
 *  @returns String - Name of the board with prefix /ti/boards and
 *                    suffix .syscfg.json stripped off.  If no board
 *                    was specified, the device name is returned.
 */
function getDeviceOrLaunchPadName(convertToBoard)
{
    let name = system.deviceData.deviceId;

    if(system.deviceData.board != null)
    {
        name = system.deviceData.board.source;

        /* Strip off everything up to and including the last '/' */
        name = name.replace(/.*\//, "");

        /* Strip off everything after and including the first '.' */
        name = name.replace(/\..*/, "");
    }

    // Check if this is a standalone device without a LaunchPad
    if(convertToBoard && !name.includes("LAUNCHXL"))
    {
        // Find the LaunchPad name in deviceToBoard dictionary
        let key = null;
        for(key in deviceToBoard)
        {
            if(name.includes(key))
            {
                name = deviceToBoard[key];
                break;
            }
        }
    }

    return(name);
}


/*
 * ======== getBoardPhySettings ========
 * Determines which rf_defaults script to use based on device or inst.rfDesign
 *
 * @param inst - Instance of this module
 *
 * @returns Obj - rf_defaults script from which to get phy settings in
 *                radioconfig format. If device is not supported, returns null
 */
function getBoardPhySettings(inst)
{
    let phySettings;

    if(inst !== null && system.deviceData.deviceId === "CC1352P1F3RGZ")
    {
        const rfDesign = inst.rfDesign;

        // Get the RF Design configurable
        if(rfDesign === "LAUNCHXL-CC1352P-4")
        {
            phySettings = system.getScript("/ti/ti154stack/rf_config/"
                + "CC1352P_4_LAUNCHXL_rf_defaults.js");
        }
        else if(rfDesign === "LAUNCHXL-CC1352P1")
        {
            phySettings = system.getScript("/ti/ti154stack/rf_config/"
                + "CC1352P1_LAUNCHXL_rf_defaults.js");
        }
        else if(rfDesign === "LAUNCHXL-CC1352P-2")
        {
            phySettings = system.getScript("/ti/ti154stack/rf_config/"
                + "CC1352P_2_LAUNCHXL_rf_defaults.js");
        }
    }
    else
    {
        // Initialize with launchpad mapped from device
        phySettings = system.getScript("/ti/ti154stack/rf_config/"
            + TI154Common.getLaunchPadFromDevice() + "_rf_defaults.js");
    }

    return(phySettings);
}


// Settings for ti/devices/CCFG module
const zstackCCFGSettings = {
    CC1352R1_LAUNCHXL_CCFG_SETTINGS: {
        forceVddr: true
    },
    CC1352P1_LAUNCHXL_CCFG_SETTINGS: {},
    CC1352P_2_LAUNCHXL_CCFG_SETTINGS: {
        forceVddr: true
    },
    CC1352P_4_LAUNCHXL_CCFG_SETTINGS: {},
    CC26X2R1_LAUNCHXL_CCFG_SETTINGS: {},
    CC2652RB_LAUNCHXL_CCFG_SETTINGS: {}
};

const boardName = getDeviceOrLaunchPadName(true);
const ccfgSettings = zstackCCFGSettings[boardName + "_CCFG_SETTINGS"];

exports = {
    POLL_PERIOD_MIN: POLL_PERIOD_MIN,
    POLL_PERIOD_MAX: POLL_PERIOD_MAX,
    PAN_ID_LEN: PAN_ID_LEN,
    EPID_LEN: EPID_LEN,
    NWK_KEY_LEN: NWK_KEY_LEN,
    GPD_TYPE_LEN: GPD_TYPE_LEN,
    GPD_ID_LEN: GPD_ID_LEN,
    GPDF_SEC_KEY_LEN: GPDF_SEC_KEY_LEN,
    validateRange: validateRange,
    chanArrToBitmask: chanArrToBitmask,
    getDeviceOrLaunchPadName: getDeviceOrLaunchPadName,
    ccfgSettings: ccfgSettings,
    getBoardPhySettings: getBoardPhySettings
};
