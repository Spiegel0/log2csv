/**
 * @file dlogg-mac-ftdi.c
 * @brief Alternative MAC using libftdi to access the data logger
 * @details <p>The MAC uses the same interfaces as the standard MAC which
 * utilizes the termios interface. The FTDI MAC doesn't implement a different
 * name prefix because it doesn't make much sense using both MAC modules in
 * parallel. The FTDI MAC layer access the first suitable USB device or, if a
 * device number is given, it opens that device.</p>
 * <p>To use the alternative MAC the kernel module ftdi_sio may need to be
 * unloaded and the libraries libftdi1.1 and libusb1.0 need to be available.
 * </p>
 *
 * @author Michael Spiegel, michael.h.spiegel@gmail.com
 *
 * Copyright (C) 2016 Michael Spiegel
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

#include <fieldbus-mac.h>
#include <logging-adapter.h>

#include <assert.h>
#include <ftdi.h>
#include <stdint.h>
#include <time.h>

#include "dlogg-mac.h"
#include "dlogg-mac-common.h"

/** @brief Configuration directive specifying the internal device number */
#define DLOGG_MAC_CONFIG_DEV_NR "device-nr"

/** @brief The d-logg's transmission baud-rate */
#define DLOGG_MAC_BAUDRATE (115200)

/** @brief Number of poll requests while reading data */
#define DLOGG_MAC_RETRY (20)

/** @brief Pointer to the main ftdi library context structure */
static struct ftdi_context * dlogg_mac_ftdi = NULL;

/** @brief structure encapsulating common data */
static struct {
	/** @brief Flag indicating that the USB device was previously opened */
	unsigned devOpened :1;
} dlogg_mac_cData;

/* Function prototypes */
static inline common_type_error_t dlogg_mac_initUART(int ttyID);
static inline common_type_error_t dlogg_mac_openUSBDevice(int ttyID);
static inline common_type_error_t dlogg_mac_setUARTParams(void);

