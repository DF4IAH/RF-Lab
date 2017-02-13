#include "stdafx.h"

#include "agentModelVariant.h"


agentModelVariant::agentModelVariant(void)
				 : _running(FALSE)
				 ,_runState(C_MODEL_RUNSTATES_NOOP)
				 , _done(FALSE)
{
}

agentModelVariant::~agentModelVariant(void)
{
}


void agentModelVariant::run(void)
{
}


bool agentModelVariant::isRunning(void)
{
	return false;
}

void agentModelVariant::Release(void)
{
}

bool agentModelVariant::shutdown(void)
{
	return false;
}

void agentModelVariant::wmCmd(int wmId, LPVOID arg)
{
}

int agentModelVariant::requestPos(void)
{
	return 0;
}

void agentModelVariant::setLastTickPos(int tickPos)
{
}

int agentModelVariant::getLastTickPos(void)
{
	return 0;
}
