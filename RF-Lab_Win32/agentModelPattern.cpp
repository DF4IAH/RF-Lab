#include "stdafx.h"
#include "agentModelPattern.h"

#include "resource.h"
#include "WinSrv.h"


template <class T>  void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		//(*ppT)->Release();
		*ppT = nullptr;
	}
}


agentModelPattern::agentModelPattern(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt)
				 : pAgtComReq{ nullptr }
				 , pAgtComRsp{ nullptr }
				 , pAgtCom{ nullptr }
				 , _arg(nullptr)
				 , lastTickPos(0)
{
	/* enter first step of FSM */
	_runState = C_MODELPATTERN_RUNSTATES_OPENCOM;

	for (int i = 0; i < 1 /*C_COMINST_ENUM*/; ++i) {
		pAgtComReq[i] = new unbounded_buffer<agentComReq>;
		pAgtComRsp[i] = new unbounded_buffer<agentComRsp>;
		if (pAgtComReq[i] && pAgtComRsp[i]) {
			pAgtCom[i] = new agentCom(*(pAgtComReq[i]), *(pAgtComRsp[i]));
		}
	}
}


void agentModelPattern::run(void)
{
	// start the antenna meassure model
	if (pAgtCom[C_COMINST_ROT]) {
		pAgtCom[C_COMINST_ROT]->start();

		_running = TRUE;
		while (_running) {
			// model's working loop
			switch (_runState) {
			case C_MODELPATTERN_RUNSTATES_OPENCOM:
			{
				char buf[C_BUF_SIZE];
				agentComReq comReqData;
				comReqData.cmd = C_COMREQ_OPEN;
				snprintf(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d", 3, CBR_19200, 8, NOPARITY, ONESTOPBIT);  // COM port and its parameters
				comReqData.parm = string(buf);
				send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
				_runState = C_MODELPATTERN_RUNSTATES_OPENCOM_WAIT;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_OPENCOM_WAIT:
			{
				agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]));
				if (comRspData.stat == C_COMRSP_OK) {
					_runState = C_MODELPATTERN_RUNSTATES_INIT;
				}
				else {
					_runState = C_MODELPATTERN_RUNSTATES_CLOSE_COM;
				}
			}
			break;

			case C_MODELPATTERN_RUNSTATES_INIT:
			{
				agentComReq comReqData;

				// Zollix commands to be sent
				comReqData.cmd = C_COMREQ_COM_SEND;

				comReqData.parm = string("VX,20000\r");				// Zolix: top speed 20.000 ticks per sec
				send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

				comReqData.parm = string("AX,30000\r");				// Zolix: acceleration speed 30.000
				send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

				comReqData.parm = string("FX,2500\r");				// Zolix: initial speed 2.500 ticks per sec
				send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

				// read current tick position of rotor and store value
				(void) requestPos();

				_runState = C_MODELPATTERN_RUNSTATES_INIT_WAIT;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_INIT_WAIT:
			{
				agentComRsp comRspData;

				//Sleep(100);
				bool state = try_receive(*(pAgtComRsp[C_COMINST_ROT]), comRspData);
				if (state) {
					// consume each reported state
					if (comRspData.stat == C_COMRSP_FAIL) {
						_runState = C_MODELPATTERN_RUNSTATES_CLOSE_COM;
					}
				}
				else {
					_runState = C_MODELPATTERN_RUNSTATES_RUNNING;
				}
			}
			break;

			case C_MODELPATTERN_RUNSTATES_RUNNING:
			{

				Sleep(10);
			}
			break;

			case C_MODELPATTERN_RUNSTATES_GOTO_X:
			{
				agentComReq comReqData;
				agentComRsp comRspData;
				char cbuf[C_BUF_SIZE];

				int gotoMilliPos = 0;
				if (_arg) {
					gotoMilliPos = *((int*)_arg);
					_arg = nullptr;
				}
				
				requestPos();
				sendPos(gotoMilliPos * 0.8);
				_runState = C_MODELPATTERN_RUNSTATES_RUNNING;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_CLOSE_COM:
			{
				agentComReq comReqData;
				comReqData.cmd = C_COMREQ_END;
				comReqData.parm = string();

				// send shutdown request for each active agent
				for (int i = 0; i < C_COMINST__COUNT; i++) {
					if (pAgtComReq[i]) {
						send(*(pAgtComReq[i]), comReqData);
					}
				}
				_runState = C_MODELPATTERN_RUNSTATES_CLOSE_COM_WAIT;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_CLOSE_COM_WAIT:
			{
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
				_runState = C_MODELPATTERN_RUNSTATES_NOOP;
				_running = false;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_NOOP:
			default:
				Sleep(250);
				break;
			}
		}
	}

	// Move the agent to the finished state.
	_done = TRUE;
}


int agentModelPattern::requestPos(void)
{
	agentComReq comReqData;
	agentComRsp comRspData;
	int pos = 0;

	// request position
	comReqData.cmd = C_COMREQ_COM_SEND_RECEIVE;
	comReqData.parm = string("?X\r");
	send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

	// read current position
	comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]));
	if (comRspData.stat == C_COMRSP_DATA) {
		int posDiff = 0;

		const char* str_start = strrchr(comRspData.data.c_str(), '?');
		if (str_start) {
			sscanf_s(str_start, "?X,%d", &pos);
			agentModel::setLastTickPos(pos);
		}
	}
	return pos;
}

void agentModelPattern::sendPos(int tickPos)
{
	agentComReq comReqData;
	int posDiff = tickPos - getLastTickPos();
	char cbuf[C_BUF_SIZE];

	snprintf(cbuf, C_BUF_SIZE - 1, "%cX,%d\r", (posDiff > 0 ? '+' : '-'), abs(posDiff));
	comReqData.cmd = C_COMREQ_COM_SEND;
	comReqData.parm = string(cbuf);
	send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
}

inline bool agentModelPattern::isRunning(void)
{
	return _running;
}


void agentModelPattern::Release(void)
{
	// signaling
	if (_running) {
		(void)shutdown();
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


bool agentModelPattern::shutdown(void)
{
	bool old_running = _running;

	// signal to shutdown
	//_running = FALSE;
	_runState = C_MODELPATTERN_RUNSTATES_CLOSE_COM;

	return old_running;
}

void agentModelPattern::wmCmd(int wmId, LPVOID arg)
{
	switch (wmId)
	{
	case ID_ROTOR_GOTO_0:
		if (_runState == C_MODELPATTERN_RUNSTATES_RUNNING) {
			_arg = nullptr;
			_runState = C_MODELPATTERN_RUNSTATES_GOTO_X;
		}
		break;

	case ID_ROTOR_GOTO_X:
		if (arg) {
			if (_runState == C_MODELPATTERN_RUNSTATES_RUNNING) {
				_arg = arg;
				_runState = C_MODELPATTERN_RUNSTATES_GOTO_X;
			}
		}
		break;
	}
}

void agentModelPattern::setLastTickPos(int pos)
{
	lastTickPos = pos;
}

int agentModelPattern::getLastTickPos(void)
{
	return lastTickPos;
}
