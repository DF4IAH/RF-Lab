#pragma once

// D2Draw-style
#include <d2d1.h>



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

	bool isReady();
	LRESULT setWindow(HWND hWnd);
	void paint();
	void resize();

private:
	void calculateLayout();
	HRESULT createGraphicsResources();
	void discardGraphicsResources();
	void threadsStart();
	void threadsStop();

public:
	static void srvStart();
	static void srvStop();
	static LRESULT srvSetWindow(HWND hWnd);
	static void srvPaint();
	static void srvResize();
};
