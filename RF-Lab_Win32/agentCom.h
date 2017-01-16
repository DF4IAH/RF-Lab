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
	bool					_done;
	ISource<agentComReq>&	_src;
	ITarget<agentComRsp>&	_tgt;


public:
	explicit agentCom(ISource<agentComReq>& src, ITarget<agentComRsp>& tgt);
	void Release();
	bool isRunning();

protected:
	void run();

};
