#include "stdafx.h"
#include "agentModelPattern.h"
#include "agentModel.h"

#include "resource.h"
#include "WinSrv.h"

#include <process.h>


template <class T>  void SafeReleaseDelete(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		delete *ppT;
		*ppT = nullptr;
	}
}

template <class T>  void SafeDelete(T **ppT)
{
	if (*ppT)
	{
		delete *ppT;
		*ppT = nullptr;
	}
}


agentModelPattern::agentModelPattern(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, class agentModel *am, AGENT_ALL_SIMUMODE_t mode)
				 : pAgtComReq{ nullptr }
				 , pAgtComRsp{ nullptr }
				 , pAgtCom{ nullptr }
				 , _arg(nullptr)

				 , pAgtMod(am)

				 , hThreadAgtUsbTmc(nullptr)
				 , sThreadDataAgentModelPattern( { 0 } )

				 , ai( { 0 } )

				 , pAgtUsbTmc(nullptr)

				 , processing_ID(0)
				 , simuMode(mode)

				 , initState(0)

				 , lastTickPos(0)

				 , txOn(false)
				 , txFrequency(0)
				 , txPower(0)

				 , rxFrequency(0.0)
				 , rxSpan(0.0)
				 , rxLevelMax(0.0)
{
	/* Clear instrument structure */
	memset(&ai, 0, sizeof(ArrayOfInstruments_t));
}

ArrayOfInstruments_t* agentModelPattern::getAIPtr(void)
{
	return &ai;
}

inline bool agentModelPattern::isRunning(void)
{
	return _running;
}

bool agentModelPattern::shutdown(void)
{
	if (_running) {
		_runState = C_MODELPATTERN_RUNSTATES_CLOSE_COM;
		_noWinMsg = true;
	}
	return _running;
}

void agentModelPattern::Release(void)
{
	/* signaling and wait until all threads are done */
	if (_running) {
		(void)shutdown();
	}

	/* Wait until run() thread terminates */
	agent::wait(this);
}


void agentModelPattern::agentsInit(void)
{
	/* Init the USB_TMC communication */
	pAgtUsbTmcReq = new unbounded_buffer<agentUsbReq>;
	pAgtUsbTmcRsp = new unbounded_buffer<agentUsbRsp>;

	for (int i = 0; i < C_COMINST__COUNT; ++i) {
		pAgtComReq[i] = new unbounded_buffer<agentComReq>;
		pAgtComRsp[i] = new unbounded_buffer<agentComRsp>;
		pAgtCom[i] = nullptr;

		if (pAgtComReq[i] && pAgtComRsp[i]) {
			switch (i) {
			case C_COMINST_ROT:
				pAgtCom[i] = new agentCom(*(pAgtComReq[i]), *(pAgtComRsp[i]));
				break;

			case C_COMINST_TX:
				if (!(simuMode & AGENT_ALL_SIMUMODE_NO_TX)) {
					pAgtCom[i] = new agentCom(*(pAgtComReq[i]), *(pAgtComRsp[i]));
				}
				break;

			case C_COMINST_RX:
				if (!(simuMode & AGENT_ALL_SIMUMODE_NO_RX)) {
					pAgtCom[i] = new agentCom(*(pAgtComReq[i]), *(pAgtComRsp[i]));
				}
				break;
			}
		}
	}
}

void agentModelPattern::agentsShutdown(void)
{
	/* Signal the USB_TMC object to shutdown */
	pAgtUsbTmc->shutdown();

	/* Delay loop as long it needs to stop this run() method */
	while (!pAgtUsbTmc->isDone()) {
		Sleep(25);
	}

	/* Release USB-TMC connection */
	agent::wait(pAgtUsbTmc, AGENT_PATTERN_RECEIVE_TIMEOUT);
	SafeReleaseDelete(&pAgtUsbTmc);
	SafeDelete(&pAgtUsbTmcReq);
	SafeDelete(&pAgtUsbTmcRsp);

	/* Release serial connection objects */
	for (int i = 0; i < C_COMINST__COUNT; i++) {
		if (pAgtCom[i]) {
			agent::wait(pAgtCom[i], AGENT_PATTERN_RECEIVE_TIMEOUT);
		}
		SafeReleaseDelete(&(pAgtCom[i]));
		SafeDelete(&(pAgtComReq[i]));
		SafeDelete(&(pAgtComRsp[i]));
	}
}


void agentModelPattern::threadsStart(void)
{
	sThreadDataAgentModelPattern.threadNo = 1;
	sThreadDataAgentModelPattern.o = this;
	processing_ID = C_MODELPATTERN_PROCESS_NOOP;
	_beginthread(&procThreadProcessID, 0, &sThreadDataAgentModelPattern);

	hThreadAgtUsbTmc = CreateMutex(NULL, FALSE, NULL);		// Mutex is still free
	pAgtUsbTmc = new USB_TMC(pAgtUsbTmcReq, pAgtUsbTmcRsp);
}

void agentModelPattern::threadsStop(void)
{
	/* End thread */
	processing_ID = C_MODELPATTERN_PROCESS_END;
	//pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Shutting down ...", L"");
	WaitForSingleObject(hThreadAgtUsbTmc, 10000L);			// Wait until the thread has ended (timeout = 10 secs
	CloseHandle(hThreadAgtUsbTmc);
}


