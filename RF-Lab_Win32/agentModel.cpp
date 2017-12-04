#include "stdafx.h"

#include "resource.h"
#include "CommCtrl.h"
#include "WinSrv.h"

#include "agentModel_InstList.h"
#include "agentModelPattern.h"

#include "externals.h"

#include "agentModel.h"


template <class T>  void SafeReleaseDelete(T **ppT)
{
	if (*ppT)
	{
		//(*ppT)->Release();
		*ppT = nullptr;
	}
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

	/* Set up list of instruments */
	{
		g_am_InstList_locked = true;

		#if 0
			FsLoadInstruments(C_FS_INSTRUMENTS_FILENAME_DEFAULT);
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
