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
	/* overwriting agentModel member functions() */
	bool isRunning();
	void Release();
	bool shutdown();
	void wmCmd(int wmId);

protected:
	void run();

};
