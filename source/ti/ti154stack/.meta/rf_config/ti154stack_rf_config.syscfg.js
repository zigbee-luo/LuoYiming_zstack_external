
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
 *  ======== ti154stack_rf_config.syscfg.js ========
 */

"use strict";

// Get common utility functions
const Common = system.getScript("/ti/ti154stack/ti154stack_common.js");

// Get transmit power settings script
const powerScript = system.getScript("/ti/ti154stack/power_config/"
    + "ti154stack_power_config");

// Get network settings script
const networkScript = system.getScript("/ti/ti154stack/network_config/"
    + "ti154stack_network_config");

// Get network settings script
const oadScript = system.getScript("/ti/ti154stack/oad_config/"
    + "ti154stack_oad_config");

// Get RF setting descriptions
const Docs = system.getScript("/ti/ti154stack/rf_config/"
    + "ti154stack_rf_config_docs.js");

// Get proprietary Sub-1 GHz RF defaults for the LaunchPad or device being used
const propPhySettings = system.getScript("/ti/ti154stack/rf_config/"
    + Common.getDeviceOrLaunchPadName(true)
    + "_rf_defaults.js").defaultPropPhyList;

// Get IEEE 2.4 GHz RF defaults for the LaunchPad or device being used
const ieeePhySettings = system.getScript("/ti/ti154stack/rf_config/"
+ Common.getDeviceOrLaunchPadName(true)
+ "_rf_defaults.js").defaultIEEEPhyList;

// Configurables for the RF Configuration module
const config = {
    name: "radioSettings",
    displayName: "Radio",
    description: "Configure PHY settings for radio operations",
    config: [
        {
            name: "freqBand",
            displayName: "Frequency Band",
            options: getFrequencyBandOptions(),
            default: getDefaultFreqBand(Common.IS_SUB1GHZ_DEVICE()),
            description: Docs.freqBand.description,
            longDescription: Docs.freqBand.longDescription,
            onChange: onFreqBandChange
        },
        {
            name: "freqSub1",
            displayName: "Sub-1 GHz Frequency",
            options: getSub1GHzFrequencies(),
            default: getDefaultFreqSub1(),
            hidden: !Common.IS_SUB1GHZ_DEVICE(),
            description: Docs.freqSub1.description,
            longDescription: Docs.freqSub1.longDescription,
            onChange: onFreqSub1orPhyTypeChange
        },
        {
            name: "phyType",
            displayName: "Phy Type",
            options: getPhyTypeOptions(),
            default: getDefaultPhyType(Common.IS_SUB1GHZ_DEVICE()),
            getDisabledOptions: getDisabledPhyTypeOptions(),
            hidden: false,
            description: Docs.phyType.description,
            longDescription: Docs.phyType.longDescription,
            onChange: onFreqSub1orPhyTypeChange
        },
        {
            name: "phyID",
            displayName: "Phy ID",
            default: getDefaultPhy154Settings().ID,
            readOnly: Docs.phyID.readOnly,
            description: Docs.phyID.description,
            longDescription: Docs.phyID.longDescription
        },
        {
            name: "channelPage",
            displayName: "Channel Page",
            default: getDefaultPhy154Settings().channelPage,
            readOnly: Docs.channelPage.readOnly,
            description: Docs.channelPage.description,
            longDescription: Docs.channelPage.longDescription
        }
    ]
};

/*
 *******************************************************************************
 Radio Group-Specific Functions
 *******************************************************************************
 */

/*
 * ======== onFreqBandChange ========
 * On change function for freqBand config
 * Updates visibility and values of frequency band-dependent configs
 *
 * @param inst - 15.4 instance
 * @param ui   - user interface object
 */
function onFreqBandChange(inst, ui)
{
    // Set visibility of dependent configs
    setRFConfigHiddenState(inst, ui, "freqSub1");
    powerScript.setPowerConfigHiddenState(inst, ui, "transmitPowerSubG");
    powerScript.setPowerConfigHiddenState(inst, ui, "transmitPower24G");

    // Phy type must be updated before phy ID and channel page
    setPhyType(inst);
    setPhyIDChannelPage(inst);

    // Update values of frequency dependent configs
    networkScript.setDefaultChannelMasks(inst, inst.freqBand);
    oadScript.setDefaultOADBlockSize(inst, inst.freqBand);
}

