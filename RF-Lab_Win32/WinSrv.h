#pragma once

// D2Draw-style
#include <d2d1.h>


enum C_THREAD_COM_CMDS { 
	C_THREAD_COM_END
};


class CThreadCom:CWinThread
{
private:
	UINT state;

public:
	void send(UINT cmd);

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

	CThreadCom              *pThreadCom = NULL;


public:
	WinSrv();
	~WinSrv();

private:
	void threadsStart();
	void threadsStop();

	static UINT threadCom(LPVOID pParam);

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

