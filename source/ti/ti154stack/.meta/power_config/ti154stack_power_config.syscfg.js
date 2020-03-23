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
 *  ======== ti154stack_power_config.syscfg.js ========
 */

"use strict";

// Get common utility functions
const Common = system.getScript("/ti/ti154stack/ti154stack_common.js");

// Get power setting descriptions
const Docs = system.getScript("/ti/ti154stack/power_config/"
    + "ti154stack_power_config_docs.js");

// Dictionary mapping device to value at which forceVddr needs to be defined
// Values taken from radio config module
const boardToForceVddrTxPower = {
    CC1312R: "14",
    CC1352R: "14",
    CC1352P1: "14",
    CC1352P_2: "14",
    CC1352P_4: "14.5"
};

// Default tx power value for subG
const defaultTxPowerValueSubG = 0;

// Default tx power value for 2.4G
const defaultTxPowerValue24G = 0;

// Configurables for the static 15.4 Power Settings group
const config = {
    displayName: "Power",
    description: "Configure power settings for radio operation",
    config: [
        {
            name: "transmitPowerSubG",
            displayName: "Transmit Power",
            options: getTxPowerConfigOptions(true),
            default: getDefaultTxPower(true),
            hidden: !Common.IS_SUB1GHZ_DEVICE(),
            description: Docs.transmitPowerSubG.description,
            longDescription: Docs.transmitPowerSubG.longDescription
        },
        {
            name: "transmitPower24G",
            displayName: "Transmit Power",
            options: getTxPowerConfigOptions(false),
            default: getDefaultTxPower(false),
            hidden: Common.IS_SUB1GHZ_DEVICE(),
            description: Docs.transmitPower24G.description,
            longDescription: Docs.transmitPower24G.longDescription
        },
        {
            name: "rxOnIdle",
            displayName: "Non Sleepy Device",
            default: false,
            hidden: true,
            description: Docs.rxOnIdle.description,
            longDescription: Docs.rxOnIdle.longDescription
        }
    ]
};

/*
 *******************************************************************************
 Power Group Config Functions
 *******************************************************************************
 */

/*!
 *  ======== getRFSettingsConfig ========
 *  Get list of RF settings from Radio Config
 *
 * @param isSub1BandSet - Boolean. Determines whether to return the rf settings
 *                     for subG or IEEE band.
 *
 *                     If the band selected is not supported (e.g. when
 *                     populating the tx power config pertaining to an
 *                     unsupported band), the supported band settings will be
 *                     used as 'dummy' values.
 * @returns          - Config for RF settings
 */
function getRFSettingsConfig(isSub1BandSet)
{
    // RF Settings from Radio Config
    let rfSettings;

    // Return RF setting based on selection and support for that band
    if((isSub1BandSet && Common.IS_SUB1GHZ_DEVICE())
        || !Common.IS_24GHZ_DEVICE())
    {
        rfSettings = system.getScript("/ti/devices/radioconfig/settings/prop");
    }
    else if((!isSub1BandSet && Common.IS_24GHZ_DEVICE())
        || !Common.IS_SUB1GHZ_DEVICE())
    {
        rfSettings = system.getScript("/ti/devices/radioconfig/settings/"
        + "ieee_15_4");
    }

    return(_.cloneDeep(rfSettings.config));
}

/*!
 *  ======== getDefaultTxPower ========
 *  Get the default tx power value for a given band
 *
 * @param isSub1BandSet - Boolean. Determines whether to return the default
 *                     power option for subG or IEEE bands
 *
 *  @returns         - default tx power option defined by constant if
 *                     available, otherwise returns first value in
 *                     tx power list
 */
function getDefaultTxPower(isSub1BandSet)
{
    let defaultTxPower;
    let desiredDefault;
    const txPowerList = getTxPowerConfigOptions(isSub1BandSet);

    if(isSub1BandSet)
    {
        desiredDefault = defaultTxPowerValueSubG;
    }
    else
    {
        desiredDefault = defaultTxPowerValue24G;
    }

    // Check if desired default is supported
    if(_.some(txPowerList, {name: desiredDefault}))
    {
        defaultTxPower = desiredDefault;
    }
    else
    {
        defaultTxPower = txPowerList[0].name;
    }

    return(defaultTxPower);
}

/*!
 *  ======== getTxPowerConfigOptions ========
 *  Get list of available Tx power values based on board and band set
 *
 * @param isSub1BandSet - Boolean. Determines whether to return the transmit
 *                      power options for subG or IEEE bands
 *
 *  @returns          - list of transmit power options available
 */
