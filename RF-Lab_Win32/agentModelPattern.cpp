#include "stdafx.h"
#include <process.h>

#include "resource.h"
#include "WinSrv.h"

#include "agentModel.h"

#include "externals.h"

#include "agentModelPattern.h"


/* External global vars */
extern HWND	g_hWnd;


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


agentModelPattern::agentModelPattern(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, class WinSrv *winSrv, class agentModel *am, AGENT_ALL_SIMUMODE_t mode)
				 : pAgtComReq{ nullptr }
				 , pAgtComRsp{ nullptr }
				 , pAgtCom{ nullptr }
				 , _arg(nullptr)

				, _winSrv(winSrv)

				 , pAgtMod(am)

				 , hThreadAgtUsbTmc(nullptr)
				 , sThreadDataAgentModelPattern( { 0 } )

				 , pAgtUsbTmc(nullptr)

				 , processing_ID(0)
				 , simuMode(mode)

				 , initState(0)
				 , guiPressedConnect(FALSE)

				 , lastTickPos(0)

				 , txOn(false)
				 , txFrequency(0)
				 , txPower(0)

				 , rxFrequency(0.0)
				 , rxSpan(0.0)
				 , rxLevelMax(0.0)
{
	/* Fast access to connected instruments */
	pConInstruments[C_COMINST_ROT] = g_am_InstList.end();
	pConInstruments[C_COMINST_TX] = g_am_InstList.end();
	pConInstruments[C_COMINST_RX] = g_am_InstList.end();
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
			if (pAgtCom[i]->isDone()) {
				agent::wait(pAgtCom[i], AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			SafeReleaseDelete(&(pAgtCom[i]));
		}
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
	if (!_noWinMsg) {
		pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Shutting down ...", L"");
	}
	WaitForSingleObject(hThreadAgtUsbTmc, 10000L);			// Wait until the thread has ended (timeout = 10 secs
	CloseHandle(hThreadAgtUsbTmc);
}


void agentModelPattern::run(void)
{
	agentComReq comReqData = { 0 };
	bool isComTxConDone = false;
	bool isComRxConDone = false;

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

				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Starting COM threads", L"STARTING ...");
				}

				/* Set up list of instruments */
				pAgtMod->setupInstrumentList();

				/* Start the communication agents */
				agentsInit();

				/* Start thread for process IDs to operate */
				threadsStart();

				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Starting COM threads", L"... DONE");
				}


				/* Request libusb to init */
				{
					agentUsbReq usbReqData;

					usbReqData.cmd = C_USBREQ_DO_REGISTRATION;
					send(*pAgtUsbTmcReq, usbReqData);

					agentUsbRsp usbRspData = receive(*pAgtUsbTmcRsp, AGENT_PATTERN_USBTMC_TIMEOUT);
					if (usbRspData.stat == C_USBRSP_REGISTRATION_DONE) {
						/* USB ready */

						if (!_noWinMsg) {
							pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"USB bus registration", L"... DONE");
						}
					}
				}

				_runState = C_MODELPATTERN_RUNSTATES_CHECK_CONNECTIONS;
			}
			break;

			/* Request list of instruments with their connection settings */
			case C_MODELPATTERN_RUNSTATES_CHECK_CONNECTIONS:
			{
				/* Check for active Instruments */
				checkInstruments();

				/* Update GUI with active/absent instruments */
				_winSrv->instUpdateConnectedInstruments();

				_runState = C_MODELPATTERN_RUNSTATES_WAIT_FOR_GUI;
				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Please select instruments from the menu", L"--- SELECT NOW ---");
				}
			}
			break;

			case C_MODELPATTERN_RUNSTATES_WAIT_FOR_GUI:
			{
				/* Wait for user decissions at the GUI */
				if (guiPressedConnect) {
					_runState = C_MODELPATTERN_RUNSTATES_COM_CONNECT;
				}
				else {
					Sleep(25);
				}
			}
			break;


			/* Connect the selected COM / IEC instruments */
			case C_MODELPATTERN_RUNSTATES_COM_CONNECT:
			{
				char buf[C_BUF_SIZE];

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

					/* Give some time for the threads to init */
					Sleep(25);
				}

				initState = 0x01;

				/* Open Rotor */
				if (pAgtCom[C_COMINST_ROT]) {
					if (!_noWinMsg) {
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Connect COMs", L"ROTOR ...");
					}

					/* Search rotor */
					am_InstList_t::iterator it = g_am_InstList.begin();
					while (it != g_am_InstList.end()) {
						/* Check for rotor */
						if (it->listFunction != INST_FUNC_ROTOR) {
							/* Try next */
							it++;
							continue;
						}

						/* Rotor found */
						if (it->actSelected && it->actLink && (it->linkType & LINKTYPE_COM)) {
							int len = _snprintf_s(buf, C_BUF_SIZE,
								C_OPENPARAMS_STR,
								it->linkSerPort, it->linkSerBaud, it->linkSerBits, it->linkSerParity, it->linkSerStopbits,
								it->linkSerIecAddr);
							buf[len] = 0;

							/* Open port */
							comReqData.cmd = C_COMREQ_OPEN_ZOLIX;
							comReqData.parm = string(buf);
							send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

							agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
							if (comRspData.stat == C_COMRSP_DATA) {
								string rspIdnStr = string(comRspData.data);
								if (rspIdnStr.empty()) {
									/* Stop and drop COM-Server for the rotor */
									comReqData.cmd = C_COMREQ_END;
									send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
									receive(*(pAgtComRsp[C_COMINST_ROT]));

									if (pAgtCom[C_COMINST_ROT]->isDone()) {
										agent::wait(pAgtCom[C_COMINST_ROT], AGENT_PATTERN_RECEIVE_TIMEOUT);
									}
									SafeReleaseDelete(&(pAgtCom[C_COMINST_ROT]));
								}
								else {
									/* Set instrument which is linked to this agentCom thread */
									pAgtCom[C_COMINST_ROT]->setInstrument(&it);

									/* Fast access table */
									it->actIfcType = ACT_IFC_COM;
									pConInstruments[C_COMINST_ROT] = it;

									initState = 0x02;
								}
							}
							break;
						}
					}
				}

				/* Open TX & RX */
				if (pAgtCom[C_COMINST_TX] && pAgtCom[C_COMINST_RX]) {
					if (!_noWinMsg) {
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Connect COMs", L"RF-Gen & Spec ...");
					}

					/* Iterate over all instruments */
					am_InstList_t::iterator it = g_am_InstList.begin();
					while (it != g_am_InstList.end()) {
						if (!it->actSelected || !it->actLink || !(it->linkType & LINKTYPE_COM) || 
							((it->listFunction != INST_FUNC_GEN) && (it->listFunction != INST_FUNC_SPEC))) {
							/* Try next */
							it++;
							continue;
						}

						/* COM request template */
						int len = _snprintf_s(buf, C_BUF_SIZE,
							C_OPENPARAMS_STR,
							it->linkSerPort, it->linkSerBaud, it->linkSerBits, it->linkSerParity, it->linkSerStopbits,
							it->linkSerIecAddr);
						buf[len] = 0;

						/* Open RF-Generator */
						if (it->listFunction == INST_FUNC_GEN && !isComTxConDone) 
						{
							/* Open port */
							comReqData.cmd = C_COMREQ_OPEN_IDN;
							comReqData.parm = string(buf);
							send(*(pAgtComReq[C_COMINST_TX]), comReqData);

							agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_TX]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
							if (comRspData.stat == C_COMRSP_DATA) {
								string rspIdnStr = string(comRspData.data);
								if (rspIdnStr.empty()) {
									/* Close connection */
									comReqData.cmd = C_COMREQ_CLOSE;
									send(*(pAgtComReq[C_COMINST_TX]), comReqData);
									receive(*(pAgtComRsp[C_COMINST_TX]));

								}
								else {
									/* Set instrument which is linked to this agentCom thread */
									pAgtCom[C_COMINST_TX]->setInstrument(&it);
									isComTxConDone = true;

									/* Fast access table */
									it->actIfcType = ACT_IFC_COM;
									pConInstruments[C_COMINST_TX] = it;
								}
							}
						}

						/* Open Spectrum Analysator */
						if (it->listFunction == INST_FUNC_SPEC && !isComRxConDone)
						{
							/* Open port */
							comReqData.cmd = C_COMREQ_OPEN_IDN;
							comReqData.parm = string(buf);
							send(*(pAgtComReq[C_COMINST_RX]), comReqData);

							agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_RX]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
							if (comRspData.stat == C_COMRSP_DATA) {
								string rspIdnStr = string(comRspData.data);
								if (rspIdnStr.empty()) {
									/* Close connection */
									comReqData.cmd = C_COMREQ_CLOSE;
									send(*(pAgtComReq[C_COMINST_RX]), comReqData);
									receive(*(pAgtComRsp[C_COMINST_RX]));

								}
								else {
									/* Set instrument which is linked to this agentCom thread */
									pAgtCom[C_COMINST_RX]->setInstrument(&it);
									isComRxConDone = true;

									/* Fast access table */
									it->actIfcType = ACT_IFC_COM;
									pConInstruments[C_COMINST_RX] = it;
								}
							}
						}

						/* Short-cut when both are opened */
						if (isComTxConDone && isComRxConDone) {
							break;
						}

						/* Iterate over all instruments */
						it++;
					}

					/* Stop and drop COM-Server for the TX when not connected */
					if (!isComTxConDone) {
						comReqData.cmd = C_COMREQ_END;
						send(*(pAgtComReq[C_COMINST_TX]), comReqData);
						receive(*(pAgtComRsp[C_COMINST_TX]));

						if (pAgtCom[C_COMINST_TX]->isDone()) {
							agent::wait(pAgtCom[C_COMINST_TX], AGENT_PATTERN_RECEIVE_TIMEOUT);
						}
						SafeReleaseDelete(&(pAgtCom[C_COMINST_TX]));
					}

					/* Stop and drop COM-Server for the RX when not connected */
					if (!isComRxConDone) {
						comReqData.cmd = C_COMREQ_END;
						send(*(pAgtComReq[C_COMINST_RX]), comReqData);
						receive(*(pAgtComRsp[C_COMINST_RX]));

						if (pAgtCom[C_COMINST_RX]->isDone()) {
							agent::wait(pAgtCom[C_COMINST_RX], AGENT_PATTERN_RECEIVE_TIMEOUT);
						}
						SafeReleaseDelete(&(pAgtCom[C_COMINST_RX]));
					}

					/* Jump to next state */
					_runState = C_MODELPATTERN_RUNSTATES_USB_CONNECT;
				}
			break;

			/* Connect the selected USB instruments */
			case C_MODELPATTERN_RUNSTATES_USB_CONNECT:
			{
				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Connect USB", L"RF-Gen & Spec ...");
				}

				/* Following state if all goes well */
				_runState = C_MODELPATTERN_RUNSTATES_INST_COM_INIT;

				/* Iterate over all instruments */
				am_InstList_t::iterator it = g_am_InstList.begin();
				while (it != g_am_InstList.end()) {
					if (!it->actSelected || !it->actLink || !(it->linkType & LINKTYPE_USB) ||
						((it->listFunction != INST_FUNC_GEN) && (it->listFunction != INST_FUNC_SPEC))) {
						/* Try next */
						it++;
						continue;
					}

					/* Open USB device */
					{
						agentComReqUsbDev_t usbDev;
						usbDev.usbIdVendor  = it->linkUsbIdVendor;
						usbDev.usbIdProduct = it->linkUsbIdProduct;

						/* Request to open */
						agentUsbReq usbReqData;
						usbReqData.cmd = C_USBREQ_CONNECT;
						usbReqData.data = &usbDev;
						send(*pAgtUsbTmcReq, usbReqData);

						/* Get USB handle */
						agentUsbRsp usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_USBTMC_TIMEOUT*/ );
						if (usbRspData.stat == C_USBRSP_DEV_CONNECT_HANDLE) {
							/* Success connection USB device */
							it->pLinkUsbHandle = (libusb_device_handle*)usbRspData.data;

							switch (it->listFunction) {
							case INST_FUNC_GEN:
								/* Fast access table */
								it->actIfcType = ACT_IFC_USB;
								pConInstruments[C_COMINST_TX] = it;
								break;

							case INST_FUNC_SPEC:
								/* Fast access table */
								it->actIfcType = ACT_IFC_USB;
								pConInstruments[C_COMINST_RX] = it;
								break;
							}
						}
						else if (usbRspData.stat == C_USBRSP_ERR__LIBUSB_ASSIGNMENT_MISSING) {
							char errMsg[256] = { 0 };
							sprintf_s(errMsg, sizeof(errMsg) >> 1, "Error when connecting USB device %s\n\nUse ZADIG to assign libusb to that device.\n", it->listEntryName.c_str());
							MessageBoxA(pAgtMod->getWnd(), errMsg, "LibUSB connection error", MB_ICONERROR);
							_runState = C_MODELPATTERN_RUNSTATES_SHUTDOWN;
							break;
						}
						else {
							char errMsg[256] = { 0 };
							sprintf_s(errMsg, sizeof(errMsg) >> 1, "Error when connecting USB device %s\n\nLibUSB denies connection.\n", it->listEntryName.c_str());
							MessageBoxA(pAgtMod->getWnd(), errMsg, "LibUSB connection error", MB_ICONERROR);
							_runState = C_MODELPATTERN_RUNSTATES_SHUTDOWN;
							break;
						}
					}

					/* Next instrument to check */
					it++;
				}
			}
			break;


			/* Initilization of selected COM / IEC instrument(s) */
			case C_MODELPATTERN_RUNSTATES_INST_COM_INIT:
			{
				agentComReq comReqData;
				agentComRsp comRspData;

				initState = 0x40;
				_runState = C_MODELPATTERN_RUNSTATES_INST_USB_INIT;				// next runstate default setting

				/* COM: Rotor Init */
				if (pAgtCom[C_COMINST_ROT]) {
					/* Zolix commands to init device */
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

							if (!_noWinMsg) {
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 1 ~ rotor init done", L"");
							}
							initState = 0x44;
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0x45;
							if (!_noWinMsg) {
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 1 ~ rotor init exception!", L"ERROR!");
							}
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}
				}
				if (_runState == C_MODELPATTERN_RUNSTATES_INIT_ERROR) {
					break;
				}

				/* COM: TX Init */
				if (pAgtCom[C_COMINST_TX]) {
					/* Syncing with the device */
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
									if (!_noWinMsg) {
										pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 2 ~ TX init exception! (1)", L"ERROR!");
									}
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
								if (!_noWinMsg) {
									pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 2 ~ TX init done", L"");
								}
								initState = 0x56;
							}
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0x5F;
							if (!_noWinMsg) {
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 2 ~ TX init exception! (2)", L"ERROR!");
							}
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
						if (!_noWinMsg) {
							pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 2 ~ TX default parameters are set", L"");
						}
					}
