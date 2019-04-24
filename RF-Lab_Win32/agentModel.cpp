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

		#if 1
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
	map< string, confAttributes_t > m;
	string							attrName;
	string							attrType;
	float							attrTurnLeftMaxDeg		= 0.0f;
	float							attrTurnRightMaxDeg		= 0.0f;
	uint32_t						attrTicks360Deg			= 0UL;
	float							attrSpeedStart			= 0.0f;
	float							attrSpeedAccl			= 0.0f;
	float							attrSpeedTop			= 0.0f;
	float							attrFreqMinHz			= 0.0f;
	float							attrFreqMaxHz			= 0.0f;
	float							attrFreqMinDbm			= 0.0f;
	float							attrFreqMaxDbm			= 0.0f;
	string							attrDevice;
	uint16_t						attrComBaud				= 0U;
	uint8_t							attrComBits				= 0U;
	string							attrComPar;
	uint8_t							attrComStop				= 0U;
	uint8_t							attrGpibAddr			= 0U;
	string							attrServerType;
	uint16_t						attrServerPort			= 0U;
	uint16_t						attrUsbVendorID			= 0U;
	uint16_t						attrUsbProductID		= 0U;
	confAttributes_t				cA;

	/* Load config file */
	{
		FILE* fh = NULL;
		errno_t err;

		err = fopen_s(&fh, filename, "rb");
		if (!errno && fh) {
			char variant = '-';
			char lineBuf[256];

			while (1) {
				char* p = fgets(lineBuf, sizeof(lineBuf) - 1, fh);
				if (!p) {
					/* End of file */
					if (variant != '-') {
						//pushInstrumentDataset();
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
						pushInstrumentDataset(&m, attrName, &cA);

						/* Start a new set of attributes */
						memset(&cA, 0, sizeof(cA));
						attrName.clear();
					}

					/* Types */
					if (!_strnicmp(p, "[INSTRUMENT]", 12)) {
						variant = 'I';
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
					else if (!_strnicmp(p, "[INTERFACE]", 11)) {
						variant = 'F';
					}
					else {
						/* File format error */
						const char Msg[] = "Unknown section type in brackets";
						strncpy_s(errMsgBuf, sizeof(errMsgBuf), Msg, strlen(Msg));
						errLine = lineCtr;
						goto _fsLoadInstruments_Error;
					}
				}

				/* INTERFACE attributes */
				else if ((variant == 'I') && !_strnicmp(p, "Name=", 5)) {
					attrName.assign(p + 5, p + lineLen);
				}
				else if ((variant == 'I') && !_strnicmp(p, "Type=", 5)) {
					attrType.assign(p + 5, p + lineLen);
				}
				else if ((variant == 'I') && !_strnicmp(p, "Turn_left_max_deg=", 18)) {
					try {
						attrTurnLeftMaxDeg = stof(string(p + 18, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Turn_right_max_deg=", 19)) {
					try {
						attrTurnRightMaxDeg = stof(string(p + 19, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Ticks_360deg=", 13)) {
					try {
						attrTicks360Deg = stoi(string(p + 13, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Speed_Start=", 12)) {
					try {
						attrSpeedStart = stof(string(p + 12, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Speed_Accl=", 11)) {
					try {
						attrSpeedAccl = stof(string(p + 11, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Speed_Top=", 10)) {
					try {
						attrSpeedTop = stof(string(p + 10, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Freq_min_Hz=", 12)) {
					try {
						attrFreqMinHz = stof(string(p + 12, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Freq_max_Hz=", 12)) {
					try {
						attrFreqMaxHz = stof(string(p + 12, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Amp_min_dBm=", 12)) {
					try {
						attrFreqMinDbm = stof(string(p + 12, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'I') && !_strnicmp(p, "Amp_max_dBm=", 12)) {
					try {
						attrFreqMaxDbm = stof(string(p + 12, p + lineLen));
					}
					catch (...) {
					}
				}

				else if ((variant == 'C') && !_strnicmp(p, "Name=", 5)) {
					attrName.assign(p + 5, p + lineLen);
				}
				else if ((variant == 'C') && !_strnicmp(p, "Device=", 7)) {
					attrDevice.assign(p + 7, p + lineLen);
				}
				else if ((variant == 'C') && !_strnicmp(p, "Baud=", 5)) {
					try {
						attrComBaud = stoi(string(p + 5, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'C') && !_strnicmp(p, "Bits=", 5)) {
					try {
						attrComBits = stoi(string(p + 5, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'C') && !_strnicmp(p, "Par=", 4)) {
					attrComPar.assign(p + 4, p + lineLen);
				}
				else if ((variant == 'C') && !_strnicmp(p, "Stop=", 5)) {
					try {
						attrComStop = stoi(string(p + 5, p + lineLen));
					}
					catch (...) {
					}
				}

				else if ((variant == 'U') && !_strnicmp(p, "Name=", 5)) {
					attrName.assign(p + 5, p + lineLen);
				}
				else if ((variant == 'U') && !_strnicmp(p, "Vendor_ID=", 10)) {
					try {
						attrUsbVendorID = stoi(string(p + 10, p + lineLen));
					}
					catch (...) {
					}
				}
				else if ((variant == 'U') && !_strnicmp(p, "Product_ID=", 11)) {
					try {
						attrUsbProductID = stoi(string(p + 11, p + lineLen));
					}
					catch (...) {
					}
				}

				else if ((variant == 'G') && !_strnicmp(p, "Name=", 5)) {
					attrName.assign(p + 5, p + lineLen);
				}
				else if ((variant == 'G') && !_strnicmp(p, "Addr=", 5)) {
					try {
						attrGpibAddr = stoi(string(p + 5, p + lineLen));
					}
					catch (...) {
					}
				}

				else if ((variant == 'F') && !_strnicmp(p, "Name=", 5)) {
					attrName.assign(p + 5, p + lineLen);
				}
				else if ((variant == 'F') && !_strnicmp(p, "ServerType=", 11)) {
					attrServerType.assign(p + 11, p + lineLen);
				}
				else if ((variant == 'F') && !_strnicmp(p, "ServerPort=", 11)) {
					try {
						attrServerPort = stoi(string(p + 11, p + lineLen));
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

	}

	return;


_fsLoadInstruments_Error:
	{
		wchar_t errMsg[256] = { 0 };

		swprintf_s(errMsg, sizeof(errMsg)>>1,  L"Error when reading config file\n\n%hs\n\n%hs\nLine: %ld\n", errMsgBuf, _fs_instrument_settings_filename, errLine);
		MessageBoxW(_hWnd, errMsg, L"Configuration error", MB_ICONERROR);
	}
}

void agentModel::pushInstrumentDataset(map< string, confAttributes_t >* m, const string name, const confAttributes_t* cA)
{
	if (m && cA) {
		//m->insert_or_assign(name, cA);
	}
}

void agentModel::scanInstruments(void)
{

}


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
