/**
 * @file dlogg-current-data.h
 * @brief Communicates with the dlogg logger and reads the current data.
 * @details The module provides function used to access the read data structure
 * as well as present end devices.
 * @author Michael Spiegel, michael.h.spiegel@gmail.com
 */

#ifndef DLOGG_CURRENT_DATA_H_
#define DLOGG_CURRENT_DATA_H_

#include <stdint.h>

/** @brief The module-type request acknowledgment code */
#define DLOGG_CD_MOD_TYPE_ACK (0x4321)
/** @brief BLNet module type code */
#define DLOGG_CD_MOD_TYPE_BLNET (0xA3)
/** @brief BL232 - backup module type code */
#define DLOGG_CD_MOD_TYPE_BL232_BACKUP (0xA2)
/** @brief D-LOGG and BL232 1DL module type code */
#define DLOGG_CD_MOD_TYPE_DLOGG_1D (0xA8)
/** @brief D-LOGG and BL232 2DL module type code */
#define DLOGG_CD_MOD_TYPE_DLOGG_2D (0xD1)

/** @brief One data line mode code */
#define DLOGG_CD_MODE_1DL (0xA8)
/** @brief Two data lines mode code */
#define DLOGG_CD_MODE_2DL (0xD1)
/** @brief CAN logging mode code */
#define DLOGG_CD_MODE_CAN (0xDC)
/** @brief backup mode code */
#define DLOGG_CD_MODE_BACKUP (0xA2)

/** @brief UVR 61-3 device code */
#define DLOGG_CD_DEVICE_UVR61_3 (0x90)
/** @brief No device registered */
#define DLOGG_CD_DEVICE_NO (0xAB)

/** @brief UVR 61-3 protocol version 1.4 sample type */
#define DLOGG_CD_SAMPLE_UVR_61_3_V14 (0)

/**
 * @brief Structure-type encapsulating the logging module's information
 */
typedef struct {
	/** @brief The module type code */
	uint8_t type;

	/**
	 * @brief The firmware version
	 * @details On BLNET 100 = 1.00, on BL232 and DLoggUSB 10 = 1.0
	 */
	uint8_t firmware;
} dlogg_cd_moduleType_t;

/** @brief Structure encapsulating available meta-data */
typedef struct {
	/** @brief The read module type */
	dlogg_cd_moduleType_t moduleType;
	/** @brief The read mode of operation */
	uint8_t mode;
} dlogg_cd_metadata_t;

/** @brief Encapsulates a single input's data */
typedef union {
	struct {
		/** @brief The low byte of the value */
		uint8_t lowValue;
		/** @brief The signature bit */
		unsigned sign :1;
		/** @brief The input type encoding */
		unsigned type :3;
		/** @brief The most significant value bits */
		unsigned highValue :4;
	} val;
	/** @brief The raw type encoding */
	uint8_t raw[2];
} dlogg_cd_input_t;

/**
 * @brief Defines a analog output sample
 */
typedef union {
	struct {
		/** @brief Flag indicating that the output is active */
		unsigned active :1;
		/** @brief The output voltage (0-10V) in 0.1V */
		unsigned voltage :7;
	} val;
	/** @brief The raw output data */
	uint8_t raw;
} dlogg_cd_analogOutput_t;

/**
 * @brief Defines a bit-field storing output drive data
 */
typedef struct {
	/** @brief Flag indicating the status of the output */
	unsigned active :1;
	/** @brief Bits to ignore */
	unsigned ign :2;
	/** @brief speed step [0,30] */
	unsigned speed :5;
} dlogg_cd_outputDrive_t;

/**
 * @brief Defines the 6 Byte heat meter representation
 */
typedef union {
	struct {
		/** @brief Current power in a little endian format in 0.1kW */
		uint8_t cur[2];
		/** @brief Little endian counter in 0.1kWh */
		uint8_t kwh[2];
		/** @brief Little endian counter in 1MWh */
		uint8_t mwh[2];
	} val;
	/** @brief The raw meter data representation */
	uint8_t raw[6];
} dlogg_cd_heatMeterSmall_t;

/**
 * @brief Encapsulates one sample of the UVR 61-3 control unit
 * @details The encoding is only valid for the 1.4 version of the protocol
 */
typedef struct {
	/**
	 * @brief The control inputs available.
	 * @details The first six values correspond to internal inputs and the rest
	 * corresponds to external inputs configured
	 */
	dlogg_cd_input_t inputs[15];
	/** @brief The digital output data, lsb corresponds to output 1*/
	uint8_t output;
	/** @brief Speed control data */
	dlogg_cd_outputDrive_t outputDrive;
	/** @brief The analog output values */
	dlogg_cd_analogOutput_t analogOutput[2];
	/**
	 * @brief heat meter register
	 * @details The lsb corresponds to the status of the first heat meter
	 */
	uint8_t heatMeterRegister;
	/** @brief The heat meter data */
	dlogg_cd_heatMeterSmall_t heatMeter[3];

} dlogg_cd_dataUVR61_3_v14_t;

/**
 * @brief Type definition encapsulating a sample
 */
typedef struct {
	/** @brief The sample type code */
	uint8_t sampleType;
	/** Union encapsulating the sample's data */
	union {
		dlogg_cd_dataUVR61_3_v14_t uvr61_3_v14;
	} data;
} dlogg_cd_sample_t;

/* ************************************************************************** */
/* Function prototypes                                                        */
/* ************************************************************************** */

/**
 * @brief returns the previously read meta data section.
 * @details before accessing the meta-data the sync function must be called.
 * The caller must not write to the meta-data section passed.
 * @param lineID The communication line's unique identifier
 * @return The currently active meta-data section
 */
dlogg_cd_metadata_t * dlogg_cd_getMetadata(uint8_t lineID);

/**
 * @brief Returns the currently buffered sample
 * @details It is assumed that the sync function was called successfully before.
 * The caller mustn't modify the given data structure.
 * @param device The device number or logger's channel
 * @param lineID The communication line's id.
 * @return The available sample
 */
dlogg_cd_sample_t * dlogg_cd_getCurrentData(uint8_t device, uint8_t lineID);

#endif /* DLOGG_CURRENT_DATA_H_ */
