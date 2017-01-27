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



agentModel::agentModel()
	: _src(nullptr)
	, _tgt(nullptr)
{
	am = this;
	_curModel = nullptr;
}

agentModel::agentModel(ISource<agentModelReq> *src, ITarget<agentModelRsp> *tgt)
				 : _src(src)
				 , _tgt(tgt)
{
	am = this;

	/* default model to start: ModelPattern */
	_curModel = new agentModelPattern();
}

agentModel::~agentModel(void)
{
	if (am && am->_curModel) {
		am->_curModel->Release();
		delete _curModel;
	}
}

// to be overwritten
void agentModel::run()
{
}

bool agentModel::isRunning()
{
	if (am && am->_curModel) {
		return am->_curModel->isRunning();
	}
	else {
		return false;
	}
}


void agentModel::Release()
{
	if (am && am->_curModel) {
		return am->_curModel->Release();
	}
}


bool agentModel::shutdown()
{
	if (am && am->_curModel) {
		return am->_curModel->shutdown();
	} else {
		return false;
	}
}

void agentModel::wmCmd(int wmId)
{
	if (am && am->_curModel) {
		am->_curModel->wmCmd(wmId);
	}
}
