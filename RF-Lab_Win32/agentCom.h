#pragma once

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>

using namespace concurrency;
using namespace std;


enum C_COMREQ_ENUM {
	C_COMREQ_END = 0,
	C_COMREQ_SER_SNDRSV
};

enum C_COMRSP_ENUM {
	C_COMRSP_END = 0,
	C_COMRSP_FAIL,
	C_COMRSP_OK
};


struct agentComReq
{
	SHORT				cmd;
	wstring				parm;
};

struct agentComRsp
{
	SHORT				stat;
	wstring				data;
};


class agentCom : public agent
{
private:
	bool					_running;
	ISource<agentComReq>&	_comSrc;
	ITarget<agentComRsp>&	_comTgt;


public:
	explicit agentCom(ISource<agentComReq>& comSrc, ITarget<agentComRsp>& comTgt);
	bool isRunning();

protected:
	void run();

};
