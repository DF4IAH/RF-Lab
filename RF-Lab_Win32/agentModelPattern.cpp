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
				 : _pAgtComReq{ nullptr }
				 , _pAgtComRsp{ nullptr }
				 , _pAgtCom{ nullptr }
				 , _arg(nullptr)

				, _winSrv(winSrv)

				 , _pAgtMod(am)

				 , _hThreadAgtUsbTmc(nullptr)
				 , _sThreadDataAgentModelPattern( { 0 } )

				 , _pAgtUsbTmc(nullptr)

				 , _measDataEntries()

				 , _processing_ID(C_MODELPATTERN_PROCESS_NOOP)
				 , _processing_arg1(0.0)
				 , _processing_arg2(0.0)
				 , _processing_arg3(0.0)
				 , _simuMode(mode)

				 , _initState(0)
				 , _guiPressedConnect(FALSE)

				 , _curPosTicksAbs(0)
{
	/* Fast access to connected instruments */
	_pConInstruments[C_COMINST_ROT] = g_InstList.end();
	_pConInstruments[C_COMINST_TX]  = g_InstList.end();
	_pConInstruments[C_COMINST_RX]  = g_InstList.end();

	/* Make sure pointer vars are being set-up */
	_measDataEntries.posDeg	   = nullptr;
	_measDataEntries.rxPwrMag   = nullptr;
	_measDataEntries.rxPwrPhase = nullptr;
}

agentModelPattern::~agentModelPattern()
{
	/* Free dynamic allocated lists */
	SafeDelete(&_measDataEntries.posDeg);
	SafeDelete(&_measDataEntries.rxPwrMag);
	SafeDelete(&_measDataEntries.rxPwrPhase);
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
	_pAgtUsbTmcReq = new unbounded_buffer<AgentUsbReq_t>;
	_pAgtUsbTmcRsp = new unbounded_buffer<AgentUsbRsp_t>;

	for (int i = 0; i < C_COMINST__COUNT; ++i) {
		_pAgtComReq[i] = new unbounded_buffer<AgentComReq_t>;
		_pAgtComRsp[i] = new unbounded_buffer<AgentComRsp_t>;
		_pAgtCom[i] = nullptr;

		if (_pAgtComReq[i] && _pAgtComRsp[i]) {
			switch (i) {
			case C_COMINST_ROT:
				_pAgtCom[i] = new agentCom(*(_pAgtComReq[i]), *(_pAgtComRsp[i]));
				break;

			case C_COMINST_TX:
				if (!(_simuMode & AGENT_ALL_SIMUMODE_NO_TX)) {
					_pAgtCom[i] = new agentCom(*(_pAgtComReq[i]), *(_pAgtComRsp[i]));
				}
				break;

			case C_COMINST_RX:
				if (!(_simuMode & AGENT_ALL_SIMUMODE_NO_RX)) {
					_pAgtCom[i] = new agentCom(*(_pAgtComReq[i]), *(_pAgtComRsp[i]));
				}
				break;
			}
		}
	}
}

