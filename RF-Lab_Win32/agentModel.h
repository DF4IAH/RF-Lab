#pragma once

//#include "agentModelPattern.h"

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
	C_MODEL_RUNSTATES_GOTO0,
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
	ISource<agentModelReq>				*_src;
	ITarget<agentModelRsp>				*_tgt;
	agentModel							*_curModel;

public:
	static agentModel					*am;


public:
	explicit agentModel();
	explicit agentModel(ISource<agentModelReq> *src, ITarget<agentModelRsp> *tgt);
	~agentModel(void);

protected:
	void run();

public:
	/* default class functions() to be overwritten */
	static bool isRunning();
	static void Release();
	static bool shutdown();
	static void wmCmd(int wmId);

};
