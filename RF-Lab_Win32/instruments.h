#pragma once

/* Agents Library */
#include <agents.h>
#include "agentCom.h"

/* LibUSB */
#include "libusb.h"
#include "libusbi.h"

#include "USB_TMC_types.h"


/* Instruments */

typedef enum INSTRUMENT_ENUM {
	INSTRUMENT_NONE								= 0x0000,

	/* Rotors */
	INSTRUMENT_ROTORS__ALL						= 0x1000,
	INSTRUMENT_ROTORS_SER__ZOLIX_SC300			= 0x1101,
	INSTRUMENT_ROTORS_SER__GENERIC				= 0x1151,	// and higher

	/* Transmitters */
	INSTRUMENT_TRANSMITTERS__ALL				= 0x2000,
	INSTRUMENT_TRANSMITTERS_SER__RS_SMR40		= 0x2101,	// IEC
	INSTRUMENT_TRANSMITTERS_SER__AGILENT_N5173B = 0x2102,	// IEC
	INSTRUMENT_TRANSMITTERS_SER__GENERIC		= 0x2151,	// and higher
	INSTRUMENT_TRANSMITTERS_USB__GENERIC		= 0x2251,	// and higher
	INSTRUMENT_TRANSMITTERS_ETH__GENERIC		= 0x2351,	// and higher

	/* Receivers */
	INSTRUMENT_RECEIVERS__ALL					= 0x3000,
	INSTRUMENT_RECEIVERS_SER__GENERIC			= 0x3151,	// and higher
	INSTRUMENT_RECEIVERS_USB__RIGOL_DSA875		= 0x3201,
	INSTRUMENT_RECEIVERS_USB__GENERIC			= 0x3251,	// and higher
	INSTRUMENT_RECEIVERS_ETH__GENERIC			= 0x3351,	// and higher

} INSTRUMENT_ENUM_t;

#define INSTRUMENT_VARIANT_MASK					  0xf000
#define INSTRUMENT_CONNECT_MASK					  0x0f00
#define INSTRUMENT_DEVICE_MASK					  0x00ff

#define INSTRUMENT_CONNECT_SER					  0x0100
#define INSTRUMENT_CONNECT_USB					  0x0200
#define INSTRUMENT_CONNECT_ETH					  0x0300

#define INSTRUMENT_DEVICE_GENERIC				  0x0051


typedef struct instrument {
	INSTRUMENT_ENUM_t		 type;
	int						 devs_idx;
	char					 idn[256];

	/* Serial entries */
	agentCom				*pAgtCom;
	uint8_t					 comPort;
	uint32_t				 comBaud;
	uint8_t					 comBits;
	uint8_t					 comParity;
	uint8_t					 comStopbits;
	bool					 isIec;
	uint8_t					 iecAddr;

	/* USB entries */
	libusb_device			*dev;
	libusb_device_handle	*dev_handle;
	uint8_t					 dev_config;
	uint8_t					 dev_interface;
	uint16_t				 dev_usb_idVendor;
	uint16_t				 dev_usb_idProduct;
	uint8_t					 dev_bulk_out_ep;
	uint8_t					 dev_bulk_in_ep;
	uint8_t					 dev_interrupt_ep;
	uint8_t					 dev_usbtmc_int_cap;
	uint8_t					 dev_usbtmc_dev_cap;
	uint8_t					 dev_usb488_dev_cap1;
	uint8_t					 dev_usb488_dev_cap2;
	uint8_t					 dev_bTag;
	uint8_t					 dev_bulkin_attributes;
	bool					 dev_usb_up;
	bool					 dev_tmc_up;

	int						 response_length;
	int						 response_bytes_read;
	int						 remaining_length;
	uint8_t					 buffer[USB_MAX_TRANSFER_LENGTH];

	struct sr_context		*ctx;						// SCPI USB-TMC code
	struct sr_usb_dev_inst	*usb;						// SCPI USB-TMC code
	int						 detached_kernel_driver;	// SCPI USB-TMC code

	/* Ethernet entries */

} instrument_t;


typedef struct ArrayOfInstruments {
	int					inst_rot_cnt;
	instrument_t		inst_rot[8];

	int					inst_tx_cnt;
	instrument_t		inst_tx[8];

	int					inst_rx_cnt;
	instrument_t		inst_rx[8];
} ArrayOfInstruments_t;



/* Functions */

INSTRUMENT_ENUM_t findSerInstrumentByIdn(const string rspIdnStr, INSTRUMENT_ENUM_t instVariant);
