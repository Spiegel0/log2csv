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
#include <sys/time.h>
#include <time.h>

#ifndef DEF_CONFIG
/** @brief The name of the default configuration file location */
#define DEF_CONFIG ("/etc/log2csv.cnf")
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
#define EXIT_ERR_OUTFILE (5)
#define EXIT_ERR_LOCAL_SYS (6)

/* Configuration directive names */
#define MAIN_CONFIG_CHANNEL "channel"
#define MAIN_CONFIG_TITLE "title"
#define MAIN_CONFIG_OUT_FILE "outFile"
#define MAIN_CONFIG_TIME_FORMAT "timeFormat"
#define MAIN_CONFIG_TIME_HEADER "timeHeader"

/** @brief The column separator used within the CSV file */
#define MAIN_CSV_SEP ";"
/** @brief The newline sequence used within the CSV file */
#define MAIN_CSV_NEWLINE "\n"
/** @brief The error sequence used if a value can't be obtained */
#define MAIN_CSV_ERR "NaN"

/** @brief The size of the buffer used to store time stamps */
#define MAIN_TIMESTAMP_BUFFER_SIZE 40

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

/** @brief The file handler of the stream writing the CSV file */
FILE* main_csvOut = NULL;

/* Function prototypes */
static void main_bailOut(const int err, const char* formatString, ...);
static void main_parseProgOpts(int argc, char** argv);
static void main_printHelp(void);
static void main_freeResources(void);
static inline void main_initConfig(void);
static inline void main_initNetwork(void);
static inline void main_initOutputFile(void);
static inline void main_addChannel(unsigned int index, config_setting_t *config);
static void main_writeCSVHeader(void);
static void main_processSamples(void);
static void main_appendString(FILE* file, const char* str);
static void main_appendResult(FILE *file, const common_type_t *result);
static void main_appendTimestamp(FILE *file, struct timeval *tv);

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
	main_initOutputFile();

	main_processSamples();

	logging_adapter_info("Successfully finished");
	main_freeResources();
	return EXIT_SUCCESS;
}

/**
 * @brief Opens the output file and eventually writes the first line
 * @details The function assumes that the configuration was previously
 * initialized and that the channelVector is fully populated. It will bail out
 * if an error occurs.
 */
static inline void main_initOutputFile() {
	const char* filename;
	int writeHeader;

	if (!config_lookup_string(&main_config, MAIN_CONFIG_OUT_FILE, &filename)) {
		main_bailOut(EXIT_ERR_CONFIG, "Can't fine the \"%s\" string configuration "
				"directive.", MAIN_CONFIG_OUT_FILE);
	}

	writeHeader = access(filename, F_OK);
	errno = 0;

	main_csvOut = fopen(filename, "a");
	if (main_csvOut == NULL ) {
		main_bailOut(EXIT_ERR_OUTFILE, "Can't open the file \"%s\" to append data",
				filename);
	}

	if (writeHeader) {
		logging_adapter_debug("File \"%s\" doens't exist. Try to create it and "
				"write a headline", filename);
		main_writeCSVHeader();
	}

}

/**
 * @brief Writes the CSVHeader to the previously opened csvOut file.
 * @details <p>It assumes that the csvOut file as well as the configuration is
 * initialized and that the channelVector is fully populated. The function will
 * bail out if an error occurred.</p>
 * <p>The first column will be the time stamp header. The field is taken from
 * the configuration. If no configuration setting is present a default value
 * will be used.</p>
 */
