/**
 * @file dlogg-current-data.c
 * @brief Implements the d-logg protocol
 * @details D-logg devices are capable of reading and logging several control
 * values of controls manufactured by Technische Alternative (www.ta.co.at) An
 * up-to-date protocol specification may be obtained by contacting the
 * Technische Alternative support team.
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

#include "dlogg-current-data.h"
#include "dlogg-mac.h"
#include <fieldbus-mac.h>
#include <logging-adapter.h>

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

/** @brief The maximum number of data samples per active-data message */
#define DLOGG_CD_MAX_SAMPLES_PER_MSG (2)

/**
 * @brief Encapsulates the data fetched from one data line.
 * @details It is planned to support multiple logging lines each communicating
 * with different control equipment. The type will be provided for future
 * compatibility only.
 */
typedef struct {
	/** @brief The line's identifier */
	uint8_t lineID;
	/** @brief The line's meta-data */
	dlogg_cd_metadata_t metaData;
	/** @brief The device's data */
	dlogg_cd_sample_t samples[DLOGG_CD_MAX_SAMPLES_PER_MSG];
} dlogg_cd_lineData_t;

/** @brief The currently buffered data */
dlogg_cd_lineData_t dlogg_cd_data;

/* Function Prototypes */
static void dlogg_cd_debug_buffer(const char* name, uint8_t* buffer,
		size_t length);
static dlogg_cd_lineData_t * dlogg_cd_getLineData(uint8_t lineID);
static inline common_type_error_t dlogg_cd_fetchMetaData(uint8_t activeLine);
static inline common_type_error_t dlogg_cd_fetchModuleType(
		dlogg_cd_moduleType_t * moduleType);
static inline common_type_error_t dlogg_cd_fetchModuleMode(uint8_t * mode);
static inline common_type_error_t dlogg_cd_fetchOperationMode(uint8_t * mode);
static inline void dlogg_cd_coffeeBreak(void);
static inline common_type_error_t dlogg_cd_fetchCurrentData(uint8_t activeLine);
static inline common_type_error_t dlogg_cd_checkDLMode(
		dlogg_cd_metadata_t * metadata);
static inline int dlogg_cd_getSampleCount(dlogg_cd_metadata_t * metadata);
static inline int dlogg_cd_getSampleType(uint8_t deviceID,
		dlogg_cd_metadata_t * metaData);
static size_t dlogg_cd_getSampleSize(uint8_t sampleID);

/**
 * @brief Fetches the meta-data and all available active-data samples
 * @return The status of the operation
 */
common_type_error_t fieldbus_mac_sync() {
	common_type_error_t err;

	err = dlogg_cd_fetchMetaData(0);
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	err = dlogg_cd_fetchCurrentData(0);
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	return err;
}

