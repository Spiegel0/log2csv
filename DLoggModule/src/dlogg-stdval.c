/**
 * @file dlogg-stdval.c
 * @brief Module fetching a previously buffered value.
 * @details The value has to be represented by the TA-standard encoding stated
 * in the controller's manual. On fetching the value, first the user input is
 * parsed into an address structure. Secondly the address structure is validated
 * against a sample-type dependent profile and the addressed value is extracted.
 * For each type of channel a separate function exists encapsulating different
 * access functionality. (This is why there are so many functions ;-) )
 *
 * @author Michael Spiegel, michael.h.spiegel@gmail.com
 *
 * Copyright (C) 2014 Michael Spiegel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <fieldbus-application.h>
#include <logging-adapter.h>
#include "dlogg-current-data.h"

#include <assert.h>
#include <string.h>

/* The configuration directive names used */
#define DLOGG_STDVAL_CONFIG_CONTROLLER "controller"
#define DLOGG_STDVAL_CONFIG_LINE "line_id"
#define DLOGG_STDVAL_CONFIG_CHN_PREFIX "channel_prefix"
#define DLOGG_STDVAL_CONFIG_CHN_NR "channel_number"

/* The prefix configuration keys */
/** @brief Internal sensor input channel prefix */
#define DLOGG_STDVAL_CONFIG_PRE_S "S"
/** @brief External sensor input chanel prefix */
#define DLOGG_STDVAL_CONFIG_PRE_E "E"
/** @brief Digital output channel prefix */
#define DLOGG_STDVAL_CONFIG_PRE_A "A"
/** @brief Drive output channel prefix */
#define DLOGG_STDVAL_CONFIG_PRE_AD "A.D"
/** @brief Analog output channel prefix */
#define DLOGG_STDVAL_CONFIG_PRE_AA "A.A"
/** @brief Heat meter power channel prefix */
#define DLOGG_STDVAL_CONFIG_PRE_WMZP "WMZ.P"
/** @brief Heat meter energy channel prefix */
#define DLOGG_STDVAL_CONFIG_PRE_WMZE "WMZ.E"

/** @brief Defines possible prefix values */
typedef enum {
	DLOGG_STDVAL_PRE_S = 0,
	DLOGG_STDVAL_PRE_E,
	DLOGG_STDVAL_PRE_A,
	DLOGG_STDVAL_PRE_AD,
	DLOGG_STDVAL_PRE_AA,
	DLOGG_STDVAL_PRE_WMZP,
	DLOGG_STDVAL_PRE_WMZE
} dlogg_stdval_prefix_t;

/** @brief Structure encapsulating the user's request */
typedef struct {
	/** @brief An internal prefix ID */
	dlogg_stdval_prefix_t prefixID;
	/** @brief The current line multiplexing channel */
	uint8_t lineID;
	/** @brief The channel number starting at zero */
	uint8_t channelID;
	/** @brief The d-logg's input channel starting at zero */
	unsigned controllerID :1;
} dlogg_stdval_addr_t;

/**
 * @brief Array containing the maximum number of available input channels per
 * sampleType and channel prefix.
 * @details The first index points to the sampleType value and the second index
 * corresponds to the prefix value.
 */
const int dlogg_stdval_capabilities[1][7] = {
//    S, E, A,A.D,A.A,WMZ.P,WMZ.E
		{ 6, 9, 3, 1, 2, 3, 3 } //UVR 61-3 v1.4
};

/* Function Prototypes */
static inline common_type_error_t dlogg_stdval_parseAddress(
		dlogg_stdval_addr_t* addr, config_setting_t *addressConfig);
static inline common_type_error_t dlogg_stdval_getPrefixID(
		dlogg_stdval_prefix_t* prefix, const char* confVal);
static inline common_type_error_t dlogg_stdval_checkAddress(
		dlogg_stdval_addr_t * addr);
