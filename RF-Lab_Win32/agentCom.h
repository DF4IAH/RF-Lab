#pragma once

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>

#include "agentModel_InstList.h"


using namespace concurrency;
using namespace std;


#define AGENT_COM_RECEIVE_TIMEOUT					500

#define C_OPENPARAMS_STR							":P=%d :B=%d :I=%d :A=%d :S=%d :E=%d"

#define C_ZOLIX_VERSION_REQ_STR1					"\r"
#define C_ZOLIX_VERSION_REQ_STR2					"VE\r"
#define C_ZOLIX_VERSION_VAL_STR						"SC300"
#define C_SYNC_STR									"\r\n"
#define C_IDN_REQ_STR								"*IDN?\r\n"

#define C_SET_IEC_ADDR_INVALID						-1


enum C_COMINST_ENUM {
	C_COMINST_ROT = 0,
	C_COMINST_TX,
	C_COMINST_RX,
	C_COMINST__COUNT
};

enum C_USBREQ_ENUM {
	C_USBREQ_END = 0,
	C_USBREQ_DO_REGISTRATION,
	C_USBREQ_IS_DEV_CONNECTED,
	C_USBREQ_CONNECT,
	C_USBREQ_DISCONNECT,
};

enum C_USBRSP_ENUM {
	C_USBRSP_END = 0,
	C_USBRSP_OK,
	C_USBRSP_ERR,
	C_USBRSP_ERR__LIBUSB_ASSIGNMENT_MISSING,
	C_USBRSP_REGISTRATION_DONE,
	C_USBRSP_REGISTRATION_LIST,
	C_USBRSP_IS_DEV_CONNECTED,
	C_USBRSP_DEV_CONNECT_HANDLE,
};

enum C_COMREQ_ENUM {
	C_COMREQ_END = 0,
	C_COMREQ_OPEN_ZOLIX,
	C_COMREQ_OPEN_IDN,
	C_COMREQ_COM_SEND,
	C_COMREQ_CLOSE,
};

enum C_COMRSP_ENUM {
	C_COMRSP_END = 0,
	C_COMRSP_DROP,
	C_COMRSP_FAIL,
	C_COMRSP_DATA,
	C_COMRSP_OK,
};

const int C_BUF_SIZE = 256;


/* USB REQ */

typedef struct agentUsbReq
{
	SHORT								 cmd;
	void								*data;
} agentUsbReq_t;

typedef struct agentUsbReqDev
{
	WORD								usbIdVendor;
	WORD								usbIdProduct;
} agentComReqUsbDev_t;


/* USB RSP */

typedef struct agentUsbRsp
{
	SHORT								 stat;
	void								*data;
} agentUsbRsp_t;


/* COM REQ */

typedef struct agentComReq
{
	SHORT								 cmd;
	string								 parm;
} agentComReq_t;


/* COM RSP */

typedef struct agentComRsp
{
	SHORT								 stat;
	string								 data;
} agentComRsp_t;



class agentCom : public agent
{
private:
	bool								 _isStarted;
	bool								 _running;
	bool								 _done;
	ISource<agentComReq>&				 _src;
	ITarget<agentComRsp>&				 _tgt;

	HANDLE								 _hCom;

	am_InstList_t::iterator*			 _pInstrument;

	bool								 _isIec;
	int									 _iecAddr;


public:
	explicit agentCom(ISource<agentComReq>& src, ITarget<agentComRsp>& tgt);
	void   start(void);
	bool   isRunning(void);
	bool   isDone(void);
	void   Release(void);
	bool   shutdown(void);

	string trim(string haystack);

	void   setInstrument(am_InstList_t::iterator* instrument);
	am_InstList_t::iterator* getInstrument(void);

	bool   isIec(void);
	void   setIecAddr(int iecAddr);
	int    getIecAddr(void);

	string doComRequestResponse(const string in);
	string getZolixIdn(void);

	void   iecPrepare(int iecAddr);
	string getIdnResponse(void);


protected:
	void run(void);


public:
	static string int2String(int val);
	static string double2String(double val);
};