common_type_error_t fieldbus_mac_init(config_setting_t* configuration) {
	common_type_error_t err;
	int devNr = -1;

	assert(configuration != NULL);

	if (config_setting_lookup_int(configuration, DLOGG_MAC_CONFIG_DEV_NR, &devNr)
			&& devNr < 1) {
		logging_adapter_info("The %s configuration directive contains a negative "
				"value: %d", DLOGG_MAC_CONFIG_DEV_NR, devNr);
		return COMMON_TYPE_ERR_CONFIG;
	}
	devNr--;

	err = dlogg_mac_initUART(devNr);
	if (err != COMMON_TYPE_SUCCESS) {
		return err;
	}

	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Initializes the USB UART adapter of the D-LOGG device
 * @details If the ttyID is negative, the first fitting USB device will be used.
 * Otherwise it corresponds to the number of USB UART adapters available. The
 * order of the adapters is determined by libftdi and 0 specifies the first
 * device available. The differentiation between unset and valid numbers is used
 * to issue an appropriate warning, if multiple devices are available.
 * @param ttyID The number of the device to use or -1 if no device number is set
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_mac_initUART(int ttyID) {
	common_type_error_t err;
	struct ftdi_version_info ftdiVersion;

	// display version for debugging purpose
	ftdiVersion = ftdi_get_library_version();
	logging_adapter_debug(
			"Initialized libftdi %s (major: %d, minor: %d, micro: %d, snapshot ver: %s)",
			ftdiVersion.version_str, ftdiVersion.major, ftdiVersion.minor,
			ftdiVersion.micro, ftdiVersion.snapshot_str);

	dlogg_mac_ftdi = ftdi_new();
	if (dlogg_mac_ftdi == NULL ) {
		logging_adapter_info("Can't create a new ftdi context structure");
		return COMMON_TYPE_ERR;
	}

	err = dlogg_mac_openUSBDevice(ttyID);
	if (err != COMMON_TYPE_SUCCESS) {
		return err;
	}

	return dlogg_mac_setUARTParams();
}

/**
 * @brief Opens the given USB UART device.
 * @details Lists available devices and chooses the device corresponding to the
 * ttyID. if ttyID < 0, the device number is treated as unset and the first
 * suitable device will be selected. The first device has the identifier zero.
 * The function assumes that the ftdi context was properly initialized.
 * @param ttyID The device id
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_mac_openUSBDevice(int ttyID) {
	struct ftdi_device_list * devList = NULL;
	struct ftdi_device_list * tmpDevEntry;
	int retCode, i;

	assert(dlogg_mac_ftdi != NULL);

	retCode = ftdi_set_interface(dlogg_mac_ftdi, INTERFACE_ANY);
	if (retCode) {
		logging_adapter_info("Can't set the FTDI channel to any channel (%d)",
				retCode);
		return COMMON_TYPE_ERR;
	}

	// Query devices
	retCode = ftdi_usb_find_all(dlogg_mac_ftdi, &devList, 0, 0);
	if (retCode < 0) {
		logging_adapter_info("Can't query USB adapters (%d)", retCode);
		return COMMON_TYPE_ERR_IO;
	} else if (retCode == 0) {
		logging_adapter_info("No suitable USB device found");
		return COMMON_TYPE_ERR_DEVICE_NOT_FOUND;
	} else if (retCode <= ttyID) {
		logging_adapter_info("Invalid device id: %d of %d", ttyID + 1, retCode);
		ftdi_list_free(&devList);
		return COMMON_TYPE_ERR_INVALID_ADDRESS;
	}

	if (ttyID < 0 && retCode > 1) {
		logging_adapter_info("Device id not set by %s but %d devices available.",
				DLOGG_MAC_CONFIG_DEV_NR, retCode);
	}

	ttyID = ttyID < 0 ? 0 : ttyID;

	// Fetch device entry
	tmpDevEntry = devList;
	for (i = 0; i < ttyID; i++) {
		assert(tmpDevEntry != NULL);
		tmpDevEntry = tmpDevEntry->next;
	}

	// Open device
	retCode = ftdi_usb_open_dev(dlogg_mac_ftdi, tmpDevEntry->dev);
	ftdi_list_free(&devList);
	if (retCode) {
		logging_adapter_info("Can't open USB device %d (%d)", ttyID + 1, retCode);
		return COMMON_TYPE_ERR_IO;
	}

	dlogg_mac_cData.devOpened = 1;

	return EXIT_SUCCESS;
}

/**
 * @brief Sets the UART's transmission parameters
 * @details Assumes that the USB UART device was properly initialized and opened
 * before.
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_mac_setUARTParams() {
	int retCode;

	assert(dlogg_mac_ftdi != NULL);

	retCode = ftdi_set_line_property(dlogg_mac_ftdi, BITS_8, STOP_BIT_1, NONE);
	if (retCode) {
		logging_adapter_info("Can't set the line properties (%d)", retCode);
		return COMMON_TYPE_ERR_IO;
	}

	retCode = ftdi_set_baudrate(dlogg_mac_ftdi, DLOGG_MAC_BAUDRATE);
	if (retCode) {
		logging_adapter_info("Can't set the baud-rate (%d)", retCode);
		return COMMON_TYPE_ERR_IO;
	}

	retCode = ftdi_setdtr(dlogg_mac_ftdi, 1);
	if (retCode) {
		logging_adapter_info("Can't set the DTR line (%d)", retCode);
		return COMMON_TYPE_ERR_IO;
	}

	retCode = ftdi_setrts(dlogg_mac_ftdi, 0);
	if (retCode) {
		logging_adapter_info("Can't clear the RTS line (%d)", retCode);
		return COMMON_TYPE_ERR_IO;
	}

	return COMMON_TYPE_SUCCESS;
}

common_type_error_t dlogg_mac_send(uint8_t *buffer, size_t length,
		dlogg_mac_chksum_t * chksum) {
	int retCode;

	assert(buffer != NULL);
	assert(dlogg_mac_ftdi != NULL);

	retCode = ftdi_write_data(dlogg_mac_ftdi, buffer, length);
	if (retCode != length) {
		logging_adapter_info("Cant't write to the USB device (%d)", retCode);
		return COMMON_TYPE_ERR_IO;
	}

	dlogg_mac_updateChksum(buffer, length, chksum);

	return COMMON_TYPE_SUCCESS;
}

common_type_error_t dlogg_mac_read(uint8_t *buffer, size_t length,
		dlogg_mac_chksum_t * chksum) {
	struct ftdi_transfer_control *transferCtrl;
	struct timespec tv;
	int retCode;
	int retryCnt = DLOGG_MAC_RETRY;

	assert(buffer != NULL);
	assert(dlogg_mac_ftdi != NULL);

	transferCtrl = ftdi_read_data_submit(dlogg_mac_ftdi, buffer, length);
	if(transferCtrl == NULL){
		logging_adapter_info("Error during submitting read request");
		return COMMON_TYPE_ERR_IO;
	}

	// workaround for the missing read-timeout
	tv.tv_nsec = ((long)length) * 1000000000L / DLOGG_MAC_BAUDRATE;
	tv.tv_sec = tv.tv_nsec / 999999999;
	tv.tv_nsec = tv.tv_nsec % 999999999;

	do{
		retryCnt --;

		retCode = nanosleep(&tv,NULL);
		if(retCode){
			logging_adapter_info("Caught interrupt while reading data");
			return COMMON_TYPE_ERR;
		}

		// set timeout near device response time (just a guess)
		tv.tv_sec = 0;
		tv.tv_nsec = 100000;

		retCode = ftdi_transfer_data_done(transferCtrl);
		if(retCode < 0){
			logging_adapter_info("Can't read from the USB device (%d)", retCode);
			return COMMON_TYPE_ERR_IO;
		}
	}while(retCode < length && retryCnt >= 0);

	if(retCode < length){
		logging_adapter_info("Can't read all data (only %d of %d)",retCode, length);
		return COMMON_TYPE_ERR_TIMEOUT;
	}

	logging_adapter_debug("DLOGG-MAC: %d/%d retries left", (retryCnt + 1),
			DLOGG_MAC_RETRY);

	dlogg_mac_updateChksum(buffer, length, chksum);

	return COMMON_TYPE_SUCCESS;
}

common_type_error_t fieldbus_mac_free() {
	int retCode;
	common_type_error_t err = COMMON_TYPE_SUCCESS;

	if (dlogg_mac_ftdi != NULL ) {

		// close opened device
		if (dlogg_mac_cData.devOpened) {
			retCode = ftdi_usb_close(dlogg_mac_ftdi);
			if (retCode) {
				logging_adapter_info("Can't successfully close the USB device (%d)",
						retCode);
				err = COMMON_TYPE_ERR_IO;
			}
			dlogg_mac_cData.devOpened = 0;
		}

		ftdi_free(dlogg_mac_ftdi);
		dlogg_mac_ftdi = NULL;
	}

	return err;
}
