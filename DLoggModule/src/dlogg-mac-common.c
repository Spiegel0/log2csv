/**
 * @file dlogg-mac-common.c
 * @brief The file implements common D-LOGG MAC layer functions
 * @details It does not only provide some helper functions but also implements
 * dlogg-mac.h functions which do not access the hardware directly.
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
 *
 */

#include "dlogg-mac-common.h"

#include <assert.h>
#include <stdlib.h>
#include <logging-adapter.h>

common_type_error_t dlogg_mac_send_chksum(dlogg_mac_chksum_t * chksum) {
	return dlogg_mac_send((uint8_t *) chksum, sizeof(*chksum), NULL );
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

void dlogg_mac_updateChksum(uint8_t * buffer, size_t length,
		dlogg_mac_chksum_t * chksum) {
	int i;

	assert(buffer != NULL || chksum == NULL);

	if (chksum != NULL ) {
		for (i = 0; i < length; i++) {
			*chksum = (*chksum + buffer[i]) & 0xFF;
		}
	}
}
