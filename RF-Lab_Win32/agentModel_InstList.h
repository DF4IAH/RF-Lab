#pragma once

#include "stdafx.h"

#include <string>
#include <list>

/* LibUSB */
#include "libusb.h"
#include "libusbi.h"


using namespace std;


typedef enum {

	INST_FUNC_UNKNOWN = 0,
	INST_FUNC_ROTOR,
	INST_FUNC_GEN,
	INST_FUNC_SPEC,
	INST_FUNC_VNA,

} Inst_Function_t;


/* LinkType bitmask */
typedef uint16_t						LinkType_BM_t;

#define LINKTYPE_UNKNOWN				0x0000U
#define LINKTYPE_COM					0x0001U
#define LINKTYPE_IEC_VIA_SER			0x0002U
#define LINKTYPE_IEC					0x0010U
#define LINKTYPE_USB					0x0100U
#define LINKTYPE_ETH					0x1000U
#define LINKTYPE_ETH_SCPI				0x2000U
#define LINKTYPE_ETH_LXI				0x4000U


typedef enum {

	ACT_IFC_NONE = 0,
	ACT_IFC_COM,
	ACT_IFC_USB,
	ACT_IFC_ETH,

} Inst_ActIfcType_t;


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

	string								linkEthHostname;
	string								linkEthMAC;
	WORD								linkEthPort;
	//int								linkEthScpiXXX;
	//int								linkEthLxiXXX;

	WORD								linkUsbIdVendor;
	WORD								linkUsbIdProduct;
	libusb_device_handle			   *pLinkUsbHandle;

	//string							linkIdnSearch;

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

} am_InstListEntry_t;

typedef list<am_InstListEntry_t>		am_InstList_t;
