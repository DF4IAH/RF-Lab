#pragma once

#include "agentCom.h"

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>

using namespace concurrency;
using namespace std;


enum C_MODELREQ_ENUM {
	C_MODELREQ_END = 0
};

enum C_MODELRSP_ENUM {
	C_MODELRSP_END = 0
};


struct agentModelReq
{
	SHORT				cmd;
};

struct agentModelRsp
{
	SHORT				stat;
};



class agentModel : public agent
{
private:
	bool						_running;
	bool						_done;
	ISource<agentModelReq>&		_src;
	ITarget<agentModelRsp>&		_tgt;


public:
	explicit agentModel(ISource<agentModelReq>& src, ITarget<agentModelRsp>& tgt);
	void Release();
	bool shutdown();

protected:
	void run();

};
