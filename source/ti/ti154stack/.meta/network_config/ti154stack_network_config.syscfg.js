/*
 * Copyright (c) 2017-2018 Texas Instruments Incorporated - http://www.ti.com
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
 *  ======== ti154stack_network_config.syscfg.js ========
 */

"use strict";

// Get common utility functions
const Common = system.getScript("/ti/ti154stack/ti154stack_common.js");

// Get network setting descriptions
const Docs = system.getScript("/ti/ti154stack/network_config/"
    + "ti154stack_network_config_docs.js");

// Configurables for the static 15.4 network settings group
const config = {
    displayName: "Network",
    description: "Configure network settings",
    config: [
        {
            name: "panID",
            displayName: "Pan ID",
            default: 0xffff,
            displayFormat: "hex",
            description: Docs.panID.description,
            longDescription: Docs.panID.longDescription
        },
        {
            name: "channelMask",
            displayName: "Channel Mask",
            default: getDefaultChannelMask(Common.IS_SUB1GHZ_DEVICE()),
            options: getChannelOptions(),
            getDisabledOptions: getDisabledNetworkOptions(),
            minSelections: 0,
            description: Docs.channelMask.description,
            longDescription: Docs.channelMask.longDescription
        },
        {
            name: "fhChannelMask",
            displayName: "FH Channel Mask",
            default: selectAllOptions(getChannelOptions()),
            options: getChannelOptions(),
            getDisabledOptions: getDisabledNetworkOptions(),
            minSelections: 0,
            hidden: true,
            description: Docs.fhChannelMask.description,
            longDescription: Docs.fhChannelMask.longDescription
        },
        {
            name: "fhAsyncChannelMask",
            displayName: "FH Async Channel Mask",
            default: selectAllOptions(getChannelOptions()),
            options: getChannelOptions(),
            getDisabledOptions: getDisabledNetworkOptions(),
            minSelections: 0,
            hidden: true,
            description: Docs.fhAsyncChannelMask.description,
            longDescription: Docs.fhAsyncChannelMask.longDescription
        },
        {
            name: "fhNetname",
            displayName: "FH Network Name",
            default: "FHTest",
            hidden: true,
            description: Docs.fhNetname.description,
            longDescription: Docs.fhNetname.longDescription
        },
        {
            name: "maxDevices",
            displayName: "Max Devices",
            default: 50,
            description: Docs.maxDevices.description,
            longDescription: Docs.maxDevices.longDescription
        },
        {
            displayName: "Application",
            description: "Configure settings for application-level operations",
            config: [
                {
                    name: "reportingInterval",
                    displayName: "Reporting Interval (ms)",
                    default: 3000,
                    description: Docs.reportingInterval.description,
                    longDescription: Docs.reportingInterval.longDescription
                },
                {
                    name: "pollingInterval",
                    displayName: "Polling Interval (ms)",
                    default: 2000,
                    description: Docs.pollingInterval.description,
                    longDescription: Docs.pollingInterval.longDescription
                },
                {
                    name: "trackingDelayTime",
                    displayName: "Tracking Time Delay (ms)",
                    default: 5000,
                    description: Docs.trackingDelayTime.description,
                    longDescription: Docs.trackingDelayTime.longDescription
                },
                {
                    name: "scanBackoffInterval",
                    displayName: "Scan Back-off Interval (ms)",
                    default: 5000,
                    hidden: true,
                    description: Docs.scanBackoffInterval.description,
                    longDescription: Docs.scanBackoffInterval.longDescription
                },
                {
                    name: "orphanBackoffInterval",
                    displayName: "Orphan Back-off Interval (ms)",
                    default: 300000,
                    hidden: true,
                    description: Docs.orphanBackoffInterval.description,
                    longDescription: Docs.orphanBackoffInterval.longDescription
                },
                {
                    name: "scanDuration",
                    displayName: "Scan Duration (s)",
                    default: 5,
                    description: Docs.scanDuration.description,
                    longDescription: Docs.scanDuration.longDescription
                }
            ]
        },
        {
            displayName: "MAC",
            description: "Configure settings for MAC-level operations",
            config: [
                {
                    name: "macBeaconOrder",
                    displayName: "MAC Beacon Order",
                    default: 15,
                    description: Docs.macBeaconOrder.description,
                    longDescription: Docs.macBeaconOrder.longDescription,
                    readOnly: Docs.macBeaconOrder.readOnly
                },
                {
                    name: "macSuperframeOrder",
                    displayName: "MAC Super Frame Order",
                    default: 15,
                    description: Docs.macSuperframeOrder.description,
                    longDescription: Docs.macSuperframeOrder.longDescription,
                    readOnly: Docs.macSuperframeOrder.readOnly
                },
                {
                    name: "minBe",
                    displayName: "Min Back-off Exponent",
                    default: 3,
                    description: Docs.minBe.description,
                    longDescription: Docs.minBe.longDescription
                },
                {
                    name: "maxBe",
                    displayName: "Max Back-off Exponent",
                    default: 5,
                    description: Docs.maxBe.description,
                    longDescription: Docs.maxBe.longDescription
                },
                {
                    name: "fhBroadcastInterval",
                    displayName: "Broadcast Interval (ms)",
                    default: 10000,
                    hidden: true,
                    description: Docs.fhBroadcastInterval.description,
                    longDescription: Docs.fhBroadcastInterval.longDescription
                },
                {
                    name: "fhBroadcastDwellTime",
                    displayName: "Broadcast Dwell Time (ms)",
                    default: 100,
                    hidden: true,
                    description: Docs.fhBroadcastDwellTime.description,
                    longDescription: Docs.fhBroadcastDwellTime.longDescription
                }
            ]
        }
    ]
};