#endif
				}

				/* COM: RX Init */
				if (pAgtCom[C_COMINST_RX]) {
					/* Syncing with the device */
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
									if (!_noWinMsg) {
										pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 3 ~ RX init exception! (1)", L"ERROR!");
									}
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
							if (!_noWinMsg) {
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 3 ~ RX init done", L"");
							}
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0xAF;
							if (!_noWinMsg) {
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 3 ~ RX init exception! (2)", L"ERROR!");
							}
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
						if (!_noWinMsg) {
							pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: RX 3 ~ default parameters are set", L"");
						}
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
							if (!_noWinMsg) {
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 3 ~ RX display parameters are set", L"");
							}
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0xCF;
							if (!_noWinMsg) {
								pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: 3 ~ RX display parameters", L"WARNING!");
							}
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}
				}
				_runState = C_MODELPATTERN_RUNSTATES_INST_USB_INIT;
			}
			break;

			/* Initilization of selected USB instrument(s) */
			case C_MODELPATTERN_RUNSTATES_INST_USB_INIT:
			{
				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"USB bus init", L"... DONE");
				}

				// TODO: coding





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

				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"INIT ERROR: please connect missing device(s)", L"--- MISSING ---");
					Sleep(1000L);
				}

				_runState = C_MODELPATTERN_RUNSTATES_RUNNING;
			}
			break;


			/* Send instrument close connections */
			case C_MODELPATTERN_RUNSTATES_USB_CLOSE_SEND:
			{
				agentUsbReq usbReqData;
				usbReqData.cmd = C_USBREQ_END;
				comReqData.parm = string();

				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"USB: close", L"SHUTTING DOWN ...");
					Sleep(500L);
				}

				try {
					/* Send shutdown request to the USB_TMC module */
					send(*pAgtUsbTmcReq, usbReqData);
				}
				catch (const Concurrency::operation_timed_out& e) {
					(void)e;
				}

				_runState = C_MODELPATTERN_RUNSTATES_COM_CLOSE_SEND;
			}
			break;
			
			case C_MODELPATTERN_RUNSTATES_COM_CLOSE_SEND:
			{
				agentComReq comReqData;
				comReqData.cmd = C_COMREQ_END;
				comReqData.parm = string();

				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: close", L"SHUTTING DOWN ...");
					Sleep(500L);
				}

				try {
					/* Send shutdown request for each active agent */
					for (int i = 0; i < C_COMINST__COUNT; i++) {
						if (pAgtComReq[i]) {
							send(*(pAgtComReq[i]), comReqData);
						}
					}
				}
				catch (const Concurrency::operation_timed_out& e) {
					(void)e;
				}

				_runState = C_MODELPATTERN_RUNSTATES_USB_CLOSE_WAIT;
			}
			break;


			/* Wait until closing is done */
			case C_MODELPATTERN_RUNSTATES_USB_CLOSE_WAIT:
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

				/* Stop the ProcessID thread */
				threadsStop();

				_runState = C_MODELPATTERN_RUNSTATES_COM_CLOSE_WAIT;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_COM_CLOSE_WAIT:
			{
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

				/* Shutdown the COM and USB agents */
				agentsShutdown();

				if (_loopShut) {
					_runState = C_MODELPATTERN_RUNSTATES_NOOP;
					_running = false;

					if (!_noWinMsg) {
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: shut down", L"... CLOSED");
					}
				}
				else if (_runReinit) {
					_runState = C_MODELPATTERN_RUNSTATES_BEGIN;

					if (!_noWinMsg) {
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: shut down", L"REINIT ...");
						Sleep(500L);
					}
				}
				else {
					_runState = C_MODELPATTERN_RUNSTATES_NOOP;
					_running = false;

					if (!_noWinMsg) {
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: shut down", L"... CLOSED");
					}
				}
			}
			break;


			/* WinSrv SHUTDOWN request */
			case C_MODELPATTERN_RUNSTATES_SHUTDOWN:
			{
				_runReinit = false;
				_loopShut = true;
				_runState = C_MODELPATTERN_RUNSTATES_USB_CLOSE_SEND;
			}
			break;


			/* WinSrv REINIT request */
			case C_MODELPATTERN_RUNSTATES_REINIT:
			{
				/* Close any devices and restart init process */
				_runReinit = true;
				_loopShut = false;
				_runState = C_MODELPATTERN_RUNSTATES_USB_CLOSE_SEND;
			}
			break;


			/* Out of order */
			case C_MODELPATTERN_RUNSTATES_NOOP:
			default:
				Sleep(25L);
				break;
			}  // switch ()
			}
		}

		catch (const Concurrency::operation_timed_out& e) {
			char msgBuf[256];
			DWORD dwChars = 0;

			strcpy_s(msgBuf, e.what());
			WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msgBuf, (DWORD)strlen(msgBuf), &dwChars, NULL);

			if (_runState < C_MODELPATTERN_RUNSTATES_USB_CLOSE_SEND) {
				_runState = C_MODELPATTERN_RUNSTATES_USB_CLOSE_SEND;

			} else if (_runState <= C_MODELPATTERN_RUNSTATES_COM_CLOSE_SEND) {
				if (_runReinit) {
					_runState = C_MODELPATTERN_RUNSTATES_BEGIN;
					if (!_noWinMsg) {
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: broken", L"REINIT");
						Sleep(500L);
					}

				} else {
					_runState = C_MODELPATTERN_RUNSTATES_NOOP;
					_running = false;
					if (!_noWinMsg) {
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: broken", L"CLOSED");
						Sleep(500L);
					}
				}
			}
		}
	}

	if (!_noWinMsg) {
		pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Shutting down - please close the application", L"BYE");
		Sleep(500L);
	}

	_done = true;
	agent::done();
}


