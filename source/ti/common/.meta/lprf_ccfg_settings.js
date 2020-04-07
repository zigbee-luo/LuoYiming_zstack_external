/*
 * Copyright (c) 2019 Texas Instruments Incorporated - http://www.ti.com
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
 *
 */

/*
 *  ======== lp_common_ccfg_settings.js ========
 */

/*
 * TO USE THIS FILE:
 *    Your stack MUST export a ccfgSettings object.
 *
 *    In your .syscfg file, you can use the following code snippet to set the
 *    CCFG settings:
 *     var device = scripting.addModule("ti/devices/CCFG");
 *     const ccfgSettings = system.getScript("/ti/common/lprf_ccfg_settings.js").ccfgSettings;
 *     for(var setting in ccfgSettings)
 *     {
 *         device[setting] = ccfgSettings[setting];
 *     }
 */

"use strict";

let ccfgSettings = {};

// Used to find if stack module is currently added to the configuration
const stacks = [
    "/ti/easylink/easylink",
    "/ti/ble5stack/ble",
    "/ti/dmm/dmm",
    "/ti/thread/thread",
    "/ti/ti154stack/ti154stack",
    "/ti/zstack/zstack",
    "/ti/devices/radioconfig/custom"
];

// Dictionary mapping a device name to default LaunchPad; used to discover the
// appropriate RF settings when a device is being used without a LaunchPad
const deviceToBoard = {
    CC1352R: "CC1352R1_LAUNCHXL",
    CC1352P: "CC1352P1_LAUNCHXL",
    CC1312: "CC1312R1_LAUNCHXL",
    CC2642: "CC26X2R1_LAUNCHXL",
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

// Settings for ti/devices/CCFG module
const boardSpecificCCFGSettings = {
    CC1312R1_LAUNCHXL_CCFG_SETTINGS: {
        enableBootloader: true,
        enableBootloaderBackdoor: true,
        dioBootloaderBackdoor: 15,
        levelBootloaderBackdoor: "Active low"
    },
    CC1352R1_LAUNCHXL_CCFG_SETTINGS: {
        enableBootloader: true,
        enableBootloaderBackdoor: true,
        dioBootloaderBackdoor: 15,
        levelBootloaderBackdoor: "Active low"
    },
    CC1352P1_LAUNCHXL_CCFG_SETTINGS: {
        xoscCapArray: true,
        xoscCapArrayDelta: 0xC1,
        enableBootloader: true,
        enableBootloaderBackdoor: true,
        dioBootloaderBackdoor: 15,
        levelBootloaderBackdoor: "Active low"
    },
    CC1352P_2_LAUNCHXL_CCFG_SETTINGS: {
        xoscCapArray: true,
        xoscCapArrayDelta: 0xC1,
        enableBootloader: true,
        enableBootloaderBackdoor: true,
        dioBootloaderBackdoor: 15,
        levelBootloaderBackdoor: "Active low"
    },
    CC1352P_4_LAUNCHXL_CCFG_SETTINGS: {
        xoscCapArray: true,
        xoscCapArrayDelta: 0xC1,
        enableBootloader: true,
        enableBootloaderBackdoor: true,
        dioBootloaderBackdoor: 15,
        levelBootloaderBackdoor: "Active low"
    },
    CC26X2R1_LAUNCHXL_CCFG_SETTINGS: {
        enableBootloader: true,
        enableBootloaderBackdoor: true,
        dioBootloaderBackdoor: 13,
        levelBootloaderBackdoor: "Active low"
    },
    CC2652RB_LAUNCHXL_CCFG_SETTINGS: {
        enableBootloader: true,
        enableBootloaderBackdoor: true,
        dioBootloaderBackdoor: 13,
        levelBootloaderBackdoor: "Active low",
        srcClkLF: "LF RCOSC"
    }
};

// Get the LaunchPad specific CCFG Settings
if(system.deviceData.board)
{
    const boardName = getDeviceOrLaunchPadName(false);
    ccfgSettings = Object.assign(ccfgSettings,
        boardSpecificCCFGSettings[boardName + "_CCFG_SETTINGS"]);
}

// Get the stack specific CCFG settings
let stack = "";
for(stack of stacks)
{
    if(system.modules[stack])
    {
        // Workaround for rf driver examples provided by EasyLink
        if(stack === "/ti/devices/radioconfig/custom")
        {
            stack = "/ti/easylink/easylink";
        }

        const stackCommon = system.getScript(stack + "_common.js");

        // Verify that the stack has ccfgSettings to provide before setting them
        if(stackCommon.ccfgSettings)
        {
            ccfgSettings = Object.assign(ccfgSettings,
                stackCommon.ccfgSettings);
        }
    }
}

exports = {
    ccfgSettings: ccfgSettings
};