/*
 *******************************************************************************
 Network Group Config Functions
 *******************************************************************************
 */

/*!
 *  ======== getDefaultChannelMask ========
 *  Returns the default channel mask config based on default frequency band
 *  (sub-1 GHz if supported)
 *
 * @param subGSelected  - Sub-1 GHz band currently selected
 * @returns             - default channel mask config
 */
function getDefaultChannelMask(subGSelected)
{
    const channels = subGSelected ? [0, 1, 2, 3] : [11, 12, 13, 14];
    return(channels);
}

/*!
 *  ======== selectAllOptions ========
 *  Returns array with all values from provided drop down options array
 *
 * @param options  - drop down options array
 * @returns        - array with all values from options array
 */
function selectAllOptions(options)
{
    return(_.map(options, "name"));
}

/*!
 *  ======== setDefaultChannelMasks ========
 *  Sets the default channel mask config based on default frequency band
 *  (sub-1 GHz if supported)
 *
 * @param inst   - module instance containing the config to be changed
 * @param freqBandSelected  - frequency band currently selected
 */
function setDefaultChannelMasks(inst, freqBandSelected)
{
    const subGSelected = (freqBandSelected === "freqBandSub1");
    inst.channelMask = getDefaultChannelMask(subGSelected);

    const currSupportedChannels = getCurrSupportedChannels(inst.freqBand,
        inst.freqSub1, inst.phyType);
    inst.fhChannelMask = currSupportedChannels;
    inst.fhAsyncChannelMask = currSupportedChannels;
}

/*
 * ======== getDeviceChannelRange ========
 * Generate and return array of channels supported by device
 *
 * @returns array - array of ints
 */
function getDeviceChannelRange()
{
    let range = [];
    if(Common.IS_433MHZ_DEVICE())
    {
        range = _.range(0, 7); // Range not inclusive
    }
    else if(Common.IS_SUB1GHZ_DEVICE())
    {
        range = _.range(0, 129);
    }
    else
    {
        // Only supports 2.4 GHz
        range = _.range(11, 27);
    }

    return(range);
}

/*
 * ======== getCurrSupportedChannels ========
 * Generate and return array of channels currently supported based on
 * frequency band and phy type currently selected
 *
 * freqBand - frequency band selected
 * freqSub1 - Sub-1 GHz frequency selected (if any)
 * phyType - phy type selected
 *
 * @returns array - array of ints
 */
