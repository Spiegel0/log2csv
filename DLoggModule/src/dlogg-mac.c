/**
 * @file dlogg-mac.c
 * @brief The file contains the d-logg mac layer
 * @author Michael Spiegel, michael.h.spiegel@gmail.com
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

/* Configuration directives */
#define DLOGG_MAC_CONFIG_INTERFACE "interface"

/** @brief The timeout value in tenth of seconds */
#define DLOGG_MAC_TIMEOUT (8)

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
static void dlogg_mac_updateChksum(uint8_t * buffer, size_t length,
		dlogg_mac_chksum_t* chksum);
static void dlogg_mac_debug_buffer(const char* name, uint8_t* buffer,
		size_t length);

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
 * @param inerface The interface path to open
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

common_type_error_t fieldbus_mac_sync() {
	common_type_error_t err;
	dlogg_mac_chksum_t chksum = 0;

	uint8_t buffer[7] = {0x20, 0x10, 0x18, 0,0,0,0};
	err = dlogg_mac_send(buffer, sizeof(buffer), &chksum);
	if(err != COMMON_TYPE_SUCCESS)
		return err;
	err = dlogg_mac_send_chksum(&chksum);
	if(err != COMMON_TYPE_SUCCESS)
		return err;

	chksum = 0;
	err = dlogg_mac_read(buffer,2,&chksum);
	if(err != COMMON_TYPE_SUCCESS)
		return err;

	dlogg_mac_debug_buffer("Ack Received", buffer, 2);
	if(buffer[0] != 0x21 || buffer[1] != 0x43)
		return COMMON_TYPE_ERR_INVALID_RESPONSE;

	chksum = 0; // without ack
	err = dlogg_mac_read(buffer, 2, &chksum);
	if(err != COMMON_TYPE_SUCCESS)
		return err;
	dlogg_mac_debug_buffer("Module ID", buffer, 2);
	err = dlogg_mac_read_chksum(&chksum);

	return err;
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

common_type_error_t dlogg_mac_send_chksum(dlogg_mac_chksum_t * chksum) {
	return dlogg_mac_send((uint8_t *) chksum, sizeof(*chksum), NULL );
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
					"expected", (unsigned int) remaining);
			return COMMON_TYPE_ERR_TIMEOUT;
		} else if (rd < 0) {
			logging_adapter_info("Can't read %u more bytes of data from d-logg: %s",
					(unsigned int) remaining, strerror(errno));
			return COMMON_TYPE_ERR_IO;
		}
		remaining -= rd;
	}

	dlogg_mac_updateChksum(buffer, length, chksum);

	return COMMON_TYPE_SUCCESS;
}

common_type_error_t dlogg_mac_read_chksum(dlogg_mac_chksum_t * chksum) {
	common_type_error_t err;
	dlogg_mac_chksum_t chksumRead;
	assert(chksum != NULL);
	err = dlogg_mac_read((uint8_t *) &chksumRead, sizeof(chksumRead), NULL );
	if (err != COMMON_TYPE_SUCCESS) {
		return err;
	}

	if (*chksum != chksumRead) {
		logging_adapter_info("Received invalid checksum %u, %u expected.",
				(unsigned int) chksumRead, (unsigned int) *chksum);
		return COMMON_TYPE_ERR_INVALID_RESPONSE;
	}
	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Updates the checksum value, if any
 * @details The result will be written to the given checksum location. The
 * checksum is defined as the sum of the sent/received bytes mod 256. The
 * chksum field will contain the partial sum of the buffer mod 256.
 * @param buffer The valid buffer to check
 * @param length The number of elements to check
 * @param chksum A valid pointer to a checksum location or null, if no checksum
 * is to be calculated
 */
static void dlogg_mac_updateChksum(uint8_t * buffer, size_t length,
		dlogg_mac_chksum_t * chksum) {
	int i;
	assert(buffer != NULL || chksum == NULL);

	if (chksum != NULL ) {
		for (i = 0; i < length; i++) {
			*chksum = (*chksum + buffer[i]) & 0xFF;
		}
	}
}

/**
 * @brief Writes the buffer's content nicely formatted to the debug logger
 * @param name The buffer's name
 * @param buffer A valid buffer reference holding at least length bytes
 * @param length The number of bytes to write
 */
static void dlogg_mac_debug_buffer(const char* name, uint8_t* buffer,
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
		if (sprintf(&strBuffer[3 * i], "%x ", (unsigned int) buffer[i]) != 3) {
			logging_adapter_debug("sprintf failed");
			return;
		}
	}

	logging_adapter_debug("Buffer %s (length: %u): | %s|", name,
			(unsigned int) length, strBuffer);

	free(strBuffer);
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

