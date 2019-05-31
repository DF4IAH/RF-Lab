#include "stdafx.h"
#include <process.h>

#include "resource.h"
#include "WinSrv.h"

#include "agentModel.h"

#include "externals.h"

#include "agentModelPattern.h"


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
		_runState = C_MODELPATTERN_RUNSTATES_SHUTDOWN;
		_noWinMsg = true;
	}
	return _running;
}

void agentModelPattern::Release(void)
{
	/* signaling and wait until all threads are done */
	(void)shutdown();

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
	pAgtUsbTmc->start();
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

				/* Setting up servers */
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

				_runState = C_MODELPATTERN_RUNSTATES_FETCH_SETTINGS;
			}
			break;


			/* Request list of instruments with their connection settings */
			case C_MODELPATTERN_RUNSTATES_FETCH_SETTINGS:
			{
				// TODO: coding
				/* Check for active Instruments */
				checkInstruments();

				/* Update GUI with active/absent instruments */
				//xxx();

				_runState = C_MODELPATTERN_RUNSTATES_COM_REGISTRATION;
			}
			break;


			/* Find COM / IEC instruments for registration */
			case C_MODELPATTERN_RUNSTATES_COM_REGISTRATION:
			{
				char buf[C_BUF_SIZE];
				agentComReq comReqData;

				/* Start serial communication servers if not already done */
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
#ifdef OLD_CODE
					comReqData.cmd = C_COMREQ_OPEN_ZOLIX;
					_snprintf_s(buf, C_BUF_SIZE, C_OPENPARAMS_STR,
						C_ROT_COM_PORT, C_ROT_COM_BAUD, C_ROT_COM_BITS, C_ROT_COM_PARITY, C_ROT_COM_STOPBITS,
						C_SET_IEC_ADDR_INVALID);
					comReqData.parm = string(buf);
					send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
					agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
					if (comRspData.stat == C_COMRSP_DATA) {
						string rspIdnStr = string(comRspData.data);
						if (rspIdnStr.empty()) {
							comReqData.cmd = C_COMREQ_CLOSE;
							send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
							receive(*(pAgtComRsp[C_COMINST_ROT]));

						}
						else {
							addSerInstrument(INSTRUMENT_ROTORS_SER__ZOLIX_SC300,
								pAgtCom[C_COMINST_ROT], C_ROT_COM_PORT, C_ROT_COM_BAUD, C_ROT_COM_BITS, C_ROT_COM_PARITY, C_ROT_COM_STOPBITS,
								false, 0,
								rspIdnStr);

							initState = 0x02;
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 1 ~ rotor port opened", L"");
						}
					}
#endif
				}

				/* Open TX */
				if (pAgtCom[C_COMINST_TX]) {
					comReqData.cmd = C_COMREQ_OPEN_IDN;
					if (C_TX_COM_IS_IEC) {
						pAgtCom[C_COMINST_TX]->setIecAddr(C_TX_COM_IEC_ADDR);
						_snprintf_s(buf, C_BUF_SIZE, C_OPENPARAMS_STR,
							C_TX_COM_IEC_PORT, C_TX_COM_IEC_BAUD, C_TX_COM_IEC_BITS, C_TX_COM_IEC_PARITY, C_TX_COM_IEC_STOPBITS,
							C_TX_COM_IEC_ADDR);
						comReqData.parm = string(buf);
						send(*(pAgtComReq[C_COMINST_TX]), comReqData);
						agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_TX]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
						if (comRspData.stat == C_COMRSP_DATA) {
							string rspIdnStr = string(comRspData.data);
							if (rspIdnStr.empty()) {
								comReqData.cmd = C_COMREQ_CLOSE;
								send(*(pAgtComReq[C_COMINST_TX]), comReqData);
								receive(*(pAgtComRsp[C_COMINST_TX]));

							}
							else {
								INSTRUMENT_ENUM_t type = findSerInstrumentByIdn(rspIdnStr, INSTRUMENT_VARIANT_TX | INSTRUMENT_CONNECT_SER);
								addSerInstrument(type,
									pAgtCom[C_COMINST_TX], C_TX_COM_IEC_PORT, C_TX_COM_IEC_BAUD, C_TX_COM_IEC_BITS, C_TX_COM_IEC_PARITY, C_TX_COM_IEC_STOPBITS,
									true, C_TX_COM_IEC_ADDR,
									rspIdnStr);

								initState = 0x03;
								if (!_noWinMsg)
									pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 2 ~ TX IEC port opened", L"");
							}
						}

					}
					else {
						pAgtCom[C_COMINST_TX]->setIecAddr(C_SET_IEC_ADDR_INVALID);
						_snprintf_s(buf, C_BUF_SIZE, C_OPENPARAMS_STR,
							C_TX_COM_PORT, C_TX_COM_BAUD, C_TX_COM_BITS, C_TX_COM_PARITY, C_TX_COM_STOPBITS,
							C_SET_IEC_ADDR_INVALID);
						comReqData.parm = string(buf);
						send(*(pAgtComReq[C_COMINST_TX]), comReqData);
						agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_TX]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
						if (comRspData.stat == C_COMRSP_DATA) {
							string rspIdnStr = string(comRspData.data);
							if (rspIdnStr.empty()) {
								comReqData.cmd = C_COMREQ_CLOSE;
								send(*(pAgtComReq[C_COMINST_TX]), comReqData);
								receive(*(pAgtComRsp[C_COMINST_TX]));

							}
							else {
								INSTRUMENT_ENUM_t type = findSerInstrumentByIdn(rspIdnStr, INSTRUMENT_VARIANT_TX | INSTRUMENT_CONNECT_SER);
								addSerInstrument(type,
									pAgtCom[C_COMINST_TX], C_TX_COM_PORT, C_TX_COM_BAUD, C_TX_COM_BITS, C_TX_COM_PARITY, C_TX_COM_STOPBITS,
									false, 0,
									rspIdnStr);

								initState = 0x03;
								if (!_noWinMsg)
									pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 2 ~ TX port opened", L"");
							}
						}
					}
				}

				/* Open RX */
				if (pAgtCom[C_COMINST_RX]) {
					comReqData.cmd = C_COMREQ_OPEN_IDN;
					if (C_RX_COM_IS_IEC) {
						pAgtCom[C_COMINST_RX]->setIecAddr(C_RX_COM_IEC_ADDR);
						_snprintf_s(buf, C_BUF_SIZE, C_OPENPARAMS_STR,
							C_RX_COM_IEC_PORT, C_RX_COM_IEC_BAUD, C_RX_COM_IEC_BITS, C_RX_COM_IEC_PARITY, C_RX_COM_IEC_STOPBITS,
							C_RX_COM_IEC_ADDR);
						comReqData.parm = string(buf);
						send(*(pAgtComReq[C_COMINST_RX]), comReqData);
						agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_RX]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT*/);
						if (comRspData.stat == C_COMRSP_DATA) {
							string rspIdnStr = string(comRspData.data);
							if (rspIdnStr.empty()) {
								comReqData.cmd = C_COMREQ_CLOSE;
								send(*(pAgtComReq[C_COMINST_RX]), comReqData);
								receive(*(pAgtComRsp[C_COMINST_RX]));

							}
							else {
								INSTRUMENT_ENUM_t type = findSerInstrumentByIdn(rspIdnStr, INSTRUMENT_VARIANT_RX | INSTRUMENT_CONNECT_SER);
								addSerInstrument(type,
									pAgtCom[C_COMINST_RX], C_RX_COM_IEC_PORT, C_RX_COM_IEC_BAUD, C_RX_COM_IEC_BITS, C_RX_COM_IEC_PARITY, C_RX_COM_IEC_STOPBITS,
									true, C_RX_COM_IEC_ADDR,
									rspIdnStr);

								initState = 0x04;
								if (!_noWinMsg)
									pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 3 ~ RX IEC port opened", L"");
							}
						}

					}
					else {
						pAgtCom[C_COMINST_RX]->setIecAddr(C_SET_IEC_ADDR_INVALID);
						_snprintf_s(buf, C_BUF_SIZE, C_OPENPARAMS_STR,
							C_RX_COM_PORT, C_RX_COM_BAUD, C_RX_COM_BITS, C_RX_COM_PARITY, C_RX_COM_STOPBITS,
							C_SET_IEC_ADDR_INVALID);
						comReqData.parm = string(buf);
						send(*(pAgtComReq[C_COMINST_RX]), comReqData);
						agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_RX]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
						if (comRspData.stat == C_COMRSP_DATA) {
							string rspIdnStr = string(comRspData.data);
							if (rspIdnStr.empty()) {
								comReqData.cmd = C_COMREQ_CLOSE;
								send(*(pAgtComReq[C_COMINST_RX]), comReqData);
								receive(*(pAgtComRsp[C_COMINST_RX]));

							}
							else {
								INSTRUMENT_ENUM_t type = findSerInstrumentByIdn(rspIdnStr, INSTRUMENT_VARIANT_RX | INSTRUMENT_CONNECT_SER);
								addSerInstrument(type,
									pAgtCom[C_COMINST_RX], C_RX_COM_PORT, C_RX_COM_BAUD, C_RX_COM_BITS, C_RX_COM_PARITY, C_RX_COM_STOPBITS,
									false, 0,
									string());

								initState = 0x04;
								if (!_noWinMsg)
									pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 3 ~ RX port opened", L"");
							}
						}
					}
				}

				_runState = C_MODELPATTERN_RUNSTATES_USB_REGISTRATION;
			}
			break;


			/* Find USB instruments for registration */
			case C_MODELPATTERN_RUNSTATES_USB_REGISTRATION:
			{
				agentUsbReq usbReqData;

				usbReqData.cmd = C_USBREQ_DO_REGISTRATION;
				send(*pAgtUsbTmcReq, usbReqData);

				agentUsbRsp usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_USBTMC_TIMEOUT*/);
				if (usbRspData.stat == C_USBRSP_REGISTRATION_LIST) {
					/* Fill in into registration list all USB and SER/IEC devices */
					ArrayOfInstruments_t* usbInsts = (ArrayOfInstruments_t*)usbRspData.data;

					//xxx();  // TODO: implementation here
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

				_runState = C_MODELPATTERN_RUNSTATES_INST_SELECTION;
			}
			break;


			/* WinSrv user interaction: selection of instruments */
			case C_MODELPATTERN_RUNSTATES_INST_SELECTION:
			{
				/* Wait until the instruments are selected */
				{
					// TODO: code
				}

				_runState = C_MODELPATTERN_RUNSTATES_INST_COM_INIT;
			}
			break;


			/* Initilization of selected COM / IEC instrument(s) */
			case C_MODELPATTERN_RUNSTATES_INST_COM_INIT:
			{
				agentComReq comReqData;
				agentComRsp comRspData;

				initState = 0x40;
				_runState = C_MODELPATTERN_RUNSTATES_INST_USB_INIT;				// next runstate default setting

				// Rotor Init
				if (pAgtCom[C_COMINST_ROT]) {
					// Zolix commands to init device
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

							// read current tick position of rotor and store value
							(void)requestPos();

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
				}
				if (_runState == C_MODELPATTERN_RUNSTATES_INIT_ERROR) {
					break;
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

							do {
								comReqData.cmd = C_COMREQ_COM_SEND;
								comReqData.parm = string("*IDN?\r\n");
								send(*(pAgtComReq[C_COMINST_TX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								if (comRspData.stat != C_COMRSP_DATA) {
									initState = 0x5E;
									if (!_noWinMsg)
										pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 2 ~ TX init exception! (1)", L"ERROR!");
									_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
									break;
								}
							} while (_strnicmp(&(comRspData.data[0]), "ROHDE", 5));
							initState = 0x54;

							if (_runState != C_MODELPATTERN_RUNSTATES_INIT_ERROR) {
								comReqData.parm = string("*RST; *CLS; *WAI\r\n");
								send(*(pAgtComReq[C_COMINST_TX]), comReqData);
								comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								initState = 0x55;
								Sleep(2500L);
								if (!_noWinMsg)
									pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 2 ~ TX init done", L"");
								initState = 0x56;
							}
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0x5F;
							if (!_noWinMsg)
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 2 ~ TX init exception! (2)", L"ERROR!");
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}
					if (_runState == C_MODELPATTERN_RUNSTATES_INIT_ERROR) {
						break;
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
								if (comRspData.stat != C_COMRSP_DATA) {
									initState = 0xAE;
									if (!_noWinMsg)
										pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 3 ~ RX init exception! (1)", L"ERROR!");
									_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
									break;
								}
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
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 3 ~ RX init exception! (2)", L"ERROR!");
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}
					if (_runState == C_MODELPATTERN_RUNSTATES_INIT_ERROR) {
						break;
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


#if 0
			case C_MODELPATTERN_COM_INIT_2:
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
						_runState = C_MODELPATTERN_RUNSTATES_USB_INST_INIT;
				}
			}
			break;
#endif


			/* Initilization of selected USB instrument(s) */
			case C_MODELPATTERN_RUNSTATES_INST_USB_INIT:
			{
				// TODO: coding
				
				if (!_noWinMsg)
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"ready for jobs", L"READY");

				/* success, all devices are ready */
				_runState = C_MODELPATTERN_RUNSTATES_RUNNING;
			}
			break;


			/* All is up and running */
			case C_MODELPATTERN_RUNSTATES_RUNNING:
				Sleep(25L);
				break;


			/* Error during any INIT stage */
			case C_MODELPATTERN_RUNSTATES_INIT_ERROR:
			{
				/* Init Error occured, show requester */
				int lastGood = initState;

				if (!_noWinMsg)
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"INIT ERROR: please connect missing device(s)", L"STANDBY");

				_runState = C_MODELPATTERN_RUNSTATES_RUNNING;
			}
			break;


			/* Closing USB instrument connection */
			case C_MODELPATTERN_RUNSTATES_USB_CLOSE:
			{
				// TODO: coding

				_runState = C_MODELPATTERN_RUNSTATES_COM_CLOSE;
			}
			break;


			/* Closing COM / IEC instrument connection */
			case C_MODELPATTERN_RUNSTATES_COM_CLOSE:
			{
				agentUsbReq usbReqData;
				agentComReq comReqData;
				usbReqData.cmd = C_USBREQ_END;
				comReqData.cmd = C_COMREQ_END;
				comReqData.parm = string();

				if (!_noWinMsg)
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: shutting down ...", L"");

				try {
					/* Send shutdown request to the USB_TMC module */
					send(*pAgtUsbTmcReq, usbReqData);

					/* Send shutdown request for each active agent */
					for (int i = 0; i < C_COMINST__COUNT; i++) {
						if (pAgtComReq[i]) {
							send(*(pAgtComReq[i]), comReqData);
						}
					}

					if (_loopShut) {
						_running = false;
					}
					else {
						_runState = _runReinit ? C_MODELPATTERN_RUNSTATES_BEGIN : C_MODELPATTERN_RUNSTATES_NOOP;
					}
				}
				catch (const Concurrency::operation_timed_out& e) {
					(void)e;
				}
			}
			break;


#if 0
			case C_MODELPATTERN_RUNSTATES_COM_CLOSE_2:
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
#endif
			break;


			/* WinSrv SHUTDOWN request */
			case C_MODELPATTERN_RUNSTATES_SHUTDOWN:
			{
				_runReinit = false;
				_loopShut = true;
				_runState = C_MODELPATTERN_RUNSTATES_USB_CLOSE;
			}
			break;


			/* WinSrv REINIT request */
			case C_MODELPATTERN_RUNSTATES_REINIT:
			{
				/* Close any devices and restart init process */
				_runReinit	= true;
				_runState	= C_MODELPATTERN_RUNSTATES_USB_CLOSE;
			}
			break;


			/* Out of order */
			case C_MODELPATTERN_RUNSTATES_NOOP:
			default:
				Sleep(25L);
				break;
			}  // switch ()
		}

		catch (const Concurrency::operation_timed_out& e) {
			char msgBuf[256];
			DWORD dwChars = 0;

			strcpy_s(msgBuf, e.what());
			WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msgBuf, (DWORD)strlen(msgBuf), &dwChars, NULL);

			if (_runState < C_MODELPATTERN_RUNSTATES_USB_CLOSE) {
				_runState = C_MODELPATTERN_RUNSTATES_USB_CLOSE;

			} else if (_runState <= C_MODELPATTERN_RUNSTATES_COM_CLOSE) {
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


void agentModelPattern::checkInstruments(void)
{
	if (g_am_InstList.size()) {
		am_InstList_t::iterator it = g_am_InstList.begin();

		/* Init LIBUSB */
		{
			agentUsbReq usbReqData;
			usbReqData.cmd = C_USBREQ_DO_REGISTRATION;
			send(*pAgtUsbTmcReq, usbReqData);

			/* Wait for response */
			agentUsbRsp usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_USBTMC_TIMEOUT*/);
			SHORT stat = usbRspData.stat;
			if (stat != C_USBRSP_REGISTRATION_LIST) {
				return;
			}
		}

		/* Iterate over all instruments and check which do respond */
		while (it != g_am_InstList.end()) {
			/* From high to low priority */

			/* USB */
			if (checkInstUsb(it)) {
				/* Use USB connection */
				it->actSelected = true;
				it->actRank = 1;

			}

			/* COM */
			if (checkInstCom(it)) {
				/* Use COM connection */
				if (!it->actSelected) {
					it->actSelected = true;
					it->actRank = 2;
				}
			}

			/* Move to next instrument */
			it++;
		}
	}
}