void agentModelPattern::checkInstruments(void)
{
	am_InstList_t::iterator it = g_am_InstList.begin();

	if (!_noWinMsg) {
		pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Checking list of instruments", L"CONNECTING ...");
	}

	/* Iterate over all instruments and check which one responds */
	while (it != g_am_InstList.end()) {
		const LinkType_BM_t linkType = it->linkType;

		/* Try Ethernet connection */
		if (linkType & LINKTYPE_ETH) {
			instTryEth(it);
			//it->actLink = true;				// TODO: remove me!
		}

		/* Try USB connection */
		if (linkType & LINKTYPE_USB) {
			instTryUsb(it);
		}

		/* Try COM connection, even when IEC is interfaced to a COM port */
		if (linkType & LINKTYPE_COM) {
			instTryCom(it);
		}

		if (!linkType) {
			//it->actLink = false;
			it->actSelected = false;
		}

		/* Move to next instrument */
		it++;
	}

	if (!_noWinMsg) {
		pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Checking list of instruments...", L"... DONE");
		Sleep(500L);
	}
}


bool agentModelPattern::instTryEth(am_InstList_t::iterator it)
{

	return false;
}

bool agentModelPattern::instTryUsb(am_InstList_t::iterator it)
{
	bool isConnected = false;
	agentComReqUsbDev_t usbDev;
	usbDev.usbIdVendor = it->linkUsbIdVendor;
	usbDev.usbIdProduct = it->linkUsbIdProduct;

	agentUsbReq usbReqData;
	usbReqData.cmd = C_USBREQ_IS_DEV_CONNECTED;
	usbReqData.data = &usbDev;
	send(*pAgtUsbTmcReq, usbReqData);

	agentUsbRsp usbRspData = receive(*pAgtUsbTmcRsp, AGENT_PATTERN_USBTMC_TIMEOUT);
	if (usbRspData.stat == C_USBRSP_IS_DEV_CONNECTED) {
		isConnected = *((bool*) usbRspData.data);
		if (!isConnected) {
			/* Remove USB link bit */
			it->linkType &= ~(LINKTYPE_USB);
		}
		else {
			it->actLink = true;
		}
	}
	return isConnected;
}