/**
 * @brief Fetches active data values and stores them into the global buffer
 * @details The function assumes that the line's meta-data were previously set.
 * The sampleCount field will be updated according to the read data.
 * @param activeLine The line id, currently active
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_cd_fetchCurrentData(uint8_t activeLine) {
	common_type_error_t err;
	uint8_t buffer[DLOGG_CD_MAX_SAMPLES_PER_MSG][sizeof(dlogg_cd_sample_t)];
	int sampleType[DLOGG_CD_MAX_SAMPLES_PER_MSG]; // Decoded internal sample type
	dlogg_mac_chksum_t chksum;
	uint8_t sampleCount, i;
	dlogg_cd_lineData_t * lineData = dlogg_cd_getLineData(activeLine);

	if (lineData == NULL )
		return COMMON_TYPE_ERR_INVALID_ADDRESS;

	err = dlogg_cd_checkDLMode(&lineData->metaData);
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	//issue current data request
	buffer[0][0] = 0xAB;
	err = dlogg_mac_send(buffer[0], sizeof(buffer[0][0]), NULL );
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	sampleCount = dlogg_cd_getSampleCount(&lineData->metaData);
	assert(sampleCount <= DLOGG_CD_MAX_SAMPLES_PER_MSG);

	// clear buffer to ease debugging
	memset(buffer, 0, sizeof(buffer));

	chksum = 0;
	for (i = 0; i < sampleCount; i++) {
		uint8_t deviceID;

		// Read device ID
		err = dlogg_mac_read(&deviceID, sizeof(deviceID), &chksum);
		if (err != COMMON_TYPE_SUCCESS)
			return err;

		logging_adapter_debug("Got device ID 0x%x in sample %u",
				(unsigned) deviceID, (unsigned) i);

		sampleType[i] = dlogg_cd_getSampleType(deviceID, &lineData->metaData);
		if (sampleType[i] < 0)
			return COMMON_TYPE_ERR_INVALID_RESPONSE;

		// Read device data
		err = dlogg_mac_read(buffer[i], dlogg_cd_getSampleSize(sampleType[i]),
				&chksum);
		if (err != COMMON_TYPE_SUCCESS)
			return err;

		dlogg_cd_debug_buffer("raw-sample", buffer[i],
				dlogg_cd_getSampleSize(sampleType[i]));
	}

	err = dlogg_mac_read_chksum(&chksum);
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	// Checks passed, copy data
	for (i = 0; i < sampleCount; i++) {
		memcpy(&lineData->samples[i].data, buffer[i],
				sizeof(lineData->samples[i].data));
		lineData->samples[i].sampleType = sampleType[i];

		logging_adapter_debug("Buffer sample %u with sampleType 0x%x", (unsigned) i,
				(unsigned) sampleType[i]);
	}

	// update sample count
	lineData->metaData.sampleCount = sampleCount;

	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Translates the sample ID to the correct sample size
 * @param sampleType The valid sample type
 * @return The size of the sample data structure in bytes
 */
static size_t dlogg_cd_getSampleSize(uint8_t sampleType) {
	static size_t sampleSize[1] = { sizeof(dlogg_cd_dataUVR61_3_v14_t) };

	assert(sampleType < sizeof(sampleSize) / sizeof(sampleSize[0]));

	// check structures
	assert(sampleSize[0] == 53); // UVR 61-3, v1.4

	return sampleSize[sampleType];
}

/**
 * @brief Returns the sample type
 * @details If the device ID is not supported of if no data is available -1
 * will be returned
 * @param deviceID The device ID specified by TA
 * @param metaData The up-to-date meta-data structure
 * @return The internal sampleType
 */
static inline int dlogg_cd_getSampleType(uint8_t deviceID,
		dlogg_cd_metadata_t * metaData) {
	int ret = -1;

	assert(metaData != NULL);

	switch (deviceID) {
	case DLOGG_CD_DEVICE_UVR61_3:
		if (metaData->moduleType.firmware >= 29) {
			ret = DLOGG_CD_SAMPLE_UVR_61_3_V14;
		} else {
			ret = -1;
		}
		break;
	case DLOGG_CD_DEVICE_NO:
		logging_adapter_info("No device data available.");
		ret = -1;
		break;
	default:
		logging_adapter_info("Device Type 0x%x is not supported",
				(unsigned) deviceID);
		ret = -1;
	}

	return ret;
}
/**
 * @brief Returns the number of expected samples
 * @details The returned value is based on the current operational mode. It is
 * expected that the meta-data section passed is valid but the smapleCount field
 * won't be evaluated.
 * @param metadata A valid meta-data reference
 * @return The number of samples expected
 */
static inline int dlogg_cd_getSampleCount(dlogg_cd_metadata_t * metadata) {
	assert(metadata != NULL);
	if (metadata->mode == DLOGG_CD_MODE_1DL) {
		return 1;
	} else if (metadata->mode == DLOGG_CD_MODE_2DL) {
		return 2;
	} else {
		assert(0);
		return -1;
	}
}

/**
 * @brief Checks the current Mode and returns whether it is supported
 * @param metadata The meta data section used to determine the mode
 * @return COMMON_TYPE_SUCCESS iff the mode is supported
 */
