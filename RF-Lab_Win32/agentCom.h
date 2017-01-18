#pragma once

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>

using namespace concurrency;
using namespace std;


enum C_COMINST_ENUM {
	C_COMINST_ROT = 0,
	C_COMINST_TX,
	C_COMINST_RX,
	C_COMINST__COUNT
};

enum C_COMREQ_ENUM {
	C_COMREQ_END = 0,
	C_COMREQ_OPEN,
	C_COMREQ_COM_SEND,
	C_COMREQ_CLOSE
};

enum C_COMRSP_ENUM {
	C_COMRSP_END = 0,
	C_COMRSP_FAIL,
	C_COMRSP_OK
};

const int C_BUF_SIZE = 256;


struct agentComReq
{
	SHORT								 cmd;
	string								 parm;
};

struct agentComRsp
{
	SHORT								 stat;
	string								 data;
};


class agentCom : public agent
{
private:
	bool								 _running;
	bool								 _done;
	ISource<agentComReq>&				 _src;
	ITarget<agentComRsp>&				 _tgt;

	HANDLE								 _hCom;


public:
	explicit agentCom(ISource<agentComReq>& src, ITarget<agentComRsp>& tgt);
	bool isRunning();
	void Release();
	bool shutdown();

protected:
	void run();

};
