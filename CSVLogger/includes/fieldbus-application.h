/**
 * @file fieldbus-application.h
 * @brief Defines the fieldbus application layer interface.
 * @details <p>The interface provides function definitions to access specific
 * values remotely. Addressing will be provided by passing generic configuration
 * snippets.</p>
 * @author Michael Spiegel, michael.h.spiegel@gmail.com
 */

#ifndef FIELDBUS_APPLICATION_H_
#define FIELDBUS_APPLICATION_H_

#include "common-type.h"

#include <libconfig.h>

/**
 * @brief Initializes the module.
 * @details <p>The init function will be called once before using any other
 * function. It may be called again after invoking the free function. The
 * function can safely assume that every configured MAC module as well as the
 * logging facilities will be loaded and initialized before. It must not call
 * the MAC init function on it's own </p>
 * @return The status of the initialization
 */
common_type_error_t fieldbus_application_init(void);

/** @brief The pointer type of the fieldbus_application_init function */
typedef common_type_error_t (*fieldbus_application_init_t)(void);

/** @brief The name of the fieldbus_application_init function */
#define FIELDBUS_APPLICATION_INIT_NAME "fieldbus_application_init"

/**
 * @brief Issues a synchronization command
 * @details <p>The function is called once before starting a new communication
 * cycle. Before calling the function the MAC's layers sync function will be
 * called and must not be called again by this function.</p>
 * @return The status of the synchronization
 */
common_type_error_t fieldbus_application_sync(void);

/** @brief The pointer type of the fieldbus_application_sync function */
typedef common_type_error_t (*fieldbus_application_sync_t)(void);

/** @brief The name of the fieldbus_application_fetchValue function */
#define FIELDBUS_APPLICATION_SYNC_NAME "fieldbus_application_sync"

/**
 * @brief Retrieves a measured value from the end device.
 * @details <p>The measured value has to be returned appropriately scaled by the
 * corresponding SI unit. On returning string references the buffer must be
 * valid until the next function of the module is called. The error type must
 * only be used if the value can not be fetched.</p>
 * <p>The given address will be directly passed from the read configuration
 * file. It encapsulates every information entered but may not be complete.</p>
 * @param address The configuration snippet specifying the address
 * @return The value read or an appropriate error.
 */
common_type_t fieldbus_application_fetchValue(config_setting_t *address);

/** @brief The pointer type of the fieldbus_application_fetchValue function */
typedef common_type_t (*fieldbus_application_fetchValue_t)(
		config_setting_t *address);

/** @brief The name of the fieldbus_application_fetchValue function */
#define FIELDBUS_APPLICATION_FETCH_VALUE_NAME "fieldbus_application_fetchValue"

/**
 * @brief Frees used resources.
 * @details After calling the function only init, reconfiguring the module, may
 * be called again.
 * @return The status of the operation.
 */
common_type_error_t fieldbus_application_free(void);

/** @brief The pointer type of the fieldbus_application_free function */
typedef common_type_error_t (*fieldbus_application_free_t)(void);

/** @brief The name of the fieldbus_application_free function */
#define FIELDBUS_APPLICATION_FREE_NAME "fieldbus_application_free"

#endif /* FIELDBUS_APPLICATION_H_ */
