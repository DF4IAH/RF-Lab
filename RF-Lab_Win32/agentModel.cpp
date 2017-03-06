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


agentModel::agentModel(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, AGENT_MODELS am_variant)
				 : _src(src)
				 , _tgt(tgt)
				 , _am_variant(am_variant)
{
	g_am = this;

	switch (am_variant) {
	case AGENT_MODEL_PATTERN:
		_curModel = new agentModelPattern(src, tgt);
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


/* agentModelPattern - Rotor */

int agentModel::requestPos(void)
{
	if (g_am && g_am->_curModel) {
		return g_am->_curModel->requestPos();
	}
	return MAXINT16;
}

void agentModel::setLastTickPos(int tickPos)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->setLastTickPos(tickPos);
	}
}

int agentModel::getLastTickPos(void)
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

void agentModel::setRxFrequencyValue(double value)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->setRxFrequencyValue(value);
	}
}

void agentModel::setRxSpanValue(double value)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->setRxSpanValue(value);
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
