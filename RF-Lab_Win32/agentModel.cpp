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
				 , _runState(C_MODEL_RUNSTATES_OPENCOM)
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
	// start the antenna meassure model
	if (pAgtCom[C_COMINST_ROT]) {
		pAgtCom[C_COMINST_ROT]->start();

		_running = TRUE;
		while (_running) {
			// model's working loop
			switch (_runState) {
			case C_MODEL_RUNSTATES_OPENCOM:
			{
				char buf[C_BUF_SIZE];
				agentComReq comReqData;
				comReqData.cmd = C_COMREQ_OPEN;
				snprintf(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d", 3, CBR_19200, 8, NOPARITY, ONESTOPBIT);  // COM port and its parameters
				comReqData.parm = string(buf);
				send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
				_runState = C_MODEL_RUNSTATES_OPENCOM_WAIT;
			}
			break;

			case C_MODEL_RUNSTATES_OPENCOM_WAIT:
			{
				if (pAgtComRsp[C_COMINST_ROT]->has_value()) {
					agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]));
					if (comRspData.stat == C_COMRSP_OK) {
						_runState = C_MODEL_RUNSTATES_INIT;
					}
					else {
						_runState = C_MODEL_RUNSTATES_NOOP;
					}
				}
			}
			break;

			case C_MODEL_RUNSTATES_INIT:
			{
				agentComReq comReqData;

				// Zollix commands to be sent
				comReqData.cmd = C_COMREQ_COM_SEND;
				comReqData.parm = string("VX,20000\n");
				send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

				comReqData.parm = string("AX,30000\n");
				send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

				comReqData.parm = string("FX,2500\n");
				send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

				_runState = C_MODEL_RUNSTATES_INIT_WAIT;
			}
			break;

			case C_MODEL_RUNSTATES_INIT_WAIT:
			{
				if (pAgtComRsp[C_COMINST_ROT]->has_value()) {
					// consume each reported state
					agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]));
					if (comRspData.stat == C_COMRSP_FAIL) {
						_runState = C_MODEL_RUNSTATES_CLOSE_COM;
					}
				}
				else {
					// no more acks
					_runState = C_MODEL_RUNSTATES_RUNNING;
				}
			} 
			break;

			case C_MODEL_RUNSTATES_RUNNING:
			{
			} 
			break;

			case C_MODEL_RUNSTATES_CLOSE_COM:
			{
				_runState = C_MODEL_RUNSTATES_CLOSE_COM_WAIT;
			} 
			break;

			case C_MODEL_RUNSTATES_CLOSE_COM_WAIT:
			{
				_runState = C_MODEL_RUNSTATES_NOOP;
			}
			break;

			case C_MODEL_RUNSTATES_NOOP:
			default:
				Sleep(250);
				break;
			}

			Sleep(10);
		}

		// send shutdown message
		{
			// clear result buffers
			agentComReq comReqData;
			comReqData.cmd = C_COMREQ_END;
			comReqData.parm = string();

			// send shutdown request for each active agent
			for (int i = 0; i < C_COMINST__COUNT; i++) {
				if (pAgtComReq[i]) {
					send(*(pAgtComReq[i]), comReqData);
				}
			}

			// wait for each reply message
			for (int i = 0; i < C_COMINST__COUNT; i++) {
				if (pAgtComReq[i]) {
					agentComRsp comRsp;

					// consume until END response is received
					do {
						comRsp = receive(*(pAgtComRsp[i]));
					} while (comRsp.stat != C_COMRSP_END);
				}
			}

		}
	}

	// Move the agent to the finished state.
	_done = TRUE;
	done();
}
