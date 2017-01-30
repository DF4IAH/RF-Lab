#pragma once

#include "agentModel.h"
#include "agentModelVariant.h"

#include "agentCom.h"

using namespace concurrency;
using namespace std;


class agentModelPattern : public agentModelVariant
{
private:
	unbounded_buffer<agentComReq_t>		*pAgtComReq[C_COMINST__COUNT];
	unbounded_buffer<agentComRsp_t>		*pAgtComRsp[C_COMINST__COUNT];
	agentCom							*pAgtCom[C_COMINST__COUNT];


public:
	explicit agentModelPattern(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt);

public:
	void run(void);

public:
	/* overwriting agentModel member functions() */
	bool isRunning(void);
	void Release(void);
	bool shutdown(void);
	void wmCmd(int wmId);

};
