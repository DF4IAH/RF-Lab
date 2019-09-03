#include "stdafx.h"
#include <process.h>

#include "resource.h"
#include "WinSrv.h"

#include <list>
#include <string>
#include <math.h>

#include "agentModel.h"
#include "externals.h"
#include "agentModelPattern.h"


using namespace std;


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

				 , measDataEntries()

				 , processing_ID(0)
				 , simuMode(mode)

				 , initState(0)
				 , guiPressedConnect(FALSE)

				 , lastTickPos(0)

				 , txOn(false)
				 , txFrequency(0)
				 , txPower(0.0)

				 , rxFrequency(0.0)
				 , rxSpan(0.0)
				 , rxLevelMax(0.0)
{
	/* Fast access to connected instruments */
	pConInstruments[C_COMINST_ROT] = g_InstList.end();
	pConInstruments[C_COMINST_TX]  = g_InstList.end();
	pConInstruments[C_COMINST_RX]  = g_InstList.end();

	/* Make sure pointer vars are being set-up */
	measDataEntries.posDeg	   = nullptr;
	measDataEntries.rxPwrMag   = nullptr;
	measDataEntries.rxPwrPhase = nullptr;
}

agentModelPattern::~agentModelPattern()
{
	/* Free dynamic allocated lists */
	SafeDelete(&measDataEntries.posDeg);
	SafeDelete(&measDataEntries.rxPwrMag);
	SafeDelete(&measDataEntries.rxPwrPhase);
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
	pAgtUsbTmcReq = new unbounded_buffer<AgentUsbReq_t>;
	pAgtUsbTmcRsp = new unbounded_buffer<AgentUsbRsp_t>;

	for (int i = 0; i < C_COMINST__COUNT; ++i) {
		pAgtComReq[i] = new unbounded_buffer<AgentComReq_t>;
		pAgtComRsp[i] = new unbounded_buffer<AgentComRsp_t>;
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
	AgentComReq_t comReqData = { 0 };
	bool isComTxConDone = false;
	bool isComRxConDone = false;

	/* Delay until window is up and ready */
	while (!(WinSrv::srvReady())) {
		Sleep(25);
	}

	DWORD dwWaitResult = WaitForSingleObject(g_InstListMutex, INFINITE);  // No time-out interval
	if (WAIT_ABANDONED == dwWaitResult) {
		/* Oops? */
		if (!_noWinMsg) {
			pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Internal error - can not get access to InstList", L"???");
		}

		_done = true;
		agent::done();
		return;
	}
	else if (WAIT_OBJECT_0 == dwWaitResult) {
		/* Got mutex */

		/* run code below ... */
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
					AgentUsbReq_t usbReqData;

					usbReqData.cmd = C_USBREQ_DO_REGISTRATION;
					send(*pAgtUsbTmcReq, usbReqData);

					AgentUsbRsp_t usbRspData = receive(*pAgtUsbTmcRsp, AGENT_PATTERN_USBTMC_TIMEOUT);
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
					_runState = C_MODELPATTERN_RUNSTATES_USB_CONNECT;
				}
				else {
					Sleep(25);
				}
			}
			break;


			/* Connect the selected USB instruments */
			case C_MODELPATTERN_RUNSTATES_USB_CONNECT:
			{
				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Connect USB", L"RF-Gen & Spec ...");
				}

				/* Following state if all goes well */
				_runState = C_MODELPATTERN_RUNSTATES_COM_CONNECT;

				/* Iterate over all instruments */
				InstList_t::iterator it = g_InstList.begin();
				while (it != g_InstList.end()) {
					if (!it->actSelected || !it->actLink || (it->actIfcType != ACT_IFC_USB) ||
						((it->listFunction != INST_FUNC_GEN) && (it->listFunction != INST_FUNC_SPEC))) {
						/* Try next */
						it++;
						continue;
					}

					/* Open USB-TMC device */
					{
						AgentComReqUsbDev_t usbDev;
						usbDev.usbIdVendor = it->linkUsb_dev_usb_idVendor;
						usbDev.usbIdProduct = it->linkUsb_dev_usb_idProduct;

						/* Request to open */
						AgentUsbReq_t usbReqData = { 0 };
						usbReqData.cmd = C_USBREQ_CONNECT;
						usbReqData.thisInst = *it;
						usbReqData.data1 = &usbDev;
						send(*pAgtUsbTmcReq, usbReqData);

						/* Get open state */
						AgentUsbRsp_t usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_USBTMC_TIMEOUT*/);
						*it = usbRspData.thisInst;

						/* Check for result */
						if (usbRspData.stat == C_USBRSP_OK) {

							/* Success connection USB device */
							switch (it->listFunction) {
							case INST_FUNC_GEN:
								/* Fast access table */
								pConInstruments[C_COMINST_TX] = it;
								break;

							case INST_FUNC_SPEC:
								/* Fast access table */
								pConInstruments[C_COMINST_RX] = it;
								break;
							}
						}
						else if (usbRspData.stat == C_USBRSP_ERR) {
							char errMsg[256] = { 0 };
							sprintf_s(errMsg, sizeof(errMsg), "Error when connecting USB device %s\n\nLibUSB denies connection.\nlibUSB error: %s\n", it->listEntryName.c_str(), (const char*)usbRspData.data1);
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
					InstList_t::iterator it = g_InstList.begin();
					while (it != g_InstList.end()) {
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

							AgentComRsp_t comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
							if (comRspData.stat == C_COMRSP_DATA) {
								string rspIdnStr = string(comRspData.data1);
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
					InstList_t::iterator it = g_InstList.begin();
					while (it != g_InstList.end()) {
						if (!it->actSelected || !it->actLink || (it->actIfcType != ACT_IFC_COM) ||
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

							AgentComRsp_t comRspData = receive(*(pAgtComRsp[C_COMINST_TX]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
							if (comRspData.stat == C_COMRSP_DATA) {
								string rspIdnStr = string(comRspData.data1);
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

							AgentComRsp_t comRspData = receive(*(pAgtComRsp[C_COMINST_RX]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
							if (comRspData.stat == C_COMRSP_DATA) {
								string rspIdnStr = string(comRspData.data1);
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
					_runState = C_MODELPATTERN_RUNSTATES_INST_USB_INIT;
				}
			break;


			/* Initilization of selected USB instrument(s) */
			case C_MODELPATTERN_RUNSTATES_INST_USB_INIT:
			{
				AgentUsbReq_t usbReqData;
				AgentUsbRsp_t usbRspData;

				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"USB instruments are initialized", L"USB INIT");
				}

				/* Iterate over all instruments */
				InstList_t::iterator it = g_InstList.begin();
				while (it != g_InstList.end()) {
					if (!it->actSelected || !it->actLink || !(it->linkType & LINKTYPE_USB) ||
						((it->listFunction != INST_FUNC_GEN) && (it->listFunction != INST_FUNC_SPEC))) {
						/* Try next */
						it++;
						continue;
					}

					switch (it->listFunction) {
					case INST_FUNC_GEN:
					{
						/* Send frequency in Hz */
						setTxFrequencyValue(it->txCurRfQrg);

						/* Send power in dBm */
						setTxPwrValue(it->txCurRfPwr);

						/* Send power On/Off */
						setTxOnState(it->txCurRfOn);
					}
					break;

					case INST_FUNC_SPEC:
					{
						/* Send Span in Hz */
						setRxSpanValue(it->rxCurRfSpan);

						/* Send center frequency in Hz */
						setRxFrequencyValue(it->rxCurRfQrg);

						/* Send max amplitude to adjust attenuator */
						setRxLevelMaxValue(it->rxCurRfPwrHi);
					}
					break;

					}  // switch()

					   /* Next instrument to check */
					it++;
				}

				/* Success, all devices are ready */
				_runState = C_MODELPATTERN_RUNSTATES_INST_COM_INIT;
			}
			break;

			/* Initilization of selected COM / IEC instrument(s) */
			case C_MODELPATTERN_RUNSTATES_INST_COM_INIT:
			{
				AgentComReq_t comReqData;
				AgentComRsp_t comRspData;

				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM instruments are initialized", L"COM INIT");
				}

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
							} while (_strnicmp(&(comRspData.data1[0]), "ROHDE", 5));
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
							} while (_strnicmp(&(comRspData.data1[0]), "ROHDE", 5));
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

				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Ready to run meassurements", L"--- SELECT NOW ---");
				}

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
				AgentUsbReq_t usbReqData;

				if (!_noWinMsg) {
					pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"USB: close", L"SHUTTING DOWN ...");
					Sleep(500L);
				}

				try {
					/* Iterate over all instruments */
					InstList_t::iterator it = g_InstList.begin();
					while (it != g_InstList.end()) {
						if (!it->actSelected || !it->actLink || !(it->linkType & LINKTYPE_USB) ||
							((it->listFunction != INST_FUNC_GEN) && (it->listFunction != INST_FUNC_SPEC))) {
							/* Try next */
							it++;
							continue;
						}

						Instrument_t thisInst = *it;

						/* Stop messaging to the devices */
						usbReqData.cmd = C_USBREQ_DISCONNECT;
						usbReqData.thisInst = thisInst;
						send(*pAgtUsbTmcReq, usbReqData);
						AgentUsbRsp_t usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
						*it = usbRspData.thisInst;

						/* Move to next instrument */
						it++;
					}

					/* Send shutdown request to the USB_TMC module */
					usbReqData.cmd = C_USBREQ_END;
					send(*pAgtUsbTmcReq, usbReqData);
					(void) receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
				}
				catch (const Concurrency::operation_timed_out& e) {
					(void)e;
				}

				_runState = C_MODELPATTERN_RUNSTATES_COM_CLOSE_SEND;
			}
			break;
			
			case C_MODELPATTERN_RUNSTATES_COM_CLOSE_SEND:
			{
				AgentComReq_t comReqData;
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
					AgentUsbRsp_t usbRsp;
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
						AgentComRsp_t comRsp;

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

				/* Enable Connect menu item */
				HMENU hMenu = GetMenu(g_hWnd);
				EnableMenuItem(hMenu, ID_INSTRUMENTEN_CONNECT, MF_BYCOMMAND);

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

	ReleaseMutex(g_InstListMutex);

	_done = true;
	agent::done();
}


void agentModelPattern::checkInstruments(void)
{
	InstList_t::iterator it = g_InstList.begin();

	if (!_noWinMsg) {
		pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Checking list of instruments", L"CONNECTING ...");
	}

	/* Iterate over all instruments and check which one responds */
	while (it != g_InstList.end()) {
		const LinkType_BM_t linkType = it->linkType;

		/* Try USB connection */
		if (linkType & LINKTYPE_USB) {
			instTryUsb(it);
		}

		/* Try Ethernet connection */
		if (linkType & LINKTYPE_ETH) {
			instTryEth(it);
		}

		/* Try COM connection, even when IEC is interfaced to a COM port */
		if (linkType & LINKTYPE_COM) {
			instTryCom(it);
		}

		if (!linkType) {
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


bool agentModelPattern::instTryEth(InstList_t::iterator it)
{

	return false;
}

bool agentModelPattern::instTryUsb(InstList_t::iterator it)
{
	bool isConnected = false;
	AgentComReqUsbDev_t usbDev;
	usbDev.usbIdVendor  = it->linkUsb_dev_usb_idVendor;
	usbDev.usbIdProduct = it->linkUsb_dev_usb_idProduct;

	AgentUsbReq_t usbReqData;
	usbReqData.cmd = C_USBREQ_IS_DEV_CONNECTED;
	usbReqData.data1 = &usbDev;
	send(*pAgtUsbTmcReq, usbReqData);

	AgentUsbRsp_t usbRspData = receive(*pAgtUsbTmcRsp, AGENT_PATTERN_USBTMC_TIMEOUT);
	if (usbRspData.stat == C_USBRSP_IS_DEV_CONNECTED) {
		isConnected = *((bool*) usbRspData.data1);
		if (!isConnected) {
			/* Remove USB link bit */
			it->linkType &= ~(LINKTYPE_USB);
		}
		else {
			it->actIfcType = ACT_IFC_USB;
			it->actLink = true;
		}
	}
	return isConnected;
}

bool agentModelPattern::instTryCom(InstList_t::iterator it)
{
	/* Open COM connection */
	AgentComReq_t comReqData;
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

		AgentComRsp_t comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
		if (comRspData.stat == C_COMRSP_DATA) {
			string rspIdnStr = string(comRspData.data1);
			/* Rotor IDN string */
			if (!rspIdnStr.empty()) {
				isConnected = true;
			}
		}

		if (isConnected) {
			/* If not already connected by another protocol, use COM */
			if (it->actIfcType == ACT_IFC_NONE) {
				/* Device linked */
				it->actIfcType = ACT_IFC_COM;
				it->actLink = true;
			}
			
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
				(it->linkSerIecAddr ?  it->linkSerIecAddr : C_SET_IEC_ADDR_INVALID));
			buf[len] = 0;

			comReqData.cmd = C_COMREQ_OPEN_IDN;
			comReqData.parm = string(buf);
			send(*(pAgtComReq[agtComIdx]), comReqData);

			AgentComRsp_t comRspData = receive(*(pAgtComRsp[agtComIdx]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
			if (comRspData.stat == C_COMRSP_DATA) {
				string rspIdnStr = string(comRspData.data1);
				/* IDN string */
				if (!rspIdnStr.empty()) {
					isConnected = true;
				}
			}

			if (isConnected) {
				/* If not already connected by another protocol, use COM */
				if (it->actIfcType == ACT_IFC_NONE) {
					/* Device linked */
					it->actIfcType = ACT_IFC_COM;
					it->actLink = true;
				}

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
		InstList_t::iterator it = g_InstList.begin();

		/* Iterate over all instruments and check which one responds */
		while (it != g_InstList.end()) {
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
		it = g_InstList.begin();

		/* Iterate over all instruments and check which one responds */
		while (it != g_InstList.end()) {
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

			/* Disable Connect menu item */
			EnableMenuItem(hMenu, ID_INSTRUMENTEN_CONNECT,		MF_BYCOMMAND | MF_DISABLED);
			EnableMenuItem(hMenu, ID_INSTRUMENTEN_DISCONNECT,	MF_BYCOMMAND);
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
							swprintf(l_status2, l_status_size, L"COM: 1 ~ rotor turns to  %+03d°  position", (*pi) / 1000);
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

		case ID_MODEL_PATTERN_REF_START:
			// Start recording of pattern
			runProcess(C_MODELPATTERN_PROCESS_RECORD_PATTERN_000DEG, 0);
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
		/* Different jobs to be processed */
		switch (m->o->processing_ID)
		{

		case C_MODELPATTERN_PROCESS_GOTO_X:
		{  /* Go to new direction */
			int gotoMilliDegPos = m->o->processing_arg1;
			m->o->processing_arg1 = 0;

			long posTicksNow = m->o->getLastTickPos();
			long posTicksNext = (long)(gotoMilliDegPos * (AGENT_PATTERN_ROT_TICKS_PER_DEGREE / 1000.0));
			if (posTicksNow != posTicksNext) {
				m->o->sendPos(posTicksNext);
				Sleep(calcTicks2Ms(posTicksNext - posTicksNow));
			}

			m->o->pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Ready for new jobs", L"READY");
			m->o->processing_ID = C_MODELPATTERN_PROCESS_NOOP;
		}
		break;

		case C_MODELPATTERN_PROCESS_RECORD_PATTERN_000DEG:
		{  /* Do a reference meassurement at current rotor position */
			m->o->pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Measure: Reference", L"RUN");

			int ret = m->o->runningProcessPattern(
				MEASDATA_SETUP__REFMEAS_GEN_SPEC,
				AGENT_PATTERN_000_POS_DEGREE_START,
				AGENT_PATTERN_000_POS_DEGREE_END,
				AGENT_PATTERN_000_POS_DEGREE_STEP
			);
			if (ret == -1) {
				break; // process STOPPED
			}

			m->o->processing_ID = C_MODELPATTERN_PROCESS_NOOP;
		}
		break;

		case C_MODELPATTERN_PROCESS_RECORD_PATTERN_180DEG:
		{  /* Run a 180° antenna pattern from left = -90° to the right = +90°, 5° steps */
			m->o->pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Measure: Running 180° Pattern", L"Goto START position");

			int ret = m->o->runningProcessPattern(
				MEASDATA_SETUP__ROT180_DEG5_GEN_SPEC,
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
		{  /* Run a 360° antenna pattern from left = -180° to the right = +180°, 5° steps */
			m->o->pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Measure: Running 360° Pattern", L"Goto START position");

			int ret = m->o->runningProcessPattern(
				MEASDATA_SETUP__ROT360_DEG5_GEN_SPEC,
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
			m->o->pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Stopping current measurement", L"STOPPED");
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

void agentModelPattern::setSimuMode(AGENT_ALL_SIMUMODE_t mode)
{
	simuMode = mode;
}

AGENT_ALL_SIMUMODE_t agentModelPattern::getSimuMode(void)
{
	return simuMode;
}

void agentModelPattern::getMeasData(MeasData** md)
{
	if (md) {
		/* Set pointer to the Data list */
		*md = &measDataEntries;
	}
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

		/* Make sure out-of-order execution is not relevant */
		Sleep(1);

		processing_ID = processID;
		return;
	}

	/* Cue in process ID to be done */
	if (processing_ID == C_MODELPATTERN_PROCESS_NOOP) {
		processing_arg1 = arg;

		/* Make sure out-of-order execution is not relevant */
		Sleep(1);

		processing_ID = processID;
	}
}

void agentModelPattern::setStatusPosition(double posDeg)
{
	/* Inform about the current state */
	const int l_status_size = 256;
	HLOCAL l_status3_alloc = LocalAlloc(LHND, sizeof(wchar_t) * l_status_size);
	PWCHAR l_status3 = (PWCHAR)LocalLock(l_status3_alloc);

	if (!_noWinMsg) {
		swprintf(l_status3, l_status_size, L"position: %+03.0lf°", posDeg);
		pAgtMod->getWinSrv()->reportStatus(NULL, NULL, l_status3);
	}

	LocalUnlock(l_status3_alloc);
	LocalFree(l_status3_alloc);
}



/* Data collector section */

MeasData agentModelPattern::measDataInit(MEASDATA_SETUP_ENUM measVar, std::list<double> initList)
{
	MeasData md = { measVar, 0.0, 1e-12, 0.0, 1e-12, 0.0, 0, nullptr, nullptr, nullptr };

	switch (measVar)
	{
	case MEASDATA_SETUP__REFMEAS_GEN_SPEC:
		{
			std::list<double>::iterator it = initList.begin();
			md.txQrg			= *(it++);
			md.txPwr			= *(it++);
			md.rxSpan			= *it;

			md.rxRefPwr			= 1e-12;
		}
		break;

	case MEASDATA_SETUP__ROT180_DEG5_GEN_SPEC:
	case MEASDATA_SETUP__ROT360_DEG5_GEN_SPEC:
		{
			std::list<double>::iterator it = initList.begin();
			md.txQrg		= *(it++);
			md.txPwr		= *(it++);
			md.rxSpan		= *(it++);
			md.posStep		= *it;

			md.entriesCount	= 0;
			md.posDeg		= new list<double>;
			md.rxPwrMag		= new list<double>;
			md.rxPwrPhase	= new list<double>;
		}
			break;
	}

	return md;
}

void agentModelPattern::measDataAdd(MeasData* md, std::list<double> dataList)
{
	switch (md->measVar)
	{
	case MEASDATA_SETUP__REFMEAS_GEN_SPEC:
		{
			std::list<double>::iterator it = dataList.begin();
			
			/* Skip rotor postion */
			it++;

			/* Reference Power */
			md->rxRefPwr = *(it++);
		}
		break;

	case MEASDATA_SETUP__ROT180_DEG5_GEN_SPEC:
	case MEASDATA_SETUP__ROT360_DEG5_GEN_SPEC:
		{
			std::list<double>::iterator it = dataList.begin();
			const double pos = *(it++);
			md->posDeg->push_back(pos);

			const double power = *(it++);
			md->rxPwrMag->push_back(power);

			const double phase = *it;
			md->rxPwrPhase->push_back(phase);

			/* Reference Power when directing straight ahead */
			if (-0.1 <= pos && pos <= +0.1) {
				md->rxRefPwr = power;
			}

			md->entriesCount++;
		}
			break;
	}
}

void agentModelPattern::measDataFinalize(MeasData* md, MeasData* glob)
{
	/* First free data */
	SafeDelete(&glob->posDeg);
	SafeDelete(&glob->rxPwrMag);
	SafeDelete(&glob->rxPwrPhase);

	/* There exists just a single entry, deep copy over */
	*glob = *md;

	/* Enable Menu Item Save File */
	{
		/* Enable Connect menu item */
		HMENU hMenu = GetMenu(g_hWnd);
		EnableMenuItem(hMenu, ID_FILE_SAVE, MF_BYCOMMAND);
		EnableMenuItem(hMenu, ID_DATEI_SPEICHERNALS, MF_BYCOMMAND);
		
		/* Inform in Status Line */
		if (!_noWinMsg) {
			pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Your data file is ready to save.", L"--- READY to SAVE ---");
		}
	}
}



/* agentModelPattern - Rotor */

long agentModelPattern::requestPos(void)
{
	AgentComReq_t comReqData;
	AgentComRsp_t comRspData;
	long posDeg = 0;

	try {
		/* Do sync COM */
		comReqData.cmd = C_COMREQ_COM_SEND;
		comReqData.parm = string("\r");
		for (int tries = 10; tries; tries--) {
			send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);	// drop queue
			if (!strncmp(comRspData.data1.c_str(), "OK", 2)) {
				break;
			}
			Sleep(10L);
		}

		/* Try to receive the position */
		for (int tries = 2; tries; tries--) {
			comReqData.parm = string("?X\r");
			send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
			comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);

			if (!strncmp(comRspData.data1.c_str(), "?X", 2)) {
				break;
			}
			Sleep(10L);
		}

 		if (comRspData.stat == C_COMRSP_DATA) {
			int posDiff = 0;

			if (!agentModel::parseStr2Long(&posDeg, comRspData.data1.c_str(), "?X,%ld", '?')) {
				agentModel::setLastTickPos(posDeg);
			}
		}
	}
	catch (const Concurrency::operation_timed_out& e) {
		(void)e;
	}
	return posDeg;
}

void agentModelPattern::sendPos(long ticksPos)
{
	AgentComReq_t comReqData;
	AgentComRsp_t comRspData;
	long posDiff = ticksPos - getLastTickPos();
	char cbuf[C_BUF_SIZE];

	try {
		/* Send rotation command */
		_snprintf_s(cbuf, C_BUF_SIZE - 1, "%cX,%ld\r", (posDiff > 0 ? '+' : '-'), abs(posDiff));
		comReqData.cmd = C_COMREQ_COM_SEND;
		comReqData.parm = string(cbuf);
		send(*(pAgtComReq[C_COMINST_ROT]), comReqData);

		/* Update current position value */
		setLastTickPos(ticksPos);

		comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
	}
	catch (const Concurrency::operation_timed_out& e) {
		(void)e;
	}
}

void agentModelPattern::setLastTickPos(long posDeg)
{
	lastTickPos = posDeg;
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

		try {
			switch (pConInstruments[C_CONNECTED_TX]->actIfcType) {

			case ACT_IFC_USB:
			{
				char wrkBuf[256] = { 0 };
				_snprintf_s(wrkBuf, sizeof(wrkBuf), ":OUTP %s", (txOn ? "ON" : "OFF"));  // SMC100A

				AgentUsbReq_t usbReqData;
				usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
				usbReqData.thisInst = *pConInstruments[C_CONNECTED_TX];
				usbReqData.data1 = wrkBuf;

				send(*pAgtUsbTmcReq, usbReqData);
				AgentUsbRsp_t usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

				/* Copy back */
				*pConInstruments[C_CONNECTED_TX] = usbRspData.thisInst;
			}
			break;

			case ACT_IFC_ETH:
			{

			}
			break;

			case ACT_IFC_COM:
			{
				AgentComReq_t comReqData;
				comReqData.cmd = C_COMREQ_COM_SEND;
				comReqData.parm = checked ?  string(":OUTP ON\r\n") : string(":OUTP OFF\r\n");
				send(*(pAgtComReq[C_COMINST_TX]), comReqData);

				AgentComRsp_t comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			break;

			};
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
	if ((txFrequency != value) && ((10e6 < value) && (value <= 100e9))) {
		txFrequency = value;

		try {
			switch (pConInstruments[C_CONNECTED_TX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				/* Send frequency in Hz */
				_snprintf_s(wrkBuf, sizeof(wrkBuf), ":FREQ %fHz", txFrequency);  // SMC100A
				usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
				usbReqData.thisInst = *pConInstruments[C_CONNECTED_TX];
				usbReqData.data1 = wrkBuf;

				send(*pAgtUsbTmcReq, usbReqData);
				AgentUsbRsp_t usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

				/* Copy back */
				*pConInstruments[C_CONNECTED_TX] = usbRspData.thisInst;
			}
			break;

			case ACT_IFC_ETH:
			{

			}
			break;

			case ACT_IFC_COM:
			{
				AgentComReq_t comReqData;
				AgentComRsp_t comRspData;

				/* Send frequency in Hz */
				comReqData.cmd = C_COMREQ_COM_SEND;
				comReqData.parm = string(":SOUR:FREQ ");
				comReqData.parm.append(agentCom::double2String(txFrequency));
				comReqData.parm.append("\r\n");
				send(*(pAgtComReq[C_COMINST_TX]), comReqData);
				comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			break;

			};

			/* Update current values for the frequency */
			pConInstruments[C_CONNECTED_TX]->txCurRfQrg = txFrequency;
			pConInstruments[C_CONNECTED_RX]->rxCurRfQrg = txFrequency;    // Transmitter change does change (center) frequency of the SPEC, also! 
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
	if (txPower != value && -40.0 <= value && value <= 20.0) {
		txPower = value;

		try {
			switch (pConInstruments[C_CONNECTED_TX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				/* Send TX power in dBm */
				_snprintf_s(wrkBuf, sizeof(wrkBuf), ":POW %f", txPower);  // SMC100A
				usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
				usbReqData.thisInst = *pConInstruments[C_CONNECTED_TX];
				usbReqData.data1 = wrkBuf;

				send(*pAgtUsbTmcReq, usbReqData);
				AgentUsbRsp_t usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

				/* Copy back */
				*pConInstruments[C_CONNECTED_TX] = usbRspData.thisInst;
			}
			break;

			case ACT_IFC_ETH:
			{

			}
			break;

			case ACT_IFC_COM:
			{
				AgentComReq_t comReqData;
				AgentComRsp_t comRspData;

				comReqData.cmd = C_COMREQ_COM_SEND;
				comReqData.parm = string("SOUR:POW ");
				comReqData.parm.append(agentCom::double2String(txPower));
				comReqData.parm.append("dBm\r\n");
				send(*(pAgtComReq[C_COMINST_TX]), comReqData);
				comRspData = receive(*(pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			break;

			};
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
	if (pAgtComRsp[C_COMINST_RX] && (rxFrequency != value) && ((10e6 < value) && (value <= 100e9))) {
		rxFrequency = value;

		try {
			switch (pConInstruments[C_CONNECTED_RX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				/* Send center frequency in Hz */
				_snprintf_s(wrkBuf, sizeof(wrkBuf), ":FREQ:CENT %fHz", rxFrequency);  // DSA875
				usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
				usbReqData.thisInst = *pConInstruments[C_CONNECTED_RX];
				usbReqData.data1 = wrkBuf;

				send(*pAgtUsbTmcReq, usbReqData);
				AgentUsbRsp_t usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

				/* Copy back */
				*pConInstruments[C_CONNECTED_RX] = usbRspData.thisInst;
			}
			break;

			case ACT_IFC_ETH:
			{

			}
			break;

			case ACT_IFC_COM:
			{
				AgentComReq_t comReqData;
				AgentComRsp_t comRspData;

				comReqData.cmd = C_COMREQ_COM_SEND;
				comReqData.parm = string(":SENS1:FREQ:CENT ");
				comReqData.parm.append(agentCom::double2String(rxFrequency));
				comReqData.parm.append("HZ\r\n");
				send(*(pAgtComReq[C_COMINST_RX]), comReqData);
				comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			break;

			};

			/* Update current values for the frequency */
			pConInstruments[C_CONNECTED_RX]->rxCurRfQrg = rxFrequency;
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
	if (pAgtComRsp[C_COMINST_RX] && (rxSpan != value) && (value <= 100e9)) {
		rxSpan = value;

		try {
			switch (pConInstruments[C_CONNECTED_RX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				/* Send center frequency in Hz */
				_snprintf_s(wrkBuf, sizeof(wrkBuf), ":FREQ:SPAN %fHz", rxSpan);  // DSA875
				usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
				usbReqData.thisInst = *pConInstruments[C_CONNECTED_RX];
				usbReqData.data1 = wrkBuf;

				send(*pAgtUsbTmcReq, usbReqData);
				AgentUsbRsp_t usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

				/* Copy back */
				*pConInstruments[C_CONNECTED_RX] = usbRspData.thisInst;
			}
			break;

			case ACT_IFC_ETH:
			{

			}
			break;

			case ACT_IFC_COM:
			{
				AgentComReq_t comReqData;
				AgentComRsp_t comRspData;

				comReqData.cmd = C_COMREQ_COM_SEND;
				comReqData.parm = string(":SENS1:FREQ:SPAN ");
				comReqData.parm.append(agentCom::double2String(rxSpan));
				comReqData.parm.append("HZ\r\n");
				send(*(pAgtComReq[C_COMINST_RX]), comReqData);
				comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			break;

			};
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
	if (pAgtComRsp[C_COMINST_RX] && (rxSpan != value) && (-40 <= value) && (value <= 30)) {
		rxLevelMax = value;

		try {
			switch (pConInstruments[C_CONNECTED_RX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				/* Send max amplitude to adjust attenuator */
				_snprintf_s(wrkBuf, sizeof(wrkBuf), ":POW:ATT %d", (rxLevelMax > -10.0 ? (10 + (int)rxLevelMax) : 0));  // DSA875
				usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
				usbReqData.thisInst = *pConInstruments[C_CONNECTED_RX];
				usbReqData.data1 = wrkBuf;

				send(*pAgtUsbTmcReq, usbReqData);
				AgentUsbRsp_t usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

				/* Copy back */
				*pConInstruments[C_CONNECTED_RX] = usbRspData.thisInst;
			}
			break;

			case ACT_IFC_ETH:
			{

			}
			break;

			case ACT_IFC_COM:
			{
				AgentComReq_t comReqData;
				AgentComRsp_t comRspData;

				comReqData.cmd = C_COMREQ_COM_SEND;
				comReqData.parm = string(":DISP:WIND1:TRAC1:Y:RLEV ");
				comReqData.parm.append(agentCom::double2String(rxLevelMax));
				comReqData.parm.append("DBM\r\n");
				send(*(pAgtComReq[C_COMINST_RX]), comReqData);
				comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			break;

			};
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
	if (retX && retY) {
		*retX = *retY = 0.0;

		/* Following sequence does a level metering */
		try {
			switch (pConInstruments[C_CONNECTED_RX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				if (string("RIGOL_DSA875") == pConInstruments[C_CONNECTED_RX]->listEntryName) {
					/* Stop running meassurments */
					usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
					usbReqData.thisInst = *pConInstruments[C_CONNECTED_RX];
					usbReqData.data1 = ":INIT:CONT OFF";    // DSA875
					send(*pAgtUsbTmcReq, usbReqData);
					AgentUsbRsp_t usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

					/* Start one shot meassurement and wait until being ready */
					usbReqData.thisInst = usbRspData.thisInst;
					usbReqData.data1 = ":INIT:IMM; *WAI";  // DSA875
					send(*pAgtUsbTmcReq, usbReqData);
					usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

					/* Set Marker 1 as maximum marker */
					usbReqData.thisInst = usbRspData.thisInst;
					usbReqData.data1 = ":CALC:MARK1:PEAK:SEARCH:MODE MAX";  // DSA875
					send(*pAgtUsbTmcReq, usbReqData);
					usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

					/* Set Marker 1 to Maximum and wait until ready */
					usbReqData.thisInst = usbRspData.thisInst;
					usbReqData.data1 = ":CALC:MARK1:MAX:MAX";  // DSA875
					send(*pAgtUsbTmcReq, usbReqData);
					usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
					Sleep(500);

					/* Get frequency value of the marker */
					usbReqData.cmd = C_USBREQ_USBTMC_SEND_AND_RECEIVE;
					usbReqData.thisInst = usbRspData.thisInst;
					usbReqData.data1 = ":CALC:MARK1:X?";  // DSA875
					send(*pAgtUsbTmcReq, usbReqData);
					usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
					string retStr = *(string*)usbRspData.data1;
					sscanf_s(retStr.c_str(), "%lf", retX);

					/* Get amplitude value of the marker */
					usbReqData.thisInst = usbRspData.thisInst;
					usbReqData.data1 = ":CALC:MARK1:Y?";  // DSA875
					send(*pAgtUsbTmcReq, usbReqData);
					usbRspData = receive(*pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
					retStr = *(string*)usbRspData.data1;
					sscanf_s(retStr.c_str(), "%lf", retY);

					/* Copy back */
					*pConInstruments[C_CONNECTED_RX] = usbRspData.thisInst;

					return FALSE;
				}
			}
			break;

			case ACT_IFC_ETH:
			{

			}
			break;

			case ACT_IFC_COM:
			{
				if (pAgtComReq[C_COMINST_RX]) {
					AgentComReq_t comReqData;
					AgentComRsp_t comRspData;

					if (string("R&S_FSEK20") == pConInstruments[C_CONNECTED_RX]->listEntryName) {
						/* Stop running meassurments */
						comReqData.cmd = C_COMREQ_COM_SEND;
						comReqData.parm = string(":INIT:CONT OFF\r\n");
						send(*(pAgtComReq[C_COMINST_RX]), comReqData);
						comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);

						/* Start one shot meassurement and wait until being ready */
						comReqData.parm = string(":INIT:IMM; *WAI\r\n");
						send(*(pAgtComReq[C_COMINST_RX]), comReqData);
						comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);

						/* Set marker to maximum and wait until ready */
						comReqData.parm = string(":CALC:MARKER ON; :CALC:MARKER:MAX; *WAI\r\n");
						send(*(pAgtComReq[C_COMINST_RX]), comReqData);
						comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);

						/* Get frequency value of the marker */
						comReqData.parm = string(":CALC:MARKER:X?\r\n");
						send(*(pAgtComReq[C_COMINST_RX]), comReqData);
						comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
						sscanf_s(comRspData.data1.c_str(), "%lf", retX);

						/* Get amplitude value of the marker */
						comReqData.parm = string(":CALC:MARKER:Y?\r\n");
						send(*(pAgtComReq[C_COMINST_RX]), comReqData);
						comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
						sscanf_s(comRspData.data1.c_str(), "%lf", retY);

						return FALSE;
					}
				}
			}
			break;

			};
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
		}
	}
	return TRUE;
}


/* TOOLS */

int agentModelPattern::runningProcessPattern(MEASDATA_SETUP_ENUM measVariant, double degStartPos, double degEndPos, double degResolution)
{  // run a antenna pattern from left = degStartPos to the right = degEndPos
	// TODO: max 3 seconds allowed - use own thread for running process
	/* Set-up ROTOR */
	long ticksNow = requestPos();

	/* Go to Start Position only on rotary meassurements */
	long ticksNext = calcDeg2Ticks(degStartPos);
	if (measVariant >= MEASDATA_SETUP__ROT180_DEG5_GEN_SPEC &&
		measVariant <= MEASDATA_SETUP__ROT360_DEG5_GEN_SPEC) {
		/* Send Home position and wait */
		sendPos(ticksNext);
		Sleep(calcTicks2Ms(ticksNext - ticksNow));
	}

	if (processing_ID <= C_MODELPATTERN_PROCESS_STOP) {
		return -1;
	}

	/* Inform about the start position */
	setStatusPosition(degStartPos);

	/* Set-up TX */
	{
		/* Send frequency in Hz */
		setTxFrequencyValue(pConInstruments[C_CONNECTED_TX]->txCurRfQrg);

		/* Send power in dBm */
		setTxPwrValue(pConInstruments[C_CONNECTED_TX]->txCurRfPwr);

		/* Send power ON */
		pConInstruments[C_CONNECTED_TX]->txCurRfOn = true;
		setTxOnState(pConInstruments[C_CONNECTED_TX]->txCurRfOn);
	}

	/* Set-up RX */
	{
		/* Send Span in Hz */
		setRxSpanValue(pConInstruments[C_CONNECTED_RX]->rxCurRfSpan);

		/* Send center frequency in Hz */
		setRxFrequencyValue(pConInstruments[C_CONNECTED_RX]->rxCurRfQrg);

		/* Send max amplitude to adjust attenuator */
		setRxLevelMaxValue(pConInstruments[C_CONNECTED_RX]->rxCurRfPwrHi);
	}

	/* Set-up the Data container for this meassurement */
	MeasData md;
	{
		std::list<double> init;
		init.push_back(pConInstruments[C_CONNECTED_TX]->txCurRfQrg);
		init.push_back(pConInstruments[C_CONNECTED_TX]->txCurRfPwr);
		init.push_back(pConInstruments[C_CONNECTED_RX]->rxCurRfSpan);

		if (measVariant >= MEASDATA_SETUP__ROT180_DEG5_GEN_SPEC && 
			measVariant <= MEASDATA_SETUP__ROT360_DEG5_GEN_SPEC) {
			init.push_back(degResolution);
		}
		md = measDataInit(measVariant, init);
	}

	/* Iteration of rotor steps */
	double degPosIter = degStartPos;
	while (true) {
		/* Record data */
		{
			wchar_t strbuf[256];

			/* Resting RX: meassure */
			double testX = 0.0;
			double testY = 0.0;
			getRxMarkerPeak(&testX, &testY);

			swprintf(strbuf, sizeof(strbuf)>>1, L"> Pos = %03d°: f = %E Hz, \tP = %E dBm.\n", (int)degPosIter, testX, testY);
			OutputDebugString(strbuf);

			/* Store current data */
			std::list<double> data;
			data.push_back(degPosIter); // Position
			data.push_back(testY);		// Power
			data.push_back(0.0);		// Phase
			measDataAdd(&md, data);
		}

		/* advance to new position */
		degPosIter += degResolution;
		if (degPosIter > degEndPos) {
			break;
		}

		ticksNow  = ticksNext;
		ticksNext = calcDeg2Ticks(degPosIter);
		sendPos(ticksNext);

		/* Inform about the current step position */
		setStatusPosition(degPosIter);

		Sleep(calcTicks2Ms(ticksNext - ticksNow));
		if (processing_ID <= C_MODELPATTERN_PROCESS_STOP) {
			/* Update current position value */
			setLastTickPos(ticksNext);

			goto ErrorOut_runningProcessPattern;
		}
	}

	/* Send power OFF */
	{
		pConInstruments[C_CONNECTED_TX]->txCurRfOn = false;
		setTxOnState(pConInstruments[C_CONNECTED_TX]->txCurRfOn);
	}

	if (measVariant == MEASDATA_SETUP__REFMEAS_GEN_SPEC) {
		/* Return ROTOR to center position */
		if (!_noWinMsg) {
			pAgtMod->getWinSrv()->reportStatus(NULL, NULL, L"REF done");
		}
	}

	else
	if (measVariant >= MEASDATA_SETUP__ROT180_DEG5_GEN_SPEC &&
		measVariant <= MEASDATA_SETUP__ROT360_DEG5_GEN_SPEC) {
		/* Return ROTOR to center position */
		if (!_noWinMsg) {
			pAgtMod->getWinSrv()->reportStatus(NULL, NULL, L"Goto HOME position");
		}
		sendPos(0);
		Sleep(calcDeg2Ms(degEndPos));
	}

	processing_ID = C_MODELPATTERN_PROCESS_NOOP;

	/* Move data to the global entries */
	measDataFinalize(&md, &measDataEntries);

	return 0;


ErrorOut_runningProcessPattern:
	/* Send power OFF */
	{
		pConInstruments[C_CONNECTED_TX]->txCurRfOn = false;
		setTxOnState(pConInstruments[C_CONNECTED_TX]->txCurRfOn);
	}

	/* Move data to the global entries */
	measDataFinalize(&md, &measDataEntries);

	return -1;
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
