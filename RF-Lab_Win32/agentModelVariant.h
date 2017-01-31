#pragma once

#include <agents.h>

#include "agentModel.h"

using namespace concurrency;
using namespace std;


enum C_MODEL_RUNSTATES_ENUM {
	C_MODEL_RUNSTATES_NOOP = 0
};


class agentModelVariant
{
protected:
	bool								 _running;
	short								 _runState;
	bool								 _done;

	ISource<agentModelReq_t>			*_src;
	ITarget<agentModelRsp_t>			*_tgt;


public:
	virtual void run(void);

public:
	agentModelVariant(void);
	virtual ~agentModelVariant(void);

	/* to be overwritten by the agentModelXXX classes */
	virtual bool isRunning(void);
	virtual void Release(void);
	virtual bool shutdown(void);
	virtual void wmCmd(int wmId, LPVOID arg);

};
