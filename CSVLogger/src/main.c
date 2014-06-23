/**
 * @file main.c
 * @brief Parses the configuration file and generates the logging output
 * @details First it parses the program arguments and loads the main
 * configuration. After loading the configuration it loads needed modules and
 * conducts fetching the data. Afterwards the output will be written.
 * @author Michael Spiegel, michael.h.spiegel@gmail.com
 */

#include "pluggable-fieldbus-manager.h"
#include <logging-adapter.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>
#include <unistd.h>

#ifndef DEF_CONFIG
/** @brief The name of the default configuration file location */
#define DEF_CONFIG ("etc/log2csv.cnf")
#endif

/**
 * @brief If this macro is defined the error message will be printed to stderr
 * also
 */
#define OUTPUT_ERROR_DIRECTLY

/* Exit code definitions */
#define EXIT_ERR_PROGOPTS (1)
#define EXIT_ERR_CONFIG (2)
#define EXIT_ERR_LOGGING (3)
#define EXIT_ERR_NETWORK (4)

/* Configuration directive names */
#define MAIN_CONFIG_CHANNEL "channel"
#define MAIN_CONFIG_TITLE "title"

/** @brief Entry used to form the list of channels to query */
typedef struct {
	/** @brief The identifier given by the network stack */
	int channelID;
	/**
	 * @brief The title describing the channel.
	 * @details The reference points to a location inside the configuration
	 * structure. No manual deallocation of the used memory is needed.
	 */
	const char* title;
} main_channels_t;

/** @brief The list of channels to query */
static main_channels_t *main_channelVector;
/** @brief The number of channels to query */
static int main_channelVectorLength;

/**
 * @brief The parsed program options
 */
struct {
	/**
	 * @brief Flag indicating that the configuration file name was previously set
	 */
	unsigned int configNameSet :1;
	/** @brief Flag indicating that the help switch was given */
	unsigned int help :1;
	/** @brief The name of the configuration file */
	char* configName;
	/** @brief The program's name */
	char* progname;
} main_progOpt = { .configName = DEF_CONFIG, .progname = "log2csv" };

/** @brief The global configuration */
config_t main_config;

/* Function prototypes */
static void main_bailOut(const int err, const char* formatString, ...);
static void main_parseProgOpts(int argc, char** argv);
static void main_printHelp(void);
static void main_freeResources(void);
static inline void main_initConfig(void);
static inline void main_initNetwork(void);
static inline void main_addChannel(unsigned int index,
		config_setting_t *config);

/**
 * @brief Main program entry
 * @param argc The number of passed arguments including the program's name
 * @param argv The zero terminated argument vector
 * @return 0 on success, a positive value otherwise
 */
int main(int argc, char** argv) {

	if (argc > 0 && argv != NULL && argv[0] != NULL ) {
		main_progOpt.progname = argv[0];
	}
	if (!logging_adapter_init(main_progOpt.progname)) {
		return EXIT_ERR_LOGGING;
	}

	if (argc < 1 || argv == NULL || argv[0] == NULL ) {
		main_bailOut(EXIT_ERR_PROGOPTS, "Invalid program argument vector");
	}

	main_parseProgOpts(argc, argv);

	if (main_progOpt.help) {
		main_printHelp();
		main_freeResources();
		exit(EXIT_SUCCESS);
	}
	main_initConfig();
	main_initNetwork();

	logging_adapter_info("Successfully finished");
	main_freeResources();
	return EXIT_SUCCESS;
}

/**
 * @brief Initialized global network related variables and loads the network
 * stack.
 * @details It assumes that the configuration was successfully loaded and that
 * the program options are parsed. It will bail out if an error occurs.
 */
static inline void main_initNetwork() {
	common_type_error_t err;
	config_setting_t *channelConfig;
	unsigned int i;

	err = pfm_init(config_root_setting(&main_config));
	if (err != COMMON_TYPE_SUCCESS) {
		main_bailOut(EXIT_ERR_NETWORK, "Can't initialize the network stack "
				"(err-code: %d)", err);
	}

	channelConfig = config_lookup(&main_config, MAIN_CONFIG_CHANNEL);
	if (channelConfig == NULL ) {
		main_bailOut(EXIT_ERR_CONFIG, "Can't find the list of channels \"%s\"",
				MAIN_CONFIG_CHANNEL);
	}
	if (!config_setting_is_list(channelConfig)) {
		main_bailOut(EXIT_ERR_CONFIG, "The \"%s\" directive isn't a list",
				MAIN_CONFIG_CHANNEL);

	}
	main_channelVectorLength = config_setting_length(channelConfig);
	assert(main_channelVectorLength >= 0);
	main_channelVector = malloc(
			main_channelVectorLength * sizeof(main_channelVector[0]));
	if (main_channelVector == NULL ) {
		main_bailOut(EXIT_FAILURE, "Not enough memory available");
	}

	for (i = 0; i < main_channelVectorLength; i++) {
		main_addChannel(i, config_setting_get_elem(channelConfig, i));
	}

}

