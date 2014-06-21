/**
 * @file fieldbus-mac.h
 * @brief Specifies the interface used to load and control MAC modules.
 * @details <p>The interface definition will contain functions and data types
 * used to communicate with the main application only. Protocol specific
 * interfaces used to send and receive frames will be specified separately.</p>
 * @author Michael Spiegel, michael.h.spiegel@gmail.com
 */

#ifndef FIELDBUS_MAC_H_
#define FIELDBUS_MAC_H_

#include "common-type.h"

#include <libconfig.h>

/**
 * @brief Initializes the module following the given configuration
 * @details The logging facility will be properly initialized before calling.
 * The function will be called exactly once before using any other function of
 * the module. Only after calling the free-function the init function may be
 * called again.
 * @param configuration The module's configuration.
 * @return The status of the operation.
 */
common_type_error_t fieldbus_mac_init(config_setting_t* configuration);

/** @brief The pointer type of fieldbus_mac_init */
typedef common_type_error_t (*fieldbus_mac_init_t)(
		config_setting_t* configuration);

/** @brief The name of fieldbus_mac_init */
#define FIELDBUS_MAC_INIT_NAME "fieldbus_mac_init"

/**
 * @brief Indicates a global sync event.
 * @details The sync event will be triggered exactly once a cycle before reading
 * any value. The MAC layer's sync functions will be called before the
 * application layer's function.
 * @return The status of the sync operation
 */
common_type_error_t fieldbus_mac_sync(void);

/** @brief The pointer type of fieldbus_mac_sync */
typedef common_type_error_t (*fieldbus_mac_sync_t)(void);

/** @brief The name of fieldbus_mac_sync */
#define FIELDBUS_MAC_SYNC_NAME "fieldbus_mac_sync"

/**
 * @brief Function called to free the used resources
 * @details The function will be called before terminating the program. After
 * the function is called no other function, except init, will be called
 * anymore. The application layer's free functions will be called before the
 * MAC's always.
 * @return The status of the operation
 */
common_type_error_t fieldbus_mac_free(void);

/** @brief The pointer type of fieldbus_mac_free */
typedef common_type_error_t (*fieldbus_mac_free_t)(void);

/** @brief The name of fieldbus_mac_free */
#define FIELDBUS_MAC_FREE_NAME "fieldbus_mac_free"

#endif /* FIELDBUS_MAC_H_ */