static inline common_type_error_t dlogg_cd_checkDLMode(
		dlogg_cd_metadata_t * metadata) {

	assert(metadata != NULL);

	if (metadata->mode != DLOGG_CD_MODE_1DL
			&& metadata->mode != DLOGG_CD_MODE_2DL) {
		logging_adapter_info("The device's operational mode 0x%x is not supported.",
				(unsigned) metadata->mode);
		return COMMON_TYPE_ERR_INVALID_RESPONSE;
	}

	if (metadata->moduleType.type != DLOGG_CD_MOD_TYPE_BLNET
			&& metadata->moduleType.type != DLOGG_CD_MOD_TYPE_DLOGG_1D
			&& metadata->moduleType.type != DLOGG_CD_MOD_TYPE_DLOGG_2D) {
		logging_adapter_info("The device's type 0x%x is not supported.",
				(unsigned) metadata->moduleType.type);
		return COMMON_TYPE_ERR_INVALID_RESPONSE;
	}

	if (metadata->moduleType.type == DLOGG_CD_MOD_TYPE_DLOGG_1D
			&& metadata->moduleType.type == DLOGG_CD_MOD_TYPE_DLOGG_2D
			&& metadata->moduleType.firmware < 29) {
		logging_adapter_info("The device's firmware version %ue-1 isn't supported.",
				(unsigned) metadata->moduleType.firmware);
		return COMMON_TYPE_ERR_INVALID_RESPONSE;
	}

	if (metadata->moduleType.type == DLOGG_CD_MOD_TYPE_DLOGG_1D
			&& metadata->mode != DLOGG_CD_MODE_1DL) {
		logging_adapter_info("Module type DLOGG 1DL doesn't use 1DL mode.");
		return COMMON_TYPE_ERR_INVALID_RESPONSE;
	}

	if (metadata->moduleType.type == DLOGG_CD_MOD_TYPE_DLOGG_2D
			&& metadata->mode != DLOGG_CD_MODE_2DL) {
		logging_adapter_info("Module type DLOGG 2DL doesn't use 2DL mode.");
		return COMMON_TYPE_ERR_INVALID_RESPONSE;
	}

	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Fetches the meta-data from the currently active logger
 * @details The data will be stored in the appropriate structure. If the
 * function fails the content of the data structure may be undefined
 * @param activeLine The currently active line identifier
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_cd_fetchMetaData(uint8_t activeLine) {
	common_type_error_t err;
	dlogg_cd_lineData_t * lineData = dlogg_cd_getLineData(activeLine);
	uint8_t buffer;

	if (lineData == NULL )
		return COMMON_TYPE_ERR_INVALID_ADDRESS;

	err = dlogg_cd_fetchModuleType(&lineData->metaData.moduleType);
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	err = dlogg_cd_fetchOperationMode(&buffer);
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	err = dlogg_cd_fetchModuleMode(&lineData->metaData.mode);
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	logging_adapter_debug("Metadata fetched: Operation type=0x%x, Mod. type=0x%x,"
			" Firmware=0x%x, mode=0x%x", (unsigned) buffer,
			(unsigned) lineData->metaData.moduleType.type,
			(unsigned) lineData->metaData.moduleType.firmware,
			(unsigned) lineData->metaData.mode);

	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Fetches the current operation mode and stores it in the given mode
 * variable
 * @param mode The reference to the mode destination
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_cd_fetchOperationMode(uint8_t * mode) {
	common_type_error_t err;
	// request id, second byte from winsol communication
	uint8_t buffer[2] = { 0x21, 0x43 };

	assert(mode != NULL);

	dlogg_cd_coffeeBreak(); // Won't produce any output otherwise

	err = dlogg_mac_send(buffer, sizeof(buffer), NULL );
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	err = dlogg_mac_read(mode, sizeof(*mode), NULL );
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	logging_adapter_debug("Module mode successfully fetched");
	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Fetches the current module mode and stores it in the given mode
 * variable
 * @param mode The reference to the mode destination
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_cd_fetchModuleMode(uint8_t * mode) {
	common_type_error_t err;
	uint8_t buffer = 0x81; // request id

	assert(mode != NULL);

	dlogg_cd_coffeeBreak(); // Won't produce any output otherwise

	err = dlogg_mac_send(&buffer, sizeof(buffer), NULL );
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	err = dlogg_mac_read(mode, sizeof(*mode), NULL );
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	logging_adapter_debug("Module mode successfully fetched");
	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Tries to fetch the currently active module type
 * @param moduleType The destination to write the type
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_cd_fetchModuleType(
		dlogg_cd_moduleType_t * moduleType) {
	common_type_error_t err;
	dlogg_mac_chksum_t chksum;
	uint8_t buffer[7] = { 0x20, 0x10, 0x18, 0, 0, 0, 0 }; // request data

	assert(moduleType != NULL);
	assert(sizeof(*moduleType) == 2);
	assert(sizeof(*moduleType) <= sizeof(buffer));

// Issue request
	chksum = 0;
	err = dlogg_mac_send(buffer, sizeof(buffer), &chksum);
	if (err != COMMON_TYPE_SUCCESS)
		return err;
	err = dlogg_mac_send_chksum(&chksum);
	if (err != COMMON_TYPE_SUCCESS)
		return err;

// Fetch Acknowledge
	err = dlogg_mac_read(buffer, 2, NULL );
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	dlogg_cd_debug_buffer("ModuleType Ack", buffer, 2);
	if (buffer[0] == 0xFF && buffer[1] == 0x00) {
		logging_adapter_info("Logger complained about invalid data");
		return COMMON_TYPE_ERR_IO;
	} else if (buffer[0] != 0x21 || buffer[1] != 0x43) {
		return COMMON_TYPE_ERR_INVALID_RESPONSE;
	}

// Fetch module type
	chksum = 0;
	err = dlogg_mac_read(buffer, sizeof(*moduleType), &chksum);
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	dlogg_cd_debug_buffer("ModuleType", buffer, sizeof(*moduleType));
	err = dlogg_mac_read_chksum(&chksum);
	if (err != COMMON_TYPE_SUCCESS)
		return err;

	memcpy(moduleType, buffer, sizeof(*moduleType));

	logging_adapter_debug("Module type successfully fetched");

	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Sleeps for a small amount of time
 * @details The function has to be called in order to avoid flooding the
 * data logger. Interrupts will be gracefully ignored.
 */
static inline void dlogg_cd_coffeeBreak(void) {
	struct timespec tv = { 0, 10 * 1000000 }; //seconds, nanoseconds
	(void) nanosleep(&tv, NULL );
}

dlogg_cd_metadata_t * dlogg_cd_getMetadata(uint8_t lineID) {
	dlogg_cd_lineData_t *line = dlogg_cd_getLineData(lineID);
	if (line != NULL ) {
		return &line->metaData;
	} else {
		return NULL ;
	}
}

dlogg_cd_sample_t * dlogg_cd_getCurrentData(uint8_t device, uint8_t lineID) {
	dlogg_cd_lineData_t *line = dlogg_cd_getLineData(lineID);
	if (line != NULL && device < line->metaData.sampleCount) {
		return &line->samples[device];
	} else {
		return NULL;
	}
}

/**
 * @brief Function used to fetch a line's data.
 * @details currently only lineID == 0 is supported
 * @param lineID The communication line's unique identifier
 * @return The line's data or NULL it the line wasn't registered before
 */
static dlogg_cd_lineData_t * dlogg_cd_getLineData(uint8_t lineID) {
	assert(lineID == 0);
	return &dlogg_cd_data;
}

/**
 * @brief Writes the buffer's content nicely formatted to the debug logger
 * @param name The buffer's name
 * @param buffer A valid buffer reference holding at least length bytes
 * @param length The number of bytes to write
 */
static void dlogg_cd_debug_buffer(const char* name, uint8_t* buffer,
		size_t length) {
	int i;
	char* strBuffer = malloc(length * 3 + 1);

	assert(buffer != NULL);
	assert(name != NULL);

	if (strBuffer == NULL ) {
		logging_adapter_debug("malloc failed");
		return;
	}

	for (i = 0; i < length; i++) {
		if (sprintf(&strBuffer[3 * i], "%02x ", (unsigned int) buffer[i]) != 3) {
			logging_adapter_debug("sprintf failed");
			return;
		}
	}

	logging_adapter_debug("Buffer %s (length: %u): | %s|", name,
			(unsigned int) length, strBuffer);

	free(strBuffer);
}