static inline common_type_t dlogg_stdval_fetchValue(dlogg_stdval_addr_t * addr);
static inline common_type_t dlogg_stdval_fetchSChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID);
static inline common_type_t dlogg_stdval_fetchEChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID);
static inline common_type_t dlogg_stdval_fetchAChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID);
static inline common_type_t dlogg_stdval_fetchADChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID);
static inline common_type_t dlogg_stdval_fetchAAChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID);
static inline common_type_t dlogg_stdval_fetchWMZEChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID);
static inline common_type_t dlogg_stdval_fetchWMZPChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID);
static common_type_t dlogg_stdval_input2common(dlogg_cd_input_t input);
static common_type_t dlogg_stdval_outputDrive2common(
		dlogg_cd_outputDrive_t outputDrive);
static common_type_t dlogg_stdval_analogOutput2common(
		dlogg_cd_analogOutput_t analogOutput);
static common_type_t dlogg_stdval_heatMeterSmall2commonEnergy(
		dlogg_cd_heatMeterSmall_t * heatMeter);
static common_type_t dlogg_stdval_heatMeterSmall2commonPower(
		dlogg_cd_heatMeterSmall_t * heatMeter);

common_type_error_t fieldbus_application_init(void) {
	// Nothing to be done
	return COMMON_TYPE_SUCCESS;
}

common_type_error_t fieldbus_application_sync(void) {
	// Nothing to be done
	return COMMON_TYPE_SUCCESS;
}

common_type_t fieldbus_application_fetchValue(config_setting_t *address) {
	common_type_t ret;
	dlogg_stdval_addr_t addr;

	assert(address != NULL);

	ret.type = COMMON_TYPE_ERR;
	ret.data.errVal = COMMON_TYPE_ERR_CONFIG;

	ret.data.errVal = dlogg_stdval_parseAddress(&addr, address);
	if (ret.data.errVal != COMMON_TYPE_SUCCESS)
		return ret;

	ret.data.errVal = dlogg_stdval_checkAddress(&addr);
	if (ret.data.errVal != COMMON_TYPE_SUCCESS)
		return ret;

	return dlogg_stdval_fetchValue(&addr);
}

/**
 * @brief Fetches the value specified by the given address and returns it.
 * @details it assumes that the given address is valid and previously checked.
 * @param addr A valid reference to an address structure
 * @return The fetched result or an appropriate error code.
 */
static inline common_type_t dlogg_stdval_fetchValue(dlogg_stdval_addr_t * addr) {
	common_type_t ret;
	dlogg_cd_sample_t * sample;

	assert(addr != NULL);

	ret.type = COMMON_TYPE_ERROR;
	ret.data.errVal = COMMON_TYPE_ERR;
	sample = dlogg_cd_getCurrentData(addr->controllerID, addr->lineID);
	assert(sample!=NULL);

	switch (addr->prefixID) {
	case DLOGG_STDVAL_PRE_S:
		ret = dlogg_stdval_fetchSChannel(sample, addr->channelID);
		break;
	case DLOGG_STDVAL_PRE_E:
		ret = dlogg_stdval_fetchEChannel(sample, addr->channelID);
		break;
	case DLOGG_STDVAL_PRE_A:
		ret = dlogg_stdval_fetchAChannel(sample, addr->channelID);
		break;
	case DLOGG_STDVAL_PRE_AD:
		ret = dlogg_stdval_fetchADChannel(sample, addr->channelID);
		break;
	case DLOGG_STDVAL_PRE_AA:
		ret = dlogg_stdval_fetchAAChannel(sample, addr->channelID);
		break;
	case DLOGG_STDVAL_PRE_WMZE:
		ret = dlogg_stdval_fetchWMZEChannel(sample, addr->channelID);
		break;
	case DLOGG_STDVAL_PRE_WMZP:
		ret = dlogg_stdval_fetchWMZPChannel(sample, addr->channelID);
		break;
	default:
		assert(0);
	}

	return ret;
}

