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


typedef struct Win32_InstListEntry {

	/* List information */
	int									listId;
	string								listEntryName;
	Inst_Function_t						listFunction;


	/* Instrument activation */
	int									actRank;
	bool								actSelected;
	bool								actLink;
	

	/* Link settings */
	LinkType_t							linkType;
	string								link�dnSearch;

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
	int									rotCurPosition;


	/* TX settings */
	bool								txInitRfOn;
	double								txInitRfQrg;
	double								txInitRfPwr;
	double								txMinRfQrg;
	double								txMinRfPwr;
	double								txMaxRfQrg;
	double								txMaxRfPwr;
	bool								txCurRfOn;
	double								txCurRfQrg;
	double								txCurRfPwr;


	/* RX settings */
	double								rxInitRfQrg;
	double								rxInitRfSpan;
	double								rxInitRfPwrLo;
	double								rxInitRfPwrDynamic;
	double								rxMinRfQrg;
	double								rxMinRfSpan;
	double								rxMinRfPwrLo;
	double								rxMinRfPwrDynamic;
	double								rxMaxRfQrg;
	double								rxMaxRfSpan;
	double								rxMaxRfPwrLo;
	double								rxMaxRfPwrDynamic;
	double								rxCurRfQrg;
	double								rxCurRfSpan;
	double								rxCurRfPwrLo;
	double								rxCurRfPwrDynamic;

} Win32_InstListEntry_t;

typedef list<Win32_InstListEntry_t>	Win32_InstList_t;
