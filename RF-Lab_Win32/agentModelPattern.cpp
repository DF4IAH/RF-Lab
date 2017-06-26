#include "stdafx.h"
#include "agentModelPattern.h"
#include "agentModel.h"

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


agentModelPattern::agentModelPattern(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, AGENT_ALL_SIMUMODE_t mode)
				 : pAgtComReq{ nullptr }
				 , pAgtComRsp{ nullptr }
				 , pAgtCom{ nullptr }
				 , _arg(nullptr)

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
	/* enter first step of FSM */
	_runState = C_MODELPATTERN_RUNSTATES_OPENCOM;

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


void agentModelPattern::run(void)
{
	// Start Antenna Rotor
	if (pAgtCom[C_COMINST_ROT]) {
		pAgtCom[C_COMINST_ROT]->start();
		
		if (pAgtCom[C_COMINST_TX]) {
			pAgtCom[C_COMINST_TX]->start();
		}
		
		if (pAgtCom[C_COMINST_RX]) {
			pAgtCom[C_COMINST_RX]->start();
		}

		_running = TRUE;
		while (_running) {
			// model's working loop
			switch (_runState) {
			case C_MODELPATTERN_RUNSTATES_OPENCOM:
			{
				char buf[C_BUF_SIZE];
				agentComReq comReqData;
				
				initState = 0x01;
				
				// Open Rotor
				if (pAgtCom[C_COMINST_ROT]) {
					comReqData.cmd = C_COMREQ_OPEN;
					snprintf(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d", 3, CBR_19200, 8, NOPARITY, ONESTOPBIT);  // Zolix USB port - 19200 baud, 8N1
					comReqData.parm = string(buf);
					send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
					initState = 0x02;
				}

				// Open TX
				if (pAgtCom[C_COMINST_TX]) {
					comReqData.cmd = C_COMREQ_OPEN;
					if (pAgtCom[C_COMINST_TX]->isIec()) {
						pAgtCom[C_COMINST_TX]->setIecAddr(28);																// IEC625: R&S SMR40: address == 28
						snprintf(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d", 4, CBR_19200, 8, NOPARITY, ONESTOPBIT);  // IEC625: 19200 baud, 8N1
					}
					else {
						snprintf(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d", 1, CBR_9600, 8, NOPARITY, ONESTOPBIT);	// serial port - 9600 baud, 8N1
					}
					comReqData.parm = string(buf);
					send(*(pAgtComReq[C_COMINST_TX]), comReqData);
					initState = 0x03;
				}

				// Open RX
				if (pAgtCom[C_COMINST_RX]) {
					comReqData.cmd = C_COMREQ_OPEN;
					if (pAgtCom[C_COMINST_RX]->isIec()) {
						pAgtCom[C_COMINST_RX]->setIecAddr(20);																// IEC625: R&S FSEK20: address == 20
						snprintf(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d", 4, CBR_19200, 8, NOPARITY, ONESTOPBIT);  // IEC625: 19200 baud, 8N1
					}
					else {
						snprintf(buf, C_BUF_SIZE, ":P=%d :B=%d :I=%d :A=%d :S=%d", 1, CBR_9600, 8, NOPARITY, ONESTOPBIT);	// serial port - 9600 baud, 8N1
					}
					comReqData.parm = string(buf);
					send(*(pAgtComReq[C_COMINST_RX]), comReqData);
					initState = 0x04;
				}

				_runState = C_MODELPATTERN_RUNSTATES_OPENCOM_WAIT;
			}
			break;

			case C_MODELPATTERN_RUNSTATES_OPENCOM_WAIT:
			{
				try {
					agentComReq comReqData;

					_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;		// default value

					do {
						/* receive rotor opening response */
						initState = 0x11;
						if (!pAgtCom[C_COMINST_ROT]) break;
						initState = 0x12;
						agentComRsp comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
						if (comRspData.stat != C_COMRSP_OK)	break;
						initState = 0x13;

						/* check if rotor is responding */
						comReqData.cmd = C_COMREQ_COM_SEND;
						comReqData.parm = string("?X\r");					// Zolix: never send a \n (LF) !!!
						send(*(pAgtComReq[C_COMINST_ROT]), comReqData);		// first request for sync purposes
						comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
						send(*(pAgtComReq[C_COMINST_ROT]), comReqData);
						comRspData = receive(*(pAgtComRsp[C_COMINST_ROT]), AGENT_PATTERN_RECEIVE_TIMEOUT);
						if (comRspData.stat != C_COMRSP_DATA) break;
						if (_strnicmp(&(comRspData.data[0]), "?X", 2)) break;
						initState = 0x14;

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
								Sleep(100);
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
								Sleep(100);
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
							initState = 0x3F;
						}

						/* success, all devices are ready */
						_runState = C_MODELPATTERN_RUNSTATES_INIT;
						initState = 0xFF;
					} while (FALSE);
				}
				catch (const Concurrency::operation_timed_out& e) {
					(void)e;
					_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
				}
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
							initState = 0x44;
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0x45;
							_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
						}
					}

					// read current tick position of rotor and store value
					(void) requestPos();
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
							Sleep(2500);
							initState = 0x56;
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0x5F;
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
							Sleep(2500);
							initState = 0xA6;

							comReqData.parm = string(":INIT:CONT ON\r\n");
							send(*(pAgtComReq[C_COMINST_RX]), comReqData);
							comRspData = receive(*(pAgtComRsp[C_COMINST_RX]), AGENT_PATTERN_RECEIVE_TIMEOUT);
							initState = 0xA7;
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0xAF;
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
						}
						catch (const Concurrency::operation_timed_out& e) {
							(void)e;
							initState = 0xCF;
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
				bool state = try_receive(*(pAgtComRsp[C_COMINST_ROT]), comRspData);
				if (state) {
					// consume each reported state
					if (comRspData.stat == C_COMRSP_FAIL) {
						initState = 0xDF;
						_runState = C_MODELPATTERN_RUNSTATES_INIT_ERROR;
					}
				}
				else {
					initState = 0xD1;
					_runState = C_MODELPATTERN_RUNSTATES_RUNNING;
				}
			}
			break;

			case C_MODELPATTERN_RUNSTATES_INIT_ERROR:
			{
				/* Init Error occured, show requester */
				int lastGood = initState;
				_runState = C_MODELPATTERN_RUNSTATES_CLOSE_COM;
			}

			case C_MODELPATTERN_RUNSTATES_RUNNING:
			{

				Sleep(10);
			}
			break;

			case C_MODELPATTERN_RUNSTATES_GOTO_X:
			{
				agentComReq comReqData;
				agentComRsp comRspData;

				int gotoMilliPos = 0;
				if (_arg) {
					gotoMilliPos = *((int*)_arg);
					_arg = nullptr;
				}
				
				(void) requestPos();
				sendPos((int)(gotoMilliPos * 0.8));
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
					if (pAgtCom[i]) {
						agentComRsp comRsp;

						// consume until END response is received
						do {
							comRsp = receive(*(pAgtComRsp[i]), AGENT_PATTERN_RECEIVE_TIMEOUT);
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


/* agentModelPattern - GENERAL */

void agentModelPattern::setSimuMode(int mode)
{
	simuMode = mode;
}

int agentModelPattern::getSimuMode(void)
{
	return simuMode;
}

void agentModelPattern::runProcess(int processID)
{
	/* STOP at once processing ID */
	if (processID == C_MODELPATTERN_PROCESS_STOP) {
		processing_ID = 0;
		return;
	}

	/* Cue in process ID to be done */
	if (!processing_ID) {
		processing_ID = processID;
	}
}

void agentModelPattern::processing_pattern()
{
	// TODO: need own thread to drive this method!

	double degStartPos		=  -90.0;
	double degEndPos		=   90.0;
	double degResolution	=    5.0;

	/* Set-up ROTOR */
	long ticksDiff = requestPos();
	sendPos(0);
	Sleep(calcDeg2Ms(calcTicks2Deg(ticksDiff)));

	if (!processing_ID) {
		return;
	}
	requestPos();
	sendPos(calcDeg2Ticks(degStartPos));					// Go to start position
	Sleep(calcDeg2Ms(degStartPos));

	/* Set-up TX */

	/* Set-up RX */

	/* Iteration of rotor steps */
	for (double degPosIter = degStartPos; degPosIter <= degEndPos; degPosIter += degResolution) {
		if (!processing_ID) {
			return;
		}

		/* advance to new position */
		requestPos();
		sendPos(calcDeg2Ticks(degPosIter));
		Sleep(calcDeg2Ms(degResolution));

		/* Record data */
		//measure();
	}
	
	if (!processing_ID) {
		return;
	}
	/* Return ROTOR to center position */
	requestPos();
	sendPos(0);
	Sleep(calcDeg2Ms(degEndPos));

	printf("END OF TEST\r\n");
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
			Sleep(10);
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

void agentModelPattern::sendPos(int tickPos)
{
	agentComReq comReqData;
	agentComRsp comRspData;
	int posDiff = tickPos - getLastTickPos();
	char cbuf[C_BUF_SIZE];

	try {
		snprintf(cbuf, C_BUF_SIZE - 1, "%cX,%d\r", (posDiff > 0 ? '+' : '-'), abs(posDiff));
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

inline double agentModelPattern::calcTicks2Deg(long ticks)
{
	return ((double) ticks) / AGENT_PATTERN_ROT_TICKS_PER_DEGREE;
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
	return (DWORD) (deg * AGENT_PATTERN_ROT_MS_PER_DEGREE);
}