bool agentModelPattern::instTryCom(am_InstList_t::iterator it)
{
	/* Open COM connection */
	agentComReq comReqData;
	char buf[C_BUF_SIZE];

	/* Different handling of the serial stream */
	switch (it->listFunction) {

	case INST_FUNC_ROTOR:
	{
		bool isConnected = false;

		/* Start COM server for the rotor (turntable) */
		pAgtCom[C_COMINST_ROT]->start();
		Sleep(25);

		int len = _snprintf_s(buf, C_BUF_SIZE, 
			C_OPENPARAMS_STR,
			it->linkSerPort, it->linkSerBaud, it->linkSerBits, it->linkSerParity, it->linkSerStopbits,
			it->linkSerIecAddr);
		buf[len] = 0;

		comReqData.cmd  = C_COMREQ_OPEN_ZOLIX;
		comReqData.parm = string(buf);
		send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

		agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
		if (comRspData.stat == C_COMRSP_DATA) {
			string rspIdnStr = string(comRspData.data);
			/* Rotor IDN string */
			if (!rspIdnStr.empty()) {
				isConnected = true;
			}
		}

		if (isConnected) {
			/* Device linked */
			it->actLink = true;

			if (!_noWinMsg) {
				pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: rotor", L"... FOUND");
				Sleep(500L);
			}
		}

		else {
			/* Remove COM link bit(s), is linked and selection bits */
			it->linkType	&= ~(LINKTYPE_IEC_VIA_SER | LINKTYPE_COM);
			it->actLink		 = false;
			it->actSelected	 = false;
		}

		/* Close connection again */
		comReqData.cmd = C_COMREQ_CLOSE;
		send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

		comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]));
		(void)comRspData;
	}
	break;

	case INST_FUNC_GEN:
	case INST_FUNC_SPEC:
	{
		const int agtComIdx = (it->listFunction == INST_FUNC_GEN ?  C_COMINST_TX : C_COMINST_RX);

		if (pAgtCom[agtComIdx]) {
			bool isConnected = false;

			/* Start COM servers for TX and RX */
			pAgtCom[agtComIdx]->start();
			Sleep(25);

			int len = _snprintf_s(buf, C_BUF_SIZE,
				C_OPENPARAMS_STR,
				it->linkSerPort, it->linkSerBaud, it->linkSerBits, it->linkSerParity, it->linkSerStopbits,
				C_SET_IEC_ADDR_INVALID);
			buf[len] = 0;

			comReqData.cmd = C_COMREQ_OPEN_IDN;
			comReqData.parm = string(buf);
			send(*(pAgtComReq[agtComIdx]), comReqData);

			agentComRsp comRspData = receive(*(pAgtComRsp[agtComIdx]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			if (comRspData.stat == C_COMRSP_DATA) {
				string rspIdnStr = string(comRspData.data);
				/* IDN string */
				if (!rspIdnStr.empty()) {
					isConnected = true;
				}
			}

			if (isConnected) {
				/* Device linked */
				it->actLink = true;

				/* Check for the device function */
				if (it->listFunction == INST_FUNC_GEN) {
					if (!_noWinMsg) {
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: RF generator", L"... FOUND");
						Sleep(500L);
					}
				}

				else if (it->listFunction == INST_FUNC_SPEC) {
					if (!_noWinMsg) {
						pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: Spectrum analyzer", L"... FOUND");
						Sleep(500L);
					}
				}
			}

			else {
				/* Remove COM link bit(s), is linked and selection bits */
				it->linkType &= ~(LINKTYPE_IEC_VIA_SER | LINKTYPE_COM);
				it->actLink = false;
				it->actSelected = false;
			}

			/* Close connection again */
			comReqData.cmd = C_COMREQ_CLOSE;
			send(*(pAgtComReq[agtComIdx]), comReqData);

			comRspData = receive(*(pAgtComRsp[agtComIdx]));
			(void)comRspData;
		}
	}
	break;

	default: { }
	}  // switch (it->listFunction)

	return false;
}