/**
 * @brief Adds the given channel and sets it within the list of configured
 * channels
 * @details The function will bail out if an error is detected.
 * @param index the index of the channel within the channelVector
 * @param config The configuration of the channel, not null
 */
static inline void main_addChannel(unsigned int index, config_setting_t *config) {
	const char* title = NULL;

	assert(config != NULL);
	assert(index < main_channelVectorLength);

	if (!config_setting_is_group(config)) {
		main_bailOut(EXIT_ERR_CONFIG,
				"The entry nr. %d of the \"%s\" configuration "
						"directive isn't a group", index + 1, MAIN_CONFIG_CHANNEL);
	}

	if (!config_setting_lookup_string(config, MAIN_CONFIG_TITLE, &title)) {
		main_bailOut(EXIT_ERR_CONFIG, "The entry nr. %d of the \"%s\" directive "
				"doesn't contain a \"%s\" string directive", index + 1,
				MAIN_CONFIG_CHANNEL, MAIN_CONFIG_TITLE);
	}
	assert(title != NULL);

	main_channelVector[index].title = title;
	main_channelVector[index].channelID = pfm_addChannel(config);

	if(main_channelVector[index].channelID < 0){
		main_bailOut(EXIT_ERR_NETWORK, "Can't register the channel within the "
				"network stack.");
	}
	logging_adapter_debug("Channel \"%s\" successfully added", title);
}

/**
 * @brief Initializes the globally available configuration
 * @details It assumes that the program options are successfully parsed and
 * that the function wasn't called before. It will bail out if something
 * terrible happens.
 */
static inline void main_initConfig(void) {
	config_init(&main_config);
	if (!config_read_file(&main_config, main_progOpt.configName)) {
		main_bailOut(EXIT_ERR_CONFIG, "Couldn't parse configuration file \"%s\" "
				"(line: %d): %s", main_progOpt.configName,
				config_error_line(&main_config), config_error_text(&main_config));
	}
}

/**
 * @brief Parses the given program options and populates the global main_progOpt
 * structure.
 * @details It is expected that the progname reference was set before and that
 * the logging facilities are properly initializes.
 * @param argc The number op passed arguments
 * @param argv The argument vector
 */
static void main_parseProgOpts(int argc, char** argv) {
	int nextOpt;

	while ((nextOpt = getopt(argc, argv, "c:h")) > 0) {
		switch (nextOpt) {
		case 'c':
			if (main_progOpt.configNameSet) {
				main_bailOut(EXIT_ERR_PROGOPTS, "The c option was previously set");
			}
			main_progOpt.configNameSet = 1;
			main_progOpt.configName = optarg;
			break;
		case 'h':
			main_progOpt.help = 1;
			break;
		case '?':
			main_bailOut(EXIT_ERR_PROGOPTS, "Invalid option '%c'", (char) optopt);
			break;
		default:
			assert(0);
		}
	}

	if (optind < argc) {
		main_bailOut(EXIT_ERR_PROGOPTS, "%i additional arguments found but none "
				"expected", argc - optind);
	}

}

/**
 * @brief Prints a simple help message
 * @details The output is written to stdout
 */
static void main_printHelp(void) {
	(void) printf("Usage:\n");
	(void) printf("  %s [-c <file>] [-h]\n\n", main_progOpt.progname);
	(void) printf("  -c <file>    Reads the configuration <file> instead of "
			"\"%s\"\n\n", DEF_CONFIG);
	(void) printf("Reads the values from the fieldbus nodes configured and "
			"appends them to a \n");
	(void) printf("specified log file in a CSV format\n");
}

/**
 * @brief Frees globally allocated resources.
 * @details The function has to be called before exiting the program or before
 * completely reinitializing it.
 */
static void main_freeResources(void) {
	common_type_error_t err;

	free(main_channelVector);

	err = pfm_free();
	if (err != COMMON_TYPE_SUCCESS) {
		logging_adapter_error("Can't free the network stack. (error-code: %d)",
				(int) err);
	}

	config_destroy(&main_config);
	logging_adapter_freeResources();
}

/**
 * @brief Logs the error message included in format string and exits
 * @details The format string has to be a printf-styled string followed by the
 * corresponding number of arguments. If the errno variable is set to a non zero
 * value the corresponding message will also be logged. The function will write
 * to the syslog facility in CRIT and ERR level.
 * If OUTPUT_ERROR_DIRECTLY is defined the error message will be printed to
 * stderr
 * @param err The application's exit code. It has to be greater than zero.
 * @param formatString The error message without any "ERROR:" - tag.
 */
static void main_bailOut(const int err, const char* formatString, ...) {
	int savedErrno = errno;
	va_list varArg;

	assert(err > 0);
	assert(formatString != NULL);

	va_start(varArg, formatString);
	logging_adapter_errorNo(savedErrno, formatString, varArg);
	va_end(varArg);

	main_freeResources();
	exit(err);

}
