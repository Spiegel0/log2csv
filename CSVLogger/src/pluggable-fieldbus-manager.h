/**
 * @file pluggable-fieldbus-manager.h
 * @brief The file defines functions used to transparently access remote
 * variables.
 * @details Each remote variable, called channel, will be identified using an
 * unique integer value. During initialization configured MAC layer modules will
 * be added. Afterwards each channel has to be registered using the addChannel
 * functions.
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

#ifndef PLUGGABLE_FIELDBUS_MANAGER_H_
#define PLUGGABLE_FIELDBUS_MANAGER_H_

#include <common-type.h>
#include <libconfig.h>

/**
 * @brief Initializes the network stack
 * @details Dynamically loads the fieldbus MAC modules and tries to initialize
 * them. The function has to be called once before any other function of the
 * module. It requires the logging facilities to be properly initialized.
 * @param configuration The root configuration used to obtain needed values
 * @return The status of the operation.
 */
common_type_error_t pfm_init(
		config_setting_t* configuration);

/**
 * @brief Opens a new virtual channel
 * @details If the channel uses a new fieldbus application module it will be
 * loaded dynamically.
 * @param channelConf The configuration snippet describing the channel.
 * @return The unique channel identifier or a negative number if the operation
 * failed
 */
int pfm_addChannel(config_setting_t* channelConf);

/**
 * @brief Synchronizes every channel
 * @details The function has to be called before reading one or more values. It
 * tries to take a consistent snapshot of the system. If one or more module
 * issues an error the function will abort the sync process and return the
 * error code.
 * @return The status of the operation
 */
common_type_error_t pfm_sync(void);

/**
 * @brief Fetches the value from the given channel.
 * @details The sync function has to be called before but not necessarily
 * directly before calling this function. Reading a channel more than once after
 * the sync signal may return inconsistent results.
 * @param id The unique channel identifier
 * @return The read value or an error code.
 */
common_type_t pfm_fetchValue(int id);

/**
 * @brief Frees used resources.
 * @details After calling the function only init, reconfiguring the module, may
 * be called again.
 * @return The status of the operation.
 */
common_type_error_t pfm_free(void);

#endif /* PLUGGABLE_FIELDBUS_MANAGER_H_ */
