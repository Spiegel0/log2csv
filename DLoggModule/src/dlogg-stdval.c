/**
 * @file dlogg-stdval.c
 * @brief Module fetching a previously buffered value.
 * @details The value has to be represented by the TA-standard encoding.
 * @author Michael Spiegel, michael.h.spiegel@gmail.com
 */

#include <fieldbus-application.h>
#include <logging-adapter.h>
#include "dlogg-current-data.h"

#include <assert.h>
#include <string.h>

/* The configuration directive names used */
#define DLOGG_STDVAL_CONFIG_CONTROLLER "controller"
#define DLOGG_STDVAL_CONFIG_LINE "line_id"
#define DLOGG_STDVAL_CONFIG_CHN_PREFIX "channel_prefix"
#define DLOGG_STDVAL_CONFIG_CHN_NR "channel_number"

/* The prefix configuration keys */
#define DLOGG_STDVAL_CONFIG_PRE_S "S"
#define DLOGG_STDVAL_CONFIG_PRE_E "E"
#define DLOGG_STDVAL_CONFIG_PRE_A "A"
#define DLOGG_STDVAL_CONFIG_PRE_AD "AD"
#define DLOGG_STDVAL_CONFIG_PRE_AO "AO"
#define DLOGG_STDVAL_CONFIG_PRE_WMZP "WMZP"
#define DLOGG_STDVAL_CONFIG_PRE_WMZE "WMZE"


/** @brief Defines possible prefix values */
typedef enum {
	DLOGG_STDVAL_PRE_S = 0,
	DLOGG_STDVAL_PRE_E,
	DLOGG_STDVAL_PRE_A,
	DLOGG_STDVAL_PRE_AD,
	DLOGG_STDVAL_PRE_AO,
	DLOGG_STDVAL_PRE_WMZP,
	DLOGG_STDVAL_PRE_WMZE
} dlogg_stdval_prefix_t;

/** @brief Structure encapsulating the user's request */
typedef struct {
	/** @brief An internal prefix ID */
	dlogg_stdval_prefix_t prefixID;
	/** @brief The current line multiplexing channel */
	uint8_t lineID;
	/** @brief The channel number starting at zero */
	uint8_t channelID;
	/** @brief The d-logg's input channel starting at zero */
	unsigned controllerID :1;
} dlogg_stdval_addr_t;

/* Function Prototypes */
static inline common_type_error_t dlogg_stdval_parseAddress(
		dlogg_stdval_addr_t* addr, config_setting_t *addressConfig);
static inline common_type_error_t dlogg_stdval_getPrefixID(
		dlogg_stdval_prefix_t* prefix, const char* confVal);


common_type_error_t fieldbus_application_init(void) {
	// Nothing to be done
	return COMMON_TYPE_SUCCESS;
}

common_type_error_t fieldbus_application_sync(void) {
	// Nothing to be done
	return COMMON_TYPE_SUCCESS;
}

common_type_t fieldbus_application_fetchValue(config_setting_t *address) {
	common_type_t ret;
	dlogg_stdval_addr_t addr;

	assert(address != NULL);

	ret.type = COMMON_TYPE_ERROR;
	ret.data.errVal = COMMON_TYPE_ERR_CONFIG;

	ret.data.errVal = dlogg_stdval_parseAddress(&addr, address);
	if(ret.data.errVal != COMMON_TYPE_SUCCESS)
		return ret;


	return ret;
}