function getCurrSupportedChannels(freqBand, freqSub1, phyType)
{
    let range = [];
    if(freqBand === "freqBand24")
    {
        range = _.range(11, 27); // Channels 11 - 26
    }
    else
    {
        if(freqSub1 === "freq433")
        {
            range = getDeviceChannelRange(); // Channels 0 - 6
        }

        if(freqSub1 === "freq863")
        {
            if(phyType === "phy5kbps" || phyType === "phy50kbps")
            {
                range = _.range(0, 34); // Channels 0 - 33
            }
            else
            {
                range = _.range(0, 17); // Channels 0 - 16
            }
        }

        if(freqSub1 === "freq915")
        {
            if(phyType === "phy5kbps" || phyType === "phy50kbps")
            {
                range = getDeviceChannelRange(); // Channels 0 - 128
            }
            else
            {
                range = _.range(0, 64); // Channels 0 - 64
            }
        }
    }

    return(range);
}

/*
 * ======== getChannelOptions ========
 * Generate and return options array for channel configs drop down menu
 *
 * @returns array - array of name and display name objects for each channel
 */
function getChannelOptions()
{
    const options = [];

    // Get largest subset of channels allowed by device
    const allowedRange = getDeviceChannelRange();

    // Create an array of drop down options for channel configs
    _.each(allowedRange, (channel) =>
    {
        options.push({
            name: channel,
            displayName: `Channel ${channel}`
        });
    });

    return(options);
}

/*
 * ======== setBeaconSuperFrameOrders ========
 * Update beacon and super frame order configs value and visibility based on
 * selected mode
 *
 * @param inst    - module instance containing the config that changed
 * @param ui      - user interface object
 */
function setBeaconSuperFrameOrders(inst, ui)
{
    if(inst.mode === "beacon")
    {
        ui.macBeaconOrder.readOnly = false;
        ui.macSuperframeOrder.readOnly = false;
        inst.macSuperframeOrder = 8;
        inst.macBeaconOrder = 8;
    }
    else
    {
        ui.macBeaconOrder.readOnly = Docs.macBeaconOrder.readOnly;
        ui.macSuperframeOrder.readOnly = Docs.macSuperframeOrder.readOnly;
        inst.macSuperframeOrder = 15;
        inst.macBeaconOrder = 15;
    }
}

/*
 *  ======== getDisabledNetworkOptions ========
 *  Generates a list of channel options that should be disabled based on
 *  frequency band and data rate selected
 *
 * @returns Array - array of channel options that should be disabled
 */
function getDisabledNetworkOptions()
{
    return(inst) =>
    {
        const disabledOptions = [];

        const allChannelsSupportedByDevice = getDeviceChannelRange();
        const currSupportedChannels = getCurrSupportedChannels(inst.freqBand,
            inst.freqSub1, inst.phyType);

        // Disable any channels supported by device but not by current
        // frequency band and data-rate selections
        const disableChannels = _.difference(allChannelsSupportedByDevice,
            currSupportedChannels);
        _.each(disableChannels, (channel) =>
        {
            disabledOptions.push({
                name: channel,
                reason: "Not supported by device, frequency band or data rate "
                + "selected",
                displayName: `Channel ${channel}`
            });
        });

        return(disabledOptions);
    };
}

/*
 * ======== getNetworkConfigHiddenState ========
 * Get the expected visibility of the selected config
 *
 * @param inst    - module instance containing the config that changed
 * @param cfgName - name of config
 * @returns bool  - true if hidden, false if visible
 */
function getNetworkConfigHiddenState(inst, cfgName)
{
    const freqHoppingSelected = (inst.mode === "frequencyHopping");
    const beaconModeSelected = (inst.mode === "beacon");
    const isCollectorProject = (inst.project.includes("collector"));
    const isSensorProject = (inst.project.includes("sensor"));
    let isVisible = true;
    switch(cfgName)
    {
        case "channelMask":
        {
            isVisible = !freqHoppingSelected;
            break;
        }
        case "fhNetname":
        case "fhChannelMask":
        case "fhAsyncChannelMask":
        {
            isVisible = freqHoppingSelected;
            break;
        }
        case "fhBroadcastInterval":
        case "fhBroadcastDwellTime":
        {
            isVisible = isCollectorProject && freqHoppingSelected;
            break;
        }
        case "orphanBackoffInterval":
        case "scanBackoffInterval":
        {
            isVisible = isSensorProject && !freqHoppingSelected;
            break;
        }
        case "trackingDelayTime":
        case "maxDevices":
        {
            isVisible = isCollectorProject;
            break;
        }
        case "pollingInterval":
        {
            isVisible = !beaconModeSelected;
            break;
        }
        case "reportingInterval":
        case "panID":
        case "scanDuration":
        case "macBeaconOrder":
        case "macSuperframeOrder":
        case "minBe":
        case "maxBe":
        default:
        {
            isVisible = true;
            break;
        }
    }

    // Return whether config is hidden
    return(!isVisible);
}