void agentModelPattern::agentsShutdown(void)
{
	/* Signal the USB_TMC object to shutdown */
	_pAgtUsbTmc->shutdown();

	/* Delay loop as long it needs to stop this run() method */
	while (!_pAgtUsbTmc->isDone()) {
		Sleep(25);
	}

	/* Release USB-TMC connection */
	agent::wait(_pAgtUsbTmc, AGENT_PATTERN_RECEIVE_TIMEOUT);
	SafeReleaseDelete(&_pAgtUsbTmc);
	SafeDelete(&_pAgtUsbTmcReq);
	SafeDelete(&_pAgtUsbTmcRsp);

	/* Release serial connection objects */
	for (int i = 0; i < C_COMINST__COUNT; i++) {
		if (_pAgtCom[i]) {
			if (_pAgtCom[i]->isDone()) {
				agent::wait(_pAgtCom[i], AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			SafeReleaseDelete(&(_pAgtCom[i]));
		}
		SafeDelete(&(_pAgtComReq[i]));
		SafeDelete(&(_pAgtComRsp[i]));
	}
}


void agentModelPattern::threadsStart(void)
{
	_sThreadDataAgentModelPattern.threadNo = 1;
	_sThreadDataAgentModelPattern.o = this;
	_processing_ID = C_MODELPATTERN_PROCESS_NOOP;
	_beginthread(&procThreadProcessID, 0, &_sThreadDataAgentModelPattern);

	_hThreadAgtUsbTmc = CreateMutex(NULL, FALSE, NULL);		// Mutex is still free
	_pAgtUsbTmc = new USB_TMC(_pAgtUsbTmcReq, _pAgtUsbTmcRsp);
	_pAgtUsbTmc->start();
}

void agentModelPattern::threadsStop(void)
{
	/* End thread */
	_processing_ID = C_MODELPATTERN_PROCESS_END;
	if (!_noWinMsg) {
		_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Shutting down ...", L"");
	}
	WaitForSingleObject(_hThreadAgtUsbTmc, 10000L);			// Wait until the thread has ended (timeout = 10 secs
	CloseHandle(_hThreadAgtUsbTmc);
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
			_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Internal error - can not get access to InstList", L"???");
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
//#define TEST_NEW_CODE_JUMP
#ifdef TEST_NEW_CODE_JUMP
				const int rotSpan = 180;
				const int rotStep =   5;
				const int pwrRef  = -10;
				WinSrv::evalTmpTemplateFile(rotSpan, rotStep, pwrRef);
#endif

				/* In case of a REINIT, clear this flag */
				_runReinit = false;

				if (!_noWinMsg) {
					_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Starting COM threads", L"STARTING ...");
				}

				/* Set up list of instruments */
				_pAgtMod->setupInstrumentList();

				/* Start the communication agents */
				agentsInit();

				/* Start thread for process IDs to operate */
				threadsStart();

				if (!_noWinMsg) {
					_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Starting COM threads", L"... DONE");
				}


				/* Request libusb to init */
				{
					AgentUsbReq_t usbReqData;

					usbReqData.cmd = C_USBREQ_DO_REGISTRATION;
					send(*_pAgtUsbTmcReq, usbReqData);

					AgentUsbRsp_t usbRspData = receive(*_pAgtUsbTmcRsp, AGENT_PATTERN_USBTMC_TIMEOUT);
					if (usbRspData.stat == C_USBRSP_REGISTRATION_DONE) {
						/* USB ready */

						if (!_noWinMsg) {
							_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"USB bus registration", L"... DONE");
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
					_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Please select instruments from the menu", L"--- SELECT NOW ---");
				}

//#define DEBUGGING_FILE_SAVE
//#define DEBUGGING_ACTIONS
#ifdef DEBUGGING_FILE_SAVE
				{
					/* Enable Connect menu item */
					HMENU hMenu = GetMenu(g_hWnd);
					EnableMenuItem(hMenu, ID_FILE_SAVE, MF_BYCOMMAND);
					EnableMenuItem(hMenu, ID_DATEI_SPEICHERNALS, MF_BYCOMMAND);
				}

# ifdef DEBUGGING_ACTIONS
				{
					/* Enable the menu items */
					pAgtMod->getWinSrv()->instActivateMenuItem(ID_AKTOR_ITEM0_, true);
					pAgtMod->getWinSrv()->instActivateMenuItem(ID_GENERATOR_ITEM0_, true);
					pAgtMod->getWinSrv()->instActivateMenuItem(ID_SPEK_ITEM0_, true);
				}
# endif
#endif
			}
			break;

			case C_MODELPATTERN_RUNSTATES_WAIT_FOR_GUI:
			{
				/* Wait for user decissions at the GUI */
				if (_guiPressedConnect) {
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
					_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Connect USB", L"RF-Gen & Spec ...");
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
						send(*_pAgtUsbTmcReq, usbReqData);

						/* Get open state */
						AgentUsbRsp_t usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_USBTMC_TIMEOUT*/);
						*it = usbRspData.thisInst;

						/* Check for result */
						if (usbRspData.stat == C_USBRSP_OK) {

							/* Success connection USB device */
							switch (it->listFunction) {
							case INST_FUNC_GEN:
								/* Fast access table */
								_pConInstruments[C_COMINST_TX] = it;
								break;

							case INST_FUNC_SPEC:
								/* Fast access table */
								_pConInstruments[C_COMINST_RX] = it;
								break;
							}
						}
						else if (usbRspData.stat == C_USBRSP_ERR) {
							char errMsg[256] = { 0 };
							sprintf_s(errMsg, sizeof(errMsg), "Error when connecting USB device %s\n\nLibUSB denies connection.\nlibUSB error: %s\n", it->listEntryName.c_str(), (const char*)usbRspData.data1);
							MessageBoxA(_pAgtMod->getWnd(), errMsg, "LibUSB connection error", MB_ICONERROR);
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
					if (_pAgtCom[C_COMINST_ROT]) {
						_pAgtCom[C_COMINST_ROT]->start();
					}

					if (_pAgtCom[C_COMINST_TX]) {
						_pAgtCom[C_COMINST_TX]->start();
					}

					if (_pAgtCom[C_COMINST_RX]) {
						_pAgtCom[C_COMINST_RX]->start();
					}

					/* Give some time for the threads to init */
					Sleep(25);
				}

				_initState = 0x01;

				/* Open Rotor */
				if (_pAgtCom[C_COMINST_ROT]) {
					if (!_noWinMsg) {
						_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Connect COMs", L"ROTOR ...");
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
							send(*(_pAgtComReq[C_COMINST_ROT]), comReqData);

							AgentComRsp_t comRspData = receive(*(_pAgtComRsp[C_COMINST_ROT]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
							if (comRspData.stat == C_COMRSP_DATA) {
								string rspIdnStr = string(comRspData.data1);
								if (rspIdnStr.empty()) {
									/* Stop and drop COM-Server for the rotor */
									comReqData.cmd = C_COMREQ_END;
									send(*(_pAgtComReq[C_COMINST_ROT]), comReqData);
									receive(*(_pAgtComRsp[C_COMINST_ROT]));

									if (_pAgtCom[C_COMINST_ROT]->isDone()) {
										agent::wait(_pAgtCom[C_COMINST_ROT], AGENT_PATTERN_RECEIVE_TIMEOUT);
									}
									SafeReleaseDelete(&(_pAgtCom[C_COMINST_ROT]));
								}
								else {
									/* Set instrument which is linked to this agentCom thread */
									_pAgtCom[C_COMINST_ROT]->setInstrument(&it);

									/* Fast access table */
									_pConInstruments[C_COMINST_ROT] = it;

									_initState = 0x02;
								}
							}
							break;
						}
					}
				}

				/* Open TX & RX */
				if (_pAgtCom[C_COMINST_TX] && _pAgtCom[C_COMINST_RX]) {
					if (!_noWinMsg) {
						_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Connect COMs", L"RF-Gen & Spec ...");
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
							send(*(_pAgtComReq[C_COMINST_TX]), comReqData);

							AgentComRsp_t comRspData = receive(*(_pAgtComRsp[C_COMINST_TX]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
							if (comRspData.stat == C_COMRSP_DATA) {
								string rspIdnStr = string(comRspData.data1);
								if (rspIdnStr.empty()) {
									/* Close connection */
									comReqData.cmd = C_COMREQ_CLOSE;
									send(*(_pAgtComReq[C_COMINST_TX]), comReqData);
									receive(*(_pAgtComRsp[C_COMINST_TX]));

								}
								else {
									/* Set instrument which is linked to this agentCom thread */
									_pAgtCom[C_COMINST_TX]->setInstrument(&it);
									isComTxConDone = true;

									/* Fast access table */
									_pConInstruments[C_COMINST_TX] = it;
								}
							}
						}

						/* Open Spectrum Analysator */
						if (it->listFunction == INST_FUNC_SPEC && !isComRxConDone)
						{
							/* Open port */
							comReqData.cmd = C_COMREQ_OPEN_IDN;
							comReqData.parm = string(buf);
							send(*(_pAgtComReq[C_COMINST_RX]), comReqData);

							AgentComRsp_t comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
							if (comRspData.stat == C_COMRSP_DATA) {
								string rspIdnStr = string(comRspData.data1);
								if (rspIdnStr.empty()) {
									/* Close connection */
									comReqData.cmd = C_COMREQ_CLOSE;
									send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
									receive(*(_pAgtComRsp[C_COMINST_RX]));

								}
								else {
									/* Set instrument which is linked to this agentCom thread */
									_pAgtCom[C_COMINST_RX]->setInstrument(&it);
									isComRxConDone = true;

									/* Fast access table */
									_pConInstruments[C_COMINST_RX] = it;
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
						send(*(_pAgtComReq[C_COMINST_TX]), comReqData);
						receive(*(_pAgtComRsp[C_COMINST_TX]));

						if (_pAgtCom[C_COMINST_TX]->isDone()) {
							agent::wait(_pAgtCom[C_COMINST_TX], AGENT_PATTERN_RECEIVE_TIMEOUT);
						}
						SafeReleaseDelete(&(_pAgtCom[C_COMINST_TX]));
					}

					/* Stop and drop COM-Server for the RX when not connected */
					if (!isComRxConDone) {
						comReqData.cmd = C_COMREQ_END;
						send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
						receive(*(_pAgtComRsp[C_COMINST_RX]));

						if (_pAgtCom[C_COMINST_RX]->isDone()) {
							agent::wait(_pAgtCom[C_COMINST_RX], AGENT_PATTERN_RECEIVE_TIMEOUT);
						}
						SafeReleaseDelete(&(_pAgtCom[C_COMINST_RX]));
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
					_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"USB instruments are initialized", L"USB INIT");
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

					/* Set the device with the init values */
					perfomInitValues(it);

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
					_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM instruments are initialized", L"COM INIT");
				}

				_initState = 0x40;
				_runState = C_MODELPATTERN_RUNSTATES_RUNNING;					// next runstate default setting

				/* COM: Rotor Init */
				if (_pAgtCom[C_COMINST_ROT]) {
					/* Zolix commands to init device */
					{
						_initState = 0x41;
						try {
							comReqData.cmd = C_COMREQ_COM_SEND;					// Zolix: never send a \n (LF) !!!
							comReqData.parm = string("VX,20000\r");				// Zolix: top speed 20.000 ticks per sec
							send(*(_pAgtComReq[C_COMINST_ROT]), comReqData);
							comRspData = receive(*(_pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							_initState = 0x42;

							comReqData.parm = string("AX,30000\r");				// Zolix: acceleration speed 30.000
							send(*(_pAgtComReq[C_COMINST_ROT]), comReqData);
							comRspData = receive(*(_pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							_initState = 0x43;

							comReqData.parm = string("FX,2500\r");				// Zolix: initial speed 2.500 ticks per sec
							send(*(_pAgtComReq[C_COMINST_ROT]), comReqData);
							comRspData = receive(*(_pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);

							/* Read current tick position of rotor and store value */
							(void)receivePosTicksAbs();

							if (!_noWinMsg) {
								_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: rotor init done", L"");
							}
							_initState = 0x44;
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							_initState = 0x45;
							if (!_noWinMsg) {
								_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: rotor init exception! (6)", L"ERROR!");
								Sleep(5000);
							}
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}
				}
				if (_runState == C_MODELPATTERN_RUNSTATES_INIT_ERROR) {
					break;
				}

				/* COM: TX Init */
				if (_pAgtCom[C_COMINST_TX]) {
					/* Syncing with the device */
					{
						_initState = 0x51;
						try {
							if (_pAgtCom[C_COMINST_TX]->isIec()) {
								comReqData.cmd = C_COMREQ_COM_SEND;
								comReqData.parm = string("++addr ");
								comReqData.parm.append(agentCom::int2String(_pAgtCom[C_COMINST_TX]->getIecAddr()));
								comReqData.parm.append("\r\n");
								send(*(_pAgtComReq[C_COMINST_TX]), comReqData);
								comRspData = receive(*(_pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								_initState = 0x52;

								comReqData.parm = string("++auto 1\r\n");
								send(*(_pAgtComReq[C_COMINST_TX]), comReqData);
								comRspData = receive(*(_pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								_initState = 0x53;
							}

							int cnt;
							for (cnt = 10; cnt; cnt--) {
								comReqData.cmd = C_COMREQ_COM_SEND;
								comReqData.parm = string("*IDN?\r\n");
								send(*(_pAgtComReq[C_COMINST_TX]), comReqData);

								comRspData = receive(*(_pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								if (comRspData.stat == C_COMRSP_DATA) {
									if (memchr(comRspData.data1.c_str(), L',', comRspData.data1.length())) {
										/* Success */
										break;
									}
								}
								Sleep(100);
							}

							if (!cnt) {
								/* No success */
								if (!_noWinMsg) {
									_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: TX init exception! (1)", L"ERROR!");
									Sleep(5000);
								}
								_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
							}

							if (_runState != C_MODELPATTERN_RUNSTATES_INIT_ERROR) {
								_initState = 0x54;

								comReqData.parm = string("*RST; *CLS; *WAI\r\n");
								send(*(_pAgtComReq[C_COMINST_TX]), comReqData);

								comRspData = receive(*(_pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								_initState = 0x55;
								Sleep(2500L);

								/* set TX device default parameters */
								perfomInitValues(_pConInstruments[C_CONNECTED_TX]);
								if (!_noWinMsg) {
									_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: TX default parameters are set", L"");
									Sleep(500L);
								}

								if (!_noWinMsg) {
									_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: TX init done", L"");
								}
								_initState = 0x56;
							}
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							_initState = 0x5F;
							if (!_noWinMsg) {
								_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: TX init exception! (2)", L"ERROR!");
								Sleep(5000);
							}
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}
					if (_runState == C_MODELPATTERN_RUNSTATES_INIT_ERROR) {
						break;
					}
				}

				/* COM: RX Init */
				if (_pAgtCom[C_COMINST_RX]) {
					/* Syncing with the device */
					{
						_initState = 0xA0;
						try {
							if (_pAgtCom[C_COMINST_RX]->isIec()) {
								comReqData.cmd = C_COMREQ_COM_SEND;
								comReqData.parm = string("++addr ");
								comReqData.parm.append(agentCom::int2String(_pAgtCom[C_COMINST_RX]->getIecAddr()));
								comReqData.parm.append("\r\n");
								send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
								comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								_initState = 0xA1;

								comReqData.parm = string("++auto 1\r\n");
								send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
								comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								_initState = 0xA2;
							}

							comReqData.cmd = C_COMREQ_COM_SEND;
							comReqData.parm = string("*IDN?\r\n");
							_initState = 0xA3;
							do {
								send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
								comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
								if (comRspData.stat != C_COMRSP_DATA) {
									_initState = 0xAE;
									if (!_noWinMsg) {
										_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: RX init exception! (3)", L"ERROR!");
										Sleep(5000);
									}
									_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
									break;
								}
							} while (_strnicmp(&(comRspData.data1[0]), "ROHDE", 5));
							_initState = 0xA4;

							comReqData.parm = string("*RST; *CLS; *WAI\r\n");
							send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							_initState = 0xA5;
							Sleep(2500L);
							_initState = 0xA6;

							comReqData.parm = string(":INIT:CONT ON\r\n");
							send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							_initState = 0xA7;

							/* set RX device default parameters */
							perfomInitValues(_pConInstruments[C_CONNECTED_RX]);
							if (!_noWinMsg) {
								_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: RX default parameters are set", L"");
								Sleep(500L);
							}

							if (!_noWinMsg) {
								_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: RX init done", L"");
							}
						}

						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							_initState = 0xAF;
							if (!_noWinMsg) {
								_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: RX init exception! (4)", L"ERROR!");
								Sleep(5000);
							}
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}
					if (_runState == C_MODELPATTERN_RUNSTATES_INIT_ERROR) {
						break;
					}

					// Display settings
					{
						_initState = 0xC0;
						try {
							comReqData.cmd = C_COMREQ_COM_SEND;
							comReqData.parm = string(":DISP:TRAC1:Y:SPAC LOG; :DISP:TRAC1:Y 50DB\r\n");
							send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							_initState = 0xC1;

							comReqData.parm = string(":DISP:TEXT \"RF-Lab: Ant pattern\"\r\n");
							send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							_initState = 0xC2;

							comReqData.parm = string(":DISP:TEXT:STAT ON\r\n");
							send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							_initState = 0xC3;
							if (!_noWinMsg) {
								_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: RX display parameters are set", L"");
							}
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							_initState = 0xCF;
							if (!_noWinMsg) {
								_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM ERR: RX display parameters (5)", L"WARNING!");
								Sleep(5000);
							}
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}
				}

				if (!_noWinMsg) {
					_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Ready to run meassurements", L"--- SELECT NOW ---");
				}
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
				int lastGood = _initState;

				if (!_noWinMsg) {
					_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"INIT ERROR: please connect missing device(s)", L"--- MISSING ---");
				}

				_runState = C_MODELPATTERN_RUNSTATES_RUNNING;
			}
			break;


			/* Send instrument close connections */
			case C_MODELPATTERN_RUNSTATES_USB_CLOSE_SEND:
			{
				AgentUsbReq_t usbReqData;

				if (!_noWinMsg) {
					_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"USB: close", L"SHUTTING DOWN ...");
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
						send(*_pAgtUsbTmcReq, usbReqData);
						AgentUsbRsp_t usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
						*it = usbRspData.thisInst;

						/* Move to next instrument */
						it++;
					}

					/* Send shutdown request to the USB_TMC module */
					usbReqData.cmd = C_USBREQ_END;
					send(*_pAgtUsbTmcReq, usbReqData);
					(void) receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
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
					_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: close", L"SHUTTING DOWN ...");
					Sleep(500L);
				}

				try {
					/* Send shutdown request for each active agent */
					for (int i = 0; i < C_COMINST__COUNT; i++) {
						if (_pAgtComReq[i]) {
							send(*(_pAgtComReq[i]), comReqData);
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
						usbRsp = receive(*_pAgtUsbTmcRsp, AGENT_PATTERN_USBTMC_TIMEOUT);
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
					if (_pAgtCom[i]) {
						AgentComRsp_t comRsp;

						/* consume until END response is received */
						try {
							do {
								comRsp = receive(*(_pAgtComRsp[i]), AGENT_PATTERN_RECEIVE_TIMEOUT);
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
				_guiPressedConnect = FALSE;

				/* Remove known instruments */
				for (int idx = 0; idx < C_CONNECTED__COUNT; ++idx) {
					_pConInstruments[idx] = g_InstList.end();
				}

				if (_runReinit) {
					_runState = C_MODELPATTERN_RUNSTATES_BEGIN;

					if (!_noWinMsg) {
						_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: shut down", L"REINIT ...");
					}
				}
				else {
					_runState = C_MODELPATTERN_RUNSTATES_NOOP;
					_running = false;

					if (!_noWinMsg) {
						_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: shut down", L"... CLOSED");
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
						_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: broken", L"REINIT");
						Sleep(500L);
					}

				} else {
					_runState = C_MODELPATTERN_RUNSTATES_NOOP;
					_running = false;
					if (!_noWinMsg) {
						_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: broken", L"CLOSED");
						Sleep(500L);
					}
				}
			}
		}
	}

	if (!_noWinMsg) {
		_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Shutting down - please close the application", L"BYE");
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
		_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Checking list of instruments", L"CONNECTING ...");
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

		/* Try COM connection, also when GPIB is interfaced via a COM port */
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
		_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Checking list of instruments...", L"... DONE");
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
	send(*_pAgtUsbTmcReq, usbReqData);

	AgentUsbRsp_t usbRspData = receive(*_pAgtUsbTmcRsp, AGENT_PATTERN_USBTMC_TIMEOUT);
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
		_pAgtCom[C_COMINST_ROT]->start();
		Sleep(25);

		int len = _snprintf_s(buf, C_BUF_SIZE, 
			C_OPENPARAMS_STR,
			it->linkSerPort, it->linkSerBaud, it->linkSerBits, it->linkSerParity, it->linkSerStopbits,
			it->linkSerIecAddr);
		buf[len] = 0;

		comReqData.cmd  = C_COMREQ_OPEN_ZOLIX;
		comReqData.parm = string(buf);
		send(*(_pAgtComReq[C_COMINST_ROT]), comReqData);

		AgentComRsp_t comRspData = receive(*(_pAgtComRsp[C_COMINST_ROT]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
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
				_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: rotor", L"... FOUND");
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
		send(*(_pAgtComReq[C_COMINST_ROT]), comReqData);

		comRspData = receive(*(_pAgtComRsp[C_COMINST_ROT]));
		(void)comRspData;
	}
	break;

	case INST_FUNC_GEN:
	case INST_FUNC_SPEC:
	{
		const int agtComIdx = (it->listFunction == INST_FUNC_GEN ?  C_COMINST_TX : C_COMINST_RX);

		if (_pAgtCom[agtComIdx]) {
			bool isConnected = false;

			/* Start COM servers for TX and RX */
			_pAgtCom[agtComIdx]->start();
			Sleep(25);

			int len = _snprintf_s(buf, C_BUF_SIZE,
				C_OPENPARAMS_STR,
				it->linkSerPort, it->linkSerBaud, it->linkSerBits, it->linkSerParity, it->linkSerStopbits,
				(it->linkSerIecAddr ?  it->linkSerIecAddr : C_SET_IEC_ADDR_INVALID));
			buf[len] = 0;

			comReqData.cmd = C_COMREQ_OPEN_IDN;
			comReqData.parm = string(buf);
			send(*(_pAgtComReq[agtComIdx]), comReqData);

			AgentComRsp_t comRspData = receive(*(_pAgtComRsp[agtComIdx]) /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
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
						_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: RF generator", L"... FOUND");
						Sleep(250L);
					}
				}

				else if (it->listFunction == INST_FUNC_SPEC) {
					if (!_noWinMsg) {
						_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: Spectrum analyzer", L"... FOUND");
						Sleep(250L);
					}
				}
			}

			else {
				/* Remove COM link bit(s), is linked and selection bits */
				it->linkType &= ~(LINKTYPE_IEC_VIA_SER | LINKTYPE_COM);
				it->actLink = false;
				it->actSelected = false;
				it->actIfcType = ACT_IFC_NONE;
			}

			/* Close connection again */
			comReqData.cmd = C_COMREQ_CLOSE;
			send(*(_pAgtComReq[agtComIdx]), comReqData);

			comRspData = receive(*(_pAgtComRsp[agtComIdx]));
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
			/* Reset selections */
			_winSrv->_menuInfo.rotorEnabled = false;
			_winSrv->_menuInfo.rfGenEnabled = false;
			_winSrv->_menuInfo.specEnabled	= false;

			initDevices();
			break;


			/* Rotor commands */

		case ID_ROTOR_GOTO_0:
			if (_runState == C_MODELPATTERN_RUNSTATES_RUNNING) {
				if (!_noWinMsg) {
					_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"COM: 1 ~ rotor turns to  0�  position", L"RUNNING ...");
				}
				runProcess(C_MODELPATTERN_PROCESS_GOTO_X, AGENT_PATTERN_000_POS_DEGREE_START, 0.0, 0.0);
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
							swprintf(l_status2, l_status_size, L"COM: 1 ~ rotor turns to  %+03d�  position", (*pi) / 1000);
							_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", l_status2, L"RUNNING ...");
						}

						LocalUnlock(l_status2_alloc);
						LocalFree(l_status2_alloc);
					}

					/* Start rotor */
					runProcess(C_MODELPATTERN_PROCESS_GOTO_X, *pi, 0.0, 0.0);
				}
			}
			break;

		case ID_ROTOR_STOP:
		case ID_MODEL_PATTERN_STOP:
			runProcess(C_MODELPATTERN_PROCESS_STOP, 0.0, 0.0, 0.0);
			break;

		case ID_MODEL_PATTERN_REF_START:
			// Start recording of pattern
			runProcess(C_MODELPATTERN_PROCESS_RECORD_REFERENCE, AGENT_PATTERN_000_POS_DEGREE_START, AGENT_PATTERN_000_POS_DEGREE_END, AGENT_PATTERN_STEP_001);
			break;

		case ID_MODEL_PATTERN010_STEP001_START:
			// Init display part
			// xxx();

			// Start recording of pattern
			runProcess(C_MODELPATTERN_PROCESS_RECORD_PATTERN, AGENT_PATTERN_010_POS_DEGREE_START, AGENT_PATTERN_010_POS_DEGREE_END, AGENT_PATTERN_STEP_001);
			break;

		case ID_MODEL_PATTERN010_STEP005_START:
			// Init display part
			// xxx();

			// Start recording of pattern
			runProcess(C_MODELPATTERN_PROCESS_RECORD_PATTERN, AGENT_PATTERN_010_POS_DEGREE_START, AGENT_PATTERN_010_POS_DEGREE_END, AGENT_PATTERN_STEP_005);
			break;

		case ID_MODEL_PATTERN180_STEP001_START:
			// Init display part
			// xxx();

			// Start recording of pattern
			runProcess(C_MODELPATTERN_PROCESS_RECORD_PATTERN, AGENT_PATTERN_180_POS_DEGREE_START, AGENT_PATTERN_180_POS_DEGREE_END, AGENT_PATTERN_STEP_001);
			break;

		case ID_MODEL_PATTERN180_STEP005_START:
			// Init display part
			// xxx();

			// Start recording of pattern
			runProcess(C_MODELPATTERN_PROCESS_RECORD_PATTERN, AGENT_PATTERN_180_POS_DEGREE_START, AGENT_PATTERN_180_POS_DEGREE_END, AGENT_PATTERN_STEP_005);
			break;

		case ID_MODEL_PATTERN360_STEP001_START:
			// Init display part
			// xxx();

			// Start recording of pattern
			runProcess(C_MODELPATTERN_PROCESS_RECORD_PATTERN, AGENT_PATTERN_360_POS_DEGREE_START, AGENT_PATTERN_360_POS_DEGREE_END, AGENT_PATTERN_STEP_001);
			break;

		case ID_MODEL_PATTERN360_STEP005_START:
			// Init display part
			// xxx();

			// Start recording of pattern
			runProcess(C_MODELPATTERN_PROCESS_RECORD_PATTERN, AGENT_PATTERN_360_POS_DEGREE_START, AGENT_PATTERN_360_POS_DEGREE_END, AGENT_PATTERN_STEP_005);
			break;

		}  // switch (message)


		/* RF-Generator settings */
		// nothing yet

		/* RF-Analyzer settings */
		// nothing yet
	}
}

void agentModelPattern::procThreadProcessID(void* pContext)
{
	threadDataAgentModelPattern_t* m = (threadDataAgentModelPattern_t*)pContext;
	WaitForSingleObject(m->o->_hThreadAgtUsbTmc, INFINITE);	// Thread now holds his mutex until the end of its method

	/* Loop as long no end-signalling has entered */
	bool doLoop = true;
	do {
		/* Different jobs to be processed */
		switch (m->o->_processing_ID)
		{

		case C_MODELPATTERN_PROCESS_GOTO_X:
		{  /* Go to new direction */
			const double gotoMilliDegPos = m->o->_processing_arg1;
			m->o->_processing_arg1 = 0.0;

			long _curPosTicksAbs = m->o->getCurPosTicksAbs();
			long gotoPosTicksAbs = (long)(gotoMilliDegPos * (AGENT_PATTERN_ROT_TICKS_PER_DEGREE / 1000.0));
			if (_curPosTicksAbs != gotoPosTicksAbs) {
				m->o->_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Going to position", L"WAIT");
				m->o->sendPosTicksAbs(gotoPosTicksAbs);

				Sleep(calcTicks2Ms(gotoPosTicksAbs - _curPosTicksAbs));
			}

			m->o->_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Ready for new jobs", L"READY");
			m->o->_processing_ID = C_MODELPATTERN_PROCESS_NOOP;
		}
		break;

		case C_MODELPATTERN_PROCESS_RECORD_REFERENCE:
		{  /* Do a reference meassurement at current rotor position */
			const double gotoMilliDegPos = m->o->_processing_arg1;
			m->o->_processing_arg1 = 0.0;
			
			m->o->_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Measure: Reference", L"RUN");

			int ret = m->o->runningProcessPattern(
				MEASDATA_SETUP__REFMEAS_GEN_SPEC,
				gotoMilliDegPos,
				gotoMilliDegPos,
				AGENT_PATTERN_STEP_001
			);
			if (ret == -1) {
				break; // process STOPPED
			}

			m->o->_processing_ID = C_MODELPATTERN_PROCESS_NOOP;
		}
		break;

		case C_MODELPATTERN_PROCESS_RECORD_PATTERN:
		{  /* Run a 10� antenna pattern from left = -5� to the right = +5�, 1� steps */
			wchar_t buf[MAX_PATH];
			const int rotStart	= (int)(0.01 + m->o->_processing_arg1);
			const int rotEnd	= (int)(0.01 + m->o->_processing_arg2);
			const int rotStep	= (int)(0.01 + m->o->_processing_arg3);
			m->o->_processing_arg1 = 0.0;
			m->o->_processing_arg2 = 0.0;
			m->o->_processing_arg3 = 0.0;

			wsprintf(buf, L"Measure: Running %d� Pattern with %d� steps", abs(rotEnd - rotStart), rotStep);
			m->o->_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", buf, L"Goto START position");

			int ret = m->o->runningProcessPattern(
				MEASDATA_SETUP__PATTERN_GEN_SPEC,
				rotStart,
				rotEnd,
				rotStep
			);
			if (ret == -1) {
				break; // process STOPPED
			}

			m->o->_processing_ID = C_MODELPATTERN_PROCESS_NOOP;
		}
		break;

		case C_MODELPATTERN_PROCESS_STOP:
		{  /* end "STOP at once" signalling */
			m->o->_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Stopping current measurement", L"STOPPED");
			m->o->_processing_ID = C_MODELPATTERN_PROCESS_NOOP;
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
				while (m->o->_runState != C_MODELPATTERN_RUNSTATES_WAIT_FOR_GUI) {
					Sleep(25);
				}
				m->o->_guiPressedConnect = TRUE;
				m->o->_processing_ID = C_MODELPATTERN_PROCESS_NOOP;
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

	ReleaseMutex(m->o->_hThreadAgtUsbTmc);					// Thread returns mutex to signal its end
}


/* agentModelPattern - GENERAL */

void agentModelPattern::setSimuMode(AGENT_ALL_SIMUMODE_t mode)
{
	_simuMode = mode;
}

AGENT_ALL_SIMUMODE_t agentModelPattern::getSimuMode(void)
{
	return _simuMode;
}

void agentModelPattern::getMeasData(MeasData** md)
{
	if (md) {
		/* Set pointer to the Data list */
		*md = &_measDataEntries;
	}
}


void agentModelPattern::connectDevices(void)
{
	if (_running && (_runState == C_MODELPATTERN_RUNSTATES_WAIT_FOR_GUI)) {
		/* Enable the menu items */
		_pAgtMod->getWinSrv()->instActivateMenuItem(ID_AKTOR_ITEM0_, true);
		_pAgtMod->getWinSrv()->instActivateMenuItem(ID_GENERATOR_ITEM0_, true);
		_pAgtMod->getWinSrv()->instActivateMenuItem(ID_SPEK_ITEM0_, true);

		/* Init the connections */
		runProcess(C_MODELPATTERN_PROCESS_CONNECT_DEVICES, 0.0, 0.0, 0.0);
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

void agentModelPattern::runProcess(int processID, double arg1, double arg2, double arg3)
{
	/* STOP at once processing ID  or  no process running */
	if (processID == C_MODELPATTERN_PROCESS_STOP || _processing_ID == C_MODELPATTERN_PROCESS_NOOP) {
		_processing_arg1 = arg1;
		_processing_arg2 = arg2;
		_processing_arg3 = arg3;

		/* Make sure out-of-order execution is not relevant */
		Sleep(1);

		_processing_ID = processID;
	}
}

void agentModelPattern::perfomInitValues(InstList_t::iterator it)
{
	_initState = 0x90;

	switch (it->listFunction) {

	case INST_FUNC_GEN:
	{
		/* Send frequency in Hz */
		setTxFrequencyValue(it->txCurRfQrg);
		_initState = 0x91;

		/* Send power in dBm */
		setTxPwrValue(it->txCurRfPwr);
		_initState = 0x92;

		/* Send power On/Off */
		setTxOnState(it->txCurRfOn);
		_initState = 0x93;
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
}

void agentModelPattern::setStatusPosition(double posDeg)
{
	/* Inform about the current state */
	const int l_status_size = 256;
	HLOCAL l_status2_alloc = LocalAlloc(LHND, sizeof(wchar_t) * l_status_size);
	PWCHAR l_status2 = (PWCHAR)LocalLock(l_status2_alloc);

	if (!_noWinMsg) {
		swprintf(l_status2, l_status_size, L"Current rotor azimuth:  %+04.0lf �", posDeg);
		_pAgtMod->getWinSrv()->reportStatus(NULL, l_status2, NULL);
	}

	LocalUnlock(l_status2_alloc);
	LocalFree(l_status2_alloc);
}

void agentModelPattern::setStatusRxPower_dBm(double rxPwr)
{
	/* Inform about the current state */
	const int l_status_size = 256;
	HLOCAL l_status3_alloc = LocalAlloc(LHND, sizeof(wchar_t) * l_status_size);
	PWCHAR l_status3 = (PWCHAR)LocalLock(l_status3_alloc);

	if (!_noWinMsg) {
		swprintf(l_status3, l_status_size, L"Current RX level:  %+3.2lf dBm", rxPwr);
		_pAgtMod->getWinSrv()->reportStatus(NULL, NULL, l_status3);
	}

	LocalUnlock(l_status3_alloc);
	LocalFree(l_status3_alloc);
}



/* Data collector section */

MeasData agentModelPattern::measDataInit(MEASDATA_SETUP_ENUM measVar, std::list<double> initList)
{
	/* Secure default values */
	MeasData md = {
		measVar,
		2.4e+9, -30.0, 100e+6,
		-30.0,
		0.0, 0.0, 1.0,
		0, nullptr, nullptr, nullptr
	};

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

	case MEASDATA_SETUP__PATTERN_GEN_SPEC:
	{
			std::list<double>::iterator it = initList.begin();
			md.txQrg		= *(it++);
			md.txPwr		= *(it++);
			md.rxSpan		= *(it++);
			md.posFrom		= *(it++);
			md.posTo		= *(it++);
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

	case MEASDATA_SETUP__PATTERN_GEN_SPEC:
	{
			std::list<double>::iterator it = dataList.begin();
			const double pos = *(it++);
			md->posDeg->push_back(pos);

			const double power = *(it++);
			md->rxPwrMag->push_back(power);

			const double phase = *it;
			md->rxPwrPhase->push_back(phase);

			/* Reference Power when directing straight ahead */
			if (-0.01 <= pos && pos <= +0.01) {
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
			_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Your data file is ready to save.", L"--- READY to SAVE ---");
		}
	}
}



/* agentModelPattern - Rotor */

void agentModelPattern::sendPosTicksAbs(long newPosTicksAbs)
{
	/* Calculate steps to turn */
	const long posTicksDiff = newPosTicksAbs - getCurPosTicksAbs();

	/* When turning is required send positioning command */
	if (posTicksDiff) {
		AgentComReq_t comReqData;
		char cbuf[C_BUF_SIZE];

		try {
			/* Send rotation command */
			_snprintf_s(cbuf, C_BUF_SIZE - 1, "%cX,%ld\r", (posTicksDiff > 0L ?  '+' : '-'), abs(posTicksDiff));
			comReqData.cmd = C_COMREQ_COM_SEND;
			comReqData.parm = string(cbuf);
			send(*(_pAgtComReq[C_COMINST_ROT]), comReqData);

			/* Wait for end of communication */
			AgentComRsp_t comRspData = receive(*(_pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);

			/* Update new position value */
			setCurPosTicksAbs(newPosTicksAbs);
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
		}
	}
}

long agentModelPattern::receivePosTicksAbs(void)
{
	/* Fall-back value in case the communication does not work */
	long _curPosTicksAbs = getCurPosTicksAbs();

	try {
		AgentComReq_t comReqData;
		AgentComRsp_t comRspData;

		/* Doing sync COM */
		comReqData.cmd = C_COMREQ_COM_SEND;
		comReqData.parm = string("\r");
		for (int tries = 10; tries; tries--) {
			send(*(_pAgtComReq[C_COMINST_ROT]), comReqData);
			comRspData = receive(*(_pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			if (!strncmp(comRspData.data1.c_str(), "OK", 2)) {
				break;
			}
			Sleep(100);
		}

		/* Trying to receive the position information */
		for (int tries = 2; tries; tries--) {
			comReqData.parm = string("?X\r");
			send(*(_pAgtComReq[C_COMINST_ROT]), comReqData);
			comRspData = receive(*(_pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);

			if (!strncmp(comRspData.data1.c_str(), "?X", 2)) {
				break;
			}
			Sleep(100);
		}

		/* Parse valid data */
		if (comRspData.stat == C_COMRSP_DATA) {
			if (!agentModel::parseStr2Long(&_curPosTicksAbs, comRspData.data1.c_str(), "?X,%ld", '?')) {
				agentModel::setCurPosTicksAbs(_curPosTicksAbs);
			}
		}

		/* Update with current rotor position */
		setCurPosTicksAbs(_curPosTicksAbs);
	}
	catch (const Concurrency::operation_timed_out& e) {
		/* Drop any communication timeout exception */
		(void)e;
	}

	return _curPosTicksAbs;
}

void agentModelPattern::setCurPosTicksAbs(long newPosTicksAbs)
{
	_curPosTicksAbs = newPosTicksAbs;
}

long agentModelPattern::getCurPosTicksAbs(void)
{
	return _curPosTicksAbs;
}


/* agentModelPattern - TX */

void agentModelPattern::setTxOnState(bool checked)
{
	try {
		switch (_pConInstruments[C_CONNECTED_TX]->actIfcType) {

		case ACT_IFC_USB:
		{
			char wrkBuf[256] = { 0 };
			_snprintf_s(wrkBuf, sizeof(wrkBuf), ":OUTP %s", (checked ?  "ON" : "OFF"));  // SMC100A

			AgentUsbReq_t usbReqData;
			usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
			usbReqData.thisInst = *_pConInstruments[C_CONNECTED_TX];
			usbReqData.data1 = wrkBuf;

			send(*_pAgtUsbTmcReq, usbReqData);
			AgentUsbRsp_t usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

			/* Copy back */
			*_pConInstruments[C_CONNECTED_TX] = usbRspData.thisInst;
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
			send(*(_pAgtComReq[C_COMINST_TX]), comReqData);

			AgentComRsp_t comRspData = receive(*(_pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
		}
		break;

		};

		/* Update current values for the ON state */
		_pConInstruments[C_CONNECTED_TX]->txCurRfOn = checked;
	}
	catch (const Concurrency::operation_timed_out& e) {
		(void)e;
	}
}

bool agentModelPattern::getTxOnState(void)
{
	return _pConInstruments[C_CONNECTED_TX]->txCurRfOn;
}

bool agentModelPattern::getTxOnDefault(void)
{
	return _pConInstruments[C_CONNECTED_TX]->txInitRfOn;
}

void agentModelPattern::setTxFrequencyValue(double value)
{
	if ((10e6 < value) && (value <= 100e9)) {
		try {
			switch (_pConInstruments[C_CONNECTED_TX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				/* Send frequency in Hz */
				_snprintf_s(wrkBuf, sizeof(wrkBuf), ":FREQ %fHz", value);  // SMC100A
				usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
				usbReqData.thisInst = *_pConInstruments[C_CONNECTED_TX];
				usbReqData.data1 = wrkBuf;

				send(*_pAgtUsbTmcReq, usbReqData);
				AgentUsbRsp_t usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

				/* Copy back */
				*_pConInstruments[C_CONNECTED_TX] = usbRspData.thisInst;
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
				comReqData.parm.append(agentCom::double2String(value));
				comReqData.parm.append("\r\n");
				send(*(_pAgtComReq[C_COMINST_TX]), comReqData);
				comRspData = receive(*(_pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			break;

			};

			/* Update current values for the frequency */
			_pConInstruments[C_CONNECTED_TX]->txCurRfQrg = value;
			_pConInstruments[C_CONNECTED_RX]->rxCurRfQrg = value;    // Transmitter change does change (center) frequency of the SPEC, also! 
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
		}
	}
}

double agentModelPattern::getTxFrequencyValue(void)
{
	return _pConInstruments[C_CONNECTED_TX]->txCurRfQrg;
}

double agentModelPattern::getTxFrequencyDefault(void)
{
	return _pConInstruments[C_CONNECTED_TX]->txInitRfQrg;
}

void agentModelPattern::setTxPwrValue(double value)
{
	if (-40.0 <= value && value <= +20.0) {
		try {
			switch (_pConInstruments[C_CONNECTED_TX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				/* Send TX power in dBm */
				_snprintf_s(wrkBuf, sizeof(wrkBuf), ":POW %f", value);  // SMC100A
				usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
				usbReqData.thisInst = *_pConInstruments[C_CONNECTED_TX];
				usbReqData.data1 = wrkBuf;

				send(*_pAgtUsbTmcReq, usbReqData);
				AgentUsbRsp_t usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

				/* Copy back */
				*_pConInstruments[C_CONNECTED_TX] = usbRspData.thisInst;
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
				comReqData.parm.append(agentCom::double2String(value));
				comReqData.parm.append("dBm\r\n");
				send(*(_pAgtComReq[C_COMINST_TX]), comReqData);
				comRspData = receive(*(_pAgtComRsp[C_COMINST_TX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			break;

			};

			/* Update current values for the power */
			_pConInstruments[C_CONNECTED_TX]->txCurRfPwr = value;
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
		}
	}
}

double agentModelPattern::getTxPwrValue(void)
{
	return _pConInstruments[C_CONNECTED_TX]->txCurRfPwr;
}

double agentModelPattern::getTxPwrDefault(void)
{
	return _pConInstruments[C_CONNECTED_TX]->txInitRfPwr;
}


/* agentModelPattern - RX */

void agentModelPattern::setRxFrequencyValue(double value)
{
	if (_pAgtComRsp[C_COMINST_RX] && ((10e6 < value) && (value <= 100e9))) {
		try {
			switch (_pConInstruments[C_CONNECTED_RX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				/* Send center frequency in Hz */
				_snprintf_s(wrkBuf, sizeof(wrkBuf), ":FREQ:CENT %fHz", value);  // DSA875
				usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
				usbReqData.thisInst = *_pConInstruments[C_CONNECTED_RX];
				usbReqData.data1 = wrkBuf;

				send(*_pAgtUsbTmcReq, usbReqData);
				AgentUsbRsp_t usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

				/* Copy back */
				*_pConInstruments[C_CONNECTED_RX] = usbRspData.thisInst;
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
				comReqData.parm.append(agentCom::double2String(value));
				comReqData.parm.append("HZ\r\n");
				send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
				comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			break;

			};

			/* Update current values for the frequency */
			_pConInstruments[C_CONNECTED_RX]->rxCurRfQrg = value;
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
		}
	}
}

double agentModelPattern::getRxFrequencyValue(void)
{
	return _pConInstruments[C_CONNECTED_RX]->rxCurRfQrg;
}

void agentModelPattern::setRxSpanValue(double value)
{
	if (_pAgtComRsp[C_COMINST_RX] && (value <= 100e9)) {
		try {
			switch (_pConInstruments[C_CONNECTED_RX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				/* Send center frequency in Hz */
				_snprintf_s(wrkBuf, sizeof(wrkBuf), ":FREQ:SPAN %fHz", value);  // DSA875
				usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
				usbReqData.thisInst = *_pConInstruments[C_CONNECTED_RX];
				usbReqData.data1 = wrkBuf;

				send(*_pAgtUsbTmcReq, usbReqData);
				AgentUsbRsp_t usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

				/* Copy back */
				*_pConInstruments[C_CONNECTED_RX] = usbRspData.thisInst;
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
				comReqData.parm.append(agentCom::double2String(value));
				comReqData.parm.append("HZ\r\n");
				send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
				comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			break;

			};

			/* Update current values for the frequency */
			_pConInstruments[C_CONNECTED_RX]->rxCurRfSpan = value;
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
		}
	}
}

double agentModelPattern::getRxSpanValue(void)
{
	return _pConInstruments[C_CONNECTED_RX]->rxCurRfSpan;
}

double agentModelPattern::getRxSpanDefault(void)
{
	return _pConInstruments[C_CONNECTED_RX]->rxInitRfSpan;
}

void agentModelPattern::setRxLevelMaxValue(double value)
{
	if (_pAgtComRsp[C_COMINST_RX] && (-40.0 <= value) && (value <= +30.0)) {
		try {
			switch (_pConInstruments[C_CONNECTED_RX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				/* Send max amplitude to adjust attenuator */
				_snprintf_s(wrkBuf, sizeof(wrkBuf), ":POW:ATT %d", (value > -10.0 ? (10 + (int)value) : 0));  // DSA875
				usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
				usbReqData.thisInst = *_pConInstruments[C_CONNECTED_RX];
				usbReqData.data1 = wrkBuf;

				send(*_pAgtUsbTmcReq, usbReqData);
				AgentUsbRsp_t usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

				/* Copy back */
				*_pConInstruments[C_CONNECTED_RX] = usbRspData.thisInst;
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
				comReqData.parm.append(agentCom::double2String(value));
				comReqData.parm.append("DBM\r\n");
				send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
				comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
			}
			break;

			};

			/* Update current values for the max power level */
			_pConInstruments[C_CONNECTED_RX]->rxCurRfPwrHi = value;
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
		}
	}
}

double agentModelPattern::getRxLevelMaxValue(void)
{
	return _pConInstruments[C_CONNECTED_RX]->rxCurRfPwrHi;
}


bool agentModelPattern::getRxMarkerPeak(double* retX, double* retY)
{
	if (retX && retY) {
		*retX = *retY = 0.0;

		/* Following sequence does a level metering */
		try {
			switch (_pConInstruments[C_CONNECTED_RX]->actIfcType) {

			case ACT_IFC_USB:
			{
				AgentUsbReq_t usbReqData;
				char wrkBuf[256] = { 0 };

				if (string("RIGOL_DSA875") == _pConInstruments[C_CONNECTED_RX]->listEntryName) {
					/* Stop running meassurments */
					usbReqData.cmd = C_USBREQ_USBTMC_SEND_ONLY;
					usbReqData.thisInst = *_pConInstruments[C_CONNECTED_RX];
					usbReqData.data1 = ":INIT:CONT OFF";    // DSA875
					send(*_pAgtUsbTmcReq, usbReqData);
					AgentUsbRsp_t usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

					/* Start one shot meassurement and wait until being ready */
					usbReqData.thisInst = usbRspData.thisInst;
					usbReqData.data1 = ":INIT:IMM; *WAI";  // DSA875
					send(*_pAgtUsbTmcReq, usbReqData);
					usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

					/* Set Marker 1 as maximum marker */
					usbReqData.thisInst = usbRspData.thisInst;
					usbReqData.data1 = ":CALC:MARK1:PEAK:SEARCH:MODE MAX";  // DSA875
					send(*_pAgtUsbTmcReq, usbReqData);
					usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);

					/* Set Marker 1 to Maximum and wait until ready */
					usbReqData.thisInst = usbRspData.thisInst;
					usbReqData.data1 = ":CALC:MARK1:MAX:MAX";  // DSA875
					send(*_pAgtUsbTmcReq, usbReqData);
					usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
					Sleep(500);

					/* Get frequency value of the marker */
					usbReqData.cmd = C_USBREQ_USBTMC_SEND_AND_RECEIVE;
					usbReqData.thisInst = usbRspData.thisInst;
					usbReqData.data1 = ":CALC:MARK1:X?";  // DSA875
					send(*_pAgtUsbTmcReq, usbReqData);
					usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
					string retStr = *(string*)usbRspData.data1;
					sscanf_s(retStr.c_str(), "%lf", retX);

					/* Get amplitude value of the marker */
					usbReqData.thisInst = usbRspData.thisInst;
					usbReqData.data1 = ":CALC:MARK1:Y?";  // DSA875
					send(*_pAgtUsbTmcReq, usbReqData);
					usbRspData = receive(*_pAgtUsbTmcRsp /*, AGENT_PATTERN_RECEIVE_TIMEOUT */);
					retStr = *(string*)usbRspData.data1;
					sscanf_s(retStr.c_str(), "%lf", retY);

					/* Copy back */
					*_pConInstruments[C_CONNECTED_RX] = usbRspData.thisInst;

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
				if (_pAgtComReq[C_COMINST_RX]) {
					AgentComReq_t comReqData;
					AgentComRsp_t comRspData;

					if (string("R&S_FSEK20") == _pConInstruments[C_CONNECTED_RX]->listEntryName) {
						/* Stop running meassurments */
						comReqData.cmd = C_COMREQ_COM_SEND;
						comReqData.parm = string(":INIT:CONT OFF\r\n");
						send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
						comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);

						/* Start one shot meassurement and wait until being ready */
						comReqData.parm = string(":INIT:IMM; *WAI\r\n");
						send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
						comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);

						/* Set marker to maximum and wait until ready */
						comReqData.parm = string(":CALC:MARKER ON; :CALC:MARKER:MAX; *WAI\r\n");
						send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
						comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);

						/* Get frequency value of the marker */
						comReqData.parm = string(":CALC:MARKER:X?\r\n");
						send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
						comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
						sscanf_s(comRspData.data1.c_str(), "%lf", retX);

						/* Get amplitude value of the marker */
						comReqData.parm = string(":CALC:MARKER:Y?\r\n");
						send(*(_pAgtComReq[C_COMINST_RX]), comReqData);
						comRspData = receive(*(_pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
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

int agentModelPattern::runningProcessPattern(MEASDATA_SETUP_ENUM measVariant, double degStartPos, double degEndPos, double degStep)
{  /* run a antenna pattern from left = degStartPos to the right = degEndPos */

	/* Go to Start Position only on rotary measurements */
	{
		long prevPosTicksAbs = getCurPosTicksAbs();
		long startPosTicksAbs = calcDeg2Ticks(degStartPos);
		if (measVariant == MEASDATA_SETUP__PATTERN_GEN_SPEC) {
			/* Send Home position and wait */
			if (!_noWinMsg) {
				_pAgtMod->getWinSrv()->reportStatus(L"Model: Pattern", L"Going to start position", L"WAIT");
			}
			sendPosTicksAbs(startPosTicksAbs);

			/* Delay abt. the time the rotor needs to position */
			Sleep(calcTicks2Ms(startPosTicksAbs - prevPosTicksAbs));
		}

		if (_processing_ID <= C_MODELPATTERN_PROCESS_STOP) {
			return -1;
		}

		/* Inform about the start position */
		setStatusPosition(degStartPos);
	}

	/* Set-up TX */
	{
		/* Send frequency in Hz */
		setTxFrequencyValue(_pConInstruments[C_CONNECTED_TX]->txCurRfQrg);

		/* Send power in dBm */
		setTxPwrValue(_pConInstruments[C_CONNECTED_TX]->txCurRfPwr);

		/* Send power ON */
		_pConInstruments[C_CONNECTED_TX]->txCurRfOn = true;
		setTxOnState(_pConInstruments[C_CONNECTED_TX]->txCurRfOn);
	}

	/* Set-up RX */
	{
		/* Send Span in Hz */
		setRxSpanValue(_pConInstruments[C_CONNECTED_RX]->rxCurRfSpan);

		/* Send center frequency in Hz */
		setRxFrequencyValue(_pConInstruments[C_CONNECTED_RX]->rxCurRfQrg);

		/* Send max amplitude to adjust attenuator */
		setRxLevelMaxValue(_pConInstruments[C_CONNECTED_RX]->rxCurRfPwrHi);
	}

	/* Set-up the Data container for this meassurement */
	MeasData md;
	{
		std::list<double> init;
		init.push_back(_pConInstruments[C_CONNECTED_TX]->txCurRfQrg);
		init.push_back(_pConInstruments[C_CONNECTED_TX]->txCurRfPwr);
		init.push_back(_pConInstruments[C_CONNECTED_RX]->rxCurRfSpan);

		if (measVariant == MEASDATA_SETUP__PATTERN_GEN_SPEC) {
			init.push_back(degStartPos);
			init.push_back(degEndPos);
			init.push_back(degStep);
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
			double testY = 0.0, testY_sample = 0.0;

			/* Multiple samples to be averaged */
			const int LoopCnt = 5;
			for (int loopIdx = 0; loopIdx < LoopCnt; ++loopIdx) {
				getRxMarkerPeak(&testX, &testY_sample);
				testY += testY_sample;
			}
			testY /= LoopCnt;

			swprintf(strbuf, sizeof(strbuf)>>1, L"> Pos = %03d�: f = %E Hz, \tP = %E dBm.\n", (int)degPosIter, testX, testY);
			OutputDebugString(strbuf);

			/* Store current data */
			std::list<double> data;
			data.push_back(degPosIter); // Position
			data.push_back(testY);		// Power
			data.push_back(0.0);		// Phase
			measDataAdd(&md, data);

			/* Inform about the current RX power */
			setStatusRxPower_dBm(testY);
		}

		/* In case the STOP button is being pressed */
		if (_processing_ID == C_MODELPATTERN_PROCESS_STOP) {
			goto ErrorOut_runningProcessPattern;
		}

		/* Calc new position (turning right / left) */
		degPosIter += degStep;
		if (degStep > 0.0) {
			if (degPosIter > (degEndPos + 0.0005)) {
				break;
			}
		}
		else if (degStep < 0.0) {
			if (degPosIter < (degEndPos - 0.0005)) {
				break;
			}
		}
		else {
			/* Single position */
			break;
		}

		/* Calc next position */
		long prevPosTicksAbs = getCurPosTicksAbs();
		long nextPosTicksAbs = calcDeg2Ticks(degPosIter);

		/* Send new position desired */
		sendPosTicksAbs(nextPosTicksAbs);

		/* Delay abt. the time the rotor needs to position */
		Sleep(calcTicks2Ms(nextPosTicksAbs - prevPosTicksAbs));

		/* Inform about the current step position */
		setStatusPosition(degPosIter);
	}  // while (true)

	/* Send power OFF */
	{
		_pConInstruments[C_CONNECTED_TX]->txCurRfOn = false;
		setTxOnState(_pConInstruments[C_CONNECTED_TX]->txCurRfOn);
	}

	if (measVariant == MEASDATA_SETUP__REFMEAS_GEN_SPEC) {
		if (!_noWinMsg) {
			_pAgtMod->getWinSrv()->reportStatus(NULL, NULL, L"REF done");
		}
	}

	else
	if (measVariant == MEASDATA_SETUP__PATTERN_GEN_SPEC) {
		/* Return ROTOR to center position */
		if (!_noWinMsg) {
			_pAgtMod->getWinSrv()->reportStatus(NULL, NULL, L"Going to HOME position");
		}
		sendPosTicksAbs(0L);

		/* Inform about the current step position */
		setStatusPosition(0.0);

		/* Delay abt. the time the rotor needs to position */
		Sleep(calcDeg2Ms(degEndPos));
		if (!_noWinMsg) {
			_pAgtMod->getWinSrv()->reportStatus(NULL, NULL, L"Homing done");
		}
	}

	_processing_ID = C_MODELPATTERN_PROCESS_NOOP;

	/* Move data to the global entries */
	measDataFinalize(&md, &_measDataEntries);

	/* Expand temporary file name template */
	const int rotSpan = (int) (0.1 + degEndPos - degStartPos);
	const int rotStep = (int) (0.1 + degStep);
	const int pwrRef  = (int) _pConInstruments[C_CONNECTED_TX]->txCurRfPwr;
	WinSrv::evalTmpTemplateFile(rotSpan, rotStep, pwrRef);
	WinSrv::copyTmpFilePathName2currentFilePathName();

	return 0;


ErrorOut_runningProcessPattern:
	/* Send power OFF */
	{
		_pConInstruments[C_CONNECTED_TX]->txCurRfOn = false;
		setTxOnState(_pConInstruments[C_CONNECTED_TX]->txCurRfOn);
	}

	/* Move data to the global entries */
	measDataFinalize(&md, &_measDataEntries);

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
	if (deg < 0.0) {
		deg = -deg;
	}
	return (DWORD)(500L + deg * AGENT_PATTERN_ROT_MS_PER_DEGREE);
}

inline DWORD agentModelPattern::calcTicks2Ms(long ticks)
{
	return calcDeg2Ms(calcTicks2Deg(ticks));
}
