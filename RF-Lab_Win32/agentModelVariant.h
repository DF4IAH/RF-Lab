#pragma once

#include <agents.h>

#include "agentModel.h"

using namespace concurrency;
using namespace std;


class agentModelVariant
{
protected:
	bool								 _running;
	short								 _runState;
	bool								 _done;

	ISource<agentModelReq_t>			*_src;
	ITarget<agentModelRsp_t>			*_tgt;


public:
	void run(void);

public:
	agentModelVariant(void);
	~agentModelVariant(void);

	/* to be overwritten by the agentModelXXX classes */
	bool isRunning(void);
	void Release(void);
	bool shutdown(void);
	void wmCmd(int wmId);

};