void agentModelPattern::run(void)
{
	/* Delay until window is up and ready */
	while (!(WinSrv::srvReady())) {
		Sleep(25);
	}

	/* model's working loop */
	_running	= true;
	_runReinit	= false;
	_runState	= C_MODELPATTERN_RUNSTATES_BEGIN;
	while (_running) {
		try {
			switch (_runState) {
			case C_MODELPATTERN_RUNSTATES_BEGIN:
			{
				/* In case of a REINIT, clear this flag */
				_runReinit = false;

				if (!_noWinMsg)
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Starting COM threads ...", L"");

				/* Start the communication agents */
				agentsInit();

				/* Start thread for process IDs to operate */
				threadsStart();

				if (!_noWinMsg)
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM threads created", L"");

				_runState = C_MODELPATTERN_RUNSTATES_OPENCOM;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_OPENCOM:
			{
				char buf[C_BUF_SIZE];
				agentComReq comReqData;

				/* Start serial communication servers */
				{
					if (pAgtCom[C_COMINST_ROT]) {
						pAgtCom[C_COMINST_ROT]->start();
					}

					if (pAgtCom[C_COMINST_TX]) {
						pAgtCom[C_COMINST_TX]->start();
					}

					if (pAgtCom[C_COMINST_RX]) {
						pAgtCom[C_COMINST_RX]->start();
					}
				}

				initState = 0x01;
				if (!_noWinMsg)
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Opening COMs ...", L"");

				/* Open Rotor */
				if (pAgtCom[C_COMINST_ROT]) {
					try {
						comReqData.cmd = C_COMREQ_OPEN;
						_snprintf_s(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d",
							C_ROT_COM_PORT, C_ROT_COM_BAUD, C_ROT_COM_BITS, C_ROT_COM_PARITY, C_ROT_COM_STOPBITS);								// Zolix USB port - 19200 baud, 8N1
						comReqData.parm = string(buf);
						send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
						agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
						if (comRspData.stat == C_COMRSP_OK) {
							initState = 0x02;
							addSerInstrument(pAgtCom[C_COMINST_ROT],
								C_ROT_COM_PORT, C_ROT_COM_BAUD, C_ROT_COM_BITS, C_ROT_COM_PARITY, C_ROT_COM_STOPBITS,
								false/* IEC*/, 0/* IEC addr */);

							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 1 ~ rotor port opened", L"");
						}
					}
					catch (const Concurrency::operation_timed_out& e) {
						(void)e;
					}
				}

				/* Open TX */
				if (pAgtCom[C_COMINST_TX]) {
					try {
						comReqData.cmd = C_COMREQ_OPEN;
						if (pAgtCom[C_COMINST_TX]->isIec()) {
							pAgtCom[C_COMINST_TX]->setIecAddr(C_TX_COM_IEC_ADDR);																// IEC625: R&S SMR40: address == 28
							_snprintf_s(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d",
								C_TX_COM_IEC_PORT, C_TX_COM_IEC_BAUD, C_TX_COM_IEC_BITS, C_TX_COM_IEC_PARITY, C_TX_COM_IEC_STOPBITS);			// IEC625: 19200 baud, 8N1
							comReqData.parm = string(buf);
							send(*(pAgtComReq[C_COMINST_TX]), comReqData);
							agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							if (comRspData.stat == C_COMRSP_OK) {
								initState = 0x03;
								addSerInstrument(pAgtCom[C_COMINST_TX],
									C_TX_COM_IEC_PORT, C_TX_COM_IEC_BAUD, C_TX_COM_IEC_BITS, C_TX_COM_IEC_PARITY, C_TX_COM_IEC_STOPBITS,
									true /* is IEC */, C_TX_COM_IEC_ADDR);

								if (!_noWinMsg)
									pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 2 ~ TX IEC port opened", L"");
							}
						}
						else {
							_snprintf_s(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d",
								C_TX_COM_PORT, C_TX_COM_BAUD, C_TX_COM_BITS, C_TX_COM_PARITY, C_TX_COM_STOPBITS);								// serial port - 9600 baud, 8N1
							comReqData.parm = string(buf);
							send(*(pAgtComReq[C_COMINST_TX]), comReqData);
							agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							if (comRspData.stat == C_COMRSP_OK) {
								initState = 0x03;
								addSerInstrument(pAgtCom[C_COMINST_TX],
									C_TX_COM_PORT, C_TX_COM_BAUD, C_TX_COM_BITS, C_TX_COM_PARITY, C_TX_COM_STOPBITS,
									false /* is IEC */, 0);

								if (!_noWinMsg)
									pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 2 ~ TX port opened", L"");
							}
						}
					}
					catch (const Concurrency::operation_timed_out& e) {
						(void)e;
					}
				}

				/* Open RX */
				if (pAgtCom[C_COMINST_RX]) {
					try {
						comReqData.cmd = C_COMREQ_OPEN;
						if (pAgtCom[C_COMINST_RX]->isIec()) {
							pAgtCom[C_COMINST_RX]->setIecAddr(C_RX_COM_IEC_ADDR);																// IEC625: R&S FSEK20: address == 20
							_snprintf_s(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d",
								C_RX_COM_IEC_PORT, C_RX_COM_IEC_BAUD, C_RX_COM_IEC_BITS, C_RX_COM_IEC_PARITY, C_RX_COM_IEC_STOPBITS);			// IEC625: 19200 baud, 8N1
							comReqData.parm = string(buf);
							send(*(pAgtComReq[C_COMINST_RX]), comReqData);
							agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							if (comRspData.stat == C_COMRSP_OK) {
								initState = 0x04;
								addSerInstrument(pAgtCom[C_COMINST_TX],
									C_RX_COM_IEC_PORT, C_RX_COM_IEC_BAUD, C_RX_COM_IEC_BITS, C_RX_COM_IEC_PARITY, C_RX_COM_IEC_STOPBITS,
									true /* is IEC */, C_RX_COM_IEC_ADDR);

								if (!_noWinMsg)
									pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 3 ~ RX IEC port opened", L"");
							}
						}
						else {
							_snprintf_s(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d",
								C_RX_COM_PORT, C_RX_COM_BAUD, C_RX_COM_BITS, C_RX_COM_PARITY, C_RX_COM_STOPBITS);								// serial port - 9600 baud, 8N1
							comReqData.parm = string(buf);
							send(*(pAgtComReq[C_COMINST_RX]), comReqData);
							agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							if (comRspData.stat == C_COMRSP_OK) {
								initState = 0x04;
								addSerInstrument(pAgtCom[C_COMINST_TX],
									C_RX_COM_PORT, C_RX_COM_BAUD, C_RX_COM_BITS, C_RX_COM_PARITY, C_RX_COM_STOPBITS,
									false /* is IEC */, 0);

								if (!_noWinMsg)
									pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 3 ~ RX port opened", L"");
							}
						}
					}
					catch (const Concurrency::operation_timed_out& e) {
						(void)e;
					}
				}

				_runState = C_MODELPATTERN_RUNSTATES_OPENCOM_WAIT;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_OPENCOM_WAIT:
			{
				try {
					agentComReq comReqData;

					_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;		// Default value

					do {
						/* receive rotor opening response */
						initState = 0x11;
						if (!pAgtCom[C_COMINST_ROT]) break;
						initState = 0x12;
						agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
						if (comRspData.stat != C_COMRSP_OK)	break;			// No rotor --> INIT_ERROR
						initState = 0x13;
						Sleep(10);

						/* check if rotor is responding */
						comReqData.cmd = C_COMREQ_COM_SEND;
						comReqData.parm = string("?X\r");					// Zolix: never send a \n (LF) !!!
						send(*(pAgtComReq[C_COMINST_ROT]), comReqData);		// first request for sync purposes
						comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
						send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
						comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
						if (comRspData.stat != C_COMRSP_DATA) break;
						if (_strnicmp(&(comRspData.data[0]), "?X", 2)) break;
						if (!_noWinMsg)
							pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 1 ~ rotor responds", L"");
						initState = 0x14;
						// doRegister();
						
						/* receive TX opening response */
						if (pAgtCom[C_COMINST_TX]) {
							initState = 0x21;
							comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							if (comRspData.stat != C_COMRSP_OK)	break;
							initState = 0x22;

							/* check if TX is responding */
							if (pAgtCom[C_COMINST_TX]->isIec()) {
								/* check if USB<-->IEC adapter is responding */
								initState = 0x23;
								comReqData.cmd = C_COMREQ_COM_SEND;
								comReqData.parm = string("++addr\r\n");
								send(*(pAgtComReq[C_COMINST_TX]), comReqData);	// first request for sync purposes
								comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								send(*(pAgtComReq[C_COMINST_TX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								if (comRspData.stat != C_COMRSP_DATA) break;
								if (_strnicmp(&(comRspData.data[0]), "++", 2)) break;
								initState = 0x24;

								/* IEC adapter target address */
								comReqData.cmd = C_COMREQ_COM_SEND;
								comReqData.parm = string("++addr ");
								comReqData.parm.append(agentCom::int2String(pAgtCom[C_COMINST_TX]->getIecAddr()));
								comReqData.parm.append("\r\n");
								send(*(pAgtComReq[C_COMINST_TX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								if (comRspData.stat != C_COMRSP_DATA) break;
								if (_strnicmp(&(comRspData.data[0]), "++", 2)) break;
								initState = 0x25;

								/* IEC adapter enable automatic target response handling */
								comReqData.parm = string("++auto 1\r\n");
								send(*(pAgtComReq[C_COMINST_TX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								if (comRspData.stat != C_COMRSP_DATA) break;
								if (_strnicmp(&(comRspData.data[0]), "++", 2)) break;
								initState = 0x26;
							}
							comReqData.cmd = C_COMREQ_COM_SEND;
							comReqData.parm = string("*IDN?\r\n");
							send(*(pAgtComReq[C_COMINST_TX]), comReqData);		// first request for sync purposes
							comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							send(*(pAgtComReq[C_COMINST_TX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							if (comRspData.stat != C_COMRSP_DATA) break;
							if (_strnicmp(&(comRspData.data[0]), "ROHDE", 5)) break;
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 2 ~ TX responds", L"");
							initState = 0x2F;
						}

						/* receive RX opening response */
						if (pAgtCom[C_COMINST_RX]) {
							comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							if (comRspData.stat != C_COMRSP_OK)	break;
							initState = 0x31;

							/* check if RX is responding */
							if (pAgtCom[C_COMINST_RX]->isIec()) {
								/* check if USB<-->IEC adapter is responding */
								initState = 0x32;
								comReqData.cmd = C_COMREQ_COM_SEND;
								comReqData.parm = string("++addr\r\n");
								send(*(pAgtComReq[C_COMINST_RX]), comReqData);	// first request for sync purposes
								comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								send(*(pAgtComReq[C_COMINST_RX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								if (comRspData.stat != C_COMRSP_DATA) break;
								if (!comRspData.data[0]) break;
								initState = 0x33;
								Sleep(100L);
								initState = 0x34;

								/* IEC adapter target address */
								comReqData.parm = string("++addr ");
								initState = 0xE0;
								comReqData.parm.append(agentCom::int2String(pAgtCom[C_COMINST_RX]->getIecAddr()));
								initState = 0xE1;
								comReqData.parm.append("\r\n");
								initState = 0xE2;
								send(*(pAgtComReq[C_COMINST_RX]), comReqData);
								initState = 0xE3;
								comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								initState = 0xE4;
								if (comRspData.stat != C_COMRSP_DATA) break;
								initState = 0x35;
								Sleep(100L);
								initState = 0x36;

								/* IEC adapter enable automatic target response handling */
								comReqData.parm = string("++auto 1\r\n");
								initState = 0xF1;
								send(*(pAgtComReq[C_COMINST_RX]), comReqData);
								initState = 0xF2;
								comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								initState = 0xF3;
								if (comRspData.stat != C_COMRSP_DATA) break;
								initState = 0x36;
							}
							comReqData.cmd = C_COMREQ_COM_SEND;
							comReqData.parm = string("*IDN?\r\n");
							send(*(pAgtComReq[C_COMINST_RX]), comReqData);		// first request for sync purposes
							comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							send(*(pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							if (comRspData.stat != C_COMRSP_DATA) break;
							if (_strnicmp(&(comRspData.data[0]), "ROHDE", 5)) break;
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 3 ~ RX responds", L"");
							initState = 0x3F;
						}

						/* success, all devices are ready */
						_runState = C_MODELPATTERN_RUNSTATES_USB_REGISTRATION;
						initState = 0xFF;
					} while (FALSE);
				}
				catch (const Concurrency::operation_timed_out& e) {
					(void)e;
					_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
				}
			}
			break;

			case C_MODELPATTERN_RUNSTATES_USB_REGISTRATION:
			{
				agentUsbReq usbReqData;

				/* Allow USB_TMC to access communication agent */
				pAgtUsbTmc->start();

				usbReqData.cmd = C_USBREQ_DO_REGISTRATION;
				send(*pAgtUsbTmcReq, usbReqData);

				agentUsbRsp usbRspData = receive(*pAgtUsbTmcRsp, COOPERATIVE_TIMEOUT_INFINITE /*AGENT_PATTERN_USBTMC_TIMEOUT*/);
				if (usbRspData.stat == C_USBRSP_REGISTRATION_LIST) {
					/* Fill in registration list */
					ArrayOfInstruments_t* usbInsts = (ArrayOfInstruments_t*)usbRspData.data;

					// TODO: implementation here
					for (int idx = 0; idx < usbInsts->inst_rot_cnt; ++idx) {
						// TODO: code
					}

					for (int idx = 0; idx < usbInsts->inst_rx_cnt; ++idx) {
						// TODO: code
					}

					for (int idx = 0; idx < usbInsts->inst_tx_cnt; ++idx) {
						// TODO: code
					}
				}

				_runState = C_MODELPATTERN_PROCESSES_SELECTION_WAIT;
			}
			break;

			case C_MODELPATTERN_PROCESSES_SELECTION_WAIT:
			{
				/* Wait until the instruments are selected */
				{
					// TODO: code
				}

				_runState = C_MODELPATTERN_RUNSTATES_INIT;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_INIT:
			{
				agentComReq comReqData;
				agentComRsp comRspData;

				initState = 0x40;
				_runState = C_MODELPATTERN_RUNSTATES_INIT_WAIT;					// next runstate default setting

				// Rotor Init
				if (pAgtCom[C_COMINST_ROT]) {
					// Zollix commands to init device
					{
						initState = 0x41;
						try {
							comReqData.cmd = C_COMREQ_COM_SEND;					// Zolix: never send a \n (LF) !!!
							comReqData.parm = string("VX,20000\r");				// Zolix: top speed 20.000 ticks per sec
							send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							initState = 0x42;

							comReqData.parm = string("AX,30000\r");				// Zolix: acceleration speed 30.000
							send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							initState = 0x43;

							comReqData.parm = string("FX,2500\r");				// Zolix: initial speed 2.500 ticks per sec
							send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 1 ~ rotor init done", L"");
							initState = 0x44;
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0x45;
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 1 ~ rotor init exception!", L"ERROR!");
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}

					// read current tick position of rotor and store value
					(void)requestPos();
				}

				// TX init
				if (pAgtCom[C_COMINST_TX]) {
					// syncing with the device
					{
						initState = 0x51;
						try {
							if (pAgtCom[C_COMINST_TX]->isIec()) {
								comReqData.cmd = C_COMREQ_COM_SEND;
								comReqData.parm = string("++addr ");
								comReqData.parm.append(agentCom::int2String(pAgtCom[C_COMINST_TX]->getIecAddr()));
								comReqData.parm.append("\r\n");
								send(*(pAgtComReq[C_COMINST_TX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								initState = 0x52;

								comReqData.parm = string("++auto 1\r\n");
								send(*(pAgtComReq[C_COMINST_TX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								initState = 0x53;
							}

							comReqData.cmd = C_COMREQ_COM_SEND;
							comReqData.parm = string("*IDN?\r\n");
							do {
								send(*(pAgtComReq[C_COMINST_TX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							} while (_strnicmp(&(comRspData.data[0]), "ROHDE", 5));
							initState = 0x54;

							comReqData.parm = string("*RST; *CLS; *WAI\r\n");
							send(*(pAgtComReq[C_COMINST_TX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							initState = 0x55;
							Sleep(2500L);
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 2 ~ TX init done", L"");
							initState = 0x56;
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0x5F;
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 2 ~ TX init exception!", L"ERROR!");
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}

#if 0
					// request TX output On setting
					{
						initState = 0x60;
						comReqData.cmd = C_COMREQ_COM_SEND;
						comReqData.parm = string(":OUTP:STAT?\r\n");

						try {
							send(*(pAgtComReq[C_COMINST_TX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							initState = 0x61;
							if (comRspData.stat == C_COMRSP_DATA && comRspData.data[0]) {
								int isOn = 0;

								initState = 0x62;
								const char* str_start = comRspData.data.c_str();
								if (str_start) {
									initState = 0x63;
									sscanf_s(str_start - 1, "%d", &isOn);
									agentModel::setTxOnState(isOn);
								}
							}
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							//agentModel::setTxOnState(getTxOnDefault());
							initState = 0x6F;
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}

					// request TX frequency
					{
						initState = 0x70;
						comReqData.cmd = C_COMREQ_COM_SEND;
						comReqData.parm = string(":SOUR:FREQ?\r\n");

						try {
							send(*(pAgtComReq[C_COMINST_TX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							if (comRspData.stat == C_COMRSP_DATA && comRspData.data[0]) {
								double frequency = 0.;

								initState = 0x71;
								const char* str_start = comRspData.data.c_str();
								if (str_start) {
									initState = 0x72;
									sscanf_s(str_start, "%lf", &frequency);
									agentModel::setTxFrequencyValue(frequency);
								}
							}
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							//agentModel::setTxFrequencyValue(getTxFrequencyDefault());
							initState = 0x7F;
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}

					// request TX power
					{
						initState = 0x80;
						comReqData.cmd = C_COMREQ_COM_SEND;
						comReqData.parm = string(":SOUR:POW?\r\n");

						try {
							send(*(pAgtComReq[C_COMINST_TX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							if (comRspData.stat == C_COMRSP_DATA && comRspData.data[0]) {
								double power = 0.;

								initState = 0x81;
								const char* str_start = comRspData.data.c_str();
								if (str_start) {
									initState = 0x82;
									sscanf_s(str_start, "%lf", &power);
									agentModel::setTxPwrValue(power);
								}
							}
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							//agentModel::setTxPwrValue(getTxPwrDefault());
							initState = 0x8F;
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}
#else
					// set TX device default parameters
					{
						initState = 0x90;
						setTxFrequencyValue(agentModel::getTxFrequencyDefault());
						initState = 0x91;
						setTxPwrValue(agentModel::getTxPwrDefault());
						initState = 0x92;
						setTxOnState(agentModel::getTxOnDefault());
						initState = 0x93;
						if (!_noWinMsg)
							pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 2 ~ TX default parameters are set", L"");
					}
#endif
				}

				// RX init
				if (pAgtCom[C_COMINST_RX]) {
					// syncing with the device
					{
						initState = 0xA0;
						try {
							if (pAgtCom[C_COMINST_RX]->isIec()) {
								comReqData.cmd = C_COMREQ_COM_SEND;
								comReqData.parm = string("++addr ");
								comReqData.parm.append(agentCom::int2String(pAgtCom[C_COMINST_RX]->getIecAddr()));
								comReqData.parm.append("\r\n");
								send(*(pAgtComReq[C_COMINST_RX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								initState = 0xA1;

								comReqData.parm = string("++auto 1\r\n");
								send(*(pAgtComReq[C_COMINST_RX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								initState = 0xA2;
							}

							comReqData.cmd = C_COMREQ_COM_SEND;
							comReqData.parm = string("*IDN?\r\n");
							initState = 0xA3;
							do {
								send(*(pAgtComReq[C_COMINST_RX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							} while (_strnicmp(&(comRspData.data[0]), "ROHDE", 5));
							initState = 0xA4;

							comReqData.parm = string("*RST; *CLS; *WAI\r\n");
							send(*(pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							initState = 0xA5;
							Sleep(2500L);
							initState = 0xA6;

							comReqData.parm = string(":INIT:CONT ON\r\n");
							send(*(pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							initState = 0xA7;
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 3 ~ RX init done", L"");
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0xAF;
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 3 ~ RX init exception!", L"WARNING!");
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}

					// Set RX device parameters
					{
						initState = 0xB0;
						setRxSpanValue(agentModel::getRxSpanDefault());
						initState = 0xB1;
						setRxFrequencyValue(agentModel::getTxFrequencyDefault());
						initState = 0xB2;
						setRxLevelMaxValue(agentModel::getTxPwrDefault());
						initState = 0xB3;
						if (!_noWinMsg)
							pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: RX 3 ~ default parameters are set", L"");
					}

					// Display settings
					{
						initState = 0xC0;
						try {
							comReqData.cmd = C_COMREQ_COM_SEND;
							comReqData.parm = string(":DISP:TRAC1:Y:SPAC LOG; :DISP:TRAC1:Y 50DB\r\n");
							send(*(pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							initState = 0xC1;

							comReqData.parm = string(":DISP:TEXT \"RF-Lab: Ant pattern\"\r\n");
							send(*(pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							initState = 0xC2;

							comReqData.parm = string(":DISP:TEXT:STAT ON\r\n");
							send(*(pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							initState = 0xC3;
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 3 ~ RX display parameters are set", L"");
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0xCF;
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 3 ~ RX display parameters", L"WARNING!");
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}
				}
			}
			break;

			case C_MODELPATTERN_RUNSTATES_INIT_WAIT:
			{
				agentComRsp comRspData;

				initState = 0xD0;
				if (!_noWinMsg)
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 1 ~ waiting for response from the rotor", L"");
				bool state = try_receive(*(pAgtComRsp[C_COMINST_ROT]), comRspData);
				if (state) {
					// consume each reported state
					if (comRspData.stat == C_COMRSP_FAIL) {
						initState = 0xDF;
						if (!_noWinMsg)
							pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 1 ~ no response from the rotor", L"ERROR!");
						_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
					}
				}
				else {
					initState = 0xD1;
					if (!_noWinMsg)
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"ready for jobs", L"READY");
					_runState = C_MODELPATTERN_RUNSTATES_RUNNING;
				}
			}
			break;

			case C_MODELPATTERN_RUNSTATES_INIT_ERROR:
			{
				/* Init Error occured, show requester */
				int lastGood = initState;

				if (!_noWinMsg)
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"INIT ERROR: please connect missing device(s)", L"STANDBY");

				_runState = C_MODELPATTERN_RUNSTATES_RUNNING;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_REINIT:
			{
				/* Close any devices and restart init process */
				_runReinit	= true;
				_runState	= C_MODELPATTERN_RUNSTATES_CLOSE_COM;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_CLOSE_COM:
			{
				agentUsbReq usbReqData;
				agentComReq comReqData;
				usbReqData.cmd = C_USBREQ_END;
				comReqData.cmd = C_COMREQ_END;
				comReqData.parm = string();

				if (!_noWinMsg)
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: shutting down ...", L"");

				/* Send shutdown request to the USB_TMC module */
				send(*pAgtUsbTmcReq, usbReqData);

				/* Send shutdown request for each active agent */
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
				/* Wait for the USB_TMC module to get finished */
				try {
					agentUsbRsp usbRsp;
					do {
						usbRsp = receive(*pAgtUsbTmcRsp, AGENT_PATTERN_USBTMC_TIMEOUT);
					} while (usbRsp.stat != C_USBRSP_END);
				}
				catch (const Concurrency::operation_timed_out& e) {
					(void)e;
				}

				/* wait for each reply message */
				for (int i = 0; i < C_COMINST__COUNT; i++) {
					if (pAgtCom[i]) {
						agentComRsp comRsp;

						/* consume until END response is received */
						try {
							do {
								comRsp = receive(*(pAgtComRsp[i]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							} while (comRsp.stat != C_COMRSP_END);
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
						}
					}
				}

				/* Stop the ProcessID thread */
				threadsStop();

				/* Shutdown the agents */
				agentsShutdown();

				if (_runReinit) {
					_runState = C_MODELPATTERN_RUNSTATES_BEGIN;
					if (!_noWinMsg)
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: closed", L"REINIT");

				} else {
					_runState = C_MODELPATTERN_RUNSTATES_NOOP;
					_running = false;
					if (!_noWinMsg)
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: closed", L"CLOSED");
				}
			}
			break;

			case C_MODELPATTERN_RUNSTATES_RUNNING:
			case C_MODELPATTERN_RUNSTATES_NOOP:
			default:
				Sleep(25L);
				break;
			}
		}
		catch (const Concurrency::operation_timed_out& e) {
			char msgBuf[256];
			DWORD dwChars = 0;

			strcpy_s(msgBuf, e.what());
			WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msgBuf, (DWORD)strlen(msgBuf), &dwChars, NULL);

			if (_runState < C_MODELPATTERN_RUNSTATES_CLOSE_COM) {
				_runState = C_MODELPATTERN_RUNSTATES_CLOSE_COM;

			} else if (_runState <= C_MODELPATTERN_RUNSTATES_CLOSE_COM_WAIT) {
				if (_runReinit) {
					_runState = C_MODELPATTERN_RUNSTATES_BEGIN;
					if (!_noWinMsg)
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: broken", L"REINIT");

				} else {
					_runState = C_MODELPATTERN_RUNSTATES_NOOP;
					_running = false;
					if (!_noWinMsg)
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: broken", L"CLOSED");
				}
			}
		}
	}

	_done = true;
	agent::done();
}


void agentModelPattern::wmCmd(int wmId, LPVOID arg)
{
	switch (wmId)
	{
	case ID_ROTOR_GOTO_0:
		if (_runState == C_MODELPATTERN_RUNSTATES_RUNNING) {
			pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 1 ~ rotor turns to  0°  position", L"RUNNING");
			runProcess(C_MODELPATTERN_PROCESS_GOTO_X, 0);
		}
		break;

	case ID_ROTOR_GOTO_X:
		if (arg) {
			if (_runState == C_MODELPATTERN_RUNSTATES_RUNNING) {
				int* pi = (int*)arg;

				/* Inform about the current state */
				{
					const int l_status_size = 256;
					HLOCAL l_status2_alloc = LocalAlloc(LHND, sizeof(wchar_t) * l_status_size);
					PWCHAR l_status2 = (PWCHAR)LocalLock(l_status2_alloc);

					swprintf_s(l_status2, l_status_size, L"COM: 1 ~ rotor turns to  %+03d°  position", (*pi) / 1000);
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", l_status2, L"RUNNING");

					LocalUnlock(l_status2_alloc);
					LocalFree(l_status2_alloc);
				}

				/* Start rotor */
				runProcess(C_MODELPATTERN_PROCESS_GOTO_X, *pi);
			}
		}
		break;
	}
}

void agentModelPattern::procThreadProcessID(void* pContext)
{
	threadDataAgentModelPattern_t* m = (threadDataAgentModelPattern_t*)pContext;
	WaitForSingleObject(m->o->hThreadAgtUsbTmc, INFINITE);	// Thread now holds his mutex until the end of its method

	/* Loop as long no end-signalling has entered */
	bool doLoop = true;
	do {
		/* different jobs to be processed */
		switch (m->o->processing_ID)
		{

		case C_MODELPATTERN_PROCESS_GOTO_X:
		{  // go to direction
			int gotoMilliPos = m->o->processing_arg1;
			m->o->processing_arg1 = 0;

			long posNow = m->o->requestPos();
			long posNext = (long)(gotoMilliPos * (AGENT_PATTERN_ROT_TICKS_PER_DEGREE / 1000.0));
			m->o->sendPos(posNext);
			Sleep(calcTicks2Ms(posNext - posNow));

			m->o->pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"ready for jobs", L"READY");
			m->o->processing_ID = C_MODELPATTERN_PROCESS_NOOP;
		}
		break;

		case C_MODELPATTERN_PROCESS_RECORD_PATTERN_180DEG:
		{  /* Run a 180° antenna pattern from left = -90° to the right = +90° */

			m->o->pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"measure: running 180° pattern", L"goto START position");
			int ret = m->o->runningProcessPattern(
				AGENT_PATTERN_180_POS_DEGREE_START,
				AGENT_PATTERN_180_POS_DEGREE_END,
				AGENT_PATTERN_180_POS_DEGREE_STEP
			);
			if (ret == -1) {
				break; // process STOPPED
			}

			m->o->processing_ID = C_MODELPATTERN_PROCESS_NOOP;
		}
		break;

		case C_MODELPATTERN_PROCESS_RECORD_PATTERN_360DEG:
		{  /* run a 360° antenna pattern from left = -180° to the right = +180° */

			m->o->pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"measure: running 360° pattern", L"goto START position");
			int ret = m->o->runningProcessPattern(
				AGENT_PATTERN_360_POS_DEGREE_START,
				AGENT_PATTERN_360_POS_DEGREE_END,
				AGENT_PATTERN_360_POS_DEGREE_STEP
			);
			if (ret == -1) {
				break; // process STOPPED
			}

			m->o->processing_ID = C_MODELPATTERN_PROCESS_NOOP;
		}
		break;

		case C_MODELPATTERN_PROCESS_STOP:
		{  /* end "STOP at once" signalling */
			m->o->pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"stopping current measurement", L"STOPPED");
			m->o->processing_ID = C_MODELPATTERN_PROCESS_NOOP;
		}
		break;

		case C_MODELPATTERN_PROCESS_END:
		{
			doLoop = false;
		}
		break;

		case C_MODELPATTERN_PROCESS_NOOP:
		default:
		{
			Sleep(25L);
		}

		}  // switch(m->c->processing_ID) 
	} while (doLoop);

	ReleaseMutex(m->o->hThreadAgtUsbTmc);					// Thread returns mutex to signal its end
}


/* agentModelPattern - GENERAL */

void agentModelPattern::setSimuMode(int mode)
{
	simuMode = mode;
}

int agentModelPattern::getSimuMode(void)
{
	return simuMode;
}

void agentModelPattern::initDevices(void)
{
	if (_running && (_runState == C_MODELPATTERN_RUNSTATES_RUNNING)) {
		_runState = C_MODELPATTERN_RUNSTATES_REINIT;
	}
}

void agentModelPattern::runProcess(int processID, int arg)
{
	/* STOP at once processing ID */
	if (processID == C_MODELPATTERN_PROCESS_STOP) {
		processing_arg1 = arg;
		processing_ID = processID;
		return;
	}

	/* Cue in process ID to be done */
	if (processing_ID == C_MODELPATTERN_PROCESS_NOOP) {
		processing_arg1 = arg;
		processing_ID = processID;
	}
}

void agentModelPattern::setStatusPosition(double pos)
{
	/* Inform about the current state */
	const int l_status_size = 256;
	HLOCAL l_status3_alloc = LocalAlloc(LHND, sizeof(wchar_t) * l_status_size);
	PWCHAR l_status3 = (PWCHAR)LocalLock(l_status3_alloc);

	swprintf_s(l_status3, l_status_size, L"position: %+03.0lf°", pos);
	pAgtMod->getWinSrv()->reportStatus(NULL, NULL, l_status3);

	LocalUnlock(l_status3_alloc);
	LocalFree(l_status3_alloc);
}


/* agentModelPattern - Rotor */

long agentModelPattern::requestPos(void)
{
	agentComReq comReqData;
	agentComRsp comRspData;
	long pos = 0;

	try {
		// request position
		comReqData.cmd = C_COMREQ_COM_SEND;
		comReqData.parm = string("?X\r");

		comReqData.parm = string("\r");
		do {
			send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);	// drop queue
			Sleep(10L);
		} while (strncmp(comRspData.data.c_str(), "OK", 2));

		do {
			comReqData.parm = string("?X\r");
			send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
		} while (strncmp(comRspData.data.c_str(), "?X",2 ));

		if (comRspData.stat == C_COMRSP_DATA) {
			int posDiff = 0;

#if 0
			const char* str_start = strrchr(comRspData.data.c_str(), '?');
			if (str_start) {
				sscanf_s(str_start, "?X,%d", &pos);
				agentModel::setLastTickPos(pos);
			}
#else
			if (!agentModel::parseStr2Long(&pos, comRspData.data.c_str(), "?X,%ld", '?')) {
				agentModel::setLastTickPos(pos);
			}
#endif
		}
	}
	catch (const Concurrency::operation_timed_out& e) {
		(void)e;
	}
	return pos;
}

void agentModelPattern::sendPos(long tickPos)
{
	agentComReq comReqData;
	agentComRsp comRspData;
	long posDiff = tickPos - getLastTickPos();
	char cbuf[C_BUF_SIZE];

	try {
		_snprintf_s(cbuf, C_BUF_SIZE - 1, "%cX,%ld\r", (posDiff > 0 ? '+' : '-'), abs(posDiff));
		comReqData.cmd = C_COMREQ_COM_SEND;
		comReqData.parm = string(cbuf);
		send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
		comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
	}
	catch (const Concurrency::operation_timed_out& e) {
		(void)e;
	}
}

void agentModelPattern::setLastTickPos(long pos)
{
	lastTickPos = pos;
}

long agentModelPattern::getLastTickPos(void)
{
	return lastTickPos;
}


/* agentModelPattern - TX */

void agentModelPattern::setTxOnState(bool checked)
{
	if (txOn != checked) {
		txOn = checked;

		agentComReq comReqData;
		agentComRsp comRspData;

		try {
			comReqData.cmd = C_COMREQ_COM_SEND;
			comReqData.parm = checked ?  string(":OUTP ON\r\n") 
									  :  string(":OUTP OFF\r\n");
			send(*(pAgtComReq[C_COMINST_TX]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
		}
	}
}

bool agentModelPattern::getTxOnState(void)
{
	return txOn;
}

bool agentModelPattern::getTxOnDefault(void)
{
	return AGENT_PATTERN_TX_ON_STATE_DEFAULT;
}

void agentModelPattern::setTxFrequencyValue(double value)
{
	if ((txFrequency != value) && 
		((10e6 < value) && (value <= 100e9))) {
		agentComReq comReqData;
		agentComRsp comRspData;

		txFrequency = value;

		try {
			comReqData.cmd = C_COMREQ_COM_SEND;
			comReqData.parm = string(":SOUR:FREQ ");
			comReqData.parm.append(agentCom::double2String(txFrequency));
			comReqData.parm.append("\r\n");
			send(*(pAgtComReq[C_COMINST_TX]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
			txFrequency = getTxFrequencyDefault();
		}
	}
}

double agentModelPattern::getTxFrequencyValue(void)
{
	return txFrequency;
}

double agentModelPattern::getTxFrequencyDefault(void)
{
	return AGENT_PATTERN_TX_FREQ_VALUE_DEFAULT;
}

void agentModelPattern::setTxPwrValue(double value)
{
	if ((txPower != value && 
		(-40 <= value && value <= 20))) {
		agentComReq comReqData;
		agentComRsp comRspData;

		txPower = value;

		try {
			comReqData.cmd = C_COMREQ_COM_SEND;
			comReqData.parm = string("SOUR:POW ");
			comReqData.parm.append(agentCom::double2String(txPower));
			comReqData.parm.append("dBm\r\n");
			send(*(pAgtComReq[C_COMINST_TX]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
			txPower = getTxPwrDefault();
		}
	}
}

double agentModelPattern::getTxPwrValue(void)
{
	return txPower;
}

double agentModelPattern::getTxPwrDefault(void)
{
	return AGENT_PATTERN_TX_PWR_VALUE_DEFAULT;
}


/* agentModelPattern - RX */

void agentModelPattern::setRxFrequencyValue(double value)
{
	if (pAgtComRsp[C_COMINST_RX] &&
		(rxFrequency != value) &&
		((10e6 < value) && (value <= 100e9))) {
		agentComReq comReqData;
		agentComRsp comRspData;

		rxFrequency = value;

		try {
			comReqData.cmd = C_COMREQ_COM_SEND;
			comReqData.parm = string(":SENS1:FREQ:CENT ");
			comReqData.parm.append(agentCom::double2String(rxFrequency));
			comReqData.parm.append("HZ\r\n");
			send(*(pAgtComReq[C_COMINST_RX]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
			rxFrequency = getTxFrequencyDefault();
		}
	}
}

double agentModelPattern::getRxFrequencyValue(void)
{
	return rxFrequency;
}

void agentModelPattern::setRxSpanValue(double value)
{
	if (pAgtComRsp[C_COMINST_RX] &&
		(rxSpan != value) &&
		(value <= 100e9)) {
		agentComReq comReqData;
		agentComRsp comRspData;

		rxSpan = value;

		try {
			comReqData.cmd = C_COMREQ_COM_SEND;
			comReqData.parm = string(":SENS1:FREQ:SPAN ");
			comReqData.parm.append(agentCom::double2String(rxSpan));
			comReqData.parm.append("HZ\r\n");
			send(*(pAgtComReq[C_COMINST_RX]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
			rxSpan = getRxSpanDefault();
		}
	}
}

double agentModelPattern::getRxSpanValue(void)
{
	return rxSpan;
}

double agentModelPattern::getRxSpanDefault(void)
{
	return AGENT_PATTERN_RX_SPAN_VALUE_DEFAULT;
}

void agentModelPattern::setRxLevelMaxValue(double value)
{
	if (pAgtComRsp[C_COMINST_RX] &&
		(rxSpan != value) &&
		(-40 <= value) && (value <= 30)) {
		agentComReq comReqData;
		agentComRsp comRspData;

		rxLevelMax = value;

		try {
			comReqData.cmd = C_COMREQ_COM_SEND;
			comReqData.parm = string(":DISP:WIND1:TRAC1:Y:RLEV ");
			comReqData.parm.append(agentCom::double2String(rxLevelMax));
			comReqData.parm.append("DBM\r\n");
			send(*(pAgtComReq[C_COMINST_RX]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
			rxLevelMax = getTxPwrDefault();
		}
	}
}

double agentModelPattern::getRxLevelMaxValue(void)
{
	return rxLevelMax;
}

bool agentModelPattern::getRxMarkerPeak(double* retX, double* retY)
{
	if (retX && retY && pAgtComReq[C_COMINST_RX]) {
		agentComReq comReqData;
		agentComRsp comRspData;

		*retX = *retY = 0.;

		/* Following sequence does a level metering */
		try {
			comReqData.cmd  = C_COMREQ_COM_SEND;
			comReqData.parm = string(":INIT:CONT OFF\r\n");
			send(*(pAgtComReq[C_COMINST_RX]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);

			comReqData.parm = string(":INIT:IMM; *WAI\r\n");
			send(*(pAgtComReq[C_COMINST_RX]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);

			comReqData.parm = string(":CALC:MARKER ON; :CALC:MARKER:MAX; *WAI\r\n");
			send(*(pAgtComReq[C_COMINST_RX]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);

			comReqData.parm = string(":CALC:MARKER:X?\r\n");
			send(*(pAgtComReq[C_COMINST_RX]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			sscanf_s(comRspData.data.c_str(), "%lf", retX);

			comReqData.parm = string(":CALC:MARKER:Y?\r\n");
			send(*(pAgtComReq[C_COMINST_RX]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			sscanf_s(comRspData.data.c_str(), "%lf", retY);

			return FALSE;
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
		}
	}
	return TRUE;
}


/* TOOLS */

int agentModelPattern::runningProcessPattern(double degStartPos, double degEndPos, double degResolution)
{  // run a antenna pattern from left = degStartPos to the right = degEndPos
	// TODO: max 3 seconds allowed - use own thread for running process
	/* Set-up ROTOR */
	long ticksNow = requestPos();
	long ticksNext = calcDeg2Ticks(degStartPos);
	sendPos(ticksNext);										// Go to start position
	Sleep(calcTicks2Ms(ticksNext - ticksNow));
	if (processing_ID <= C_MODELPATTERN_PROCESS_STOP) {
		return -1;
	}

	/* Inform about the start position */
	setStatusPosition(degStartPos);

	/* Set-up TX */

	/* Set-up RX */

	/* Iteration of rotor steps */
	double degPosIter = degStartPos;
	while (true) {
		/* Record data */
		//measure();  // TODO
		if (processing_ID <= C_MODELPATTERN_PROCESS_STOP) {
			return -1;
		}

		/* advance to new position */
		degPosIter += degResolution;
		if (degPosIter > degEndPos) {
			break;
		}

		ticksNow = requestPos();
		ticksNext = calcDeg2Ticks(degPosIter);
		sendPos(ticksNext);
		Sleep(calcTicks2Ms(ticksNext - ticksNow));
		if (processing_ID <= C_MODELPATTERN_PROCESS_STOP) {
			return -1;
		}

		/* Inform about the current step position */
		setStatusPosition(degPosIter);
	}

	if (processing_ID <= C_MODELPATTERN_PROCESS_STOP) {
		return -1;
	}

	/* Return ROTOR to center position */
	pAgtMod->getWinSrv()->reportStatus(NULL, NULL, L"goto HOME position");
	requestPos();
	sendPos(0);
	Sleep(calcDeg2Ms(degEndPos));

	processing_ID = C_MODELPATTERN_PROCESS_NOOP;
	return 0;
}

inline double agentModelPattern::calcTicks2Deg(long ticks)
{
	return ((double)ticks) / AGENT_PATTERN_ROT_TICKS_PER_DEGREE;
}

inline long agentModelPattern::calcDeg2Ticks(double deg)
{
	if (deg >= 0.0) {
		return (long)(deg * AGENT_PATTERN_ROT_TICKS_PER_DEGREE + 0.5);
	}
	else {
		return (long)(deg * AGENT_PATTERN_ROT_TICKS_PER_DEGREE - 0.5);
	}
}

inline DWORD agentModelPattern::calcDeg2Ms(double deg)
{
	if (deg < 0) {
		deg = -deg;
	}
	return (DWORD)(500L + deg * AGENT_PATTERN_ROT_MS_PER_DEGREE);
}

inline DWORD agentModelPattern::calcTicks2Ms(long ticks)
{
	return calcDeg2Ms(calcTicks2Deg(ticks));
}
