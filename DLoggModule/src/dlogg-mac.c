/**
 * @file dlogg-mac.c
 * @brief The file contains the d-logg MAC layer implementation
 * @details It uses the termios API to access the USB UART device present in
 * D-LOGG. Some functions specified by dlogg-mac.h which don't directly use any
 * hardware connections are out-sourced to dlogg-mac-common.c. Placing these
 * functions in a different file enhances re-usability of the code, if another
 * hardware access API is used.
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

#include <fieldbus-mac.h>
#include <logging-adapter.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include "dlogg-mac.h"
#include "dlogg-mac-common.h"

/* Configuration directives */
#define DLOGG_MAC_CONFIG_INTERFACE "interface"

/** @brief The timeout value in tenth of seconds */
#define DLOGG_MAC_TIMEOUT (20)

/** @brief The file handler used to access the tty */
static int dlogg_mac_ttyFD = -1;

/** @brief The old terminal device settings */
static struct termios dlogg_mac_oldTio;

/** @brief Structure containing encapsulated data */
static struct {
	/**
	 * @brief Flag indicating that the oldTio structure was successfully obtained.
	 */
	unsigned int restoreTioSettings :1;
} dlogg_mac_cData;

/* Function Prototypes */
static inline common_type_error_t dlogg_mac_initTTY(const char* interface);

common_type_error_t fieldbus_mac_init(config_setting_t* configuration) {
	const char* interface;

	assert(configuration != NULL);

	if (!config_setting_is_group(configuration)) {
		logging_adapter_info("The MAC configuration isn't a group");
		return COMMON_TYPE_ERR_CONFIG;
	}

	if (!config_setting_lookup_string(configuration, DLOGG_MAC_CONFIG_INTERFACE,
			&interface)) {
		logging_adapter_info("Can't find the \"%s\" string configuration directive "
				"inside MAC group", DLOGG_MAC_CONFIG_INTERFACE);
		return COMMON_TYPE_ERR_CONFIG;
	}

	return dlogg_mac_initTTY(interface);
}

/**
 * @brief initializes the tty interface
 * @details It assumes that the global file descriptor is currently closed (-1)
 * and that the given interface string is valid. After successfully opening the
 * device the termois settings will be saved globally.
 * @param interface The interface path to open
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_mac_initTTY(const char* interface) {
	struct termios ttySettings, controlSettings;
	assert(dlogg_mac_ttyFD < 0);
	assert(interface != NULL);
	assert(!dlogg_mac_cData.restoreTioSettings);

	errno = 0;
	dlogg_mac_ttyFD = open(interface, O_RDWR | O_NOCTTY);
	if (dlogg_mac_ttyFD < 0) {
		logging_adapter_info("Can't open the device \"%s\": %s", interface,
				strerror(errno));
		return COMMON_TYPE_ERR_DEVICE_NOT_FOUND;
	}

	logging_adapter_debug("Successfully opened d-logg device \"%s\"", interface);

	// save old state
	if (tcgetattr(dlogg_mac_ttyFD, &dlogg_mac_oldTio)) {
		logging_adapter_info("Can't obtain the \"%s\" devices settings: %s",
				interface, strerror(errno));
		return COMMON_TYPE_ERR_IO;
	}
	dlogg_mac_cData.restoreTioSettings = 1;

	// Assemble new settings
	memset(&ttySettings, 0, sizeof(ttySettings));
	memset(&controlSettings, 0, sizeof(controlSettings));

	if (cfsetspeed(&ttySettings, B115200)) {
		logging_adapter_info("Can't set the \"%s\" device's speed: %s", interface,
				strerror(errno));
		return COMMON_TYPE_ERR_IO;
	}

	// Use non canonical mode, 8 bits, no parity, 1 stop bit, DTR: on, RTS: off
	ttySettings.c_cc[VMIN] = 0;
	ttySettings.c_cc[VTIME] = DLOGG_MAC_TIMEOUT;
	ttySettings.c_cflag |= CS8 | CREAD | CLOCAL;

	if (tcsetattr(dlogg_mac_ttyFD, TCSAFLUSH, &ttySettings)) {
		logging_adapter_info("Can't change the \"%s\" device's settings: %s",
				interface, strerror(errno));
		return COMMON_TYPE_ERR_IO;
	}
	if (tcgetattr(dlogg_mac_ttyFD, &controlSettings)) {
		logging_adapter_info("Can't obtain the \"%s\" devices settings: %s",
				interface, strerror(errno));
		return COMMON_TYPE_ERR_IO;
	}

	if (memcmp(&ttySettings, &controlSettings, sizeof(ttySettings)) != 0) {
		logging_adapter_info("Can't set all of the \"%s\" device's settings: %s",
				interface, strerror(errno));
		return COMMON_TYPE_ERR_IO;
	}

	logging_adapter_debug("Configured d-logg interface device \"%s\"", interface);

	return COMMON_TYPE_SUCCESS;
}

common_type_error_t dlogg_mac_send(uint8_t *buffer, size_t length,
		dlogg_mac_chksum_t * chksum) {

	assert(buffer != NULL);

	if (write(dlogg_mac_ttyFD, buffer, length) != length) {
		logging_adapter_info("Can't write to the d-logg interface: %s",
				strerror(errno));
		return COMMON_TYPE_ERR_IO;
	}

	dlogg_mac_updateChksum(buffer, length, chksum);

	return COMMON_TYPE_SUCCESS;
}

common_type_error_t dlogg_mac_read(uint8_t *buffer, size_t length,
		dlogg_mac_chksum_t * chksum) {
	size_t remaining = length;
	ssize_t rd;

	assert(buffer != NULL);

	while (remaining > 0) {
		rd = read(dlogg_mac_ttyFD, &buffer[length - remaining], remaining);
		if (rd == 0) {
			logging_adapter_info("Timeout while reading from d-logg. %u more bytes "
					"expected, got %u so far.", (unsigned) remaining,
					(unsigned) length - remaining);
			return COMMON_TYPE_ERR_TIMEOUT;
		} else if (rd < 0) {
			logging_adapter_info("Can't read %u more bytes of data from d-logg: %s",
					(unsigned) remaining, strerror(errno));
			return COMMON_TYPE_ERR_IO;
		}
		remaining -= rd;
	}

	dlogg_mac_updateChksum(buffer, length, chksum);

	return COMMON_TYPE_SUCCESS;
}

common_type_error_t fieldbus_mac_free() {
	common_type_error_t err = COMMON_TYPE_SUCCESS;

	// Deallocate TTY
	if (dlogg_mac_ttyFD >= 0) {
		if (dlogg_mac_cData.restoreTioSettings) {
			if (tcsetattr(dlogg_mac_ttyFD, TCSADRAIN, &dlogg_mac_oldTio)) {
				logging_adapter_info("Can't successfully restore the tty settings: %s",
						strerror(errno));
				err = COMMON_TYPE_ERR_IO;
			}
		}
		dlogg_mac_cData.restoreTioSettings = 0;

		if (close(dlogg_mac_ttyFD)) {
			logging_adapter_info("Can't close the tty device: %s", strerror(errno));
			err = COMMON_TYPE_ERR_IO;
		}
		dlogg_mac_ttyFD = -1;
	}
	return err;
}

