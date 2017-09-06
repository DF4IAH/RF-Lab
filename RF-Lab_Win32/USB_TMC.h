#pragma once

/* @see https://github.com/python-ivi/python-usbtmc/blob/master/usbtmc/usbtmc.py */


#include "libusb.h"
#include "libusbi.h"

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
	INSTRUMENT_ENUM_t		type;
	int						devs_idx;
	libusb_device_handle	*devHandle;
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
	void shutdown_libusb(void);

	int findInstruments(void);
	instrument_t* addInstrument(INSTRUMENT_ENUM_t type, int devs_idx);
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
