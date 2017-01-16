#include "stdafx.h"
#include "agentModel.h"


agentModel::agentModel(ISource<agentModelReq>& src, ITarget<agentModelRsp>& tgt)
				 : _running(FALSE)
				 , _done(FALSE)
				 , _src(src)
				 , _tgt(tgt)
{
}

void agentModel::Release()
{
	if (_running) {
		shutdown();
	}

	while (!_done) {
		Sleep(1);
	}
}

bool agentModel::shutdown()
{
	bool old_running = _running;

	_running = FALSE;
	return old_running;
}


void agentModel::run()
{
	//agentComReq comReq;
	//pAgtCom[0] = new agentCom(ub_agtCom_req, ob_agtCom_rsp);

	_running = TRUE;
	while (_running) {
		// clear result buffers
		//comReq.cmd  = C_COMREQ_END;
		//comReq.parm = wstring();

		// send the request
		//send(_comTgt, comReq);

		// read the response
		//agentComRsp comRsp = receive(_comSrc);

		Sleep(1);
	}

	// Move the agent to the finished state.
	_done = TRUE;
	done();
}
