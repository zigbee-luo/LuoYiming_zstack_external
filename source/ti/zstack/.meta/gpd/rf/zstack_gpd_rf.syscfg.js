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
 *  ======== zstack_gpd_rf.syscfg.js ========
 */

"use strict";

// Get common utility functions
const Common154 = system.getScript("/ti/ti154stack/ti154stack_common.js");

// Get RF setting descriptions
const Docs = system.getScript("/ti/ti154stack/rf_config/"
    + "ti154stack_rf_config_docs.js");

let deviceName = Common154.getDeviceOrLaunchPadName(true);
if (deviceName.includes("P1")){
    deviceName = "CC1352P_2_LAUNCHXL";
}

// Get IEEE 2.4 GHz RF defaults for the LaunchPad or device being used
const ieeePhySettings = system.getScript("/ti/ti154stack/rf_config/"
    + deviceName
    + "_rf_defaults.js").defaultIEEEPhyList;

// Get proprietary Sub-1 GHz RF defaults for the LaunchPad or device being used
const propPhySettings = system.getScript("/ti/ti154stack/rf_config/"
    + deviceName
    + "_rf_defaults.js").defaultPropPhyList;

const deviceId = system.deviceData.deviceId;

/* Description text for configurables */
const channelDescription = `The IEEE 802.15.4 frequency channel to use in \
network formation or joining.`;

const channelLongDescription = channelDescription + `\n\n\
**Default:** Channel 11

**Range:** Channel 11-26`;


/* Frequency channel options for enumeration configurables */
const rfOptions = [
    {name: 11, displayName: "11 - 2405 MHz"},
    {name: 12, displayName: "12 - 2410 MHz"},
    {name: 13, displayName: "13 - 2415 MHz"},
    {name: 14, displayName: "14 - 2420 MHz"},
    {name: 15, displayName: "15 - 2425 MHz"},
    {name: 16, displayName: "16 - 2430 MHz"},
    {name: 17, displayName: "17 - 2435 MHz"},
    {name: 18, displayName: "18 - 2440 MHz"},
    {name: 19, displayName: "19 - 2445 MHz"},
    {name: 20, displayName: "20 - 2450 MHz"},
    {name: 21, displayName: "21 - 2455 MHz"},
    {name: 22, displayName: "22 - 2460 MHz"},
    {name: 23, displayName: "23 - 2465 MHz"},
    {name: 24, displayName: "24 - 2470 MHz"},
    {name: 25, displayName: "25 - 2475 MHz"},
    {name: 26, displayName: "26 - 2480 MHz"}
];

/* RF submodule for zstack module */
const gpdRfModule = {
    config: [
        {
            name: "channel",
            displayName: "Channel",
            description: channelDescription,
            longDescription: channelLongDescription,
            default: 11,
            options: rfOptions
        },
        {
            name: "freqBand",
            displayName: "Frequency Band",
            options: getFrequencyBandOptions(),
            default: getDefaultFreqBand(),
            description: Docs.freqBand.description,
            longDescription: Docs.freqBand.longDescription,
            readOnly: true
        },
        {
            name: "phyType",
            displayName: "Phy Type",
            options: getPhyTypeOptions(), 
            default: "phyIEEE",
            hidden: false,
            description: Docs.phyType.description,
            longDescription: Docs.phyType.longDescription
        },
        {
            name: "phyID",
            displayName: "Phy ID",
            default: "APIMAC_PHY_ID_NONE",
            readOnly: Docs.phyID.readOnly,
            description: Docs.phyID.description,
            longDescription: Docs.phyID.longDescription,
            hidden: true
        }
    ],
    moduleInstances: moduleInstances
};

/* RadioConfig module definition and default configurations */
function moduleInstances(inst)
{
    let isPDevice = false;

    if(deviceId.match(/CC1352P/))
    {
        isPDevice = true;
    }

    const radioConfigArgs = {
        codeExportConfig: ieeePhySettings[0].args.codeExportConfig
    };
    radioConfigArgs.codeExportConfig.paExport= isPDevice ? "combined" : "active";

    const radioConfigModule = {
        name: "radioConfig",
        displayName: "Advanced Radio Settings",
        moduleName: "/ti/devices/radioconfig/settings/ieee_15_4",
        readOnly: true,
        collapsed: true,
        args: radioConfigArgs
    };

    return([radioConfigModule]);
}

/*
 *  ======== getFrequencyBandOptions ========
 *  Gets the array of frequency bands supported by the board/devices
 *
 *  @returns Array - an array containing one or more dictionaries with the
 *                   following keys: displayName, name
 */
function getFrequencyBandOptions()
{
    let freqBandOptions = [];

    freqBandOptions.push(
        {
            name: "freqBand24",
            displayName: "2.4 GHz"            
        },
    ) 

    return freqBandOptions;
}

/*
 *  ======== getDefaultFreqBand ========
 *  Retrieves the default frequency band:
 *      * 2.4 GHz for 26X2 boards
 *      * Sub-1 GHz for others
 *
 *  @returns - name of default frequency band
 */
function getDefaultFreqBand()
{
    let defaultFreqBand;

    defaultFreqBand = "freqBand24"

    return defaultFreqBand;
}

/*
 *  ======== getPhyTypeOptions ========
 *  Retrieves the sub-1 GHz phy types from the <board_name>_rf_defaults.js
 *  file and returns an options array for the 15.4 stack
 *
 *  @returns Array - an array containing one or more dictionaries with the
 *                   following keys: displayName, name
 */
function getPhyTypeOptions()
{
    // Construct the drop down options array   
    return _.map(ieeePhySettings, phy => phy.phyDropDownOption);
}

exports = gpdRfModule;
