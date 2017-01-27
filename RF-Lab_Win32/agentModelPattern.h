#pragma once

#include "agentCom.h"
#include "agentModel.h"


class agentModelPattern : public agentModel
{
private:
	bool								 _running;
	short								 _runState;
	bool								 _done;

	unbounded_buffer<agentComReq>		*pAgtComReq[C_COMINST__COUNT];
	unbounded_buffer<agentComRsp>		*pAgtComRsp[C_COMINST__COUNT];
	agentCom							*pAgtCom[C_COMINST__COUNT];


public:
	explicit agentModelPattern(void);
	//	explicit agentModelPattern(ISource<agentModelReq>& src, ITarget<agentModelRsp>& tgt);

protected:
	/* overwriting agentModel member functions() */
	bool isRunning();
	void Release();
	bool shutdown();
	void wmCmd(int wmId);

protected:
	void run();

};