/**
 * @brief Fetches the given input value from the given sample
 * @details It assumes that all values are valid and that the ranges are checked
 * @param sample The reference to the addressed sample
 * @param channelID The valid channel identifier
 * @return The result of the operation
 */
static inline common_type_t dlogg_stdval_fetchSChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID) {
	common_type_t ret;

	assert(sample != NULL);

	ret.type = COMMON_TYPE_ERROR;
	ret.data.errVal = COMMON_TYPE_ERR;

	switch (sample->sampleType) {
	case DLOGG_CD_SAMPLE_UVR_61_3_V14:
		assert(channelID < 6);
		ret = dlogg_stdval_input2common(sample->data.uvr61_3_v14.inputs[channelID]);
		break;
	default:
		assert(0);
	}
	return ret;
}

/**
 * @brief Fetches the given external input value from the given sample
 * @details It assumes that all values are valid and that the ranges are checked
 * @param sample The reference to the addressed sample
 * @param channelID The valid channel identifier
 * @return The result of the operation
 */
static inline common_type_t dlogg_stdval_fetchEChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID) {
	common_type_t ret;

	assert(sample != NULL);

	ret.type = COMMON_TYPE_ERROR;
	ret.data.errVal = COMMON_TYPE_ERR;

	switch (sample->sampleType) {
	case DLOGG_CD_SAMPLE_UVR_61_3_V14:
		assert(channelID < 9);
		ret = dlogg_stdval_input2common(
				sample->data.uvr61_3_v14.inputs[channelID + 6]);
		break;
	default:
		assert(0);
	}
	return ret;
}

/**
 * @brief Fetches the given output value from the given sample
 * @details It assumes that all values are valid and that the ranges are checked
 * @param sample The reference to the addressed sample
 * @param channelID The valid channel identifier
 * @return The result of the operation
 */
static inline common_type_t dlogg_stdval_fetchAChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID) {
	common_type_t ret;

	assert(sample != NULL);

	ret.type = COMMON_TYPE_ERROR;
	ret.data.errVal = COMMON_TYPE_ERR;

	switch (sample->sampleType) {
	case DLOGG_CD_SAMPLE_UVR_61_3_V14:
		assert(channelID < 3);
		ret.type = COMMON_TYPE_LONG;
		ret.data.longVal =
				sample->data.uvr61_3_v14.output & (1 << channelID) ? 1 : 0;
		break;
	default:
		assert(0);
	}
	return ret;
}

/**
 * @brief Fetches the output drive control value from the given sample
 * @details It assumes that all values are valid and that the ranges are checked
 * @param sample The reference to the addressed sample
 * @param channelID The valid channel identifier
 * @return The result of the operation
 */
static inline common_type_t dlogg_stdval_fetchADChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID) {
	common_type_t ret;

	assert(sample != NULL);

	ret.type = COMMON_TYPE_ERROR;
	ret.data.errVal = COMMON_TYPE_ERR;

	switch (sample->sampleType) {
	case DLOGG_CD_SAMPLE_UVR_61_3_V14:
		assert(channelID < 1);
		ret = dlogg_stdval_outputDrive2common(sample->data.uvr61_3_v14.outputDrive);
		break;
	default:
		assert(0);
	}
	return ret;
}

/**
 * @brief Fetches the analog output value from the given sample
 * @details It assumes that all values are valid and that the ranges are checked
 * @param sample The reference to the addressed sample
 * @param channelID The valid channel identifier
 * @return The result of the operation
 */
static inline common_type_t dlogg_stdval_fetchAAChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID) {
	common_type_t ret;

	assert(sample != NULL);

	ret.type = COMMON_TYPE_ERROR;
	ret.data.errVal = COMMON_TYPE_ERR;

	switch (sample->sampleType) {
	case DLOGG_CD_SAMPLE_UVR_61_3_V14:
		assert(channelID < 2);
		ret = dlogg_stdval_analogOutput2common(
				sample->data.uvr61_3_v14.analogOutput[channelID]);
		break;
	default:
		assert(0);
	}
	return ret;
}