/**
 * @brief Parses the given configuration structure
 * @details The parameters set will be stored into the address structure but
 * availability of addresses will remain unchecked.
 * @param addr The address structure to store the results
 * @param addressConfig The configuration to read
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_stdval_parseAddress(
		dlogg_stdval_addr_t * addr, config_setting_t *addressConfig) {
	common_type_error_t err;
	long int lineID = 0, channel, controller = 1;
	const char * prefix;

	assert(addressConfig != NULL);
	assert(addr != NULL);

	if (!config_setting_is_group(addressConfig)) {
		logging_adapter_info("The address setting is not a group directive");
		return COMMON_TYPE_ERR_CONFIG;
	}

	// Use default value, if not set
	config_setting_lookup_int(addressConfig, DLOGG_STDVAL_CONFIG_LINE, &lineID);

	if (lineID < 0 || lineID > 255) {
		logging_adapter_info("Value of %s, %n out of range [0,255]",
				DLOGG_STDVAL_CONFIG_LINE, (int) lineID);
		return COMMON_TYPE_ERR_CONFIG;
	}

	if (!config_setting_lookup_int(addressConfig, DLOGG_STDVAL_CONFIG_CHN_NR,
			&channel)) {
		logging_adapter_info("Cant find the \"%s\" directive within the address "
				"group", DLOGG_STDVAL_CONFIG_CHN_NR);
		return COMMON_TYPE_ERR_CONFIG;
	}

	if (channel < 1 || channel > 256) {
		logging_adapter_info("Value of %s, %n out of range [1,256]",
				DLOGG_STDVAL_CONFIG_CHN_NR, (int) channel);
		return COMMON_TYPE_ERR_CONFIG;
	}

	// Use default value, if not set
	config_setting_lookup_int(addressConfig, DLOGG_STDVAL_CONFIG_CONTROLLER,
			&controller);

	if (controller < 1 || controller > 2) {
		logging_adapter_info("Value of %s, %n out of range [1,2]",
				DLOGG_STDVAL_CONFIG_CONTROLLER, (int) controller);
		return COMMON_TYPE_ERR_CONFIG;
	}

	if (!config_setting_lookup_string(addressConfig,
			DLOGG_STDVAL_CONFIG_CHN_PREFIX, &prefix)) {
		logging_adapter_info("Cant find the \"%s\" directive within the address "
				"group", DLOGG_STDVAL_CONFIG_CHN_PREFIX);
		return COMMON_TYPE_ERR_CONFIG;
	}

	err = dlogg_stdval_getPrefixID(&addr->prefixID, prefix);
	if(err != COMMON_TYPE_SUCCESS)
		return err;

	addr->lineID = lineID;
	addr->channelID = channel - 1;
	addr->controllerID = controller - 1;
	return COMMON_TYPE_SUCCESS;
}

/**
 * @brief Parses the given prefix configuration value
 * @details If the prefix isn't known, the prefix variable remains unchanged
 * @param prefix The location to store the prefix
 * @param confVal The configured prefix
 * @return The status of the operation
 */
static inline common_type_error_t dlogg_stdval_getPrefixID(
		dlogg_stdval_prefix_t* prefix, const char* confVal) {

	assert(prefix != NULL);
	assert(confVal != NULL);

	if(strcmp(DLOGG_STDVAL_CONFIG_PRE_A, confVal) == 0){
		*prefix = DLOGG_STDVAL_PRE_A;
	}else if(strcmp(DLOGG_STDVAL_CONFIG_PRE_AD, confVal) == 0){
		*prefix = DLOGG_STDVAL_PRE_AD;
	}else if(strcmp(DLOGG_STDVAL_CONFIG_PRE_AO, confVal) == 0){
		*prefix = DLOGG_STDVAL_PRE_AO;
	}else if(strcmp(DLOGG_STDVAL_CONFIG_PRE_E, confVal) == 0){
		*prefix = DLOGG_STDVAL_PRE_E;
	}else if(strcmp(DLOGG_STDVAL_CONFIG_PRE_S, confVal) == 0){
		*prefix = DLOGG_STDVAL_PRE_S;
	}else if(strcmp(DLOGG_STDVAL_CONFIG_PRE_WMZE, confVal) == 0){
		*prefix = DLOGG_STDVAL_PRE_WMZE;
	}else if(strcmp(DLOGG_STDVAL_CONFIG_PRE_WMZP, confVal) == 0){
		*prefix = DLOGG_STDVAL_PRE_WMZP;
	}else{
		logging_adapter_info("Unknown Channel prefix %s", confVal);
		return COMMON_TYPE_ERR_CONFIG;
	}
	return COMMON_TYPE_SUCCESS;
}

common_type_error_t fieldbus_application_free(void) {
	// Nothing to be done
	return COMMON_TYPE_SUCCESS;
}
