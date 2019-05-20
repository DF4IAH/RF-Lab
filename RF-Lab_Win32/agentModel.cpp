#include "stdafx.h"

#include "resource.h"
#include "CommCtrl.h"
#include "WinSrv.h"

#include "agentModel_InstList.h"
#include "agentModelPattern.h"

#include "externals.h"

#include <string>
#include <search.h>
#include <locale.h>
#include <locale>

#include "agentModel.h"



const char C_FS_INSTRUMENTS_FILENAME_DEFAULT[] = "/Users/Labor/RF-Lab.cfg";



template <class T>  void SafeReleaseDelete(T **ppT)
{
	if (*ppT)
	{
		//(*ppT)->Release();
		*ppT = nullptr;
	}
}



int compare(void *pvlocale, const void *str1, const void *str2)
{
	char *s1 = *(char**)str1;
	char *s2 = *(char**)str2;

	locale& loc = *(reinterpret_cast< locale * > (pvlocale));

	return use_facet< collate<char> >(loc).compare(
		s1, s1 + strlen(s1),
		s2, s2 + strlen(s2));
}



agentModel::agentModel(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, class WinSrv *winSrv, HWND hWnd, AGENT_MODELS am_variant, AGENT_ALL_SIMUMODE_t mode)
				 : _src(src)
				 , _tgt(tgt)
				 , _winSrv(winSrv)
				 , _hWnd(hWnd)
				 , _am_variant(am_variant)
				 , _simuMode(mode)
{
	g_am = this;

	errno_t err = strncpy_s(_fs_instrument_settings_filename, sizeof(_fs_instrument_settings_filename) - 1, C_FS_INSTRUMENTS_FILENAME_DEFAULT, strlen(C_FS_INSTRUMENTS_FILENAME_DEFAULT));

	/* Set up list of instruments */
	{
		g_am_InstList_locked = true;

		#ifndef USE_PRELOAD_INSTRUMENTS
			fsLoadInstruments(_fs_instrument_settings_filename);
			scanInstruments();
		#else
			preloadInstruments();	// TODO: remove me later!
		#endif
		
		g_am_InstList_locked = false;
	}

	/* Inform UI about up-to-date list */
	{
		// @see https://msdn.microsoft.com/en-us/library/windows/desktop/ms647553(v=vs.85).aspx#accessing_menu_items_programmatically
		// MENUINFO x;
		// InsertMenuItem();



		// SendMessageW(GetDlgItem(_hWnd, IDC_ROTOR_POS_X_NEW_SLIDER), TBM_GETPOS, 0, 0);
	}

	switch (am_variant) {
	case AGENT_MODEL_PATTERN:
		_curModel = new agentModelPattern(src, tgt, this, mode);
		break;

	case AGENT_MODEL_NONE:
	default:
		delete _curModel; _curModel = nullptr;
	}
}

agentModel::~agentModel(void)
{
	if (g_am && g_am->_curModel) {
		delete _curModel;
	}
	g_am = nullptr;
}


class agentModelVariant* agentModel::getCurModCtx(void)
{
	if (g_am && g_am->_curModel) {
		return _curModel;
	}
	return nullptr;
}

void agentModel::run(void)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->run();
		done();
	}
}

bool agentModel::isRunning(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->isRunning();
	} else {
		return false;
	}
}


void agentModel::Release(void)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->Release();
	}
}


bool agentModel::shutdown(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->shutdown();
	}
	else {
		return false;
	}
}

void agentModel::wmCmd(int wmId, LPVOID arg)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->wmCmd(wmId, arg);
	}
}

class WinSrv* agentModel::getWinSrv(void)
{
	return _winSrv;
}


/* Tools */

bool agentModel::parseStr2Bool(bool* ret, const char* ary, const char* fmt, char delimRight)
{
	if (ret) {
		const char* cmpPtr = ary;

		*ret = FALSE;

		if (ary && fmt) {
			if (delimRight) {
				cmpPtr = strrchr(ary, delimRight);
			}

			if (cmpPtr) {
				sscanf_s(cmpPtr, fmt, ret);
				return FALSE;					// Good
			}
		}
	}
	return TRUE;								// Error
}

bool agentModel::parseStr2Long(long* ret, const char* ary, const char* fmt, char delimRight)
{
	if (ret) {
		const char* cmpPtr = ary;

		*ret = 0;

		if (ary && fmt) {
			if (delimRight) {
				cmpPtr = strrchr(ary, delimRight);
			}

			if (cmpPtr) {
				sscanf_s(cmpPtr, fmt, ret);
				return FALSE;					// Good
			}
		}
	}
	return TRUE;								// Error
}

