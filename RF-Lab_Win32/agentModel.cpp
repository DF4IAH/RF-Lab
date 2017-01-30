#include <typeinfo>

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


agentModel::agentModel()
	: _src(nullptr)
	, _tgt(nullptr)
	, _curModel(nullptr)
{
	// no sub-agentModels to be assigned
	if (!g_am) {
		g_am = this;
	}
}

agentModel::agentModel(ISource<agentModelReq> *src, ITarget<agentModelRsp> *tgt)
				 : _src(src)
				 , _tgt(tgt)
				 , _curModel(nullptr)
{
	// no sub-agentModels to be assigned
	if (!g_am) {
		g_am = this;
	}
}

agentModel::~agentModel(void)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->Release();
		delete _curModel; _curModel = nullptr;
	}
}


// to be overwritten
void agentModel::run(void)
{
	if (g_am && g_am->_curModel) {
		g_am->_curModel->run();
	}
}

void agentModel::prepare(agentModel *am)
{
	if (g_am) {
		if (!g_am->_curModel) {
			g_am->_curModel = am;
		}
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
