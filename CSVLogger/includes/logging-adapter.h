/**
 * @file logging-adapter.h
 * @brief Defines a basic library independent logging interface.
 * @details The logging_adapter module provides a unique and simple to use
 * interface to log status information. Every message should not be terminated
 * by a newline character. Any newline formatting will be inserted by the
 * logging back-end used, if necessary.
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

#ifndef LOGGING_ADAPTER_H_
#define LOGGING_ADAPTER_H_

#include <stdarg.h>

/**
 * @brief Initializes the logging adapter.
 * @details <p>The function has to be called before calling any other function,
 * except logging_adapter_free(). The return value has to be zero if an
 * error occurred. In this case errno may be set accordingly and the program
 * will immediately exit.</p>
 * <p>The progname reference must be valid until the function returns. It's not
 * necessary to keep it any further.</p>
 * @param progname A valid string reference containing the program's name.
 * @return The status of the operation, 1 on success
 */
int logging_adapter_init(const char* progname);

/**
 * @brief Reports the printf-styled error message.
 * @param formatString The string to report.
 */
void logging_adapter_error(const char* formatString, ...);

/**
 * @brief Reports the printf-styled error message.
 * @details Additionally it checks the errno variable and reports any additional
 * information referenced by errno.
 * @param err The error number taken from errno.h or zero if no additional
 * information is given.
 * @param formatString The string to report.
 * @param varArg The list of arguments
 */
void logging_adapter_errorNo(int err, const char* formatString, va_list varArg);

/**
 * @brief Reports the printf-styled informational message.
 * @param formatString The string to report.
 */
void logging_adapter_info(const char* formatString, ...);

/**
 * @brief Reports the printf-styled debugging message.
 * @param formatString The string to report.
 */
void logging_adapter_debug(const char* formatString, ...);

/**
 * @brief Frees allocated resources.
 * @details After calling this function only logging_adapter_init() may be
 * called again. Calling this function without initializing the module will be
 * tolerated without producing any error.
 */
void logging_adapter_freeResources(void);

#endif /* LOGGING_ADAPTER_H_ */
