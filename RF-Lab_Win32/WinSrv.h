#pragma once

// D2Draw-style
#include <d2d1.h>

#include "agentModel.h"
#include "agentCom.h"


enum FILETYPE_ENUM {

	FILETYPE_UNKNOWN		= 0,
	FILETYPE_TXT,
	FILETYPE_CSV,
	FILETYPE_S1P,
	FILETYPE_S2P,
	FILETYPE_S3P,
	FILETYPE_S4P,

};

typedef struct {

  //AGENT_ALL_SIMUMODE_t				measSimuMode;
	MeasData*							measData;

	/* Pretty print entities - Frequency */
	int									measTxFreqHz_int;
	int									measTxFreqHz_frac3;
	wchar_t								meastxFreqHz_exp;

	/* Pretty print entities - Power */
	int									measTxPowerDbm_int;
	int									measTxPowerDbm_frac3;

} MEASTYPE;


class WinSrv
{
public:
	typedef struct PresentationData_s {
		char                             statusLine[100];
	} PresentationData_t;

	const int							 _StatusBarParts = 3;

	typedef struct InstMenuItemAry {
		HMENU							 hMenu;
		int								 idx;
	} InstMenuItemAry_t;

	typedef struct MenuInfo {
		bool							 rotorEnabled;
		bool							 rfGenEnabled;
		bool							 specEnabled;
		bool							 modelPatternAct;
	} MenuInfo_t;

	MenuInfo_t							 _menuInfo;


private:
	HWND								 _hWnd;
	HWND								 _hWndStatus;

	ID2D1Factory						*_pFactory;
	ID2D1HwndRenderTarget				*_pRenderTarget;
	ID2D1SolidColorBrush				*_pBrush;
	D2D1_SIZE_F							 _size;

	PresentationData_t					 _PD;

	unbounded_buffer<agentModelReq>		 _ub_agtModel_req;
	unbounded_buffer<agentModelRsp>		 _ob_agtModel_rsp;
	//overwrite_buffer<agentModelRsp>	 _ob_agtModel_rsp;
	agentModel							*_pAgtModel;

	bool								 _winExitReceived;
	bool								 _ready;

	wchar_t								 _cLastFilePath[MAX_PATH];
	wchar_t								 _cLastFileName[MAX_PATH];


public:
					WinSrv();
					~WinSrv();

	bool			isReady();
	LRESULT			setWindow(HWND _hWnd);
	void			paint();
	void			resize();


private:
	void			calculateLayout();
	HRESULT			createGraphicsResources();
	void			discardGraphicsResources();
	void			threadsStart();
	void			threadsStop();
	void			wmCmd(HWND _hWnd, int wmId, LPVOID arg = nullptr);
	HWND			DoCreateStatusBar(HWND hwndParent, HMENU idStatus, HINSTANCE	hinst, int cParts);
	void			OnStatusbarSize(HWND _hWndStatus, int cParts, RECT* size);
	bool			ready();
	bool			checkForModelPattern(HMENU hMenuAnst);


public:
	static void		srvStart();
	static void		srvStop();
	static void		srvWinExit();
	static LRESULT	srvSetWindow(HWND _hWnd);
	static bool		srvReady();
	static void		srvPaint();
	static void		srvResize();
	static void		srvWmCmd(HWND _hWnd, int wmId, LPVOID arg = nullptr);

	void			reportStatus(LPVOID modelVariant, LPVOID modelStatus, LPVOID modelInfo);

	bool			instMenuGetItem(HMENU* hMenuSub, int* menuIdx, HMENU hMenuAnst, wchar_t* caption);
	void			instUpdateConnectedInstruments(void);
	void			instActivateMenuItem(UINT winID, bool uEnable);

	static FILETYPE_ENUM getFileType(wchar_t* filename);
	static void		saveCurrentDataset(void);

	static wchar_t*	getLastFilePath(void);
	static wchar_t*	getLastFileName(void);
	static void		setLastFilePath(wchar_t* s);
	static void     getMeasType(MEASTYPE* measType);

};