/*
 * ======== onFreqSub1orPhyTypeChange ========
 * On change function for freqBandSub1 and phy type configs
 * Updates values of phy ID and channel page based on frequency and phy type
 *
 * @param ui   - user interface object
 */
function onFreqSub1orPhyTypeChange(inst)
{
    setPhyIDChannelPage(inst);

    // Update values of frequency dependent configs
    networkScript.setDefaultChannelMasks(inst, inst.freqBand);
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
    const freqBandOptions = [];

    if(Common.IS_SUB1GHZ_DEVICE())
    {
        freqBandOptions.push(
            {
                name: "freqBandSub1",
                displayName: "Sub-1 GHz"
            }
        );
    }

    if(Common.IS_24GHZ_DEVICE())
    {
        freqBandOptions.push(
            {
                name: "freqBand24",
                displayName: "2.4 GHz"
            }
        );
    }

    return(freqBandOptions);
}

/*
 *  ======== getSub1GHzFrequencies ========
 *  Gets the array of sub-1 GHz frequencies supported by the board/devices
 *
 *  @returns Array - an array containing one or more dictionaries with the
 *                   following keys: displayName, name
 */
function getSub1GHzFrequencies()
{
    let freqSub1Options;

    if(Common.IS_433MHZ_DEVICE())
    {
        freqSub1Options = [
            {
                name: "freq433",
                displayName: "433 MHz"
            }
        ];
    }
    else
    {
        freqSub1Options = [
            {
                name: "freq863",
                displayName: "863 MHz"
            },
            {
                name: "freq915",
                displayName: "915 MHz"
            }
        ];
    }

    return(freqSub1Options);
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
    const phyList = propPhySettings.concat(ieeePhySettings);
    return(_.map(phyList, (phy) => phy.phyDropDownOption));
}

/*
 *  ======== getDisabledPhyTypeOptions ========
 *  Generates a list of options that should be disabled in phy type drop-down
 *      * 250kbps disabled for Sub-1 GHz
 *      * non-250kbps disabled for 2.4GHz
 *
 * @returns Array - array of phy options that should be disabled
 */
function getDisabledPhyTypeOptions()
{
    return(inst) =>
    {
        const disabledOptions = [];

        // Disable IEEE phy type if Sub-1 GHz selected (if 2.4 GHz supported)
        if(inst.freqBand === "freqBandSub1" && Common.IS_24GHZ_DEVICE())
        {
            disabledOptions.push({
                name: "phyIEEE",
                reason: "Only available on 2.4 GHz projects"
            });
        }

        // Disable non-IEEE phy types if 2.4 GHz selected (if Sub-1 supported)
        if(inst.freqBand === "freqBand24" && Common.IS_SUB1GHZ_DEVICE())
        {
            let phyType = null;
            for(phyType of propPhySettings)
            {
                disabledOptions.push({
                    name: phyType.phyDropDownOption.name,
                    reason: "Only available on Sub-1 GHz projects"
                });
            }
        }
        return(disabledOptions);
    };
}

/*
 *  ======== getPhy154Settings ========
 *  Retrieves array of 15.4 phy ID and channel page settings corresponding
 *  to selected data rate from the <board_name>_rf_defaults.js
 *
 *  @param freqBand - Frequency band (Sub-1 or 2.4 GHz)
 *  @param freqSub1 - Sub-1 GHz frequency (Has no effect for 2.4 GHz)
 *  @param phyType - Name of phy type (phy5kbps, phy50kbps, phy200kbps, phyIEEE)
 *  @returns Array - an array containing dictionary with channel page and phy ID
 */
function getPhy154Settings(freqBand, freqSub1, phyType)
{
    let phy154Setting;
    if(freqBand === "freqBandSub1")
    {
        // Find phy object associated with phy type
        const phyObj = _.find(propPhySettings,
            (settings) => (settings.phyDropDownOption.name === phyType));

        // Get phy ID and channel page of given sub-1 frequency and data rate
        phy154Setting = phyObj.phy154Settings[freqSub1];
    }
    else
    {
        // Only one phy type for 2.4GHz
        phy154Setting = ieeePhySettings[0].phy154Settings.phyIEEE;
    }

    return(phy154Setting);
}