bool agentModelPattern::checkInstUsb(am_InstList_t::iterator it)
{
	if (it->linkUsbIdVendor || it->linkUsbIdProduct) {
		const agentUsbReqDev data = { it->linkUsbIdVendor, it->linkUsbIdProduct };

		agentUsbReq usbReqData;
		usbReqData.cmd = C_USBREQ_IS_DEV_CONNECTED;
		usbReqData.data = (void*)&data;
		send(*pAgtUsbTmcReq, usbReqData);

		agentUsbRsp usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_USBTMC_TIMEOUT*/);
		bool result = *((bool*)usbRspData.data);
		return result;
	}
	else {
		return false;
	}
}

bool agentModelPattern::checkInstCom(am_InstList_t::iterator it)
{
	char buf[C_BUF_SIZE];
	agentComReq comReqData;

	/* Different handling of the serial stream */
	switch (it->listFunction) {
	case INST_FUNC_ROTOR:
	{
		if (pAgtCom[C_COMINST_ROT]) {
			/* Start COM server for the rotor (turntable) */
			pAgtCom[C_COMINST_ROT]->start();
			Sleep(25);

			int len = _snprintf_s(buf, C_BUF_SIZE, C_OPENPARAMS_STR,
				C_ROT_COM_PORT, C_ROT_COM_BAUD, C_ROT_COM_BITS, C_ROT_COM_PARITY, C_ROT_COM_STOPBITS,
				C_SET_IEC_ADDR_INVALID);
			buf[len] = 0;
			comReqData.cmd	= C_COMREQ_OPEN_ZOLIX;
			comReqData.parm = string(buf);
			send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

			agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
			if (comRspData.stat == C_COMRSP_DATA) {
				string rspIdnStr = string(comRspData.data);
				if (rspIdnStr.empty()) {
					comReqData.cmd = C_COMREQ_CLOSE;
					send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
					receive(*(pAgtComRsp[C_COMINST_ROT]));
// TODO: here, new code to check!
				}
				else {
					addSerInstrument(INSTRUMENT_ROTORS_SER__ZOLIX_SC300,
						pAgtCom[C_COMINST_ROT], C_ROT_COM_PORT, C_ROT_COM_BAUD, C_ROT_COM_BITS, C_ROT_COM_PARITY, C_ROT_COM_STOPBITS,
						false, 0,
						rspIdnStr);

					initState = 0x02;
					if (!_noWinMsg)
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 1 ~ rotor port opened", L"");
				}
			}
		}
	}
	break;

	case INST_FUNC_GEN:
	{
		/* Start COM server for the TX */
		if (pAgtCom[C_COMINST_TX]) {
			pAgtCom[C_COMINST_TX]->start();
		}

	}
	break;

	case INST_FUNC_SPEC:
	{
		/* Start COM server for the RX */
		if (pAgtCom[C_COMINST_RX]) {
			pAgtCom[C_COMINST_RX]->start();
		}

	}
	break;

	default: { }
	}  // switch ()

	return false;
}


