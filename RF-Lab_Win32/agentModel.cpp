#include "stdafx.h"
#include "agentModel.h"


agentModel::agentModel(ISource<int>& source, ITarget<wstring>& target)
	: _source(source)
	, _target(target)
{
}

void agentModel::run()
{
	// Send the request.
	wstringstream ss;
	ss << L"agent1: sending request..." << endl;
	wcout << ss.str();

	send(_target, wstring(L"request"));

	// Read the response.
	int response = receive(_source);

	ss = wstringstream();
	ss << L"agent1: received '" << response << L"'." << endl;
	wcout << ss.str();

	// Move the agent to the finished state.
	done();
}
