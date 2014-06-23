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
#define PFM_CONFIG_MAC "mac"
#define PFM_CONFIG_NAME "name"
#define PFM_CONFIG_TYPE "type"
#define PFM_CONFIG_ADDRESS "address"

/** @brief Structure encapsulating the MAC module's data*/
typedef struct {
	/** @brief The handler of the library returned by dlopen */
	void * handler;
	/** The sync function pointer of the module */
	fieldbus_mac_sync_t sync;
	/** The free function pointer of the module */
	fieldbus_mac_free_t free;
} pfm_mac_t;

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
} pfm_app_t;

/** @brief Structure defining a single data channel */
typedef struct {
	/** @brief The configuration snippet defining the address */
	config_setting_t *address;
	/** @brief The associated application layer module */
	pfm_app_t* app;
} pfm_channel_t;

/** @brief The size of the MAC module vector*/
static unsigned int pfm_macVectorLength = 0;
/** @brief The vector containing loaded MAC module handler */
static pfm_mac_t *pfm_macVector;

/** @brief The number of loaded application modules */
static unsigned int pfm_appVectorLength = 0;
/** @brief The vector of loaded application modules */
static pfm_app_t * pfm_appVector = NULL;

/** @brief The number of available channels */
static unsigned int pfm_channelVectorLength = 0;
/** @brief The vector containing every initialized channel */
static pfm_channel_t *pfm_channelVector = NULL;

/* Function prototypes */
static inline common_type_error_t pfm_installMacModule(
		config_setting_t *modConfig, const unsigned int index);
static inline common_type_error_t pfm_freeMac(void);
static inline common_type_error_t pfm_freeAppModules(void);
static pfm_app_t * pfm_getAppModule(const char* driverName);
static pfm_app_t * pfm_loadAppModule(const char* name);
static void pfm_appVectorRollback(void);
static inline fieldbus_application_init_t pfm_lookupAppInterfaceFunctions(
		pfm_app_t *app);
static inline int pfm_newChannel(pfm_app_t *app, config_setting_t *address);

/**
 * @brief Extracts the module's names and loads them.
 * @param configuration The root configuration containing the mac member. The
 * root configuration has to be a group setting always.
 * @return The status of the operation
 */
