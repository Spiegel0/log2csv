/**
 * @file dlogg-mac-common.h
 * @brief The header file specifies common MAC function used by the termios
 * based implementation as well as the libftdi based one.
 * @details The definitions of this module arn't meant to be used outside the
 * MAC layer.
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

#ifndef DLOGG_MAC_COMMON_H_
#define DLOGG_MAC_COMMON_H_

#include "dlogg-mac.h"

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
void dlogg_mac_updateChksum(uint8_t * buffer, size_t length,
		dlogg_mac_chksum_t* chksum);


#endif /* DLOGG_MAC_COMMON_H_ */