/*
 *  ======== setPhyType ========
 *  Update phy type based on frequency band
 *
 *  @param inst - Instance of this module
 */
function setPhyType(inst)
{
    const isSubGSelected = (inst.freqBand === "freqBandSub1");
    inst.phyType = getDefaultPhyType(isSubGSelected);
}

/*
 *  ======== setPhyIDChannelPage ========
 *  Update phy ID and channel page based on phy type and frequency band
 *
 *  @param inst - Instance of this module
 */
function setPhyIDChannelPage(inst)
{
    const newSettings = getPhy154Settings(inst.freqBand, inst.freqSub1,
        inst.phyType);
    inst.phyID = newSettings.ID;
    inst.channelPage = newSettings.channelPage;
}

/*
 *  ======== getDefaultFreqBand ========
 *  Retrieves the default frequency band:
 *      * 2.4 GHz for 26X2 boards
 *      * Sub-1 GHz for others
 *
 *  @param getSubGDefault - Boolean, True selects Sub-1 GHz default value
 *  @returns - name of default frequency band
 */
function getDefaultFreqBand(getSubGDefault)
{
    let defaultFreqBand;
    if(getSubGDefault)
    {
        defaultFreqBand = "freqBandSub1";
    }
    else
    {
        defaultFreqBand = "freqBand24";
    }

    return(defaultFreqBand);
}

/*
 *  ======== getDefaultPhyType ========
 *  Retrieves the default phyType
 *      * 50kbps for Sub-1 GHz
 *      * 250kbps for 2.4 Ghz
 *
 *  @param getSubGDefault - Boolean, True selects Sub-1 GHz default value
 *  @returns - name of default phyType (50kbps, 2-GFSK)
 */
function getDefaultPhyType(getSubGDefault)
{
    let defaultPhyType;
    if(getSubGDefault)
    {
        defaultPhyType = "phy50kbps";
    }
    else
    {
        defaultPhyType = "phyIEEE";
    }

    return(defaultPhyType);
}

/*
 *  ======== getDefaultFreqSub1 ========
 *  Retrieves the default sub-1 GHz frequency value
 *
 *  @returns - name of default frequency (433 for P4, 915 otherwise)
 */
function getDefaultFreqSub1()
{
    let defaultFreq;
    if(Common.IS_433MHZ_DEVICE())
    {
        defaultFreq = "freq433";
    }
    else
    {
        defaultFreq = "freq915";
    }

    return(defaultFreq);
}

/*
 *  ======== getDefaultPhy154Settings ========
 *  Gets default channel page and phy ID array
 *
 *  @returns Array - an array with default channel page and phy ID
 */
function getDefaultPhy154Settings()
{
    const defaultFreqBand = getDefaultFreqBand(Common.IS_SUB1GHZ_DEVICE());
    const defaultFreqSub1 = getDefaultFreqSub1();
    const defaultPhyType = getDefaultPhyType(Common.IS_SUB1GHZ_DEVICE());

    return(getPhy154Settings(defaultFreqBand, defaultFreqSub1, defaultPhyType));
}

/*
 * ======== getRFConfigHiddenState ========
 * Get the expected visibility of the selected config
 *
 * @param inst    - module instance containing the config that changed
 * @param cfgName - name of config
 * @returns bool  - true if hidden, false if visible
 */