/*
 * ======== setNetworkConfigHiddenState ========
 * Sets the visibility of the selected config
 *
 * @param inst    - module instance containing the config that changed
 * @param ui      - user interface object
 * @param cfgName - name of config
 */
function setNetworkConfigHiddenState(inst, ui, cfgName)
{
    // Set visibility of config
    ui[cfgName].hidden = getNetworkConfigHiddenState(inst, cfgName);

    if(ui[cfgName].hidden)
    {
        // get a list of all nested and unnested configs
        const configToReset = Common.findConfig(config.config, cfgName);
        // restore the default value for the hidden parameter.
        Common.restoreDefaultValue(inst, configToReset, cfgName);
    }
}

/*
 * ======== validateOneChannelSelected ========
 * Validate that at least one channel is selected
 *
 * @param inst       - Network settings instance to be validated
 * @param validation - object to hold detected validation issues
 * @param cfgName    - name of channel mask config to be validated
 */
function validateOneChannelSelected(inst, validation, cfgName)
{
    // Verify that at least one channel is selected (if config not hidden)
    if((!getNetworkConfigHiddenState(inst, cfgName))
        && (inst[cfgName].length === 0))
    {
        validation.logError("Must select at least one channel", inst, cfgName);
    }
}

/*
 * ======== validateOrder ========
 * Validate the beacon or superframe order for beacon mode
 *
 * @param inst       - Network settings instance to be validated
 * @param validation - object to hold detected validation issues
 * @param cfgName    - name of beacon/superframe order config to be validated
 */
function validateOrder(inst, validation, cfgName)
{
    if(inst.mode === "beacon")
    {
        Common.validateRangeInt(inst, validation, cfgName, 1, 14);
        if(inst[cfgName] !== 8)
        {
            validation.logInfo("Optimal value is 8", inst, cfgName);
        }
    }
}

/*
 * ======== validateBackOffExponents ========
 * Validate minBe and maxBe configs
 *
 * @param inst       - Network settings instance to be validated
 * @param validation - object to hold detected validation issues
 */
function validateBackOffExponents(inst, validation)
{
    if(inst.maxBe < inst.minBe)
    {
        validation.logError("Max must be more than min", inst,
            ["minBe", "maxBe"]);
    }

    Common.validateRangeInt(inst, validation, "minBe", 0,
        Common.cTypeMax.u_int8);
    Common.validateRangeInt(inst, validation, "maxBe", 0,
        Common.cTypeMax.u_int8);
}

/*
 * ======== validate ========
 * Validate this inst's configuration
 *
 * @param inst       - Network settings instance to be validated
 * @param validation - object to hold detected validation issues
 */
