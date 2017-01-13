#pragma once

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>

using namespace concurrency;
using namespace std;


enum C_AGENTMODELMSG_ENUM {
	C_AGENTMODELMSG_END
};



class agentModel : public agent
{
private:
	ISource<int>&		_source;
	ITarget<wstring>&	_target;


public:
	explicit agentModel(ISource<int>& source, ITarget<wstring>& target);

protected:
	void run();

};