function getTxPowerConfigOptions(isSub1BandSet)
{
    // Get drop down options of RF tx power config
    const txPowerOptsObj = getTxPowerRFConfig(isSub1BandSet);
    const txPowerHiOpts = txPowerOptsObj.txPowerHi.options;
    const txPowerOpts = txPowerOptsObj.txPower.options;

    let txPowerValueList = _.unionWith(txPowerOpts, txPowerHiOpts, _.isEqual);

    // Round all tx power values
    _.forEach(txPowerValueList, (option) =>
    {
        option.name = _.round(option.name);
    });

    // Remove any duplicates
    txPowerValueList = _.uniqBy(txPowerValueList, "name");

    // Sort values in descending order
    txPowerValueList = _.orderBy(txPowerValueList, "name", "desc");

    return(txPowerValueList);
}

/*!
 *  ======== getTxPowerRFConfig ========
 *  Get the transmit power value options list from radio config module
 *
 * @param isSub1BandSet - Boolean. Determines whether to return the default
 *                        power option for subG or IEEE bands
 * @returns             - Object that holds radio config txPower configs for
 *                        regular and high PA
 */
function getTxPowerRFConfig(isSub1BandSet)
{
    // Dummy empty values
    let txPowerOptions = {options: []};
    let txPowerHiOptions = {options: []};

    // Get RF settings from Radio Config
    const rfSettings = getRFSettingsConfig(isSub1BandSet, Common.BOARD);

    switch(Common.BOARD)
    {
        case "CC1352P1_LAUNCHXL":
            // 1352P1 only supports Sub-1 GHz
            txPowerOptions = Common.findConfig(rfSettings, "txPower");
            txPowerHiOptions = Common.findConfig(rfSettings, "txPowerHi");
            break;
        case "CC1352P_2_LAUNCHXL":
            txPowerOptions = Common.findConfig(rfSettings, "txPower");

            // On 1352P2 the high PA is enabled for 2.4 GHz
            if(!isSub1BandSet)
            {
                txPowerHiOptions = Common.findConfig(rfSettings, "txPowerHi");
            }
            break;
        case "CC1352P_4_LAUNCHXL":
            // 1352P4 only supports Sub-1 GHz
            txPowerOptions = Common.findConfig(rfSettings, "txPower433");
            txPowerHiOptions = Common.findConfig(rfSettings, "txPower433Hi");
            break;
        default:
            // High PA not supported for 1312R1, 1352R1, 26X2R1, 2652RB
            txPowerOptions = Common.findConfig(rfSettings, "txPower");
            break;
    }

    return{
        txPower: txPowerOptions,
        txPowerHi: txPowerHiOptions
    };
}

/*!
 *  ======== mapCurrTxPowerToRFConfig ========
 * Returns an object containing parameters that must be set in radio config
 * module to set radio config tx power to that of 15.4 transmit power
 *
 * @param isSub1BandSet - Boolean. Determines whether to return the default
 *                        power option for subG or IEEE bands
 * @param inst          - Module instance containing the config that changed
 * @returns                 - Object that holds params corresponding to tx power
 *                            set in radio config module
 *                              - cfgName: tx power config name
 *                                         Valid options: txPower, txPower433,
 *                                                        txPowerHi,
 *                                                        txPower433Hi
 *                              - highPA: true/false if supported; undefined
 *                                        otherwise
 *                              - txPower: unrounded tx power value
 */