#ifdef OLD
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
#endif


void agentModelPattern::wmCmd(int wmId, LPVOID arg)
{
	HMENU hMenu = GetMenu(g_hWnd);

	/* Select device and deselect all other devices in Menu and instrument list */
	if (wmId >= ID_AKTOR_ITEM0_ && wmId < (ID_SPEK_ITEM0_ + 255)) {
		Inst_Function_t instFunc = INST_FUNC_UNKNOWN;

		/* Iterate over the instrument list */
		am_InstList_t::iterator it = g_am_InstList.begin();

		/* Iterate over all instruments and check which one responds */
		while (it != g_am_InstList.end()) {
			/* Selected menu item found ? */
			if (it->winID == wmId) {
				/* Hold new state */
				it->actSelected = true;

				/* Activate menu-item */
				CheckMenuItem(hMenu, it->winID, MF_BYCOMMAND | MF_CHECKED);

				/* Function group the selection is modified for */
				instFunc = it->listFunction;

				switch (instFunc) {
				case INST_FUNC_ROTOR:
					_winSrv->_menuInfo.rotorEnabled = true;
					break;

				case INST_FUNC_GEN:
					_winSrv->_menuInfo.rfGenEnabled = true;
					break;

				case INST_FUNC_SPEC:
					_winSrv->_menuInfo.specEnabled = true;
					break;
				}

				break;
			}
			/* Iterate to next instrument */
			it++;
		}

		/* Turn off all selections of other instruments of the same function */
		it = g_am_InstList.begin();

		/* Iterate over all instruments and check which one responds */
		while (it != g_am_InstList.end()) {
			/* Any other menu item of the same function found ? */
			if (it->winID != wmId &&
				it->listFunction == instFunc)
			{
				/* Set new state */
				it->actSelected = false;

				/* Deactivate menu-item */
				CheckMenuItem(hMenu, it->winID, MF_BYCOMMAND | MF_UNCHECKED);
			}
			/* Iterate to next instrument */
			it++;
		}

		/* All functions (Rotor, RfGen and Spec) are selected */
		if (_winSrv->_menuInfo.rotorEnabled && _winSrv->_menuInfo.rfGenEnabled && _winSrv->_menuInfo.specEnabled) {
			/* Enable connected / disconnected menu items */
			EnableMenuItem(hMenu, ID_INSTRUMENTEN_CONNECT, MF_BYCOMMAND);
		}
	}

	else {
		switch (wmId)
		{
			/* Connect and Disconnet */

		case ID_INSTRUMENTEN_CONNECT:
			connectDevices();
			break;

		case ID_INSTRUMENTEN_DISCONNECT:
			initDevices();
			break;


			/* Rotor commands */

		case ID_ROTOR_GOTO_0:
			if (_runState == C_MODELPATTERN_RUNSTATES_RUNNING) {
				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 1 ~ rotor turns to  0°  position", L"RUNNING ...");
				}
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

						if (!_noWinMsg) {
							swprintf_s(l_status2, l_status_size, L"COM: 1 ~ rotor turns to  %+03d°  position", (*pi) / 1000);
							pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", l_status2, L"RUNNING ...");
						}

						LocalUnlock(l_status2_alloc);
						LocalFree(l_status2_alloc);
					}

					/* Start rotor */
					runProcess(C_MODELPATTERN_PROCESS_GOTO_X, *pi);
				}
			}
			break;

		case ID_ROTOR_STOP:
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


			/* HF-Generator settings */

			// nothing yet


			/* HF-Generator settings */

			// nothing yet

		}  // switch (message)
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

		case C_MODELPATTERN_PROCESS_CONNECT_DEVICES:
		{
			/* GUI response, all devices are selected to connect und run a model */
			if (m->o->_running) {
				while (m->o->_runState < C_MODELPATTERN_RUNSTATES_WAIT_FOR_GUI) {
					Sleep(25);
				}
				m->o->guiPressedConnect = TRUE;
				m->o->processing_ID = C_MODELPATTERN_PROCESS_NOOP;
			}
		}
		break;

		case C_MODELPATTERN_PROCESS_NOOP:
		default:
		{
			Sleep(25);
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


void agentModelPattern::connectDevices(void)
{
	if (_running && (_runState == C_MODELPATTERN_RUNSTATES_WAIT_FOR_GUI)) {
		/* Enable the menu items */
		pAgtMod->getWinSrv()->instActivateMenuItem(ID_AKTOR_ITEM0_, true);
		pAgtMod->getWinSrv()->instActivateMenuItem(ID_GENERATOR_ITEM0_, true);
		pAgtMod->getWinSrv()->instActivateMenuItem(ID_SPEK_ITEM0_, true);

		/* Init the connections */
		runProcess(C_MODELPATTERN_PROCESS_CONNECT_DEVICES, 0);
	}
}

void agentModelPattern::initDevices(void)
{
	if (_running && (
		(_runState == C_MODELPATTERN_RUNSTATES_RUNNING) ||
		(_runState == C_MODELPATTERN_RUNSTATES_WAIT_FOR_GUI))
	) {
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

	if (!_noWinMsg) {
		swprintf_s(l_status3, l_status_size, L"position: %+03.0lf°", pos);
		pAgtMod->getWinSrv()->reportStatus(NULL, NULL, l_status3);
	}

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
		/* Do sync COM */
		comReqData.cmd = C_COMREQ_COM_SEND;
		comReqData.parm = string("\r");
		for (int tries = 10; tries; tries--) {
			send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);	// drop queue
			if (!strncmp(comRspData.data.c_str(), "OK", 2)) {
				break;
			}
			Sleep(10L);
		}

		/* Try to receive the position */
		for (int tries = 2; tries; tries--) {
			comReqData.parm = string("?X\r");
			send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);

			if (!strncmp(comRspData.data.c_str(), "?X", 2)) {
				break;
			}
			Sleep(10L);
		}

 		if (comRspData.stat == C_COMRSP_DATA) {
			int posDiff = 0;

			if (!agentModel::parseStr2Long(&pos, comRspData.data.c_str(), "?X,%ld", '?')) {
				agentModel::setLastTickPos(pos);
			}
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
	if (!_noWinMsg) {
		pAgtMod->getWinSrv()->reportStatus(NULL, NULL, L"goto HOME position");
	}
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
