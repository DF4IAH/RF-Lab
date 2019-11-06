#pragma once


#include "stdafx.h"

#include <string>
#include <list>

/* Agents Library */
#include <agents.h>
//#include "agentCom.h"

/* LibUSB */
#include "libusb.h"
#include "libusbi.h"

#include "USB_TMC_types.h"


using namespace std;



/* Instrument functions */

typedef enum {

	INST_FUNC_UNKNOWN = 0,
	INST_FUNC_ROTOR,
	INST_FUNC_GEN,
	INST_FUNC_SPEC,
	INST_FUNC_VNA,

} Inst_Function_t;


/* LinkType Bitmasks */

typedef uint16_t						LinkType_BM_t;

#define LINKTYPE_UNKNOWN				0x0000U
#define LINKTYPE_COM					0x0001U
#define LINKTYPE_IEC_VIA_SER			0x0002U
#define LINKTYPE_IEC					0x0010U
#define LINKTYPE_USB					0x0100U
#define LINKTYPE_ETH					0x1000U
#define LINKTYPE_ETH_SCPI				0x2000U
#define LINKTYPE_ETH_LXI				0x4000U


/* Activated Interface Type */

typedef enum {

	ACT_IFC_NONE = 0,
	ACT_IFC_COM,
	ACT_IFC_USB,
	ACT_IFC_ETH,

} Inst_ActIfcType_t;



/* Instruments */

typedef enum INSTRUMENT_ENUM {
	INSTRUMENT_NONE								= 0x0000,

	/* Rotors */
	INSTRUMENT_ROTORS__ALL						= 0x1000,
	INSTRUMENT_ROTORS_SER__ZOLIX_SC300			= 0x1101,
	INSTRUMENT_ROTORS_SER__GENERIC				= 0x1191,	// and higher

	/* Transmitters */
	INSTRUMENT_TRANSMITTERS__ALL				= 0x2000,
	INSTRUMENT_TRANSMITTERS_SER__RS_SMR40		= 0x2101,	// IEC
	INSTRUMENT_TRANSMITTERS_SER__RS_SMB100A		= 0x2102,	// IEC
	INSTRUMENT_TRANSMITTERS_SER__RS_SMC100A		= 0x2103,	// IEC
	INSTRUMENT_TRANSMITTERS_SER__AGILENT_N5173B = 0x2111,	// IEC
	INSTRUMENT_TRANSMITTERS_SER__GENERIC		= 0x2191,	// and higher
	INSTRUMENT_TRANSMITTERS_USB__RS_SMB100A		= 0x2202,
	INSTRUMENT_TRANSMITTERS_USB__RS_SMC100A		= 0x2203,
	INSTRUMENT_TRANSMITTERS_USB__GENERIC		= 0x2291,	// and higher
	INSTRUMENT_TRANSMITTERS_ETH__GENERIC		= 0x2391,	// and higher

	/* Receivers */
	INSTRUMENT_RECEIVERS__ALL					= 0x3000,
	INSTRUMENT_RECEIVERS_SER__RS_FSEK20			= 0x3101,	// IEC
	INSTRUMENT_RECEIVERS_SER__GENERIC			= 0x3191,	// and higher
	INSTRUMENT_RECEIVERS_USB__RIGOL_DSA875		= 0x3221,
	INSTRUMENT_RECEIVERS_USB__GENERIC			= 0x3291,	// and higher
	INSTRUMENT_RECEIVERS_ETH__RS_FSL18			= 0x3311,
	INSTRUMENT_RECEIVERS_ETH__GENERIC			= 0x3391,	// and higher

} INSTRUMENT_ENUM_t;

#define INSTRUMENT_VARIANT_MASK					  0xf000
#define INSTRUMENT_CONNECT_MASK					  0x0f00
#define INSTRUMENT_DEVICE_MASK					  0x00ff

#define INSTRUMENT_VARIANT_ROTORS				  0x1000
#define INSTRUMENT_VARIANT_TX					  0x2000
#define INSTRUMENT_VARIANT_RX					  0x3000

#define INSTRUMENT_CONNECT_SER					  0x0100
#define INSTRUMENT_CONNECT_USB					  0x0200
#define INSTRUMENT_CONNECT_ETH					  0x0300