function getRFConfigHiddenState(inst, cfgName)
{
    let isVisible = true;
    switch(cfgName)
    {
        case "freqSub1":
        {
            isVisible = (inst.freqBand === "freqBandSub1");
            break;
        }
        case "freqBand":
        case "phyType":
        case "phyID":
        case "channelPage":
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
 * ======== setRFConfigHiddenState ========
 * Sets the visibility of the selected config
 *
 * @param inst    - module instance containing the config that changed
 * @param ui      - user interface object
 * @param cfgName - name of config
 */
function setRFConfigHiddenState(inst, ui, cfgName)
{
    // Set visibility of config
    ui[cfgName].hidden = getRFConfigHiddenState(inst, cfgName);
    if(ui[cfgName].hidden)
    {
        // get a list of all nested and unnested configs
        const configToReset = Common.findConfig(config.config, cfgName);
        // restore the default value for the hidden parameter.
        Common.restoreDefaultValue(inst, configToReset, cfgName);
    }
}

/*
 * ======== setFreqBandReadOnlyState ========
 * Sets the read only status of freqBand config. If config is read only, a
 * read only reason is displayed
 *
 * @param ui       - user interface object
 * @param readOnly - true if freqBand config must be set to read only
 */
function setFreqBandReadOnlyState(ui, readOnly)
{
    // Set read only state of config
    ui.freqBand.readOnly = (readOnly) ? Docs.freqBand.readOnly : false;
}

/*
 * ======== validate ========
 * Validate this inst's configuration
 *
 * @param inst       - RF Settings instance to be validated
 * @param validation - object to hold detected validation issues
 */
function validate(inst, validation)
{
    // Verify that phy type is supported by frequency band. Should never happen
    // because phy type is reset to default value when switching frequency band
    if(((inst.freqBand === "freqBandSub1") && (inst.phyType === "phyIEEE"))
        || ((inst.freqBand === "freqBand24") && (inst.phyType !== "phyIEEE")))
    {
        validation.logError("PHY type not supported by frequency band",
            inst, "phyType");
    }
}

/*
 *******************************************************************************
 Module Dependencies and Exports
 *******************************************************************************
 */

/*
 *  ======== addRFSettingDependency ========
 *  Creates an RF setting dependency module
 *
 * @param inst  - Module instance containing the config that changed
 * @returns dictionary - containing a single RF setting dependency module
 */
function addRFSettingDependency(inst)
{
    // Find PHY object
    const phyList = propPhySettings.concat(ieeePhySettings);
    const selectedPhy = _.find(phyList,
        (setting) => (setting.phyDropDownOption.name === inst.phyType));

    const radioConfigArgs = _.cloneDeep(selectedPhy.args);

    // Only generate either default PA or high PA table as required
    radioConfigArgs.codeExportConfig.paExport = "active";

    // Get params to map current 15.4 tx power config to radio config tx
    // power config
    const txPower154Obj = powerScript.mapCurrTxPowerToRFConfig(inst);
    // Set radio config tx power to 15.4 tx power
    radioConfigArgs[txPower154Obj.cfgName] = txPower154Obj.txPower;

    // Set high PA if supported by board
    if(Common.IS_HIGHPA_DEVICE())
    {
        radioConfigArgs.highPA = txPower154Obj.highPA;
    }

    // Add 15.4 specific overrides for sub-G projects
    if(inst.freqBand === "freqBandSub1")
    {
        const overridesMacro = "TI_154_STACK_OVERRIDES";
        radioConfigArgs.codeExportConfig.stackOverride = "ti/ti154stack/"
            + "common/boards/ti_154stack_overrides.h";
        radioConfigArgs.codeExportConfig.stackOverrideMacro = overridesMacro;
    }

    return({
        name: "radioConfig",
        displayName: selectedPhy.phyDropDownOption.displayName,
        moduleName: selectedPhy.moduleName,
        description: "Radio configuration",
        readOnly: true,
        hidden: true,
        collapsed: true,
        group: "radioSettings",
        args: radioConfigArgs
    });
}

/*
 *  ======== moduleInstances ========
 *  Determines what modules are added as non-static sub-modules
 *
 *  @param inst  - Module instance containing the config that changed
 *  @returns     - Dependency module of selected phyType
 */
function moduleInstances(inst)
{
    // Add radio config module associated with phy selected
    return(addRFSettingDependency(inst));
}

// Exports to the top level 15.4 module
exports = {
    config: config,
    validate: validate,
    moduleInstances: moduleInstances,
    getPhy154Settings: getPhy154Settings,
    setFreqBandReadOnlyState: setFreqBandReadOnlyState,
    setRFConfigHiddenState: setRFConfigHiddenState,
    getRFConfigHiddenState: getRFConfigHiddenState
};