/**
 * @brief Fetches the heat meter energy value from the given sample
 * @details It assumes that all values are valid and that the ranges are checked
 * @param sample The reference to the addressed sample
 * @param channelID The valid channel identifier
 * @return The result of the operation
 */
static inline common_type_t dlogg_stdval_fetchWMZEChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID) {
	common_type_t ret;

	assert(sample != NULL);

	ret.type = COMMON_TYPE_ERROR;
	ret.data.errVal = COMMON_TYPE_ERR;

	switch (sample->sampleType) {
	case DLOGG_CD_SAMPLE_UVR_61_3_V14:
		assert(channelID < 3);

		if (sample->data.uvr61_3_v14.heatMeterRegister & (1 << channelID)) {
			ret = dlogg_stdval_heatMeterSmall2commonEnergy(
					&sample->data.uvr61_3_v14.heatMeter[channelID]);
		} else {
			logging_adapter_info("The heat meter %u is not active",
					(unsigned) channelID + 1);
			ret.type = COMMON_TYPE_ERROR;
			ret.data.errVal = COMMON_TYPE_ERR_INVALID_ADDRESS;
		}

		break;
	default:
		assert(0);
	}
	return ret;
}

/**
 * @brief Fetches the heat meter power value from the given sample
 * @details It assumes that all values are valid and that the ranges are checked
 * @param sample The reference to the addressed sample
 * @param channelID The valid channel identifier
 * @return The result of the operation
 */
static inline common_type_t dlogg_stdval_fetchWMZPChannel(
		dlogg_cd_sample_t* sample, uint8_t channelID) {
	common_type_t ret;

	assert(sample != NULL);

	ret.type = COMMON_TYPE_ERROR;
	ret.data.errVal = COMMON_TYPE_ERR;

	switch (sample->sampleType) {
	case DLOGG_CD_SAMPLE_UVR_61_3_V14:
		assert(channelID < 3);

		if (sample->data.uvr61_3_v14.heatMeterRegister & (1 << channelID)) {
			ret = dlogg_stdval_heatMeterSmall2commonPower(
					&sample->data.uvr61_3_v14.heatMeter[channelID]);
		} else {
			logging_adapter_info("The heat meter %u is not active",
					(unsigned) channelID + 1);
			ret.type = COMMON_TYPE_ERROR;
			ret.data.errVal = COMMON_TYPE_ERR_INVALID_ADDRESS;
		}

		break;
	default:
		assert(0);
	}
	return ret;
}

/**
 * @brief Converts the heat meter energy to a common type
 * @details It is assumed that the given reference is valid. The common type
 * will be scaled to kWh
 * @param heatMeter The heat meter structure
 * @return The conversion result
 */
static common_type_t dlogg_stdval_heatMeterSmall2commonEnergy(
		dlogg_cd_heatMeterSmall_t * heatMeter) {
	common_type_t ret;

	assert(heatMeter != NULL);

	ret.type = COMMON_TYPE_DOUBLE;
	ret.data.doubleVal = (((uint16_t) heatMeter->val.kwh[0])
			+ (((uint16_t) heatMeter->val.kwh[1]) << 8)) * 0.1;
	ret.data.doubleVal += (((uint16_t) heatMeter->val.mwh[0])
			+ (((uint16_t) heatMeter->val.mwh[1]) << 8)) * 1000.0;

	return ret;
}

/**
 * @brief Converts the heat meter power to a common type
 * @details It is assumed that the given reference is valid. The common type
 * will be scaled to kW
 * @param heatMeter The heat meter structure
 * @return The conversion result
 */
