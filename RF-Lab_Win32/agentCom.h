#pragma once

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>

using namespace concurrency;
using namespace std;


#define AGENT_COM_RECEIVE_TIMEOUT				   500


enum C_COMINST_ENUM {
	C_COMINST_ROT = 0,
	C_COMINST_TX,
	C_COMINST_RX,
	C_COMINST__COUNT
};

enum C_USBREQ_ENUM {
	C_USBREQ_END = 0,
	C_USBREQ_DO_REGISTRATION,
};

enum C_USBRSP_ENUM {
	C_USBRSP_END = 0,
	C_USBRSP_REGISTRATION_LIST,
	C_USBRSP_OK,
};

enum C_COMREQ_ENUM {
	C_COMREQ_END = 0,
	C_COMREQ_OPEN,
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


typedef struct agentUsbReq
{
	SHORT								 cmd;
} agentUsbReq_t;

typedef struct agentUsbRsp
{
	SHORT								 stat;
	void								*data;
} agentUsbRsp_t;


typedef struct agentComReq
{
	SHORT								 cmd;
	string								 parm;
} agentComReq_t;

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

	bool								 _isIec;
	int									 _iecAddr;


public:
	explicit agentCom(ISource<agentComReq>& src, ITarget<agentComRsp>& tgt);
	void start(void);
	bool isRunning(void);
	bool isIec(void);
	void setIecAddr(int iecAddr);
	int  getIecAddr(void);
	void Release(void);
	bool shutdown(void);


protected:
	void run(void);


public:
	static string int2String(int val);
	static string double2String(double val);

};
