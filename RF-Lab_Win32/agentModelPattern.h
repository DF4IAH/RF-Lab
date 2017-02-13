#pragma once

#include "agentModel.h"
#include "agentModelVariant.h"

#include "agentCom.h"

using namespace concurrency;
using namespace std;


enum C_MODELPATTERN_RUNSTATES_ENUM {
	C_MODELPATTERN_RUNSTATES_NOOP = 0,
	C_MODELPATTERN_RUNSTATES_OPENCOM,
	C_MODELPATTERN_RUNSTATES_OPENCOM_WAIT,
	C_MODELPATTERN_RUNSTATES_INIT,
	C_MODELPATTERN_RUNSTATES_INIT_WAIT,
	C_MODELPATTERN_RUNSTATES_RUNNING,
	C_MODELPATTERN_RUNSTATES_GOTO_X,
	C_MODELPATTERN_RUNSTATES_CLOSE_COM,
	C_MODELPATTERN_RUNSTATES_CLOSE_COM_WAIT
};


class agentModelPattern : public agentModelVariant
{
private:
	unbounded_buffer<agentComReq_t>		*pAgtComReq[C_COMINST__COUNT];
	unbounded_buffer<agentComRsp_t>		*pAgtComRsp[C_COMINST__COUNT];
	agentCom							*pAgtCom[C_COMINST__COUNT];
	int									 lastTickPos;

	LPVOID								 _arg;


public:
	explicit agentModelPattern(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt);

public:
	void run(void);

private:
	void sendPos(int tickPos);
	void setLastTickPos(int pos);

public:
	/* overwriting agentModel member functions() */
	bool isRunning(void);
	void Release(void);
	bool shutdown(void);
	void wmCmd(int wmId, LPVOID arg = nullptr);
	int  requestPos(void);
	int  getLastTickPos(void);

};
