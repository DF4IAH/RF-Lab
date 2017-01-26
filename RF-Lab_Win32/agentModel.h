#pragma once

#include "agentCom.h"

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>

using namespace concurrency;
using namespace std;


enum C_MODEL_RUNSTATES_ENUM {
	C_MODEL_RUNSTATES_NOOP = 0,
	C_MODEL_RUNSTATES_OPENCOM,
	C_MODEL_RUNSTATES_OPENCOM_WAIT,
	C_MODEL_RUNSTATES_INIT,
	C_MODEL_RUNSTATES_INIT_WAIT,
	C_MODEL_RUNSTATES_RUNNING,
	C_MODEL_RUNSTATES_CLOSE_COM,
	C_MODEL_RUNSTATES_CLOSE_COM_WAIT
};


enum C_MODELREQ_ENUM {
	C_MODELREQ_END = 0
};

enum C_MODELRSP_ENUM {
	C_MODELRSP_END = 0
};


struct agentModelReq
{
	SHORT								 cmd;
};

struct agentModelRsp
{
	SHORT								 stat;
};


class agentModel : public agent
{
private:
	bool								 _running;
	short								 _runState;
	bool								 _done;
	ISource<agentModelReq>&				 _src;
	ITarget<agentModelRsp>&				 _tgt;

	unbounded_buffer<agentComReq>		*pAgtComReq[C_COMINST__COUNT];
	unbounded_buffer<agentComRsp>		*pAgtComRsp[C_COMINST__COUNT];
	//overwrite_buffer<agentComRsp>		*pAgtComRsp[C_COMINST__COUNT];
	agentCom							*pAgtCom[C_COMINST__COUNT];



public:
	explicit agentModel(ISource<agentModelReq>& src, ITarget<agentModelRsp>& tgt);
	bool isRunning();
	void Release();
	bool shutdown();
	void wmCmd(int wmId);

protected:
	void run();

};
