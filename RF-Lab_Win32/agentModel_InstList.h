#pragma once

#include "stdafx.h"

#include <string>
#include <list>


using namespace std;


typedef enum Inst_Function {
	INST_FUNCTION_UNKNOWN = 0,
	INST_FUNCTION_ROTOR,
	INST_FUNCTION_TX,
	INST_FUNCTION_RX,
} Inst_Function_t;


typedef enum LinkType {
	LINKTYPE_UNKNOWN					= 0,
	LINKTYPE_SER,
	LINKTYPE_IEC_VIA_SER,
	LINKTYPE_USB,
	LINKTYPE_ETH_SCPI,
	LINKTYPE_ETH_LXI,
} LinkType_t;


typedef struct am_InstListEntry {

	/* List information */
	size_t								listId;
	string								listEntryName;
	Inst_Function_t						listFunction;


	/* Instrument activation */
	int									actRank;
	bool								actSelected;
	bool								actLink;
	

	/* Link settings */
	LinkType_t							linkType;
	string								linkIdnSearch;

	BYTE								linkSerPort;
	DWORD								linkSerBaud;
	BYTE								linkSerBits;
	BYTE								linkSerParity;
	BYTE								linkSerStopbits;

	BYTE								linkSerIecAddr;

	WORD								linkUsbIdVendor;
	WORD								linkUsbIdProduct;

	//int								linkEthScpiXXX;

	//int								linkEthLxiXXX;


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
} am_InstListEntry_t;

typedef list<am_InstListEntry_t>		am_InstList_t;