function validate(inst, validation)
{
    // Validate PAN ID range -- always visible
    Common.validateRangeHex(inst, validation, "panID", 0, 0xffff);

    // Verify that at least one channel is selected
    validateOneChannelSelected(inst, validation, "channelMask");
    validateOneChannelSelected(inst, validation, "fhChannelMask");
    validateOneChannelSelected(inst, validation, "fhAsyncChannelMask");

    // Validate FH net name if not hidden
    if(!getNetworkConfigHiddenState(inst, "fhNetname"))
    {
        if(inst.fhNetname.length >= 32)
        {
            validation.logError("Must be less than 32 characters", inst,
                "fhNetname");
        }
    }

    // Validate max devices config if not hidden
    if(!getNetworkConfigHiddenState(inst, "maxDevices"))
    {
        // Validate max devices range
        Common.validateRangeInt(inst, validation, "maxDevices", 0,
            Common.cTypeMax.int16_t);

        // Add info msgs if max devices value updated from default
        const maxDevicesDefault = Common.findConfig(config.config,
            "maxDevices").default;

        if(inst.maxDevices !== maxDevicesDefault)
        {
            validation.logInfo("The selected value requires "
            + "MAX_DEVICE_TABLE_ENTRIES to be updated in collector.opts",
            inst, "maxDevices");
        }

        if(inst.maxDevices > 150)
        {
            validation.logInfo("It is not recommended that this value be set "
                + "above 150", inst, "maxDevices");
        }
    }

    // Validate reporting interval range -- always visible
    Common.validateRangeInt(inst, validation, "reportingInterval", 0,
        Common.cTypeMax.u_int32);

    // Add info msgs if reporting interval value below recommended value
    if(inst.reportingInterval < 500)
    {
        validation.logInfo("It is not recommended that this value be set below "
            + "500", inst, "reportingInterval");
    }

    // Validate polling interval range -- always visible
    Common.validateRangeInt(inst, validation, "pollingInterval", 0,
        Common.cTypeMax.u_int32);

    // Add info msgs if polling interval value below recommended value
    if(inst.phyID.includes("LRM"))
    {
        if(inst.pollingInterval < 500)
        {
            validation.logInfo("It is not recommended that this value be set "
            + "below 500 ms for a data rate of 5 kbps", inst,
            "pollingInterval");
        }
    }
    else if(inst.pollingInterval < 100)
    {
        validation.logInfo("It is not recommended that this value be set "
            + "below 100 ms for this data rate", inst, "pollingInterval");
    }

    // Validate scan backoff interval ranges
    if(!getNetworkConfigHiddenState(inst, "trackingDelayTime"))
    {
        Common.validateRangeInt(inst, validation, "trackingDelayTime", 0,
            Common.cTypeMax.u_int32);
    }

    // Validate scan backoff interval ranges
    if(!getNetworkConfigHiddenState(inst, "scanBackoffInterval"))
    {
        Common.validateRangeInt(inst, validation, "scanBackoffInterval", 0,
            Common.cTypeMax.u_int32);
    }

    // Validate orphan backoff interval ranges
    if(!getNetworkConfigHiddenState(inst, "orphanBackoffInterval"))
    {
        Common.validateRangeInt(inst, validation, "orphanBackoffInterval", 0,
            Common.cTypeMax.u_int32);
    }

    // Validate scan duration range -- always visible
    Common.validateRangeInt(inst, validation, "scanDuration", 0,
        Common.cTypeMax.u_int8);

    // Validate superframe and beacon order configs -- always visible
    validateOrder(inst, validation, "macBeaconOrder");
    validateOrder(inst, validation, "macSuperframeOrder");

    // Validate backoff exponent configs -- always visible
    validateBackOffExponents(inst, validation);

    // Validate FH broadcast interval time range if not hidden
    if(!getNetworkConfigHiddenState(inst, "fhBroadcastInterval"))
    {
        Common.validateRangeInt(inst, validation, "fhBroadcastInterval", 0,
            Common.cTypeMax.u_int32);
    }

    // Validate FH broadcast dwell time range if not hidden
    if(!getNetworkConfigHiddenState(inst, "fhBroadcastDwellTime"))
    {
        Common.validateRangeInt(inst, validation, "fhBroadcastDwellTime", 0,
            Common.cTypeMax.u_int8);
    }
}

/*
 *******************************************************************************
 Module Dependencies and Exports
 *******************************************************************************
 */

// Exports to the top level 15.4 module
exports = {
    config: config,
    validate: validate,
    setNetworkConfigHiddenState: setNetworkConfigHiddenState,
    getNetworkConfigHiddenState: getNetworkConfigHiddenState,
    setDefaultChannelMasks: setDefaultChannelMasks,
    setBeaconSuperFrameOrders: setBeaconSuperFrameOrders,
    getChannelOptions: getChannelOptions,
    getDisabledNetworkOptions: getDisabledNetworkOptions
};
