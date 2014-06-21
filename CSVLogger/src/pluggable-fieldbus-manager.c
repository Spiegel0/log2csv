/**
 * @file pluggable-fieldbus-manager.c
 * @brief Implements the fieldbus network manager
 * @author Michael Spiegel, michael.h.spiegel@gmail.com
 */

#include "pluggable-fieldbus-manager.h"

#include <logging-adapter.h>
#include <fieldbus-mac.h>
#include <fieldbus-application.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

/* Configuration directives */
#define PLUGGABLE_FIELDBUS_MANAGER_CONFIG_MAC "mac"
#define PLUGGABLE_FIELDBUS_MANAGER_CONFIG_NAME "name"
#define PLUGGABLE_FIELDBUS_MANAGER_CONFIG_TYPE "type"
#define PLUGGABLE_FIELDBUS_MANAGER_CONFIG_ADDRESS "address"

/** @brief Structure encapsulating the MAC module's data*/
typedef struct {
	/** @brief The handler of the library returned by dlopen */
	void * handler;
	/** The sync function pointer of the module */
	fieldbus_mac_sync_t sync;
	/** The free function pointer of the module */
	fieldbus_mac_free_t free;
} pluggable_fieldbus_manager_mac_t;

/** @brief Structure encapsulating an application module's data */
typedef struct {
	/** @brief The handler used by the dl function. */
	void* handler;
	/**
	 * @brief The name of the shared object holding the module
	 * @details The string will be contained within the configuration structure
	 * and hasn't to be deallocated manually.
	 */
	const char* name;
	/** @brief fieldbus_application_sync function reference of the module */
	fieldbus_application_sync_t sync;
	/** @brief fieldbus_application_fetchValue function reference of the module */
	fieldbus_application_fetchValue_t fetchValue;
	/** @brief fieldbus_application_free function reference of the module */
	fieldbus_application_free_t free;
} pluggable_fieldbus_manager_app_t;

/** @brief Structure defining a single data channel */
typedef struct {
	/** @brief The configuration snippet defining the address */
	config_setting_t *address;
	/** @brief The associated application layer module */
	pluggable_fieldbus_manager_app_t* app;
} pluggable_filedbus_manager_channel_t;

/** @brief The size of the MAC module vector*/
static unsigned int pluggable_fieldbus_manager_macVectorLength = 0;
/** @brief The vector containing loaded MAC module handler */
static pluggable_fieldbus_manager_mac_t *pluggable_fieldbus_manager_macVector;

/** @brief The number of loaded application modules */
static unsigned int pluggable_fieldbus_manager_appVectorLength = 0;
/** @brief The vector of loaded application modules */
static pluggable_fieldbus_manager_app_t * pluggable_fieldbus_manager_appVector;

/** @brief The number of available channels */
static unsigned int pluggable_fieldbus_manager_channelVectorLength = 0;
/** @brief The vector containing every initialized channel */
static pluggable_filedbus_manager_channel_t *pluggable_fieldbus_manager_channelVector;

/* Function prototypes */
static inline common_type_error_t pluggable_fieldbus_manager_installModule(
		config_setting_t *modConfig, const unsigned int index);
static inline common_type_error_t pluggable_fieldbus_manager_freeMac(void);
static pluggable_fieldbus_manager_app_t pluggable_fieldbus_manager_getAppModule(
		const char* driverName);
static pluggable_fieldbus_manager_app_t * pluggable_fieldbus_manager_loadAppModule(
		const char* name);
static void pluggable_fieldbus_manager_appVectorRollback(void);

/**
 * @brief Extracts the module's names and loads them.
 * @param configuration The root configuration containing the mac member. The
 * root configuration has to be a group setting always.
 * @return The status of the operation
 */
