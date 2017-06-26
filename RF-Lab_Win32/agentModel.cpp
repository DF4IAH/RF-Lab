#include "stdafx.h"
#include "agentModel.h"
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


// global
agentModel *g_am = nullptr;


agentModel::agentModel(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, AGENT_MODELS am_variant, AGENT_ALL_SIMUMODE_t mode)
				 : _src(src)
				 , _tgt(tgt)
				 , _am_variant(am_variant)
				 , _simuMode(mode)
{
	g_am = this;

	switch (am_variant) {
	case AGENT_MODEL_PATTERN:
		_curModel = new agentModelPattern(src, tgt, mode);
		break;

	case AGENT_MODEL_NONE:
	default:
		_curModel = nullptr;
	}
}

agentModel::~agentModel(void)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->Release();
		delete _curModel;
	}
	g_am = nullptr;
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

void agentModel::runProcess(int processID)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->runProcess(processID);
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
