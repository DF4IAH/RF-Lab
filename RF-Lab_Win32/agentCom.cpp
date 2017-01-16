#include "stdafx.h"
#include "agentCom.h"


agentCom::agentCom(ISource<agentComReq>& src, ITarget<agentComRsp>& tgt)
				 : _running(FALSE)
				 , _done(FALSE)
				 , _src(src)
				 , _tgt(tgt)
{
}

void agentCom::Release()
{
	if (_running) {
		_running = FALSE;
	}

	while (!_done) {
		Sleep(1);
	}
}


inline bool agentCom::isRunning()
{
	return _running;
}

void agentCom::run()
{
	agentComRsp comRsp;

	_running = TRUE;
	while (_running) {
		// clear result buffers
		comRsp.stat = C_COMRSP_FAIL;
		comRsp.data = wstring();

		// receive the request
		agentComReq comReq = receive(_src);

		// command decoder
		switch (comReq.cmd) {

		case C_COMREQ_END:
			comRsp.stat = C_COMRSP_END;
			_running = FALSE;
			break;

		case C_COMREQ_SER_SNDRSV:
			// send parm string via serial port and wait for result
			//request.parm;
			comRsp.data = wstring(L"SEND - RECEIVE");
			comRsp.stat = C_COMRSP_OK;
			break;
		}

		// send the response
		send(_tgt, comRsp);
	}

	// Move the agent to the finished state.
	_done = TRUE;
	done();
}