bool agentModel::parseStr2Double(double* ret, const char* ary, const char* fmt, char delimRight)
{
	if (ret) {
		const char* cmpPtr = ary;

		*ret = 0.;

		if (ary && fmt) {
			if (delimRight) {
				cmpPtr = strrchr(ary, delimRight);
			}

			if (cmpPtr) {
				sscanf_s(cmpPtr, fmt, ret);
				return FALSE;					// Good
			}
		}
	}
	return TRUE;								// Error
}

unsigned int agentModel::getLineLength(const char* p, unsigned int len)
{
	for (uint32_t idx = 0; idx < len; idx++) {
		const char c = *(p + idx);

		if (c == '\r' || c == '\n') {
			return idx;
		}
	}

	return len;
}


/* agentModelPattern - GENERAL */

void agentModel::setSimuMode(int simuMode)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->setSimuMode(simuMode);
	}
}

int agentModel::getSimuMode(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getSimuMode();
	}
	else {
		return 0;
	}
}

void agentModel::sendModelStatus(LPVOID status1, LPVOID status2)
{

}

void agentModel::initDevices(void)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->initDevices();
	}
}

void agentModel::runProcess(int processID, int arg)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->runProcess(processID, arg);
	}
}


/* agentModelPattern - Rotor */

long agentModel::requestPos(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->requestPos();
	}
	return MAXINT32;
}

void agentModel::setLastTickPos(long tickPos)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->setLastTickPos(tickPos);
	}
}

long agentModel::getLastTickPos(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getLastTickPos();
	}
	else {
		return 0;
	}
}


/* agentModelPattern - TX */

void agentModel::setTxOnState(bool checked)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->setTxOnState(checked);
	}
}

bool agentModel::getTxOnState(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getTxOnState();
	}
	else {
		return false;
	}
}

bool agentModel::getTxOnDefault(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getTxOnDefault();
	}
	else {
		return false;
	}
}

void agentModel::setTxFrequencyValue(double value)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->setTxFrequencyValue(value);
	}
}

double agentModel::getTxFrequencyValue(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getTxFrequencyValue();
	}
	else {
		return 0.;
	}
}

double agentModel::getTxFrequencyDefault(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getTxFrequencyDefault();
	}
	else {
		return 0.;
	}
}

void agentModel::setTxPwrValue(double value)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->setTxPwrValue(value);
	}
}

double agentModel::getTxPwrValue(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getTxPwrValue();
	}
	else {
		return 0.;
	}
}

double agentModel::getTxPwrDefault(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getTxPwrDefault();
	}
	else {
		return 0.;
	}
}


/* agentModelPattern - RX */

void agentModel::setRxFrequencyValue(double value)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->setRxFrequencyValue(value);
	}
}

double agentModel::getRxFrequencyValue(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getRxFrequencyValue();
	}
	else {
		return 0.;
	}
}

void agentModel::setRxSpanValue(double value)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->setRxSpanValue(value);
	}
}

double agentModel::getRxSpanValue(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getRxSpanValue();
	}
	else {
		return 0.;
	}
}

double agentModel::getRxSpanDefault(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getRxSpanDefault();
	}
	else {
		return 0.;
	}
}

void agentModel::setRxLevelMaxValue(double value)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->setRxLevelMaxValue(value);
	}
}

double agentModel::getRxLevelMaxValue(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->getRxLevelMaxValue();
	}
	else {
		return 0.;
	}
}



