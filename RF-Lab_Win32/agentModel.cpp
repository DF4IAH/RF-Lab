#include "stdafx.h"
#include "agentModel.h"


agentModel::agentModel(ISource<agentComRsp>& comSrc, ITarget<agentComReq>& comTgt)
	: _running(FALSE)
	, _comSrc(comSrc)
	, _comTgt(comTgt)
{
}

bool agentModel::shutdown()
{
	bool old_running = _running;
	_running = FALSE;
	return old_running;
}

void agentModel::run()
{
	agentComReq comReq;

	_running = TRUE;
	while (_running) {
		// clear result buffers
		comReq.cmd  = C_COMREQ_END;
		comReq.parm = wstring();

		// send the request
		send(_comTgt, comReq);

		// read the response
		agentComRsp comRsp = receive(_comSrc);
	}

	// Move the agent to the finished state.
	done();
}
