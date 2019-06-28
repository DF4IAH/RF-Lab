#pragma once

// D2Draw-style
#include <d2d1.h>

#include "agentModel.h"
#include "agentCom.h"


class WinSrv
{
public:
	typedef struct PresentationData_s {
		char                             statusLine[100];
	} PresentationData_t;

	const int							 StatusBarParts = 3;

	typedef struct InstMenuItemAry {
		HMENU							hMenu;
		int								idx;
	} InstMenuItemAry_t;


private:
	HWND								 hWnd;
	HWND								 hWndStatus;

	ID2D1Factory						*pFactory;
	ID2D1HwndRenderTarget				*pRenderTarget;
	ID2D1SolidColorBrush				*pBrush;
	D2D1_SIZE_F							 _size;

	PresentationData_t					 _PD;

	unbounded_buffer<agentModelReq>		 _ub_agtModel_req;
	unbounded_buffer<agentModelRsp>		 _ob_agtModel_rsp;
	//overwrite_buffer<agentModelRsp>	 _ob_agtModel_rsp;
	agentModel							*pAgtModel;

	bool								 _winExitReceived;
	bool								 _ready;


public:
					WinSrv();
					~WinSrv();

	bool			isReady();
	LRESULT			setWindow(HWND hWnd);
	void			paint();
	void			resize();


private:
	void			calculateLayout();
	HRESULT			createGraphicsResources();
	void			discardGraphicsResources();
	void			threadsStart();
	void			threadsStop();
	void			wmCmd(HWND hWnd, int wmId, LPVOID arg = nullptr);
	HWND			DoCreateStatusBar(HWND hwndParent, HMENU idStatus, HINSTANCE	hinst, int cParts);
	void			OnStatusbarSize(HWND hWndStatus, int cParts, RECT* size);
	bool			ready();


public:
	static void		srvStart();
	static void		srvStop();
	static void		srvWinExit();
	static LRESULT	srvSetWindow(HWND hWnd);
	static bool		srvReady();
	static void		srvPaint();
	static void		srvResize();
	static void		srvWmCmd(HWND hWnd, int wmId, LPVOID arg = nullptr);

	void			reportStatus(LPVOID modelVariant, LPVOID modelStatus, LPVOID modelInfo);

	int				instMenuGetItem(InstMenuItemAry_t imiAry[], UINT winID, const wchar_t* caption);
	void			instUpdateConnectedInstruments(void);
	void			instActivateMenuItem(UINT winID, BOOL uEnable);

};