void agentModel::fsLoadInstruments(const char* filename)
{
	char							errMsgBuf[128];
	uint32_t						errLine					= 0UL;
	uint32_t						lineCtr					= 0UL;
	map<string, confAttributes_t>	cM;
	confAttributes_t				cA;

	/* Load config file */
	{
		FILE* fh = NULL;
		errno_t err;

		confAttrClear(&cA);

		err = fopen_s(&fh, filename, "rb");
		if (!errno && fh) {
			char variant = '-';
			char lineBuf[256];

			while (1) {
				char* p = fgets(lineBuf, sizeof(lineBuf) - 1, fh);
				if (!p) {
					/* End of file */
					if (variant != '-') {
						pushInstrumentDataset(&cM, cA.attrName, &cA);
						confAttrClear(&cA);
					}
					break;
				}

				/* Starting with Line 1 */
				++lineCtr;

				/* Get length of text */
				const unsigned int numOfElem	= (unsigned int) strlen(p);
				const unsigned int lineLen		= getLineLength(p, numOfElem);

				if (*p == '#') {
					/* comment */
					continue;
				}

				/* SECTIONS */
				else if (*p == '[') {
					/* Write attributes of previous section */
					if (variant != '-') {
						pushInstrumentDataset(&cM, cA.attrName, &cA);

						/* Start a new set of attributes */
						confAttrClear(&cA);
					}

					/* Types */
					if (!_strnicmp(p, "[INSTRUMENT]", 12)) {
						variant = 'I';
					}
					else if (!_strnicmp(p, "[INTERFACE]", 11)) {
						variant = 'i';
					}
					else if (!_strnicmp(p, "[COM]", 5)) {
						variant = 'C';
					}
					else if (!_strnicmp(p, "[USB]", 5)) {
						variant = 'U';
					}
					else if (!_strnicmp(p, "[GPIB]", 6)) {
						variant = 'G';
					}
					else {
						/* File format error */
						const char Msg[] = "Unknown section type in brackets";
						strncpy_s(errMsgBuf, sizeof(errMsgBuf), Msg, strlen(Msg));
						errLine = lineCtr;
						goto _fsLoadInstruments_Error;
					}
				}

				/* INSTRUMENT attributes */
				else if ((variant == 'I') && !_strnicmp(p, "Name=", 5)) {
					cA.attrName.assign(p + 5, p + lineLen);
					cA.attrSection = string("INSTRUMENT");
				}
				else if ((variant == 'I') && !_strnicmp(p, "Type=", 5)) {
					cA.attrType.assign(p + 5, p + lineLen);
				}

				/* Type: Rotor */
				else if ((variant == 'I') && !_strnicmp(p, "Turn_left_max_deg=", 18)) {
					try {
						cA.attrTurnLeftMaxDeg = stof(string(p + 18, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Turn_right_max_deg=", 19)) {
					try {
						cA.attrTurnRightMaxDeg = stof(string(p + 19, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Ticks_360deg=", 13)) {
					try {
						cA.attrTicks360Deg = stoi(string(p + 13, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Speed_Start=", 12)) {
					try {
						cA.attrSpeedStart = stof(string(p + 12, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Speed_Accl=", 11)) {
					try {
						cA.attrSpeedAccl = stof(string(p + 11, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Speed_Top=", 10)) {
					try {
						cA.attrSpeedTop = stof(string(p + 10, p + lineLen));
					}
					catch (...) {
					}
				}

				/* Type: Generator & Spectrum-Analyzer */
				else if ((variant == 'I') && !_strnicmp(p, "Freq_min_Hz=", 12)) {
					try {
						cA.attrFreqMinHz = stof(string(p + 12, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Freq_max_Hz=", 12)) {
					try {
						cA.attrFreqMaxHz = stof(string(p + 12, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Freq_init_Hz=", 13)) {
					try {
						cA.attrFreqInitHz = stof(string(p + 13, p + lineLen));
					}
					catch (...) {
					}
				}

				/* Type: Generator */
				else if ((variant == 'I') && !_strnicmp(p, "TXlevel_min_dBm=", 16)) {
					try {
						cA.attrTXlevelMinDbm = stof(string(p + 16, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "TXlevel_max_dBm=", 16)) {
					try {
						cA.attrTXlevelMaxDbm = stof(string(p + 16, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "TXlevel_init_dBm=", 17)) {
					try {
						cA.attrTXlevelInitDbm = stof(string(p + 17, p + lineLen));
					}
					catch (...) {
					}
				}

				/* Type: Spectrum-Analyzer */
				else if ((variant == 'I') && !_strnicmp(p, "Span_min_Hz=", 12)) {
					try {
						cA.attrSpanMinHz = stof(string(p + 12, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Span_max_Hz=", 12)) {
					try {
						cA.attrSpanMaxHz = stof(string(p + 12, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Span_init_Hz=", 13)) {
					try {
						cA.attrSpanInitHz = stof(string(p + 13, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "RXLoLevel_min_dBm=", 18)) {
					try {
						cA.attrRXLoLevelMinDbm = stof(string(p + 18, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "RXLoLevel_max_dBm=", 18)) {
					try {
						cA.attrRXLoLevelMaxDbm = stof(string(p + 18, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "RXLoLevel_init_dBm=", 19)) {
					try {
						cA.attrRXLoLevelInitDbm = stof(string(p + 19, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "RXHiLevel_min_dBm=", 18)) {
					try {
						cA.attrRXHiLevelMinDbm = stof(string(p + 18, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "RXHiLevel_max_dBm=", 18)) {
					try {
						cA.attrRXHiLevelMaxDbm = stof(string(p + 18, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "RXHiLevel_init_dBm=", 19)) {
					try {
						cA.attrRXHiLevelInitDbm = stof(string(p + 19, p + lineLen));
					}
					catch (...) {
					}
				}

				/* INTERFACE attributes */
				else if ((variant == 'i') && !_strnicmp(p, "Name=", 5)) {
					cA.attrName.assign(p + 5, p + lineLen);
					cA.attrSection = string("INTERFACE");
				}
				else if ((variant == 'i') && !_strnicmp(p, "ServerType=", 11)) {
					cA.attrServerType.assign(p + 11, p + lineLen);
				}
				else if ((variant == 'i') && !_strnicmp(p, "ServerPort=", 11)) {
					try {
						cA.attrServerPort = stoi(string(p + 11, p + lineLen));
					}
					catch (...) {
					}
				}

				/* Serial COM attributes */
				else if ((variant == 'C') && !_strnicmp(p, "Name=", 5)) {
					cA.attrName.assign(p + 5, p + lineLen);
					cA.attrSection = string("COM");
				}
				else if ((variant == 'C') && !_strnicmp(p, "Device=", 7)) {
					cA.attrComDevice.assign(p + 7, p + lineLen);
				}
				else if ((variant == 'C') && !_strnicmp(p, "Baud=", 5)) {
					try {
						cA.attrComBaud = stoi(string(p + 5, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'C') && !_strnicmp(p, "Bits=", 5)) {
					try {
						cA.attrComBits = stoi(string(p + 5, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'C') && !_strnicmp(p, "Par=", 4)) {
					cA.attrComPar.assign(p + 4, p + lineLen);
				}
				else if ((variant == 'C') && !_strnicmp(p, "Stop=", 5)) {
					try {
						cA.attrComStop = stoi(string(p + 5, p + lineLen));
					}
					catch (...) {
					}
				}

				/* USB attributes */
				else if ((variant == 'U') && !_strnicmp(p, "Name=", 5)) {
					cA.attrName.assign(p + 5, p + lineLen);
					cA.attrSection = string("USB");
				}
				else if ((variant == 'U') && !_strnicmp(p, "Vendor_ID=", 10)) {
					try {
						unsigned int hex = 0U;
						if (sscanf_s(string(p + 10, p + lineLen).c_str(), "%x", &hex)) {
							cA.attrUsbVendorID = (uint16_t) hex;
						}
					}
					catch (...) {
					}
				}
				else if ((variant == 'U') && !_strnicmp(p, "Product_ID=", 11)) {
					try {
						unsigned int hex = 0U;
						if (sscanf_s(string(p + 11, p + lineLen).c_str(), "%x", &hex)) {
							cA.attrUsbProductID = (uint16_t) hex;
						}
					}
					catch (...) {
					}
				}

				/* GPIB attributes */
				else if ((variant == 'G') && !_strnicmp(p, "Name=", 5)) {
					cA.attrName.assign(p + 5, p + lineLen);
					cA.attrSection = string("GPIB");
				}
				else if ((variant == 'G') && !_strnicmp(p, "Addr=", 5)) {
					try {
						cA.attrGpibAddr = stoi(string(p + 5, p + lineLen));
					}
					catch (...) {
					}
				}

				/* Empty line */
				else if (*p == '\r' || *p == '\n') {
					/* No function */
					continue;
				}
				else {
					/* File format error */
					const char Msg[] = "Unknown attribute";
					strncpy_s(errMsgBuf, sizeof(errMsgBuf) - 1, Msg, strlen(Msg));
					errLine = lineCtr;
					goto _fsLoadInstruments_Error;
				}
			}
			fclose(fh);
		}
		else {
			/* File not found */
			const char Msg[] = "File not found";
			strncpy_s(errMsgBuf, sizeof(errMsgBuf) - 1, Msg, strlen(Msg));
			errLine = lineCtr;
			goto _fsLoadInstruments_Error;
		}
	}

	/* Link Instruments, Interfaces and Ports together */
	{
		const size_t mSize = cM.size();
		if (mSize) {
			/* Find interfaces */
			map<string, confAttributes_t>::iterator it = cM.begin();
			while (it != cM.end()) {
				confAttributes_t attr = it->second;
				if (!_stricmp(attr.attrSection.c_str(), "INTERFACE")) {
					const string serverType = attr.attrServerType;
					const uint16_t ifcPort = attr.attrServerPort;

					/* Handle GPIB interfaces */
					if (!_stricmp(serverType.c_str(), "GPIB")) {
						map<string, confAttributes_t>::iterator it2 = cM.begin();

						/* Search instruments ... */
						while (it2 != cM.end()) {
							confAttributes_t attr2 = it2->second;

							/* ... having the same GPIB address */
							if (!_stricmp(attr2.attrSection.c_str(), "INSTRUMENT") && attr2.attrGpibAddr == ifcPort) {
								/* Instrument found - enter all communication entries */
								attr2.attrComDevice = attr.attrComDevice;
								attr2.attrComBaud = attr.attrComBaud;
								attr2.attrComBits = attr.attrComBits;
								attr2.attrComPar = attr.attrComPar;
								attr2.attrComStop = attr.attrComStop;

								it2->second = attr2;
								break;
							}
							++it2;
						}
					}
				}
				++it;
			}
		}
	}

	/* Fill in instrument list */
	{
		const size_t mSize = cM.size();
		if (mSize) {
			/* Find interfaces */
			map<string, confAttributes_t>::iterator it = cM.begin();
	
			/* Copy attributes of each instrument to global instrument list */
			while (it != cM.end()) {
				confAttributes_t attr = it->second;
				am_InstListEntry_t le = { 0 };

				/* General settings */
				le.listEntryName					= attr.attrName;

				if (!_strnicmp(attr.attrType.c_str(), "Rot", 3)) {
					le.listFunction = INST_FUNCTION_ROTOR;
				}
				else if (!_strnicmp(attr.attrType.c_str(), "Spec", 4)) {
					le.listFunction = INST_FUNCTION_RX;
				}
				else if (!_strnicmp(attr.attrType.c_str(), "Gen", 3)) {
					le.listFunction = INST_FUNCTION_TX;
				}
				else {
					/* Do not include other devices into the instrument list */
					it++;
					continue;
				}
				
				/* Rotor settings */
				if (le.listFunction == INST_FUNCTION_ROTOR) {
					le.rotInitTicksPer360deg = (int)attr.attrTicks360Deg;
					le.rotInitTopSpeed = (int)attr.attrSpeedTop;
					le.rotInitAcclSpeed = (int)attr.attrSpeedAccl;
					le.rotInitStartSpeed = (int)attr.attrSpeedStart;

					le.rotMinPosition = (int)attr.attrTurnLeftMaxDeg;
					le.rotMaxPosition = (int)attr.attrTurnRightMaxDeg;
					le.rotInitPosition = 0;
					le.rotCurPosition = le.rotInitPosition;
				}


				/* TX settings */
				if (le.listFunction == INST_FUNCTION_TX) {
					le.txInitRfOn = false;
					le.txCurRfOn = le.txInitRfOn;

					le.txMinRfQrg = attr.attrFreqMinHz;
					le.txMaxRfQrg = attr.attrFreqMaxHz;
					le.txInitRfQrg = attr.attrFreqInitHz;
					le.txCurRfQrg = le.txInitRfQrg;

					le.txMinRfPwr = attr.attrTXlevelMinDbm;
					le.txMaxRfPwr = attr.attrTXlevelMaxDbm;
					le.txInitRfPwr = attr.attrTXlevelInitDbm;
					le.txCurRfPwr = le.txInitRfPwr;
				}


				/* RX settings */
				if (le.listFunction == INST_FUNCTION_RX) {
					le.rxMinRfQrg = attr.attrFreqMinHz;
					le.rxMaxRfQrg = attr.attrFreqMaxHz;
					le.rxInitRfQrg = attr.attrFreqInitHz;
					le.rxCurRfQrg = le.rxInitRfQrg;

					le.rxMinRfSpan = attr.attrSpanMinHz;
					le.rxMaxRfSpan = attr.attrSpanMaxHz;
					le.rxInitRfSpan = attr.attrSpanInitHz;
					le.rxCurRfSpan = le.rxInitRfSpan;

					le.rxMinRfPwrLo = attr.attrRXLoLevelMinDbm;
					le.rxMaxRfPwrLo = attr.attrRXLoLevelMaxDbm;
					le.rxInitRfPwrLo = attr.attrRXLoLevelInitDbm;
					le.rxCurRfPwrLo = le.rxInitRfPwrLo;

					le.rxMinRfPwrHi = attr.attrRXHiLevelMinDbm;
					le.rxMaxRfPwrHi = attr.attrRXHiLevelMaxDbm;
					le.rxInitRfPwrHi = attr.attrRXHiLevelInitDbm;
					le.rxCurRfPwrHi = le.rxInitRfPwrHi;
				}


				if (attr.attrComDevice.length() >= 4) {
					le.linkSerPort = atoi(attr.attrComDevice.c_str() + 3);
					le.linkSerBaud = attr.attrComBaud;
					le.linkSerBits = attr.attrComBits;
					le.linkSerParity = *(attr.attrComPar.c_str());
					le.linkSerStopbits = attr.attrComStop;
				}

				le.linkSerIecAddr					= attr.attrGpibAddr;

				//le.serverType						= attr.attrServerType;   // TODO
				//le.serverPort						= attr.attrServerPort;

				le.linkUsbIdVendor					= attr.attrUsbVendorID;
				le.linkUsbIdProduct					= attr.attrUsbProductID;


				/* Create list */
				g_am_InstList.push_back(le);

				/* Continue with next entry */
				it++;
			}
		}
	}

	/* Free up structure */
	cM.clear();

	return;


_fsLoadInstruments_Error:
	{
		wchar_t errMsg[256] = { 0 };

		swprintf_s(errMsg, sizeof(errMsg)>>1,  L"Error when reading config file\n\n%hs\n\n%hs\nLine: %ld\n", errMsgBuf, _fs_instrument_settings_filename, errLine);
		MessageBoxW(_hWnd, errMsg, L"Configuration error", MB_ICONERROR);
	}
}

void agentModel::confAttrClear(confAttributes_t* cA)
{
	cA->attrName.clear();
	cA->attrSection.clear();
	cA->attrType.clear();

	cA->attrTurnLeftMaxDeg			= 0.0f;
	cA->attrTurnRightMaxDeg			= 0.0f;
	cA->attrTicks360Deg				= 0UL;
	cA->attrSpeedStart				= 0.0f;
	cA->attrSpeedAccl				= 0.0f;
	cA->attrSpeedTop				= 0.0f;

	cA->attrFreqMinHz				= 0.0f;
	cA->attrFreqMaxHz				= 0.0f;
	cA->attrFreqInitHz				= 0.0f;

	cA->attrTXlevelMinDbm			= 0.0f;
	cA->attrTXlevelMaxDbm			= 0.0f;
	cA->attrTXlevelInitDbm			= 0.0f;

	cA->attrSpanMinHz				= 0.0f;
	cA->attrSpanMaxHz				= 0.0f;
	cA->attrSpanInitHz				= 0.0f;

	cA->attrRXLoLevelMinDbm			= 0.0f;
	cA->attrRXLoLevelMaxDbm			= 0.0f;
	cA->attrRXLoLevelInitDbm		= 0.0f;

	cA->attrRXHiLevelMinDbm			= 0.0f;
	cA->attrRXHiLevelMaxDbm			= 0.0f;
	cA->attrRXHiLevelInitDbm		= 0.0f;
	
	cA->attrComDevice.clear();
	cA->attrComBaud					= 0U;
	cA->attrComBits					= 0U;
	cA->attrComPar.clear();
	cA->attrComStop					= 0U;
	
	cA->attrGpibAddr				= 0U;
	
	cA->attrServerType.clear();
	cA->attrServerPort				= 0U;
	
	cA->attrUsbVendorID				= 0U;
	cA->attrUsbProductID			= 0U;
}

void agentModel::pushInstrumentDataset(map<string, confAttributes_t>* mC, string name, const confAttributes_t* cA)
{
	if (!name.empty() && cA) {
		confAttributes_t attrTo = (*mC)[name];
		confAttributes_t attrFrom = *cA;

		/* Header information */
		if (attrTo.attrName.empty() && !attrFrom.attrName.empty()) {
			attrTo.attrName = name;
		}
		if (attrTo.attrSection.empty() && !attrFrom.attrSection.empty()) {
			attrTo.attrSection = attrFrom.attrSection;
		}
		if (!attrFrom.attrType.empty()) {
			attrTo.attrType = attrFrom.attrType;
		}

		/* Rotor data */
		if (attrFrom.attrTurnLeftMaxDeg) {
			attrTo.attrTurnLeftMaxDeg = attrFrom.attrTurnLeftMaxDeg;
		}
		if (attrFrom.attrTurnRightMaxDeg) {
			attrTo.attrTurnRightMaxDeg = attrFrom.attrTurnRightMaxDeg;
		}
		if (attrFrom.attrTicks360Deg) {
			attrTo.attrTicks360Deg = attrFrom.attrTicks360Deg;
		}
		if (attrFrom.attrSpeedStart) {
			attrTo.attrSpeedStart = attrFrom.attrSpeedStart;
		}
		if (attrFrom.attrSpeedAccl) {
			attrTo.attrSpeedAccl = attrFrom.attrSpeedAccl;
		}
		if (attrFrom.attrSpeedTop) {
			attrTo.attrSpeedTop = attrFrom.attrSpeedTop;
		}

		/* TX & RX */
		if (attrFrom.attrFreqMinHz) {
			attrTo.attrFreqMinHz = attrFrom.attrFreqMinHz;
		}
		if (attrFrom.attrFreqMaxHz) {
			attrTo.attrFreqMaxHz = attrFrom.attrFreqMaxHz;
		}
		if (attrFrom.attrFreqInitHz) {
			attrTo.attrFreqInitHz = attrFrom.attrFreqInitHz;
		}

		/* TX */
		if (attrFrom.attrTXlevelMinDbm) {
			attrTo.attrTXlevelMinDbm = attrFrom.attrTXlevelMinDbm;
		}
		if (attrFrom.attrTXlevelMaxDbm) {
			attrTo.attrTXlevelMaxDbm = attrFrom.attrTXlevelMaxDbm;
		}
		if (attrFrom.attrTXlevelInitDbm) {
			attrTo.attrTXlevelInitDbm = attrFrom.attrTXlevelInitDbm;
		}

		/* RX */
		if (attrFrom.attrSpanMinHz) {
			attrTo.attrSpanMinHz = attrFrom.attrSpanMinHz;
		}
		if (attrFrom.attrSpanMaxHz) {
			attrTo.attrSpanMaxHz = attrFrom.attrSpanMaxHz;
		}
		if (attrFrom.attrSpanInitHz) {
			attrTo.attrSpanInitHz = attrFrom.attrSpanInitHz;
		}
		if (attrFrom.attrRXLoLevelMinDbm) {
			attrTo.attrRXLoLevelMinDbm = attrFrom.attrRXLoLevelMinDbm;
		}
		if (attrFrom.attrRXLoLevelMaxDbm) {
			attrTo.attrRXLoLevelMaxDbm = attrFrom.attrRXLoLevelMaxDbm;
		}
		if (attrFrom.attrRXLoLevelInitDbm) {
			attrTo.attrRXLoLevelInitDbm = attrFrom.attrRXLoLevelInitDbm;
		}
		if (attrFrom.attrRXHiLevelMinDbm) {
			attrTo.attrRXHiLevelMinDbm = attrFrom.attrRXHiLevelMinDbm;
		}
		if (attrFrom.attrRXHiLevelMaxDbm) {
			attrTo.attrRXHiLevelMaxDbm = attrFrom.attrRXHiLevelMaxDbm;
		}
		if (attrFrom.attrRXHiLevelInitDbm) {
			attrTo.attrRXHiLevelInitDbm = attrFrom.attrRXHiLevelInitDbm;
		}

		/* COM */
		if (!attrFrom.attrComDevice.empty()) {
			attrTo.attrComDevice = attrFrom.attrComDevice;
		}
		if (attrFrom.attrComBaud) {
			attrTo.attrComBaud = attrFrom.attrComBaud;
		}
		if (attrFrom.attrComBits) {
			attrTo.attrComBits = attrFrom.attrComBits;
		}
		if (!attrFrom.attrComPar.empty()) {
			attrTo.attrComPar = attrFrom.attrComPar;
		}
		if (attrFrom.attrComStop) {
			attrTo.attrComStop = attrFrom.attrComStop;
		}

		/* GPIB */
		if (attrFrom.attrGpibAddr) {
			attrTo.attrGpibAddr = attrFrom.attrGpibAddr;
		}

		/* Network  */
		if (!attrFrom.attrServerType.empty()) {
			attrTo.attrServerType = attrFrom.attrServerType;
		}
		if (attrFrom.attrServerPort) {
			attrTo.attrServerPort = attrFrom.attrServerPort;
		}

		/* USB */
		if (attrFrom.attrUsbVendorID) {
			attrTo.attrUsbVendorID = attrFrom.attrUsbVendorID;
		}
		if (attrFrom.attrUsbProductID) {
			attrTo.attrUsbProductID = attrFrom.attrUsbProductID;
		}

		/* Make map entry visible */
		(*mC)[name] = attrTo;
	}
}

void agentModel::scanInstruments(void)
{
	am_InstList_t::iterator it = g_am_InstList.begin();

	/* Iterate over all instruments and check which one responds */
	while (it != g_am_InstList.end()) {
		/* From high to low priority */
		if (instCheckUsb(it)) {
			/* Use USB connection */
			instActivateUsb(it);

		} 
		else if (instCheckCom(it)) {
			/* Use COM connection */
			instActivateCom(it);
		}
	}
}


#ifdef USE_PRELOAD_INSTRUMENTS
// TODO: remove me later!
void agentModel::preloadInstruments(void)
{
	am_InstListEntry_t entry;

	/* Rotor: Zolix SC300-1B */
	memset(&entry, 0, sizeof(entry));
	entry.listId = g_am_InstList.size();
	entry.listEntryName = string("Zolix SC300-1B");
	entry.listFunction = INST_FUNCTION_ROTOR;

	entry.actRank = 1;	// highest of Rotors
	entry.actSelected = true;
	entry.actLink = false;

	entry.linkType = LINKTYPE_SER;
	entry.linkÍdnSearch = string("SC300");

	entry.linkSerPort = 3;
	entry.linkSerBaud = CBR_19200;
	entry.linkSerBits = 8;
	entry.linkSerParity = NOPARITY;
	entry.linkSerStopbits = ONESTOPBIT;

	entry.rotInitTicksPer360deg = 288000;
	entry.rotInitTopSpeed = 20000;
	entry.rotInitAcclSpeed = 30000;
	entry.rotInitStartSpeed = 2500;

	g_am_InstList.push_back(entry);


	/* TX: R&S SMR40 */
	memset(&entry, 0, sizeof(entry));
	entry.listId = g_am_InstList.size();
	entry.listEntryName = string("R&S SMR40");
	entry.listFunction = INST_FUNCTION_TX;

	entry.actRank = 1;	// highest of TX
	entry.actSelected = true;
	entry.actLink = false;

	entry.linkType = LINKTYPE_USB;
	entry.linkÍdnSearch = string("SMR40");

	entry.linkUsbIdVendor = 0x0aad;
	entry.linkUsbIdProduct = 0xffff;  // TODO: enter correct value

	entry.txInitRfOn = false;
	entry.txInitRfQrg = 18e+9;
	entry.txInitRfPwr = 0.;
	entry.txMinRfQrg = 100e+3;
	entry.txMinRfPwr = -30.;
	entry.txMaxRfQrg = 40e+9;
	entry.txMaxRfPwr = 10.;

	g_am_InstList.push_back(entry);


	/* RX: R&S FSEK20 */
	memset(&entry, 0, sizeof(entry));
	entry.listId = g_am_InstList.size();
	entry.listEntryName = string("R&S FSEK20");
	entry.listFunction = INST_FUNCTION_RX;

	entry.actRank = 1;	// highest of RX
	entry.actSelected = true;
	entry.actLink = false;

	entry.linkType = LINKTYPE_IEC_VIA_SER;
	entry.linkÍdnSearch = string("FSEK 20");

	entry.linkSerPort = 4;
	entry.linkSerBaud = CBR_19200;
	entry.linkSerBits = 8;
	entry.linkSerParity = NOPARITY;
	entry.linkSerStopbits = ONESTOPBIT;

	entry.linkSerIecAddr = 20;

	entry.rxInitRfQrg = 18e+9;
	entry.rxInitRfSpan = 10e+6;
	entry.rxInitRfPwrLo = -60.;
	entry.rxInitRfPwrDynamic = 60.;
	entry.rxMinRfQrg = 1e+6;
	entry.rxMinRfSpan = 10e+3;
	entry.rxMinRfPwrLo = -60.;
	entry.rxMinRfPwrDynamic = 10.;
	entry.rxMaxRfQrg = 40e+9;
	entry.rxMaxRfSpan = 40e+9;
	entry.rxMaxRfPwrLo = 0.;
	entry.rxMaxRfPwrDynamic = 60.;

	g_am_InstList.push_back(entry);
}
#endif


bool agentModel::instCheckUsb(am_InstList_t::iterator it)
{
	// TODO
	//xxx
	return false;
}


bool agentModel::instCheckCom(am_InstList_t::iterator it)
{
	return false;
}

void agentModel::instActivateUsb(am_InstList_t::iterator it)
{

}

void agentModel::instActivateCom(am_InstList_t::iterator it)
{

}
