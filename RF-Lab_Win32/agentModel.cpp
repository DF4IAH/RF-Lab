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


#if 0
agentModel::agentModel()
	: _src(nullptr)
	, _tgt(nullptr)
	, _am_variant(AGENT_MODEL_NONE)
	, _curModel(nullptr)
{
	g_am = this;
}
#endif

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

void agentModel::wmCmd(int wmId)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->wmCmd(wmId);
	}
}
