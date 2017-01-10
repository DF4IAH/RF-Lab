#pragma once

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>

using namespace concurrency;
using namespace std;


// D2Draw-style
#include <d2d1.h>



enum C_AGENTMSG_ENUM { 
	C_AGENTMSG_END
};


class agentModel : public agent
{
public:
	explicit agentModel(ISource<int>& source, ITarget<wstring>& target)
		: _source(source)
		, _target(target)
	{
	}

protected:
	void run()
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

private:
	ISource<int>&		_source;
	ITarget<wstring>&	_target;

};

class agentCom : public agent
{
public:
	explicit agentCom(ISource<wstring>& source, ITarget<int>& target)
		: _source(source)
		, _target(target)
	{
	}

protected:
	void run()
	{
		// Read the request.
		wstring request = receive(_source);

		wstringstream ss;
		ss << L"agent2: received '" << request << L"'." << endl;
		wcout << ss.str();

		// Send the response.
		ss = wstringstream();
		ss << L"agent2: sending response..." << endl;
		wcout << ss.str();

		send(_target, 42);

		// Move the agent to the finished state.
		done();
	}

private:
	ISource<wstring>& _source;
	ITarget<int>& _target;
};


class WinSrv
{
private:
	HWND					 hWnd;

	ID2D1Factory            *pFactory;
	ID2D1HwndRenderTarget   *pRenderTarget;
	ID2D1SolidColorBrush    *pBrush;

	D2D1_SIZE_F				 size;

	bool					 ready;


public:
	WinSrv();
	~WinSrv();

private:
	void threadsStart();
	void threadsStop();

public:
	bool isReady();

	LRESULT setWindow(HWND hWnd);
	void paint();
	void resize();

private:
	void calculateLayout();
	HRESULT createGraphicsResources();
	void discardGraphicsResources();

public:
	static void srvStart();
	static void srvStop();
	static LRESULT srvSetWindow(HWND hWnd);
	static void srvPaint();
	static void srvResize();
};