common_type_error_t pfm_init(config_setting_t* configuration) {
	config_setting_t *mac;
	common_type_error_t err;
	int i;

	assert(config_setting_is_group(configuration));

	// Fetch and check the mac module configuration sections
	mac = config_setting_get_member(configuration, PFM_CONFIG_MAC);
	if (mac == NULL ) {
		logging_adapter_info("Can't locate the \"%s\" list directive.",
				PFM_CONFIG_MAC);
		return COMMON_TYPE_ERR_CONFIG;
	}
	if (!config_setting_is_list(mac)) {
		logging_adapter_info("The \"%s\" directive isn't a list.", PFM_CONFIG_MAC);
		return COMMON_TYPE_ERR_CONFIG;
	}

	// Allocate the mac module vector.
	pfm_macVectorLength = config_setting_length(mac);
	assert(pfm_macVectorLength >= 0);
	pfm_macVector = malloc(pfm_macVectorLength * sizeof(pfm_macVector[0]));
	if (pfm_macVector == NULL ) {
		return COMMON_TYPE_ERR;
	}
	memset(pfm_macVector, 0, pfm_macVectorLength * sizeof(pfm_macVector[0]));

	// Load the mac modules
	for (i = 0; i < pfm_macVectorLength; i++) {
		err = pfm_installMacModule(config_setting_get_elem(mac, i), i);
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
static inline common_type_error_t pfm_installMacModule(
		config_setting_t *modConfig, const unsigned int index) {
	const char* name = "";
	char* errStr;
	common_type_error_t err;
	fieldbus_mac_init_t init;

	assert(modConfig != NULL);
	assert(index < pfm_macVectorLength);

	if (!config_setting_is_group(modConfig)) {
		logging_adapter_info("The \"%s\" directive contains an invalid list entry",
				PFM_CONFIG_MAC);
		return COMMON_TYPE_ERR_CONFIG;
	}
	if (!config_setting_lookup_string(modConfig, PFM_CONFIG_NAME, &name)) {
		logging_adapter_info(
				"Can't find the \"%s\" string directive inside the MAC "
						"module directive", PFM_CONFIG_NAME);
		return COMMON_TYPE_ERR_CONFIG;
	}
	assert(name != NULL);

	logging_adapter_debug("Try to load MAC module \"%s\"", name);

	pfm_macVector[index].handler = dlopen(name, RTLD_NOW | RTLD_GLOBAL);
	if (pfm_macVector[index].handler == NULL ) {
		logging_adapter_info("Can't load \"%s\": %s", name, dlerror());
		return COMMON_TYPE_ERR_LOAD_MODULE;
	}

	// Load end execute init function
	(void) dlerror();
	init = (fieldbus_mac_init_t) dlsym(pfm_macVector[index].handler,
			"fieldbus_mac_init");
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
	pfm_macVector[index].sync = (fieldbus_mac_sync_t) dlsym(
			pfm_macVector[index].handler, FIELDBUS_MAC_SYNC_NAME);
	errStr = dlerror();
	if (errStr != NULL ) {
		logging_adapter_info("Can't successfully load the \"%s\" "
				"function: %s", FIELDBUS_MAC_SYNC_NAME, errStr);
		return COMMON_TYPE_ERR_LOAD_MODULE;
	}

	(void) dlerror();
	pfm_macVector[index].free = (fieldbus_mac_free_t) dlsym(
			pfm_macVector[index].handler, FIELDBUS_MAC_FREE_NAME);
	errStr = dlerror();
	if (errStr != NULL ) {
		logging_adapter_info("Can't successfully load the \"%s\" "
				"function: %s", FIELDBUS_MAC_FREE_NAME, errStr);
		return COMMON_TYPE_ERR_LOAD_MODULE;
	}

	assert(pfm_macVector[index].free != NULL);
	assert(pfm_macVector[index].sync != NULL);

	return COMMON_TYPE_SUCCESS;
}

/**
 * @details Assumes that the channelConf reference isn't null.
 */
int pfm_addChannel(config_setting_t* channelConf) {
	const char* driver = "";
	config_setting_t *address;
	pfm_app_t *appModule = NULL;

	assert(channelConf != NULL);

	if (!config_setting_is_group(channelConf)) {
		logging_adapter_info("The given channel configuration isn't a valid group "
				"of directives");
		return -1;
	}

	if (!config_setting_lookup_string(channelConf, PFM_CONFIG_TYPE, &driver)) {
		logging_adapter_info("Can't load the \"%s\" string configuration "
				"directive.", PFM_CONFIG_TYPE);
		return -1;
	}

	address = config_setting_get_member(channelConf, PFM_CONFIG_ADDRESS);
	if (address == NULL ) {
		logging_adapter_info("Can't obtain the \"%s\" channel configuration's "
				"\"%s\" member", driver, PFM_CONFIG_ADDRESS);
		return -1;
	}

	// obtain the device driver
	appModule = pfm_getAppModule(driver);
	if (appModule == NULL ) {
		logging_adapter_debug("Try to load application module \"%s\"", driver);
		appModule = pfm_loadAppModule(driver);
	}
	if (appModule == NULL ) {
		return -1;
	}

	return pfm_newChannel(appModule, address);
}

/**
 * @brief Adds a new channel to the list of known channels and returns it's id
 * @details If the function is unable to obtain memory -1 is returned.
 * @param app A valid reference to the previously loaded app module
 * @param address The channel's address configuration
 * @return The index of the newly created channel
 */
static inline int pfm_newChannel(pfm_app_t *app, config_setting_t *address) {
	unsigned int index = pfm_channelVectorLength;
	pfm_channel_t * oldVector = pfm_channelVector;

	assert(app != NULL);
	assert(address != NULL);

	pfm_channelVector = realloc(pfm_channelVector,
			(pfm_channelVectorLength + 1) * sizeof(pfm_channelVector[0]));
	if (pfm_channelVector == NULL ) {
		pfm_channelVector = oldVector;
		logging_adapter_info("Can't obtain more memory");
		return -1;
	}

	pfm_channelVectorLength++;

	pfm_channelVector[index].address = address;
	pfm_channelVector[index].app = app;

	return index;
}

/**
 * @brief Loads and initializes the application module and adds it to the global
 * list.
 * @details It assumes that the name used to lookup the shared library module
 * isn't null.
 * @param name The name or path of the application layer module
 * @return A reference to the newly created list entry or null.
 */
static pfm_app_t * pfm_loadAppModule(const char* name) {
	pfm_app_t *oldVector = pfm_appVector;
	pfm_app_t *ret;
	fieldbus_application_init_t init;
	common_type_error_t err;

	assert(name != NULL);

	pfm_appVector = realloc(pfm_appVector,
			(pfm_appVectorLength + 1) * sizeof(pfm_appVector[0]));
	if (pfm_appVector == NULL ) {
		pfm_appVector = oldVector;
		logging_adapter_info("Can't obtain more memory");
		return NULL ;
	}
	pfm_appVectorLength++;
	ret = &pfm_appVector[pfm_appVectorLength - 1];
	memset(ret, 0, sizeof(ret[0]));

	ret->name = name;
	ret->handler = dlopen(name, RTLD_NOW);
	if (ret->handler == NULL ) {
		pfm_appVectorRollback();
		logging_adapter_info("Can't load application module \"%s\": %s", name,
				dlerror());
		return NULL ;
	}

	init = pfm_lookupAppInterfaceFunctions(ret);
	if (init == NULL ) {
		pfm_appVectorRollback();
		return NULL ;
	}

	err = init();
	if (err != COMMON_TYPE_SUCCESS) {
		pfm_appVectorRollback();
		logging_adapter_info("Can't initialize the \"%s\" module (err-no: %d)",
				name, (int) err);
		return NULL ;
	}

	return ret;
}

/**
 * @brief Tries to lookup the interface functions
 * @details Sets the functions within the given structure and returns the init
 * function. If the function is unable to obtain one or more interface functions
 * it will return NULL and log an appropriate error message. It assumes that the
 * given app reference is valid and that the handler as well as the name
 * contained is properly set. The function won't perform a rollback operation
 * taking the invalid application layer from the appVector.
 * @param app The reference to the app structure to manipulate
 * @return The init function or null.
 */
static inline fieldbus_application_init_t pfm_lookupAppInterfaceFunctions(
		pfm_app_t *app) {
	fieldbus_application_init_t ret = NULL;
	char* errStr;

	assert(app != NULL);
	assert(app->handler != NULL);
	assert(app->name != NULL);

	(void) dlerror();
	ret = (fieldbus_application_init_t) dlsym(app->handler,
			FIELDBUS_APPLICATION_INIT_NAME);
	errStr = dlerror();
	if (errStr != NULL ) {
		logging_adapter_info("Can't load the \"%s\" function of one fieldbus "
				"application module \"%s\": %s", FIELDBUS_APPLICATION_INIT_NAME,
				app->name, errStr);
		return NULL ;
	}

	(void) dlerror();
	app->fetchValue = (fieldbus_application_fetchValue_t) dlsym(app->handler,
			FIELDBUS_APPLICATION_FETCH_VALUE_NAME);
	errStr = dlerror();
	if (errStr != NULL ) {
		logging_adapter_info("Can't load the \"%s\" function of one fieldbus "
				"application module \"%s\": %s", FIELDBUS_APPLICATION_FETCH_VALUE_NAME,
				app->name, errStr);
		return NULL ;
	}

	(void) dlerror();
	app->free = (fieldbus_application_free_t) dlsym(app->handler,
			FIELDBUS_APPLICATION_FREE_NAME);
	errStr = dlerror();
	if (errStr != NULL ) {
		logging_adapter_info("Can't load the \"%s\" function of one fieldbus "
				"application module \"%s\": %s", FIELDBUS_APPLICATION_FREE_NAME,
				app->name, errStr);
		return NULL ;
	}

	(void) dlerror();
	app->sync = (fieldbus_application_sync_t) dlsym(app->handler,
			FIELDBUS_APPLICATION_SYNC_NAME);
	errStr = dlerror();
	if (errStr != NULL ) {
		logging_adapter_info("Can't load the \"%s\" function of one fieldbus "
				"application module \"%s\": %s", FIELDBUS_APPLICATION_SYNC_NAME,
				app->name, errStr);
		return NULL ;
	}

	return ret;
}

/**
 * @brief Fetches the application layer module with the given name.
 * @details It assumes that the given driverName reference isn't null. If the
 * module isn't present null will be returned.
 * @param driverName The name of the application layer module
 * @return The module list entry or null
 */
static pfm_app_t* pfm_getAppModule(const char* driverName) {
	unsigned int i;
	assert(driverName != NULL);
	assert(pfm_appVector == NULL || pfm_appVectorLength > 0);

	for (i = 0; i < pfm_appVectorLength; i++) {
		assert(pfm_appVector[i].name != NULL);
		if (strcmp(driverName, pfm_appVector[i].name) == 0) {
			return &pfm_appVector[i];
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
static void pfm_appVectorRollback(void) {
	assert(pfm_appVectorLength > 0);
	pfm_app_t * oldVector = pfm_appVector;
	pfm_appVectorLength--;
	pfm_appVector = realloc(pfm_appVector,
			pfm_appVectorLength * sizeof(oldVector[0]));
	if (pfm_appVector == NULL ) {
		pfm_appVector = oldVector;
	}
}

common_type_error_t pfm_sync() {
	return COMMON_TYPE_ERR;
}

common_type_t pfm_fetchValue(int id) {
	common_type_t ret;
	ret.data.errVal = COMMON_TYPE_ERR;
	ret.type = COMMON_TYPE_ERROR;
	return ret;
}

common_type_error_t pfm_free() {
	common_type_error_t err = COMMON_TYPE_SUCCESS, tmpErr;

	if (pfm_macVector != NULL ) {
		err = pfm_freeAppModules();
	}

	free(pfm_channelVector);
	pfm_channelVector = NULL;
	pfm_channelVectorLength = 0;

	if (pfm_macVector != NULL ) {
		tmpErr = pfm_freeMac();
		err = (tmpErr == COMMON_TYPE_SUCCESS ? err : tmpErr);
	}

	return err;
}

/**
 * @brief Calls the module's free function and deallocates used memory
 * @details The function won't stop immediately if an error occurs. Instead it
 * tries to deallocate as many resources as possible to avoid memory leaks. If
 * multiple errors occur the last error code will be returned.
 * @return The status of the operation
 */
static inline common_type_error_t pfm_freeAppModules() {
	unsigned int i;
	int err = 0;
	common_type_error_t tmpErr, lastErr = COMMON_TYPE_SUCCESS;

	assert(pfm_appVector != NULL || pfm_appVectorLength == 0);

	for (i = 0; i < pfm_appVectorLength; i++) {
		if (pfm_appVector[i].free != NULL ) {
			tmpErr = pfm_appVector[i].free();
			lastErr = (tmpErr == COMMON_TYPE_SUCCESS ? lastErr : tmpErr);
			err |= (tmpErr == COMMON_TYPE_SUCCESS ? 0 : 1);
		}
		if (pfm_appVector[i].handler != NULL ) {
			err |= dlclose(pfm_appVector[i].handler);
		}
	}

	free(pfm_appVector);
	pfm_appVector = NULL;
	pfm_appVectorLength = 0;

	if (err != 0) {
		logging_adapter_info("Can't successfully unload one or more modules.");
		return (
				lastErr == COMMON_TYPE_SUCCESS ? COMMON_TYPE_ERR_LOAD_MODULE : lastErr);
	}
	return COMMON_TYPE_SUCCESS;
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
static inline common_type_error_t pfm_freeMac() {
	unsigned int i;
	int err;
	common_type_error_t lastErr = COMMON_TYPE_SUCCESS, tmpErr;

	assert(pfm_macVector != NULL || pfm_macVectorLength == 0);

	err = 0;
	for (i = 0; i < pfm_macVectorLength; i++) {

		if (pfm_macVector[i].free != NULL ) {
			tmpErr = pfm_macVector[i].free();
			err |= (tmpErr == COMMON_TYPE_SUCCESS ? 0 : 1);
			lastErr = (tmpErr == COMMON_TYPE_SUCCESS ? lastErr : tmpErr);
		}

		if (pfm_macVector[i].handler != NULL ) {
			err |= dlclose(pfm_macVector[i].handler);
		}
	}

	free(pfm_macVector);
	pfm_macVector = NULL;
	pfm_macVectorLength = 0;

	if (err != 0) {
		logging_adapter_info("Can't successfully unload one or more modules.");
		return (
				lastErr == COMMON_TYPE_SUCCESS ? COMMON_TYPE_ERR_LOAD_MODULE : lastErr);
	}
	return COMMON_TYPE_SUCCESS;
}