common_type_error_t pluggable_fieldbus_manager_init(
		config_setting_t* configuration) {
	config_setting_t *mac;
	common_type_error_t err;
	int i;

	assert(config_setting_is_group(configuration));

	// Fetch and check the mac module configuration sections
	mac = config_setting_get_member(configuration,
			PLUGGABLE_FIELDBUS_MANAGER_CONFIG_MAC);
	if (mac == NULL ) {
		logging_adapter_info("Can't locate the \"%s\" list directive.",
				PLUGGABLE_FIELDBUS_MANAGER_CONFIG_MAC);
		return COMMON_TYPE_ERR_CONFIG;
	}
	if (!config_setting_is_list(mac)) {
		logging_adapter_info("The \"%s\" directive isn't a list.",
				PLUGGABLE_FIELDBUS_MANAGER_CONFIG_MAC);
		return COMMON_TYPE_ERR_CONFIG;
	}

	// Allocate the mac module vector.
	pluggable_fieldbus_manager_macVectorLength = config_setting_length(mac);
	assert(pluggable_fieldbus_manager_macVectorLength >= 0);
	pluggable_fieldbus_manager_macVector = malloc(
			pluggable_fieldbus_manager_macVectorLength
					* sizeof(pluggable_fieldbus_manager_macVector[0]));
	if (pluggable_fieldbus_manager_macVector == NULL ) {
		return COMMON_TYPE_ERR;
	}
	memset(pluggable_fieldbus_manager_macVector, 0,
			pluggable_fieldbus_manager_macVectorLength
					* sizeof(pluggable_fieldbus_manager_macVector[0]));

	// Load the mac modules
	for (i = 0; i < pluggable_fieldbus_manager_macVectorLength; i++) {
		err = pluggable_fieldbus_manager_installModule(
				config_setting_get_elem(mac, i), i);
		if (err != COMMON_TYPE_SUCCESS) {
			return err;
		}
	}

	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Loads the given module, adds it's handler to the list of known modules
 * and initializes it.
 * @details If the configuration is invalid an appropriate error message will
 * be reported.
 * @param modConfig The module's group configuration.
 * @param index The index within the mac vector structure to populate.
 * @return The status of the operation
 */
static inline common_type_error_t pluggable_fieldbus_manager_installModule(
		config_setting_t *modConfig, const unsigned int index) {
	const char* name = "";
	char* errStr;
	common_type_error_t err;
	fieldbus_mac_init_t init;

	assert(modConfig != NULL);
	assert(index < pluggable_fieldbus_manager_macVectorLength);

	if (!config_setting_is_group(modConfig)) {
		logging_adapter_info("The \"%s\" directive contains an invalid list entry",
				PLUGGABLE_FIELDBUS_MANAGER_CONFIG_MAC);
		return COMMON_TYPE_ERR_CONFIG;
	}
	if (!config_setting_lookup_string(modConfig,
			PLUGGABLE_FIELDBUS_MANAGER_CONFIG_NAME, &name)) {
		logging_adapter_info(
				"Can't find the \"%s\" string directive inside the MAC "
						"module directive", PLUGGABLE_FIELDBUS_MANAGER_CONFIG_NAME);
		return COMMON_TYPE_ERR_CONFIG;
	}
	assert(name != NULL);

	logging_adapter_debug("Try to load MAC module \"%s\"", name);

	pluggable_fieldbus_manager_macVector[index].handler = dlopen(name,
			RTLD_NOW | RTLD_GLOBAL);
	if (pluggable_fieldbus_manager_macVector[index].handler == NULL ) {
		logging_adapter_info("Can't load \"%s\": %s", name, dlerror());
		return COMMON_TYPE_ERR_LOAD_MODULE;
	}

	// Load end execute init function
	(void) dlerror();
	init = (fieldbus_mac_init_t) dlsym(
			pluggable_fieldbus_manager_macVector[index].handler, "fieldbus_mac_init");
	errStr = dlerror();
	if (errStr != NULL ) {
		logging_adapter_info("Can't successfully load the \"%s\" "
				"function: %s", FIELDBUS_MAC_INIT_NAME, errStr);
		return COMMON_TYPE_ERR_LOAD_MODULE;
	}

	err = init(modConfig);
	if (err != COMMON_TYPE_SUCCESS) {
		return err;
	}

	// Load free and sync
	(void) dlerror();
	pluggable_fieldbus_manager_macVector[index].sync =
			(fieldbus_mac_sync_t) dlsym(
					pluggable_fieldbus_manager_macVector[index].handler,
					FIELDBUS_MAC_SYNC_NAME);
	errStr = dlerror();
	if (errStr != NULL ) {
		logging_adapter_info("Can't successfully load the \"%s\" "
				"function: %s", FIELDBUS_MAC_SYNC_NAME, errStr);
		return COMMON_TYPE_ERR_LOAD_MODULE;
	}

	(void) dlerror();
	pluggable_fieldbus_manager_macVector[index].free =
			(fieldbus_mac_free_t) dlsym(
					pluggable_fieldbus_manager_macVector[index].handler,
					FIELDBUS_MAC_FREE_NAME);
	errStr = dlerror();
	if (errStr != NULL ) {
		logging_adapter_info("Can't successfully load the \"%s\" "
				"function: %s", FIELDBUS_MAC_FREE_NAME, errStr);
		return COMMON_TYPE_ERR_LOAD_MODULE;
	}

	assert(pluggable_fieldbus_manager_macVector[index].free != NULL);
	assert(pluggable_fieldbus_manager_macVector[index].sync != NULL);

	return COMMON_TYPE_SUCCESS;
}

/**
 * @details Assumes that the channelConf reference isn't null.
 */
int pluggable_fieldbus_manager_addChannel(config_setting_t* channelConf) {
	int channelID = -1;
	char* driver = "";
	config_setting_t *address;
	pluggable_fieldbus_manager_app_t *appModule = NULL;

	assert(channelConf != NULL);

	if (!config_setting_is_group(channelConf)) {
		logging_adapter_info("The given channel configuration isn't a valid group "
				"of directives");
		return -1;
	}

	if (!config_setting_lookup_string(channelConf,
			PLUGGABLE_FIELDBUS_MANAGER_CONFIG_TYPE, &driver)) {
		logging_adapter_info("Can't load the \"%s\" string configuration "
				"directive.", PLUGGABLE_FIELDBUS_MANAGER_CONFIG_TYPE);
		return -1;
	}

	address = config_setting_get_member(channelConf,
			PLUGGABLE_FIELDBUS_MANAGER_CONFIG_ADDRESS);
	if (address == NULL ) {
		logging_adapter_info(
				"Can't obtain the \"%s\" channel configuration's \"%s\" "
						"member", driver, PLUGGABLE_FIELDBUS_MANAGER_CONFIG_ADDRESS);
		return -1;
	}

	// obtain the device driver
	appModule = pluggable_fieldbus_manager_getAppModule(driver);
	if(appModule == NULL){
		appModule = pluggable_fieldbus_manager_loadAppModule(driver);
	}
	if(appModule == NULL){
		return -1;
	}
	// TODO: add Channel
	return channelID;
}

/**
 * @brief Loads and initializes the application module and adds it to the global
 * list.
 * @details It assumes that the name used to lookup the shared library module
 * isn't null.
 * @param name The name or path of the application layer module
 * @return A reference to the newly created list entry or null.
 */
static pluggable_fieldbus_manager_app_t * pluggable_fieldbus_manager_loadAppModule(
		const char* name) {
	pluggable_fieldbus_manager_app_t *oldVector =
			pluggable_fieldbus_manager_appVector;
	pluggable_fieldbus_manager_app_t *ret;

	assert(name != NULL);

	pluggable_fieldbus_manager_appVector = realloc(
			pluggable_fieldbus_manager_appVector,
			(pluggable_fieldbus_manager_appVectorLength + 1)
					* sizeof(pluggable_fieldbus_manager_appVector[0]));
	if (pluggable_fieldbus_manager_appVector == NULL ) {
		pluggable_fieldbus_manager_appVector = oldVector;
		logging_adapter_info("Can't obtain more memory");
		return NULL ;
	}
	pluggable_fieldbus_manager_appVectorLength++;
	ret =
			pluggable_fieldbus_manager_appVector[pluggable_fieldbus_manager_appVectorLength
					- 1];
	memset(ret, 0, sizeof(ret[0]));

	ret->handler = dlopen(name, RTLD_NOW);
	if (ret->handler == NULL ) {
		pluggable_fieldbus_manager_appVectorRollback();
		logging_adapter_info("Can't load application module \"%s\": %s", name,
				dlerror());
		return NULL ;
	}

	// TODO: initialize Module

	return ret;
}

/**
 * @brief Fetches the application layer module with the given name.
 * @details It assumes that the given driverName reference isn't null. If the
 * module isn't present null will be returned.
 * @param driverName The name of the application layer module
 * @return The module list entry or null
 */
static pluggable_fieldbus_manager_app_t* pluggable_fieldbus_manager_getAppModule(
		const char* driverName) {
	unsigned int i;
	assert(driverName != NULL);
	for (i = 0; i < pluggable_fieldbus_manager_appVectorLength; i++) {
		if (strcmp(driverName, pluggable_fieldbus_manager_appVector[i].name) == 0) {
			return &pluggable_fieldbus_manager_appVector[i];
		}
	}
	return NULL ;
}

/**
 * @brief Removes the last element from the appVector.
 * @details The rollback function is intended to be used after an error
 * occurred. Thus it won't propagate subsequent errors during deallocating
 * memory.
 */
static void pluggable_fieldbus_manager_appVectorRollback(void) {
	assert(pluggable_fieldbus_manager_appVectorLength > 0);
	pluggable_fieldbus_manager_app_t * oldVector =
			pluggable_fieldbus_manager_appVector;
	pluggable_fieldbus_manager_appVectorLength--;
	pluggable_fieldbus_manager_appVector = realloc(
			pluggable_fieldbus_manager_appVector,
			pluggable_fieldbus_manager_appVectorLength * sizeof(oldVector[0]));
	if(pluggable_fieldbus_manager_appVector == NULL){
		pluggable_fieldbus_manager_appVector = oldVector;
	}
}

common_type_error_t pluggable_fieldbus_manager_sync() {
	return COMMON_TYPE_ERR;
}

common_type_t pluggable_fieldbus_manager_fetchValue(int id) {
	common_type_t ret;
	ret.data.errVal = COMMON_TYPE_ERR;
	ret.type = COMMON_TYPE_ERROR;
	return ret;
}

common_type_error_t pluggable_fieldbus_manager_free() {
	common_type_error_t err = COMMON_TYPE_SUCCESS;
	if (pluggable_fieldbus_manager_macVector != NULL ) {
		err = pluggable_fieldbus_manager_freeMac();
	}
	//TODO: free appVector
	return err;
}

/**
 * @brief Frees resources allocated by the MAC layer
 * @details The function calls the module's free functions and tries to
 * deallocate them. The function doesn't stop immediately if an error occurs.
 * Instead it tries to deallocate as many resources as possible avoiding memory
 * leaks. It assumes that the macVector is allocated but the vector may not be
 * fully initialized
 * @return The status of the operation. Only the last error will be reported.
 */
static inline common_type_error_t pluggable_fieldbus_manager_freeMac() {
	unsigned int i;
	int err;
	common_type_error_t lastErr = COMMON_TYPE_SUCCESS, tmpErr;

	err = 0;
	for (i = 0; i < pluggable_fieldbus_manager_macVectorLength; i++) {

		if (pluggable_fieldbus_manager_macVector[i].free != NULL ) {
			tmpErr = pluggable_fieldbus_manager_macVector[i].free();
			err |= (tmpErr == COMMON_TYPE_SUCCESS ? 0 : 1);
			lastErr = (tmpErr == COMMON_TYPE_SUCCESS ? lastErr : tmpErr);
		}

		if (pluggable_fieldbus_manager_macVector[i].handler != NULL ) {
			err |= dlclose(pluggable_fieldbus_manager_macVector[i].handler);
		}
	}

	free(pluggable_fieldbus_manager_macVector);
	pluggable_fieldbus_manager_macVector = NULL;

	if (err != 0) {
		logging_adapter_info("Can't successfully unload one or more modules.");
		return (
				lastErr == COMMON_TYPE_SUCCESS ? COMMON_TYPE_ERR_LOAD_MODULE : lastErr);
	}
	return COMMON_TYPE_SUCCESS;
}