static void main_writeCSVHeader() {
	unsigned int i;
	size_t len;
	const char* timeHeader = "Current Time/Date";

	assert(main_csvOut != NULL);

	(void) config_lookup_string(&main_config, MAIN_CONFIG_TIME_HEADER,
			&timeHeader);

	main_appendString(main_csvOut, timeHeader);
	len = strlen(MAIN_CSV_SEP);
	if (fwrite(MAIN_CSV_SEP, sizeof(char), len, main_csvOut) != len) {
		main_bailOut(EXIT_ERR_OUTFILE, "Can't write all \"%d\" bytes to the "
				"CSV-file", len);
	}

	for (i = 0; i < main_channelVectorLength; i++) {

		main_appendString(main_csvOut, main_channelVector[i].title);

		if (i + 1 != main_channelVectorLength) {
			len = strlen(MAIN_CSV_SEP);
			if (fwrite(MAIN_CSV_SEP, sizeof(char), len, main_csvOut) != len) {
				main_bailOut(EXIT_ERR_OUTFILE, "Can't write all \"%d\" bytes to the "
						"CSV-file", len);
			}
		}
	}

	len = strlen(MAIN_CSV_NEWLINE);
	if (fwrite(MAIN_CSV_NEWLINE, sizeof(char), len, main_csvOut) != len) {
		main_bailOut(EXIT_ERR_OUTFILE, "Can't write all \"%d\" bytes to the "
				"CSV-file", len);
	}

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

	if (main_channelVector[index].channelID < 0) {
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
 * @brief Fetches the values from the configured channels and writes them to the
 * previously opened CSV file.
 * @details The network stack needs to be initialized but the function will call
 * the sync function on it's own. If something went wrong during printing or
 * synchronizing the function will bail out. If a value can't be fetched
 * correctly a place holder value will be inserted into the CSV file.
 */
static void main_processSamples() {
	struct timeval currentTime;
	unsigned int i;
	common_type_t result;
	common_type_error_t err;

	assert(main_csvOut != NULL);

	err = pfm_sync();
	if (err != COMMON_TYPE_SUCCESS) {
		main_bailOut(EXIT_ERR_NETWORK, "Can't synchronize the network clients");
	}

	if (gettimeofday(&currentTime, NULL ) != 0) {
		main_bailOut(EXIT_ERR_LOCAL_SYS, "Can't read the local system time");
	}

	main_appendTimestamp(main_csvOut, &currentTime);
	if (fprintf(main_csvOut, "%s", MAIN_CSV_SEP) < 0) {
		main_bailOut(EXIT_ERR_OUTFILE, "Can't write to the CSV file anymore");
	}

	for (i = 0; i < main_channelVectorLength; i++) {
		result = pfm_fetchValue(main_channelVector[i].channelID);
		if (result.type == COMMON_TYPE_ERROR) {
			logging_adapter_error("Can't fetch the value of \"%s\" (err-no. %d)",
					main_channelVector[i].title, (int) result.data.errVal);
		}
		main_appendResult(main_csvOut, &result);

		if (i + 1 < main_channelVectorLength) {
			if (fprintf(main_csvOut, "%s", MAIN_CSV_SEP) < 0) {
				main_bailOut(EXIT_ERR_OUTFILE, "Can't write to the CSV file anymore");
			}
		}
	}

	if (fprintf(main_csvOut, "%s", MAIN_CSV_NEWLINE) < 0) {
		main_bailOut(EXIT_ERR_OUTFILE, "Can't write to the CSV file anymore");
	}

}

/**
 * @brief Formats the given timestamp and appends it to the file
 * @details <p>It assumes that both references are valid. If an error occurs
 * during writing the data the function will bail out. The function also assumes
 * that the configuration was previously initialized.</p>
 * <p>The given timeval structure is converted to a local time and printed
 * afterwards. The format is taken from the corresponding configuration
 * directive.</p>
 * @param file The file reference to write
 * @param tv The time stamp to print
 *
 */
static void main_appendTimestamp(FILE *file, struct timeval *tv) {
	char strBuffer[MAIN_TIMESTAMP_BUFFER_SIZE];
	const char* format = "%Y-%m-%d %H:%M:%S";
	struct tm brokentime;
	assert(file != NULL);
	assert(tv != NULL);

	(void) config_lookup_string(&main_config, MAIN_CONFIG_TIME_FORMAT, &format);

	if (localtime_r(&tv->tv_sec, &brokentime) != &brokentime) {
		main_bailOut(EXIT_ERR_LOCAL_SYS, "Can't convert to local time");
	}

	if (strftime(strBuffer, MAIN_TIMESTAMP_BUFFER_SIZE, format, &brokentime)
			== 0) {
		main_bailOut(EXIT_ERR_LOCAL_SYS, "Can't successfully create the time "
				"string \"%s\"", format);
	}

	if (fwrite(strBuffer, 1, strlen(strBuffer), file) != strlen(strBuffer)) {
		main_bailOut(EXIT_ERR_OUTFILE, "Can't write to the CSV file anymore");
	}
}

/**
 * @brief Appends the value to the given file
 * @details It assumes that the given references are valid. No column
 * separation character will be printed. If the data can't be successfully
 * written to the file the function will bail out.
 * @param file
 * @param result
 */
static void main_appendResult(FILE *file, const common_type_t *result) {
	int err = 0;
	assert(file != NULL);
	assert(result != NULL);

	switch (result->type) {
	case COMMON_TYPE_DOUBLE:
		err = fprintf(file, "%.15le", result->data.doubleVal);
		break;
	case COMMON_TYPE_LONG:
		err = fprintf(file, "%lli", (long long int) result->data.longVal);
		break;
	case COMMON_TYPE_STRING:
		main_appendString(file, result->data.strVal);
		break;
	case COMMON_TYPE_ERROR:
		err = fprintf(file, MAIN_CSV_ERR);
		break;
	default:
		assert(0);
	}

	if (err < 0) {
		main_bailOut(EXIT_ERR_OUTFILE, "Can't write to the CSV file anymore");
	}
}

/**
 * @brief Tries to append the given string to the CSV file
 * @details <p>It is assumed that the given common type contains a valid string
 * reference. The string is enclosed within double quotes and any double quote
 * character will be escaped using two double quotes. No column separator is
 * appended. The terminating '\0' character won't be written.</p>
 * <p>If something went wrong the function will call main_bailOut</p>
 * @param file A valid file reference to apend the CSV data
 * @param str The string to append
 */
static void main_appendString(FILE *file, const char* str) {
	const char quote = '"';

	assert(file != NULL);
	assert(str != NULL);

	if (fwrite(&quote, sizeof(quote), 1, file) != sizeof(quote)) {
		main_bailOut(EXIT_ERR_OUTFILE, "Can't write to the CSV file");
	}

	while (*str) {

		if (*str == '"') {
			if (fwrite(&quote, sizeof(quote), 1, file) != sizeof(quote)) {
				main_bailOut(EXIT_ERR_OUTFILE, "Can't write to the CSV file");
			}
		}

		if (fwrite(str, sizeof(str[0]), 1, file) != sizeof(str[0])) {
			main_bailOut(EXIT_ERR_OUTFILE, "Can't write to the CSV file");
		}

		str++;
	}

	if (fwrite(&quote, sizeof(quote), 1, file) != sizeof(quote)) {
		main_bailOut(EXIT_ERR_OUTFILE, "Can't write to the CSV file");
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