static common_type_t dlogg_stdval_heatMeterSmall2commonPower(
		dlogg_cd_heatMeterSmall_t * heatMeter) {
	common_type_t ret;

	assert(heatMeter != NULL);

	ret.type = COMMON_TYPE_DOUBLE;
	ret.data.doubleVal = (((uint16_t) heatMeter->val.cur[0])
			+ (((uint16_t) heatMeter->val.cur[1]) << 8)) * 0.1;

	return ret;
}

/**
 * @brief Converts the analog output value to an appropriate common type value
 * @details If the output is not set an error will be returned. The output value
 * will be scaled to 1V.
 * @param analogOutput The source value
 * @return The converted target value
 */
static common_type_t dlogg_stdval_analogOutput2common(
		dlogg_cd_analogOutput_t analogOutput) {
	common_type_t ret;

	if (!analogOutput.val.activeN && analogOutput.val.voltage <= 100) {
		ret.type = COMMON_TYPE_DOUBLE;
		ret.data.doubleVal = analogOutput.val.voltage * 0.1;
	} else {
		logging_adapter_info("An analog output requested isn't set by the "
				"controller");
		ret.type = COMMON_TYPE_ERROR;
		ret.data.errVal = COMMON_TYPE_ERR_INVALID_ADDRESS;
	}

	return ret;
}
/**
 * @brief Converts the drive output value to a value between [0,1]
 * @details If the value is not set an error will be returned
 * @param outputDrive The drive value
 * @return The common type value calculated
 */
static common_type_t dlogg_stdval_outputDrive2common(
		dlogg_cd_outputDrive_t outputDrive) {
	common_type_t ret;

	if (!outputDrive.activeN && outputDrive.speed <= 30) {
		ret.type = COMMON_TYPE_DOUBLE;
		ret.data.doubleVal = ((double) outputDrive.speed) / 30.0;
	} else {
		logging_adapter_info("A drive controlled output requested isn't set by "
				"the controller");
		ret.type = COMMON_TYPE_ERROR;
		ret.data.errVal = COMMON_TYPE_ERR_INVALID_ADDRESS;
	}

	return ret;
}

/**
 * @brief Translates the input type into a properly scaled common type
 * @details Temperatures will be scaled in degree Celsius, volume flow to l/h,
 * radiation to W/m^2 and boolean values to [0,1]. If the input is not set the
 * function will return an error.
 * @param input The input to translate
 * @return The proper common type
 */
