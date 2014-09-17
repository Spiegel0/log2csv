/**
 * @file common-type.h
 * @brief Specifies a common struct encapsulating possible value representations
 * @details Additionally an error type will be defined representing common
 * error classes.
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

#ifndef COMMON_TYPE_H_
#define COMMON_TYPE_H_

#include <stdint.h>

/**
 * @brief Enumeration describing the encapsulated data type
 */
typedef enum {
	COMMON_TYPE_LONG, COMMON_TYPE_DOUBLE, COMMON_TYPE_STRING, COMMON_TYPE_ERROR
} common_type_type_t;

/**
 * @brief Enumeration defining common error types
 */
typedef enum {
	COMMON_TYPE_SUCCESS = 0,
	COMMON_TYPE_ERR,
	COMMON_TYPE_ERR_CONFIG,
	COMMON_TYPE_ERR_LOAD_MODULE,
	COMMON_TYPE_ERR_INVALID_ADDRESS,
	COMMON_TYPE_ERR_IO,
	COMMON_TYPE_ERR_TIMEOUT,
	COMMON_TYPE_ERR_INVALID_RESPONSE,
	COMMON_TYPE_ERR_DEVICE_NOT_FOUND
} common_type_error_t;

/**
 * @brief Defines a data type encoding it's primitive type and it's value.
 */
typedef struct {
	/** @brief The actual data */
	union {
		/** @brief Integer representation */
		uint64_t longVal;
		/** @brief Floating point representation */
		double doubleVal;
		/** @brief Zero terminated string representation. */
		char* strVal;
		/** @brief An error code specifying the status of an operation */
		common_type_error_t errVal;
	} data;

	/** @brief The type of the value stored in data */
	common_type_type_t type;

} common_type_t;

#endif /* COMMON_TYPE_H_ */
