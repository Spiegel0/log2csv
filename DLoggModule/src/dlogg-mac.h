/**
 * @file dlogg-mac.h
 * @brief Defines the D-LOGG mac interface used to access raw data
 * @author Michael Spiegel, michael.h.spiegel@gmail.com
 */

#ifndef DLOGG_MAC_H_
#define DLOGG_MAC_H_

#include <common-type.h>
#include <stdint.h>
#include <stdlib.h>

/** @brief Defines a type storing a checksum fragment */
typedef uint8_t dlogg_mac_chksum_t;

/** @brief The initial checksum value */
#define DLOGG_MAC_INITIAL_CHKSUM (0)

/**
 * @brief Sends the given content
 * @details If chksum is null, no checksum will be calculated. Otherwise the
 * newly generated checksum will be written to the location. The initial value
 * (take zero on the first packet's fragment) is taken to initialize the
 * checksum generation.
 * @param buffer The location of the fragment to send
 * @param length The number of bytes to send
 * @param chksum The checksum location
 * @return The status of the operation
 */
common_type_error_t dlogg_mac_send(uint8_t *buffer, size_t length,
		dlogg_mac_chksum_t * chksum);

/**
 * @brief Sends the given checksum
 * @param chksum A valid pointer to the checksum structure
 * @return The status of the operation
 */
common_type_error_t dlogg_mac_send_chksum(dlogg_mac_chksum_t * chksum);

/**
 * @brief Reads the given number of bytes
 * @details It is assumed that the buffer is capable of holding at least length
 * bytes
 * @param buffer The data buffer to store the read data
 * @param length The number of bytes to read
 * @param chksum The checksum value location. See dlogg_send for more details
 * on using this value
 * @return The status of the operation
 */
common_type_error_t dlogg_mac_read(uint8_t *buffer, size_t length,
		dlogg_mac_chksum_t * chksum);

/**
 * @brief validates the checksum
 * @param chksum The valid checksum value calculated so far
 * @return The status of the operation
 */
common_type_error_t dlogg_mac_read_chksum(dlogg_mac_chksum_t * chksum);

#endif /* DLOGG_MAC_H_ */
