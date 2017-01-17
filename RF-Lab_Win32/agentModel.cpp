#include "stdafx.h"
#include "agentModel.h"

#include "WinSrv.h"


template <class T>  void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		//(*ppT)->Release();
		*ppT = nullptr;
	}
}



agentModel::agentModel(ISource<agentModelReq>& src, ITarget<agentModelRsp>& tgt)
				 : _running(FALSE)
				 , _done(FALSE)
				 , _src(src)
				 , _tgt(tgt)
				 , pAgtComReq { nullptr }
				 , pAgtComRsp { nullptr }
				 , pAgtCom    { nullptr }
{
	for (int i = 0; i < 1 /*C_COMINST_ENUM*/; ++i) {
		pAgtComReq[i] = new unbounded_buffer<agentComReq>;
		pAgtComRsp[i] = new overwrite_buffer<agentComRsp>;
		if (pAgtComReq[i] && pAgtComRsp[i]) {
			pAgtCom[i] = new agentCom(*(pAgtComReq[i]), *(pAgtComRsp[i]));
		}
	}
}


inline bool agentModel::isRunning()
{
	return _running;
}


void agentModel::Release()
{
	// signaling
	if (_running) {
		(void) shutdown();
	}

	// wait until all threads are done
	while (!_done) {
		Sleep(1);
	}

	// release objects
	for (int i = 0; i < C_COMINST__COUNT; i++) {
		SafeRelease(&(pAgtCom[i]));
		SafeRelease(&(pAgtComReq[i]));
		SafeRelease(&(pAgtComRsp[i]));
	}
}


bool agentModel::shutdown()
{
	bool old_running = _running;

	// signal to shutdown
	_running = FALSE;

	return old_running;
}


void agentModel::run()
{
	// start the antenna measure model
	if (pAgtCom[C_COMINST_ROT]) {
		pAgtCom[C_COMINST_ROT]->start();

		_running = TRUE;
		while (_running) {
			// clear result buffers
			agentComReq comReqData;
			comReqData.cmd  = C_COMREQ_END;
			comReqData.parm = wstring();

			// send the request
			send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

			// read the response
			agentComRsp comRsp = receive(*(pAgtComRsp[C_COMINST_ROT]));
			if (comRsp.stat == C_COMRSP_END) {
				_running = FALSE;
			}

			Sleep(1);
		}
	}

	// Move the agent to the finished state.
	_done = TRUE;
	done();
}
