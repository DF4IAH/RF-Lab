#pragma once

/* @see https://sigrok.org/gitweb/?p=libsigrok.git;a=blob;f=src/scpi/scpi_usbtmc_libusb.c */
/* @see https://github.com/python-ivi/python-usbtmc/blob/master/usbtmc/usbtmc.py */


#include "libusb.h"
#include "libusbi.h"


#define MAX_TRANSFER_LENGTH				2048
#define TRANSFER_TIMEOUT				1000

/* Some USBTMC-specific enums, as defined in the USBTMC standard. */
//#define LIBUSB_CLASS_APPLICATION		0xFE  --> @see: libusb.h  enum libusb_class_code { ... };
#define SUBCLASS_USBTMC					0x03
#define USBTMC_USB488					0x01

typedef enum USB_CR_ENUM {
	/* USBTMC control requests */
	INITIATE_ABORT_BULK_OUT				=   1,
	CHECK_ABORT_BULK_OUT_STATUS			=   2,
	INITIATE_ABORT_BULK_IN				=   3,
	CHECK_ABORT_BULK_IN_STATUS			=   4,
	INITIATE_CLEAR						=   5,
	CHECK_CLEAR_STATUS					=   6,
	GET_CAPABILITIES					=   7,
	INDICATOR_PULSE						=  64,

	/* USB488 control requests */
	READ_STATUS_BYTE					= 128,
	REN_CONTROL							= 160,
	GO_TO_LOCAL							= 161,
	LOCAL_LOCKOUT						= 162,
} USB_CR_ENUM_t;

/* USBTMC status codes */
#define USBTMC_STATUS_SUCCESS			0x01

/* USBTMC capabilities */
#define USBTMC_INT_CAP_LISTEN_ONLY		0x01
#define USBTMC_INT_CAP_TALK_ONLY		0x02
#define USBTMC_INT_CAP_INDICATOR		0x04

#define USBTMC_DEV_CAP_TERMCHAR			0x01

#define USB488_DEV_CAP_DT1				0x01
#define USB488_DEV_CAP_RL1				0x02
#define USB488_DEV_CAP_SR1				0x04
#define USB488_DEV_CAP_SCPI				0x08

/* Bulk messages constants */
#define USBTMC_BULK_HEADER_SIZE			12

/* Bulk MsgID values */
#define DEV_DEP_MSG_OUT					1
#define REQUEST_DEV_DEP_MSG_IN			2
#define DEV_DEP_MSG_IN					2

/* bmTransferAttributes */
#define EOM								0x01
#define TERM_CHAR_ENABLED				0x02


typedef enum INSTRUMENT_ENUM {
	INSTRUMENT_NONE						= 0,

	/* Rotors */
	INSTRUMENT_ROTORS__ALL				= 0x10,
	INSTRUMENT_ROTORS_XXX,

	/* Transmitters */
	INSTRUMENT_TRANSMITTERS__ALL		= 0x20,
	INSTRUMENT_TRANSMITTER_FG_XXX,

	/* Receivers */
	INSTRUMENT_RECEIVERS__ALL			= 0x30,
	INSTRUMENT_RECEIVER_SA_RIGOL_DSA875,
} INSTRUMENT_ENUM_t;

typedef struct instrument {
	INSTRUMENT_ENUM_t		 type;
	int						 devs_idx;

	libusb_device			*dev;
	libusb_device_handle	*dev_handle;
	uint8_t					 dev_config;
	uint8_t					 dev_interface;
	uint8_t					 dev_bulk_out_ep;
	uint8_t					 dev_bulk_in_ep;
	uint8_t					 dev_interrupt_ep;
	uint8_t					 dev_usbtmc_int_cap;
	uint8_t					 dev_usbtmc_dev_cap;
	uint8_t					 dev_usb488_dev_cap;
	uint8_t					 dev_bTag;
	uint8_t					 dev_bulkin_attributes;

	int						 response_length;
	int						 response_bytes_read;
	int						 remaining_length;
	uint8_t					 buffer[MAX_TRANSFER_LENGTH];
} instrument_t;


class USB_TMC
{
public:
	USB_TMC();
	virtual ~USB_TMC();

private:

	/* Methods */

	int init_libusb(bool show);
	void print_devs_libusb(libusb_device **devs);
	void print_device_cap_libusb(struct libusb_bos_dev_capability_descriptor *dev_cap);
	wchar_t* uuid_to_string_libusb(const uint8_t* uuid);
	void shutdown_libusb(void);

	int findInstruments(void);
	instrument_t* addInstrument(INSTRUMENT_ENUM_t type, int devs_idx);
	void releaseInstrument_usb_iface(instrument_t *inst, int cnt);
	void openTmc(instrument_t *inst);
	INSTRUMENT_ENUM_t checkIDTable(uint16_t idVendor, uint16_t idProduct);


	/* Attributes */

	const struct libusb_version		   *version;
	libusb_device					  **devs;

	/* ... Instruments */
	int					inst_rot_cnt;
	instrument_t		inst_rot[8];

	int					inst_tx_cnt;
	instrument_t		inst_tx[8];

	int					inst_rx_cnt;
	instrument_t		inst_rx[8];
};