function mapCurrTxPowerToRFConfig(inst)
{
    let retHighPA;
    let retTxPower;
    let retCfgName;
    let curTxPower;

    // Get current tx power
    const isSub1BandSet = (inst.freqBand === "freqBandSub1");
    if(isSub1BandSet)
    {
        curTxPower = inst.transmitPowerSubG;
    }
    else
    {
        curTxPower = inst.transmitPower24G;
    }

    // Get drop down options of RF tx power config
    const txPowerOptsObj = getTxPowerRFConfig(isSub1BandSet);
    const txPowerHiOpts = txPowerOptsObj.txPowerHi.options;
    const txPowerOpts = txPowerOptsObj.txPower.options;

    // Find the first value that is less than current transmit power since
    // transmit power values in drop down are rounded up. Will be undefined
    // if config not found (e.g. high PA not supported for set band and board)
    const mappedRFTxPowerHi = _.find(txPowerHiOpts,
        (option) => (option.name <= curTxPower));

    const mappedRFTxPower = _.find(txPowerOpts,
        (option) => (option.name <= curTxPower));

    // First check if current tx power valid with high PA since high PA will be
    // used if transmit power is available with and without high PA
    if(!_.isUndefined(mappedRFTxPowerHi))
    {
        // txPowerHi config only available on high PA boards
        retHighPA = true;
        retTxPower = mappedRFTxPowerHi;

        // Get tx power config name based on board
        retCfgName = Common.IS_433MHZ_DEVICE() ? "txPower433Hi" : "txPowerHi";
    }
    else
    {
        if(Common.IS_HIGHPA_DEVICE())
        {
            // highPA config must be set to false for high PA devices
            retHighPA = false;
        }
        else
        {
            // highPA config does not exist for non-high PA devices
            retHighPA = undefined;
        }
        retTxPower = mappedRFTxPower;

        // Get tx power config name based on board
        retCfgName = Common.IS_433MHZ_DEVICE() ? "txPower433" : "txPower";
    }

    return{
        cfgName: retCfgName,
        highPA: retHighPA,
        txPower: retTxPower.name
    };
}


/*
 * ======== validate ========
 * Validate this inst's configuration
 *
 * Verify that Force VDDR is on if at required transmit power level.
 *
 * @param inst       - RF Settings instance to be validated
 * @param validation - object to hold detected validation issues
 */
function validate(inst, validation)
{
    const ccfg = system.modules["/ti/devices/CCFG"];

    // Get current 15.4 transmit power config
    const isSub1BandSet = (inst.freqBand === "freqBandSub1");
    let txPower154CfgName;
    if(isSub1BandSet)
    {
        txPower154CfgName = "transmitPowerSubG";
    }
    else
    {
        txPower154CfgName = "transmitPower24G";
    }

    // Get params that map current 15.4 tx power config value to radio config
    // tx power config
    const rfTxPowerValObj = mapCurrTxPowerToRFConfig(inst);

    // Force VDDR only applicable to non-high PA
    if(!rfTxPowerValObj.cfgName.includes("Hi"))
    {
        let key = null;
        for(key in boardToForceVddrTxPower)
        {
            // Verify that at force VDDR on at required level
            if((Common.BOARD.includes(key))
                && (rfTxPowerValObj.txPower === boardToForceVddrTxPower[key])
                && (ccfg.$static.forceVddr === false))
            {
                validation.logWarning("The selected RF TX Power requires Force "
                + "VDDR to be enabled in the Device Configuration module",
                inst, txPower154CfgName);
                break;
            }
        }
    }
    else
    {
        validation.logInfo("The selected RF TX Power enables high PA ",
            inst, txPower154CfgName);
    }
}

/*
 * ======== getPowerConfigHiddenState ========
 * Get the expected visibility of the selected config
 *
 * @param inst    - module instance containing the config that changed
 * @param cfgName - name of config
 * @returns bool  - true if hidden, false if visible
 */
function getPowerConfigHiddenState(inst, cfgName)
{
    let isVisible = true;
    switch(cfgName)
    {
        case "transmitPowerSubG":
        {
            isVisible = (inst.freqBand === "freqBandSub1");
            break;
        }
        case "transmitPower24G":
        {
            isVisible = (inst.freqBand === "freqBand24");
            break;
        }
        case "rxOnIdle":
        {
            isVisible = inst.project.includes("sensor");
            break;
        }
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
 * ======== setPowerConfigHiddenState ========
 * Sets the visibility of the selected config
 *
 * @param inst    - module instance containing the config that changed
 * @param ui      - user interface object
 * @param cfgName - name of config
 */
function setPowerConfigHiddenState(inst, ui, cfgName)
{
    // Set visibility of config
    ui[cfgName].hidden = getPowerConfigHiddenState(inst, cfgName);
    if(ui[cfgName].hidden)
    {
        // get a list of all nested and unnested configs
        const configToReset = Common.findConfig(config.config, cfgName);
        // restore the default value for the hidden parameter.
        Common.restoreDefaultValue(inst, configToReset, cfgName);
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
    mapCurrTxPowerToRFConfig: mapCurrTxPowerToRFConfig,
    getDefaultTxPower: getDefaultTxPower,
    boardToForceVddrTxPower: boardToForceVddrTxPower,
    getTxPowerConfigOptions: getTxPowerConfigOptions,
    setPowerConfigHiddenState: setPowerConfigHiddenState,
    getPowerConfigHiddenState: getPowerConfigHiddenState
};