#define INSTRUMENT_DEVICE_GENERIC				  0x0091


typedef struct {

	/* List information */
	size_t								listId;
	string								listEntryName;
	Inst_Function_t						listFunction;


	/* Instrument activation */
	bool								actSelected;
	bool								actLink;
	Inst_ActIfcType_t					actIfcType;
	UINT								winID;


	/* Link settings */
	LinkType_BM_t						linkType;


	/* Link: Ethernet */
	string								linkEthHostname;
	string								linkEthMAC;
	WORD								linkEthPort;
	//int								linkEthScpiXXX;
	//int								linkEthLxiXXX;


	/* Link: USB */
	libusb_device					   *pLinkUsb_dev;
	int									linkUsb_devs_idx;

	libusb_device_handle			   *pLinkUsb_dev_handle;
	uint8_t								linkUsb_dev_config;
	uint8_t								linkUsb_dev_interface;
	uint16_t							linkUsb_dev_usb_idVendor;
	uint16_t							linkUsb_dev_usb_idProduct;
	uint8_t								linkUsb_dev_bulk_out_ep;
	uint8_t								linkUsb_dev_bulk_in_ep;
	uint8_t								linkUsb_dev_interrupt_ep;
	uint8_t								linkUsb_dev_usbtmc_int_cap;
	uint8_t								linkUsb_dev_usbtmc_dev_cap;
	uint8_t								linkUsb_dev_usb488_dev_cap1;
	uint8_t								linkUsb_dev_usb488_dev_cap2;
	uint8_t								linkUsb_dev_bTag;
	uint8_t								linkUsb_dev_bulkin_attributes;
	bool								linkUsb_dev_usb_up;
	bool								linkUsb_dev_tmc_up;

	int									linkUsb_response_length;
	int									linkUsb_response_bytes_read;
	int									linkUsb_remaining_length;

	struct sr_context				   *_pLinkUsb_sr_ctx;					// SCPI USB-TMC code
	struct sr_usb_dev_inst			   *pLinkUsb_sr_devInst;				// SCPI USB-TMC code
	int									linkUsb_is_detached_kernel_driver;	// SCPI USB-TMC code

	char								linkUsb_idn[256];
	uint8_t								linkUsb_buffer[USB_MAX_TRANSFER_LENGTH];
	char								linkUsb_lastErrStr[256];


	/* Link: COM & IEC */
	BYTE								linkSerIecAddr;
	BYTE								linkSerPort;
	DWORD								linkSerBaud;
	BYTE								linkSerBits;
	BYTE								linkSerParity;
	BYTE								linkSerStopbits;


	/* Rotor settings */
	int									rotInitTicksPer360deg;
	int									rotInitTopSpeed;
	int									rotInitAcclSpeed;
	int									rotInitStartSpeed;

	int									rotMinPosition;
	int									rotMaxPosition;
	int									rotInitPosition;
	int									rotCurPosition;


	/* TX settings */
	bool								txInitRfOn;
	bool								txCurRfOn;

	double								txMinRfQrg;
	double								txMaxRfQrg;
	double								txInitRfQrg;
	double								txCurRfQrg;

	double								txMinRfPwr;
	double								txMaxRfPwr;
	double								txInitRfPwr;
	double								txCurRfPwr;


	/* RX settings */
	double								rxMinRfQrg;
	double								rxMaxRfQrg;
	double								rxInitRfQrg;
	double								rxCurRfQrg;

	double								rxMinRfSpan;
	double								rxMaxRfSpan;
	double								rxInitRfSpan;
	double								rxCurRfSpan;

	double								rxMinRfPwrLo;
	double								rxMaxRfPwrLo;
	double								rxInitRfPwrLo;
	double								rxCurRfPwrLo;

	double								rxMinRfPwrHi;
	double								rxMaxRfPwrHi;
	double								rxInitRfPwrHi;
	double								rxCurRfPwrHi;


	/* VNA extras */
	double								vnaMinNbPoints;
	double								vnaMaxNbPoints;
	double								vnaInitNbPoints;
	double								vnaCurNbPoints;

} Instrument_t;


typedef list<Instrument_t>				InstList_t;



/* Functions */

INSTRUMENT_ENUM_t findSerInstrumentByIdn(const string rspIdnStr, int instVariant);
