#pragma once

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>

using namespace concurrency;
using namespace std;


enum C_AGENTCOMMSG_ENUM {
	C_AGENTCOMMSG_END
};



class agentCom : public agent
{
private:
	ISource<wstring>& _source;
	ITarget<int>& _target;


public:
	explicit agentCom(ISource<wstring>& source, ITarget<int>& target);

protected:
	void run();

};
