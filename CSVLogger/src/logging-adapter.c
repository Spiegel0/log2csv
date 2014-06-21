/**
 * @file logging-adapter.c
 * @brief Implements the default logging adapter
 * @details The output may be written to stdout, stderr as well as syslog.
 * @author Michael Spiegel, michael.h.spiegel@gmail.com
 */

#include "logging-adapter.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

/** @brief The number of the syslog facility to use */
#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY (LOG_USER)
#endif

/** @brief The name of the program */
static char* logging_adapter_progname;

int logging_adapter_init(const char* progname) {
	openlog(progname, LOG_CONS, SYSLOG_FACILITY);
	logging_adapter_progname = malloc(strlen(progname) + 1);
	if (logging_adapter_progname == NULL ) {
		errno = ENOMEM;
		return 0;
	}
	strcpy(logging_adapter_progname, progname);

	return 1;
}

void logging_adapter_errorNo(int err, const char* formatString, va_list varArg) {

	assert(formatString != NULL);

	vsyslog(LOG_ERR, formatString, varArg);

	(void) fprintf(stderr, "ERROR [%s]: ", logging_adapter_progname);
	(void) vfprintf(stderr, formatString, varArg);

	if (err != 0) {
		syslog(LOG_INFO, "%s", strerror(err));
		(void) fprintf(stderr, ": %s", strerror(err));
	}
	(void) fprintf(stderr, "\n");

}

void logging_adapter_error(const char* formatString, ...) {
	va_list varArg;

	va_start(varArg, formatString);
	logging_adapter_errorNo(0, formatString, varArg);
	va_end(varArg);
}

void logging_adapter_info(const char* formatString, ...) {
	va_list varArg;

	assert(formatString != NULL);

	va_start(varArg, formatString);
	vsyslog(LOG_INFO, formatString, varArg);
	va_end(varArg);

	(void) fprintf(stdout, "INFO [%s]: ", logging_adapter_progname);
	va_start(varArg, formatString);
	(void) vfprintf(stdout, formatString, varArg);
	(void) fprintf(stdout, "\n");
	va_end(varArg);
}

void logging_adapter_debug(const char* formatString, ...) {
	va_list varArg;

	assert(formatString != NULL);

	va_start(varArg, formatString);
	vsyslog(LOG_DEBUG, formatString, varArg);
	va_end(varArg);

	(void) fprintf(stdout, "DEBUG [%s]: ", logging_adapter_progname);
	va_start(varArg, formatString);
	(void) vfprintf(stdout, formatString, varArg);
	(void) fprintf(stdout, "\n");
	va_end(varArg);

}

void logging_adapter_freeResources(void) {
	closelog();
	if(logging_adapter_progname != NULL)
		free(logging_adapter_progname);
}