static common_type_t dlogg_stdval_input2common(dlogg_cd_input_t input) {
	common_type_t ret;

	logging_adapter_debug("Got input value: type=%u, high=0x%02x, low=0x%02x, "
			"sign=%u", (unsigned) input.val.type, (unsigned) input.val.highValue,
			(unsigned) input.val.lowValue, (unsigned) input.val.sign);

	switch (input.val.type) {
	case 0: // unused
		logging_adapter_info("An input value requested is currently unused");
		ret.type = COMMON_TYPE_ERROR;
		ret.data.errVal = COMMON_TYPE_ERR_INVALID_ADDRESS;
		break;
	case 1: // digital input
		ret.type = COMMON_TYPE_LONG;
		ret.data.longVal = input.val.sign;
		break;
	case 2: // temperature
		ret.type = COMMON_TYPE_DOUBLE;
		ret.data.doubleVal = (((uint16_t) input.val.lowValue)
				+ (((uint16_t) input.val.highValue) << 8)) * 0.1;
		ret.data.doubleVal *= input.val.sign ? -1.0 : 1.0;
		break;
	case 3: // volume flow
		ret.type = COMMON_TYPE_DOUBLE;
		ret.data.doubleVal = (((uint16_t) input.val.lowValue)
				+ (((uint16_t) input.val.highValue) << 8)) * 4.0;
		ret.data.doubleVal *= input.val.sign ? -1.0 : 1.0;
		break;
	case 6: // solar radiation
		ret.type = COMMON_TYPE_DOUBLE;
		ret.data.doubleVal = (((uint16_t) input.val.lowValue)
				+ (((uint16_t) input.val.highValue) << 8));
		ret.data.doubleVal *= input.val.sign ? -1.0 : 1.0;
		break;
	case 7: // room temperature
		ret.type = COMMON_TYPE_DOUBLE;
		ret.data.doubleVal = (((uint16_t) input.val.lowValue)
				+ (((uint16_t) input.val.highValue & 0x01) << 8)) * 0.1;
		ret.data.doubleVal *= input.val.sign ? -1.0 : 1.0;
		break;
	default:
		logging_adapter_info("Invalid input type identifier read: 0x%20x",
				input.val.type);
		ret.type = COMMON_TYPE_ERROR;
		ret.data.errVal = COMMON_TYPE_ERR_INVALID_RESPONSE;
	}
	return ret;
}
/**
 * @brief Obtains the appropriate met-data structure and checks the range of the
 * internal values
 * @details The sensor number range depends on the internal sampleType
 * describing available data. Each range is present in a lookup-table containing
 * the maximum number of inputs
 * @param addr The address structure to check
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_stdval_checkAddress(
		dlogg_stdval_addr_t * addr) {
	dlogg_cd_metadata_t * metadata = dlogg_cd_getMetadata(addr->lineID);
	dlogg_cd_sample_t * addressedSample;

	assert(addr != NULL);

	if (metadata == NULL ) {
		logging_adapter_info("The line number %u is not known.",
				(unsigned) addr->lineID);
		return COMMON_TYPE_ERR_CONFIG;
	}

	if (addr->controllerID >= metadata->sampleCount) {
		logging_adapter_info("Only %u controller(s) are present at line %u. "
				"Controller %u does not exist.", (unsigned) metadata->sampleCount,
				(unsigned) addr->lineID, ((unsigned) addr->controllerID) + 1);
		return COMMON_TYPE_ERR_CONFIG;
	}

	addressedSample = dlogg_cd_getCurrentData(addr->controllerID, addr->lineID);
	assert(addressedSample != NULL);

	assert(addressedSample->sampleType < sizeof(dlogg_stdval_capabilities) //
	/ sizeof(dlogg_stdval_capabilities[0]));
	assert(addr->prefixID < sizeof(dlogg_stdval_capabilities[0]) //
	/ sizeof(dlogg_stdval_capabilities[0][0]));

	if (addr->channelID
			>= dlogg_stdval_capabilities[addressedSample->sampleType][addr->prefixID]) {
		logging_adapter_info("The controller (sampleType=0x%x) doesn't have a "
				" (prefix=%u) channel nr. %u. The maximum number allowed is %u",
				(unsigned) addressedSample->sampleType, (unsigned) addr->prefixID,
				(unsigned) addr->channelID + 1,
				(unsigned) dlogg_stdval_capabilities[addressedSample->sampleType][addr->prefixID]);
		return COMMON_TYPE_ERR_CONFIG;
	}

	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Parses the given configuration structure
 * @details The parameters set will be stored into the address structure but
 * availability of addresses will remain unchecked.
 * @param addr The address structure to store the results
 * @param addressConfig The configuration to read
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_stdval_parseAddress(
		dlogg_stdval_addr_t * addr, config_setting_t *addressConfig) {
	common_type_error_t err;
	long int lineID = 0, channel, controller = 1;
	const char * prefix;

	assert(addressConfig != NULL);
	assert(addr != NULL);

	if (!config_setting_is_group(addressConfig)) {
		logging_adapter_info("The address setting is not a group directive");
		return COMMON_TYPE_ERR_CONFIG;
	}

// Use default value, if not set
	config_setting_lookup_int(addressConfig, DLOGG_STDVAL_CONFIG_LINE, &lineID);

	if (lineID < 0 || lineID > 255) {
		logging_adapter_info("Value of %s, %i out of range [0,255]",
				DLOGG_STDVAL_CONFIG_LINE, (int) lineID);
		return COMMON_TYPE_ERR_CONFIG;
	}

	if (!config_setting_lookup_int(addressConfig, DLOGG_STDVAL_CONFIG_CHN_NR,
			&channel)) {
		logging_adapter_info("Cant find the \"%s\" int directive within the "
				"address group", DLOGG_STDVAL_CONFIG_CHN_NR);
		return COMMON_TYPE_ERR_CONFIG;
	}

	if (channel < 1 || channel > 256) {
		logging_adapter_info("Value of %s, %i out of range [1,256]",
				DLOGG_STDVAL_CONFIG_CHN_NR, (int) channel);
		return COMMON_TYPE_ERR_CONFIG;
	}

// Use default value, if not set
	config_setting_lookup_int(addressConfig, DLOGG_STDVAL_CONFIG_CONTROLLER,
			&controller);

	if (controller < 1 || controller > 2) {
		logging_adapter_info("Value of %s, %i out of range [1,2]",
				DLOGG_STDVAL_CONFIG_CONTROLLER, (int) controller);
		return COMMON_TYPE_ERR_CONFIG;
	}

	if (!config_setting_lookup_string(addressConfig,
			DLOGG_STDVAL_CONFIG_CHN_PREFIX, &prefix)) {
		logging_adapter_info("Can't find the \"%s\" string directive within the "
				"address group", DLOGG_STDVAL_CONFIG_CHN_PREFIX);
		return COMMON_TYPE_ERR_CONFIG;
	}

	err = dlogg_stdval_getPrefixID(&addr->prefixID, prefix);
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	addr->lineID = lineID;
	addr->channelID = channel - 1;
	addr->controllerID = controller - 1;

//  May be needed for detailed debugging ...
//	logging_adapter_debug("Parsed address: lineID=%u, channelID=%u, "
//			"controllerID=%u, prefix=0x%02x", (unsigned) addr->lineID,
//			(unsigned) addr->channelID, (unsigned) addr->controllerID,
//			(unsigned) addr->prefixID);

	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Parses the given prefix configuration value
 * @details If the prefix isn't known, the prefix variable remains unchanged
 * @param prefix The location to store the prefix
 * @param confVal The configured prefix
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_stdval_getPrefixID(
		dlogg_stdval_prefix_t* prefix, const char* confVal) {

	assert(prefix != NULL);
	assert(confVal != NULL);

	if (strcmp(DLOGG_STDVAL_CONFIG_PRE_A, confVal) == 0) {
		*prefix = DLOGG_STDVAL_PRE_A;
	} else if (strcmp(DLOGG_STDVAL_CONFIG_PRE_AD, confVal) == 0) {
		*prefix = DLOGG_STDVAL_PRE_AD;
	} else if (strcmp(DLOGG_STDVAL_CONFIG_PRE_AA, confVal) == 0) {
		*prefix = DLOGG_STDVAL_PRE_AA;
	} else if (strcmp(DLOGG_STDVAL_CONFIG_PRE_E, confVal) == 0) {
		*prefix = DLOGG_STDVAL_PRE_E;
	} else if (strcmp(DLOGG_STDVAL_CONFIG_PRE_S, confVal) == 0) {
		*prefix = DLOGG_STDVAL_PRE_S;
	} else if (strcmp(DLOGG_STDVAL_CONFIG_PRE_WMZE, confVal) == 0) {
		*prefix = DLOGG_STDVAL_PRE_WMZE;
	} else if (strcmp(DLOGG_STDVAL_CONFIG_PRE_WMZP, confVal) == 0) {
		*prefix = DLOGG_STDVAL_PRE_WMZP;
	} else {
		logging_adapter_info("Unknown Channel prefix %s", confVal);
		return COMMON_TYPE_ERR_CONFIG;
	}

	return COMMON_TYPE_SUCCESS;
}

common_type_error_t fieldbus_application_free(void) {
// Nothing to be done
	return COMMON_TYPE_SUCCESS;
}