instrument_t* agentModelPattern::addSerInstrument(	INSTRUMENT_ENUM_t type, 
													agentCom* pAgtCom, uint8_t comPort, uint32_t comBaud, uint8_t comBits, uint8_t comParity, uint8_t comStopbits, 
													bool isIec, uint8_t iecAddr,
													string idn)
{
	instrument_t *ret = NULL;

	switch (type & INSTRUMENT_VARIANT_MASK) {

	case INSTRUMENT_ROTORS__ALL:
	{
		/* Rotors */
		if (ai.inst_rot_cnt < (sizeof(ai.inst_rot) / sizeof(instrument_t))) {
			ret = &(ai.inst_rot[ai.inst_rot_cnt]);
			#if 0
			if (optionalInst)
				memcpy(ret, optionalInst, sizeof(instrument_t));
			#endif
			ai.inst_rot[ai.inst_rot_cnt].type			= type;
			ai.inst_rot[ai.inst_rot_cnt].pAgtCom		= pAgtCom;
			ai.inst_rot[ai.inst_rot_cnt].comPort		= comPort;
			ai.inst_rot[ai.inst_rot_cnt].comBaud		= comBaud;
			ai.inst_rot[ai.inst_rot_cnt].comBits		= comBits;
			ai.inst_rot[ai.inst_rot_cnt].comParity		= comParity;
			ai.inst_rot[ai.inst_rot_cnt].comStopbits	= comStopbits;
			ai.inst_rot[ai.inst_rot_cnt].isIec			= isIec;
			ai.inst_rot[ai.inst_rot_cnt].iecAddr		= iecAddr;

			if (!idn.empty()) {
				strncpy_s(ai.inst_rot[ai.inst_rot_cnt].idn, idn.c_str(), sizeof(ai.inst_rot[ai.inst_rot_cnt].idn));
			}
			
			ai.inst_rot_cnt++;
		}
	}
	break;

	case INSTRUMENT_TRANSMITTERS__ALL:
	{
		/* Transmitters */
		if (ai.inst_tx_cnt < (sizeof(ai.inst_tx) / sizeof(instrument_t))) {
			ret = &(ai.inst_tx[ai.inst_tx_cnt]);
			#if 0
			if (optionalInst)
				memcpy(ret, optionalInst, sizeof(instrument_t));
			#endif
			ai.inst_tx[ai.inst_tx_cnt].type				= type;
			ai.inst_tx[ai.inst_tx_cnt].pAgtCom			= pAgtCom;
			ai.inst_tx[ai.inst_tx_cnt].comPort			= comPort;
			ai.inst_tx[ai.inst_tx_cnt].comBaud			= comBaud;
			ai.inst_tx[ai.inst_tx_cnt].comBits			= comBits;
			ai.inst_tx[ai.inst_tx_cnt].comParity		= comParity;
			ai.inst_tx[ai.inst_tx_cnt].comStopbits		= comStopbits;
			ai.inst_tx[ai.inst_tx_cnt].isIec			= isIec;
			ai.inst_tx[ai.inst_tx_cnt].iecAddr			= iecAddr;

			if (!idn.empty()) {
				strncpy_s(ai.inst_tx[ai.inst_tx_cnt].idn, idn.c_str(), sizeof(ai.inst_tx[ai.inst_tx_cnt].idn));
			}

			ai.inst_tx_cnt++;
		}
	}
	break;

	case INSTRUMENT_RECEIVERS__ALL:
	{
		/* Receivers */
		if (ai.inst_rx_cnt < (sizeof(ai.inst_rx) / sizeof(instrument_t))) {
			ret = &(ai.inst_rx[ai.inst_rx_cnt]);
			#if 0
			if (optionalInst)
				memcpy(ret, optionalInst, sizeof(instrument_t));
			#endif
			ai.inst_rx[ai.inst_rx_cnt].type				= type;
			ai.inst_rx[ai.inst_rx_cnt].pAgtCom			= pAgtCom;
			ai.inst_rx[ai.inst_rx_cnt].comPort			= comPort;
			ai.inst_rx[ai.inst_rx_cnt].comBaud			= comBaud;
			ai.inst_rx[ai.inst_rx_cnt].comBits			= comBits;
			ai.inst_rx[ai.inst_rx_cnt].comParity		= comParity;
			ai.inst_rx[ai.inst_rx_cnt].comStopbits		= comStopbits;
			ai.inst_rx[ai.inst_rx_cnt].isIec			= isIec;
			ai.inst_rx[ai.inst_rx_cnt].iecAddr			= iecAddr;

			if (!idn.empty()) {
				strncpy_s(ai.inst_rx[ai.inst_rx_cnt].idn, idn.c_str(), sizeof(ai.inst_rx[ai.inst_rx_cnt].idn));
			}

			ai.inst_rx_cnt++;
		}
	}
	break;

	}  // switch();

	return ret;
}

void agentModelPattern::wmCmd(int wmId, LPVOID arg)
{
	switch (wmId)
	{
	case ID_CTRL_ALL_RESET:
		initDevices();
		break;

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


	case ID_MODEL_PATTERN_STOP:
		runProcess(C_MODELPATTERN_PROCESS_STOP, 0);
		break;

	case ID_MODEL_PATTERN_180_START:
		// Init display part
		// xxx();

		// Start recording of pattern
		runProcess(C_MODELPATTERN_PROCESS_RECORD_PATTERN_180DEG, 0);
		break;

	case ID_MODEL_PATTERN_360_START:
		// Init display part
		// xxx();

		// Start recording of pattern
		runProcess(C_MODELPATTERN_PROCESS_RECORD_PATTERN_360DEG, 0);
		break;

	}  // switch (message) {
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
		for (int tries = 100000; tries; tries--) {
			send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);	// drop queue
			if (strncmp(comRspData.data.c_str(), "OK", 2)) {
				break;
			}
			Sleep(10L);
		}

		for (int tries = 2; tries; tries--) {
			comReqData.parm = string("?X\r");
			send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);

			if (strncmp(comRspData.data.c_str(), "?X", 2)) {
				break;
			}
			Sleep(10L);
		}

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
