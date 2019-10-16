/* Windows */
#include "stdafx.h"
#include "resource.h"

/* C headers */
#include <stdio.h>
#include <string.h>

/* COM-style */
#include <shobjidl.h>

/* STL-enhancements */
#include <atlbase.h>

/* D2Draw-style */
//#include <d2d1.h>
#pragma comment(lib, "d2d1")

/* sub-module agents */
#include "agentModel.h"
#include "agentModelPattern.h"
#include "agentCom.h"

/* Application */
#include "externals.h"

#include "WinSrv.h"


WinSrv* g_instance = nullptr;
extern HINSTANCE g_hInst;                                // Aktuelle Instanz


template <class T>  void SafeReleaseDelete(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = nullptr;
	}
}


class DPIScale
{
	static float scaleX;
	static float scaleY;

public:
	static void Initialize(ID2D1Factory *pFactory)
	{
		FLOAT dpiX, dpiY;
		pFactory->GetDesktopDpi(&dpiX, &dpiY);
		scaleX = dpiX / 96.0f;
		scaleY = dpiY / 96.0f;
	}

	template <typename T>
	static D2D1_POINT_2F PixelsToDips(T x, T y)
	{
		return D2D1::Point2F(static_cast<float>(x) / scaleX, static_cast<float>(y) / scaleY);
	}
};


float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;
WinSrv* g_winSrv = nullptr;


WinSrv::WinSrv() : hWnd(nullptr)
				 , hWndStatus(nullptr)
				 , pFactory(nullptr)
				 , pRenderTarget(nullptr)
				 , pBrush(nullptr)
				 , _size(D2D1_SIZE_F())
				 , _PD({ 0 })
				 , _menuInfo({ FALSE, FALSE, FALSE })
				 , pAgtModel(nullptr)
				 , _winExitReceived(FALSE)
				 , _ready(FALSE)
				 , cLastFilePath(L"C:\\Users\\Labor\\Downloads")
				 , cLastFileName(L"Ant-Richtdiagramm.csv")
{
	g_winSrv = this;

	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (SUCCEEDED(hr)) {
		/* Start model */
		threadsStart();
	}
}


WinSrv::~WinSrv()
{
	//SafeRelease(&hwndStatus);

	SafeReleaseDelete(&pBrush);
	SafeReleaseDelete(&pRenderTarget);
	SafeReleaseDelete(&pFactory);

	CoUninitialize();

	CloseHandle(g_InstListMutex);
	g_InstListMutex = nullptr;

	g_winSrv = nullptr;
}


void WinSrv::threadsStart()
{
	// start the master model with the antenna pattern model variant
	pAgtModel  = new agentModel(
		&_ub_agtModel_req,
		&_ob_agtModel_rsp,
		this,
		hWnd,
		agentModel::AGENT_MODEL_PATTERN,
//		(AGENT_ALL_SIMUMODE_t) (AGENT_ALL_SIMUMODE_NO_RX | AGENT_ALL_SIMUMODE_NO_TX | AGENT_ALL_SIMUMODE_RUN_BARGRAPH)
//		(AGENT_ALL_SIMUMODE_t) (AGENT_ALL_SIMUMODE_NO_TX | AGENT_ALL_SIMUMODE_RUN_BARGRAPH)
		(AGENT_ALL_SIMUMODE_t) (                                                      AGENT_ALL_SIMUMODE_RUN_BARGRAPH)
	);

	// starting agent --> Execution follows: AgentModel::run() (Line 87) --> AgentModelPattern::run() (Line 179)
	pAgtModel->start();
}

void WinSrv::threadsStop()
{
	/* Shutdown the model */
	SafeReleaseDelete(&pAgtModel);
}


void WinSrv::srvWinExit()
{
	if (g_instance) {
		g_instance->_winExitReceived = TRUE;
		g_instance->threadsStop();
	}
}


bool WinSrv::isReady()
{
	return _ready ?  TRUE : FALSE;
}


LRESULT WinSrv::setWindow(HWND hWnd)
{
	_ready = FALSE;

	this->hWnd = hWnd;
	if (this->hWnd) {
		if (SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
		{
			if (SUCCEEDED(createGraphicsResources())) {
				RECT rc;

				/* Get initial window size */
				GetClientRect(hWnd, &rc);

				/* Expand to max size due to a StatusBar problem */
				ShowWindow(hWnd, SW_MAXIMIZE);

				/* Create the status bar with 3 equal spaced separations */
				this->hWndStatus = DoCreateStatusBar(this->hWnd, (HMENU)63, g_hInst, StatusBarParts);
				if (this->hWndStatus) {
					/* Resize back to the size before */
					SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, rc.right, rc.bottom, 0);

					_ready = TRUE;
					return 0;
				}
			}

			// more...   @see  https://msdn.microsoft.com/de-de/library/windows/desktop/dd756692(v=vs.85).aspx
		}
	}
	return -1;  // Fail CreateWindowEx.
}


void WinSrv::paint()
{
	HRESULT hr = createGraphicsResources();
	if (SUCCEEDED(hr))
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		// COM-style
		//FillRect(hdc, &ps->rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

		// COM-style, STL-enhanced
		//CComPtr<IFileOpenDialog> pFileOpen;
		//hr = pFileOpen.CoCreateInstance(__uuidof(FileOpenDialog));

		// D2Draw-style
		pRenderTarget->BeginDraw();

		pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Wheat));
		//pRenderTarget->DrawLine(D2D1::Point2F(0, 0), D2D1::Point2F(_size.width, _size.height), pBrush);
		//ClientToScreen(hWnd, &pt);

		//pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
		//pRenderTarget->DrawTextW(L"RF-Lab", 6, xxx, D2D1::RectF(0, 0, 100, 100), pBrush);

		hr = pRenderTarget->EndDraw();
		if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
		{
			discardGraphicsResources();
		}

		EndPaint(hWnd, &ps);
	}
}


void WinSrv::resize()
{
	if (pRenderTarget != NULL)
	{
		RECT rc;
		GetClientRect(hWnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		pRenderTarget->Resize(size);
		OnStatusbarSize(this->hWndStatus, StatusBarParts, &rc);
		calculateLayout();
		InvalidateRect(hWnd, NULL, FALSE);
	}
}


void WinSrv::calculateLayout()
{
	if (pRenderTarget != NULL)
	{
		_size = D2D1_SIZE_F(pRenderTarget->GetSize());
		//const float x = size.width / 2;
		//const float y = size.height / 2;
		//const float radius = min(x, y);
		//ellipse = D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius);
	}
}


HRESULT WinSrv::createGraphicsResources()
{
	HRESULT hr = S_OK;

	if (!pRenderTarget)
	{
		RECT rc;
		GetClientRect(hWnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		hr = pFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(hWnd, size),
			&pRenderTarget);
		if (SUCCEEDED(hr))
		{
			const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 0);
			hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);
			if (SUCCEEDED(hr))
			{
				calculateLayout();
			}
		}
	}
	return hr;
}


void WinSrv::discardGraphicsResources()
{
	SafeReleaseDelete(&pBrush);
	SafeReleaseDelete(&pRenderTarget);
}

void WinSrv::wmCmd(HWND hWnd, int wmId, LPVOID arg)
{
	/* Clear hints about selected device types */
	if (wmId == ID_INSTRUMENTEN_DISCONNECT) {
		_menuInfo.rotorEnabled = false;
		_menuInfo.rfGenEnabled = false;
		_menuInfo.specEnabled = false;
	}

	if (pAgtModel) {
		pAgtModel->wmCmd(wmId, arg);
	}
}


// Description: 
//   Creates a status bar and divides it into the specified number of parts.
// Parameters:
//   hwndParent - parent window for the status bar.
//   idStatus - child window identifier of the status bar.
//   hinst - handle to the application instance.
//   cParts - number of parts into which to divide the status bar.
// Returns:
//   The handle to the status bar.
//
HWND WinSrv::DoCreateStatusBar(HWND hwndParent, HMENU idStatus, HINSTANCE	hinst, int cParts)
{
	HWND hWndStatus;
	RECT rcClient;
	HLOCAL hloc;
	PINT paParts;
	int i, nWidth;

	/* Ensure that the common control DLL is loaded. */
	//InitCommonControls();

	/* Create the status bar. */
	hWndStatus = CreateWindowEx(
		0,                       // no extended styles
		STATUSCLASSNAME,         // name of status bar class
		(PCTSTR)NULL,            // no text when first created
		SBARS_SIZEGRIP |         // includes a sizing grip
		WS_CHILD | WS_VISIBLE,   // creates a visible child window
		0, 0, 0, 0,              // ignores size and position
		hwndParent,              // handle to parent window
		(HMENU)idStatus,         // child window identifier
		hinst,                   // handle to application instance
		NULL);                   // no window creation data

								 // Get the coordinates of the parent window's client area.
	GetClientRect(hwndParent, &rcClient);

	/* Allocate an array for holding the right edge coordinates. */
	hloc = LocalAlloc(LHND, sizeof(int) * cParts);
	paParts = (PINT)LocalLock(hloc);

	/* Calculate the right edge coordinate for each part, and
	 * copy the coordinates to the array. 
	 */
	nWidth = rcClient.right / cParts;
	int rightEdge = nWidth;
	for (i = 0; i < cParts; i++) {
		paParts[i] = rightEdge;
		rightEdge += nWidth;
	}

	/* Tell the status bar to create the window parts. */
	SendMessage(hWndStatus, SB_SETPARTS, (WPARAM)cParts, (LPARAM)paParts);

	/* Status line information */
	SendMessage(hWndStatus, SB_SETTEXT, (WPARAM)0x0000, (LPARAM)L"Status 1");
	SendMessage(hWndStatus, SB_SETTEXT, (WPARAM)0x0001, (LPARAM)L"Status 2");
	SendMessage(hWndStatus, SB_SETTEXT, (WPARAM)0x0002, (LPARAM)L"Status 3");

	/* Free the array, and return. */
	LocalUnlock(hloc);
	LocalFree(hloc);
	return hWndStatus;
}

void WinSrv::OnStatusbarSize(HWND hWndStatus, int cParts, RECT* size)
{
	HLOCAL hloc;
	PINT paParts;
	int i, nWidth;

	// Allocate an array for holding the right edge coordinates.
	hloc = LocalAlloc(LHND, sizeof(int) * cParts);
	paParts = (PINT)LocalLock(hloc);

	// Calculate the right edge coordinate for each part, and
	// copy the coordinates to the array.
	nWidth = size->right / cParts;
	int rightEdge = nWidth;
	for (i = 0; i < cParts; i++) {
		paParts[i] = rightEdge;
		rightEdge += nWidth;
	}

	// Tell the status bar to create the window parts.
	SendMessage(hWndStatus, SB_SETPARTS, (WPARAM)cParts, (LPARAM)paParts);
	SendMessage(hWndStatus, WM_SIZE, 0, 0);

	// Free the array, and return.
	LocalUnlock(hloc);
	LocalFree(hloc);
}

// Ist wahr sobald WinSrv vollständig betriebsbereit ist
bool WinSrv::ready()
{
	if (hWnd && hWndStatus) {
		return true;
	}
	return false;
}


// Statische Methoden folgen

// Startet den Visualisierungsserver an
void WinSrv::srvStart()
{
	if (!g_instance) {
		g_instance = new WinSrv();
	}
}

// Hält den Visualisierungsserver an
void WinSrv::srvStop()
{
	if (g_instance) {
		srvWinExit();

		delete g_instance;
		g_instance = nullptr;
	}
}

// Aktualisiert Window-Instanzenhandle
LRESULT WinSrv::srvSetWindow(HWND hWnd)
{
	if (g_instance) {
		return g_instance->setWindow(hWnd);
	}

	return -1;  // non-valid invocation
}

// Ist wahr sobald WinSrv vollständig betriebsbereit ist
bool WinSrv::srvReady()
{
	if (g_instance) {
		return g_instance->ready();
	}

	return false;
}

// Zeichen-Code aufrufen
void WinSrv::srvPaint()
{
	if (g_instance && g_instance->isReady()) {
		g_instance->paint();
	}
}

// Zeichen-Code aufrufen
void WinSrv::srvResize()
{
	if (g_instance && g_instance->isReady()) {
		g_instance->resize();
	}
}

// WM-Command verarbeiten
void WinSrv::srvWmCmd(HWND hWnd, int wmId, LPVOID arg)
{
	if (g_instance && g_instance->isReady()) {
		g_instance->wmCmd(hWnd, wmId, arg);
	}
}

void WinSrv::reportStatus(LPVOID modelVariant, LPVOID modelStatus, LPVOID modelInfo)
{
	if (modelVariant) {
		SendMessage(this->hWndStatus, SB_SETTEXT, (WPARAM)0x0000, (LPARAM)modelVariant);
	}

	if (modelStatus) {
		SendMessage(this->hWndStatus, SB_SETTEXT, (WPARAM)0x0001, (LPARAM)modelStatus);
	}

	if (modelInfo) {
		SendMessage(this->hWndStatus, SB_SETTEXT, (WPARAM)0x0002, (LPARAM)modelInfo);
	}
}


bool WinSrv::instMenuGetItem(HMENU* hMenuSub, int* menuIdx, HMENU hMenu, wchar_t* caption)
{
	/* Sanity checks */
	if (!hMenuSub || !menuIdx || !hMenu  || !caption) {
		return FALSE;
	}

	/* Prepare request attributes of the items */
	MENUITEMINFO menuItemInfo;
	memset(&menuItemInfo, 0, sizeof(MENUITEMINFO));
	menuItemInfo.cbSize = sizeof(MENUITEMINFO);
	menuItemInfo.fMask = MIIM_TYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU;

	int menuItemCnt = GetMenuItemCount(hMenu);

	for (int menuItemIdx = 0; menuItemIdx < menuItemCnt; menuItemIdx++) {
		TCHAR linkUsb_buffer[MAX_PATH];

		GetMenuItemInfo(
			hMenu,
			menuItemIdx,
			TRUE,
			&menuItemInfo);

		GetMenuString(
			hMenu,
			menuItemIdx,
			linkUsb_buffer,
			MAX_PATH,
			MF_BYPOSITION);

		if (!lstrcmp(caption, linkUsb_buffer)) {
			/* Found! */
			*hMenuSub = menuItemInfo.hSubMenu ? menuItemInfo.hSubMenu : hMenu;
			*menuIdx = menuItemIdx;
			return true;
		}

		if (menuItemInfo.hSubMenu) {
			HMENU recMenu = menuItemInfo.hSubMenu;
			int recMenuIdx = -1;
			HMENU recMenuSub;

			if (TRUE == instMenuGetItem(&recMenuSub, &recMenuIdx, recMenu, caption)) {
				/* Found in subree */
				*hMenuSub = recMenu;
				*menuIdx = recMenuIdx;
				return true;
			}
		}
	}

	/* Caption not found */
	return false;
}

void WinSrv::instUpdateConnectedInstruments(void)
{
	/* Update UI with the latest connected instruments */

#ifdef TUTORIALS
	// @see https://msdn.microsoft.com/en-us/library/windows/desktop/ms647553(v=vs.85).aspx#accessing_menu_items_programmatically "Menu Creation Functions"
	// @see https://docs.microsoft.com/de-de/windows/desktop/winmsg/window-features
	// @see http://www.winprog.org/tutorial/
	// @see https://www.codeproject.com/Articles/7503/An-examination-of-menus-from-a-beginner-s-point-of
	// @see https://www.codeproject.com/Articles/7503/An-examination-of-menus-from-a-beginner-s-point-of#advanced1
	InsertMenuItemA(HMENU hmenu, UINT item, BOOL fByPosition, LPCMENUITEMINFOA lpmi);
	SendMessageW(GetDlgItem(_hWnd, IDC_ROTOR_POS_X_NEW_SLIDER), TBM_GETPOS, 0, 0);
	InsertMenuItemA(hmenu, /*UINT item*/ 0, /*fByPosition*/ false, /*LPCMENUITEMINFOA*/ L"Test");

	MENUINFO mi;
	memset(&mi, 0, sizeof(MENUINFO));
	mi.cbSize = sizeof(MENUINFO);
	BOOL r = GetMenuInfo(hm, &mi);  // #define IDC_RFLAB_WIN32   109

	HWND hw = GetDlgItem(this->hWnd, 105);
#endif

#if 1
	/* Reload from ressource file */
	HMENU hMenu = LoadMenu(g_hInst, MAKEINTRESOURCEW(IDC_RFLAB_WIN32));
	SetMenu(hWnd, hMenu);
#else
	/* Get menu class */
	HMENU hMenu = GetMenu(this->hWnd);
#endif

	/* Prepare request attributes of the items */
	MENUITEMINFO menuItemInfo;
	memset(&menuItemInfo, 0, sizeof(MENUITEMINFO));
	menuItemInfo.cbSize = sizeof(MENUITEMINFO);
	menuItemInfo.fMask = MIIM_TYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU;

	/* Iterate over all menu entries */
	int menuItemCnt = GetMenuItemCount(hMenu);
	for (int menuItemIdx = 0; menuItemIdx < menuItemCnt; menuItemIdx++) {
		GetMenuItemInfo(
			hMenu,
			menuItemIdx,
			TRUE,
			&menuItemInfo);

		/* Visit each popup menu */
		if (menuItemInfo.hSubMenu) {
			TCHAR linkUsb_buffer[MAX_PATH];

			GetMenuString(
				hMenu,
				menuItemIdx,
				linkUsb_buffer,
				MAX_PATH,
				MF_BYPOSITION);

			/* Is it the popup menu we do search for? */
			if (!lstrcmp(L"Instrumenten-Liste", linkUsb_buffer)) {
				/**/
				HMENU hInstrPopupMenu = menuItemInfo.hSubMenu;
				int instrMenuCnt = GetMenuItemCount(hInstrPopupMenu);

				for (int instMenuIdx = 0; instMenuIdx < instrMenuCnt; instMenuIdx++) {
					GetMenuString(
						hInstrPopupMenu,
						instMenuIdx,
						linkUsb_buffer,
						MAX_PATH,
						MF_BYPOSITION);

					if (!lstrcmp(L"Aktoren", linkUsb_buffer)) {
						GetMenuItemInfo(
							hInstrPopupMenu,
							instMenuIdx,
							TRUE,
							&menuItemInfo);

						if (menuItemInfo.hSubMenu) {
							/* Aktoren found */
							HMENU hAktorenMenu = menuItemInfo.hSubMenu;
							int aktorenMenuCnt = GetMenuItemCount(hAktorenMenu);

							for (int aktorenMenuIdx = 0; aktorenMenuIdx < aktorenMenuCnt; aktorenMenuIdx++) {
								GetMenuString(
									hAktorenMenu,
									aktorenMenuIdx,
									linkUsb_buffer,
									MAX_PATH,
									MF_BYPOSITION);

								/* Entrypoint for actors found? */
								if (!lstrcmp(L"_aktor_", linkUsb_buffer)) {
									UINT aktorID = ID_AKTOR_ITEM0_;
									/* Iterate over the instrument list */
									InstList_t::iterator it = g_InstList.begin();
									
									/* Iterate over all instruments and check which one responds */
									while (it != g_InstList.end()) {
										if (it->listFunction == INST_FUNC_ROTOR) {
											/* Convert char[] to wchar_t[] */
											wchar_t	wName[MAX_PATH];
											size_t converted = 0;
											mbstowcs_s(&converted, wName, it->listEntryName.c_str(), MAX_PATH);

											/* Automatic select the first available device */
											if (it->actLink && !_menuInfo.rotorEnabled) {
												_menuInfo.rotorEnabled = true;
												it->actSelected = true;
											}

											it->winID = aktorID;
											AppendMenu(
												hAktorenMenu,
												MF_STRING | (it->actSelected ?  MF_CHECKED : 0) | (it->actLink ?  0 : MF_DISABLED),
												(UINT) aktorID,
												wName);
										}

										/* Advance to next entry */
										it++;
									}

									/* Remove anchor */
									RemoveMenu(
										hAktorenMenu,
										(UINT) ID_AKTOR_,
										MF_BYCOMMAND);
									break;
								}
							}
						}
					}

					else if (!lstrcmp(L"HF-Generatoren", linkUsb_buffer)) {
						GetMenuItemInfo(
							hInstrPopupMenu,
							instMenuIdx,
							TRUE,
							&menuItemInfo);

						if (menuItemInfo.hSubMenu) {
							/* HF-Generatoren found */
							HMENU hRfGenMenu = menuItemInfo.hSubMenu;
							int rfGenMenuCnt = GetMenuItemCount(hRfGenMenu);
							UINT rfGenID = ID_GENERATOR_ITEM0_;

							for (int rfGenMenuIdx = 0; rfGenMenuIdx < rfGenMenuCnt; rfGenMenuIdx++) {
								GetMenuString(
									hRfGenMenu,
									rfGenMenuIdx,
									linkUsb_buffer,
									MAX_PATH,
									MF_BYPOSITION);

								/* Entrypoint for actors found? */
								if (!lstrcmp(L"_hf_generator_", linkUsb_buffer)) {
									/* Iterate over the instrument list */
									InstList_t::iterator it = g_InstList.begin();

									/* Iterate over all instruments and check which one responds */
									while (it != g_InstList.end()) {
										if (it->listFunction == INST_FUNC_GEN) {
											/* Convert char[] to wchar_t[] */
											wchar_t	wName[MAX_PATH];
											size_t converted = 0;
											mbstowcs_s(&converted, wName, it->listEntryName.c_str(), MAX_PATH);

											/* Automatic select the first available device */
											if (it->actLink && !_menuInfo.rfGenEnabled) {
												_menuInfo.rfGenEnabled = true;
												it->actSelected = true;
											}

											it->winID = rfGenID;
											AppendMenu(
												hRfGenMenu,
												MF_STRING | (it->actSelected ? MF_CHECKED : 0) | (it->actLink ? 0 : MF_DISABLED),
												(UINT)rfGenID++,
												wName);
										}

										/* Advance to next entry */
										it++;
									}

									/* Remove anchor */
									RemoveMenu(
										hRfGenMenu,
										(UINT)ID_GENERATOR_,
										MF_BYCOMMAND);
									break;
								}
							}
						}
					}

					else if (!lstrcmp(L"Spektrumanalysatoren", linkUsb_buffer)) {
						GetMenuItemInfo(
							hInstrPopupMenu,
							instMenuIdx,
							TRUE,
							&menuItemInfo);

						if (menuItemInfo.hSubMenu) {
							/* Spektrumanalysatoren found */
							HMENU hSpekMenu = menuItemInfo.hSubMenu;
							int spekMenuCnt = GetMenuItemCount(hSpekMenu);
							UINT spekID = ID_SPEK_ITEM0_;

							for (int spekMenuIdx = 0; spekMenuIdx < spekMenuCnt; spekMenuIdx++) {
								GetMenuString(
									hSpekMenu,
									spekMenuIdx,
									linkUsb_buffer,
									MAX_PATH,
									MF_BYPOSITION);

								/* Entrypoint for actors found? */
								if (!lstrcmp(L"_spek_", linkUsb_buffer)) {
									/* Iterate over the instrument list */
									InstList_t::iterator it = g_InstList.begin();

									/* Iterate over all instruments and check which one responds */
									while (it != g_InstList.end()) {
										if (it->listFunction == INST_FUNC_SPEC) {
											/* Convert char[] to wchar_t[] */
											wchar_t	wName[MAX_PATH];
											size_t converted = 0;
											mbstowcs_s(&converted, wName, it->listEntryName.c_str(), MAX_PATH);

											/* Automatic select the first available device */
											if (it->actLink && !_menuInfo.specEnabled) {
												_menuInfo.specEnabled = true;
												it->actSelected = true;
											}

											it->winID = spekID;
											AppendMenu(
												hSpekMenu,
												MF_STRING | (it->actSelected ? MF_CHECKED : 0) | (it->actLink ? 0 : MF_DISABLED),
												(UINT)spekID++,
												wName);
										}

										/* Advance to next entry */
										it++;
									}

									/* Remove anchor */
									RemoveMenu(
										hSpekMenu,
										(UINT)ID_SPEK_,
										MF_BYCOMMAND);
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	/* Enable disconnect and search again menu item */
	EnableMenuItem(hMenu, ID_INSTRUMENTEN_DISCONNECT, MF_BYCOMMAND);

	/* Enable connect menu item if all device types are available */
	if (_menuInfo.rotorEnabled && _menuInfo.rfGenEnabled && _menuInfo.specEnabled) {
		EnableMenuItem(hMenu, ID_INSTRUMENTEN_CONNECT, MF_BYCOMMAND);
	}

	DrawMenuBar(this->hWnd);
}

void WinSrv::instActivateMenuItem(UINT winID, bool uEnable)
{
	/* Get menu structure */
	HMENU hMenuBar = GetMenu(this->hWnd);

	int   menuAnstIdx = 0;
	HMENU hMenuAnst = NULL;

	int   menuAnstAktorIdx = 0;
	HMENU hMenuAnstAktor = NULL;

	int   menuAnstGenIdx = 0;
	HMENU hMenuAnstGen = NULL;

	int   menuAnstGenOutIdx = 0;
	HMENU hMenuAnstGenOut = NULL;

	int   menuAnstSpekIdx = 0;
	HMENU hMenuAnstSpek = NULL;


	if (uEnable) {
		/* Enable items */
		switch (winID) {
		case ID_AKTOR_ITEM0_:
		{
			/* Enable menu bar item */
			instMenuGetItem(&hMenuAnst, &menuAnstIdx, hMenuBar, L"Ansteuerung");
			EnableMenuItem(hMenuBar, menuAnstIdx, MF_BYPOSITION);

			instMenuGetItem(&hMenuAnstAktor, &menuAnstAktorIdx, hMenuAnst, L"Aktor");
			EnableMenuItem(hMenuAnst, menuAnstAktorIdx, MF_BYPOSITION);

			/* Enabling each AKTOR item */
			EnableMenuItem(hMenuBar, ID_ROTOR_STOP, MF_BYCOMMAND);
			EnableMenuItem(hMenuBar, ID_ROTOR_GOTO_0, MF_BYCOMMAND);
			EnableMenuItem(hMenuBar, ID_ROTOR_GOTO_X, MF_BYCOMMAND);

			//EnableMenuItem(GetMenu(this->hWnd), ID_ROTOR_EINSTELLUNGEN, MF_BYCOMMAND);  // TODO: to be defined

			/* Update menu state for fast access */
			_menuInfo.rotorEnabled = true;

			/* Enable ModelPattern when all items are activated */
			checkForModelPattern(hMenuAnst);
		}
		break;

		case ID_GENERATOR_ITEM0_:
		{
			/* Enable menu bar item */
			instMenuGetItem(&hMenuAnst, &menuAnstIdx, hMenuBar, L"Ansteuerung");
			EnableMenuItem(hMenuBar, menuAnstIdx, MF_BYPOSITION);

			instMenuGetItem(&hMenuAnstGen, &menuAnstGenIdx, hMenuAnst, L"HF-Generator");
			EnableMenuItem(hMenuAnst, menuAnstGenIdx, MF_BYPOSITION);

			instMenuGetItem(&hMenuAnstGenOut, &menuAnstGenOutIdx, hMenuAnstGen, L"HF Ausgabe");
			EnableMenuItem(hMenuAnstGen, menuAnstGenOutIdx, MF_BYPOSITION);

			if (agentModel::getTxOnState()) {
				/* TX enabled */
				CheckMenuItem(hMenuBar, ID_HFAUSGABE_EIN, MF_BYCOMMAND | MF_CHECKED);
				CheckMenuItem(hMenuBar, ID_HFAUSGABE_AUS, MF_BYCOMMAND | MF_UNCHECKED);
			}
			else {
				/* TX disabled */
				CheckMenuItem(hMenuBar, ID_HFAUSGABE_EIN, MF_BYCOMMAND | MF_UNCHECKED);
				CheckMenuItem(hMenuBar, ID_HFAUSGABE_AUS, MF_BYCOMMAND | MF_CHECKED);
			}

			/* Enabling each HF-Generator item */
			EnableMenuItem(hMenuBar, ID_TX_SETTINGS, MF_BYCOMMAND);

			/* Update menu state for fast access */
			_menuInfo.rfGenEnabled = true;

			/* Enable ModelPattern when all items are activated */
			checkForModelPattern(hMenuAnst);
		}
			break;

		case ID_SPEK_ITEM0_:
		{
			/* Enable menu bar item */
			instMenuGetItem(&hMenuAnst, &menuAnstIdx, hMenuBar, L"Ansteuerung");
			EnableMenuItem(hMenuBar, menuAnstIdx, MF_BYPOSITION);

			instMenuGetItem(&hMenuAnstSpek, &menuAnstSpekIdx, hMenuAnst, L"Spektrum-Analysator");
			EnableMenuItem(hMenuAnst, menuAnstSpekIdx, MF_BYPOSITION);

			/* Enabling each Spek item */
			EnableMenuItem(hMenuBar, ID_RX_SETTINGS, MF_BYCOMMAND);

			/* Update menu state for fast access */
			_menuInfo.specEnabled = true;

			/* Enable ModelPattern when all items are activated */
			checkForModelPattern(hMenuAnst);
		}
			break;
		}  // switch (winID)
	}

	else {
		/* Disable items */
		// TODO: add code here
	}

	DrawMenuBar(this->hWnd);
}

bool WinSrv::checkForModelPattern(HMENU hMenuAnst)
{
	HMENU hMenu = GetMenu(this->hWnd);

	/* When every type is connected activate Pattern Model */
	if (_menuInfo.rotorEnabled && _menuInfo.rfGenEnabled && _menuInfo.specEnabled) {
		if (!_menuInfo.modelPatternAct) {
			EnableMenuItem(hMenu, ID_INSTRUMENTEN_DISCONNECT, MF_BYCOMMAND);
			EnableMenuItem(hMenu, ID_CTRL_ALL_RESET, MF_BYCOMMAND);

			/* Append ModelPattern specific items to the menu */
			InsertMenu(hMenuAnst, ID_ANSTEUERUNG_, MF_BYCOMMAND  | MF_STRING, ID_MODEL_PATTERN_STOP,			 L"Richtdiagramm: anhalten und beenden");
			InsertMenu(hMenuAnst, ID_ANSTEUERUNG_, MF_BYCOMMAND  | MF_STRING, ID_MODEL_PATTERN_REF_START,        L"Referenzmessung ausführen");

			InsertMenu(hMenuAnst, ID_ANSTEUERUNG_, MF_BYCOMMAND  | MF_SEPARATOR, ID_MODEL_PATTERN_SEP_01, 0);

			InsertMenu(hMenuAnst, ID_ANSTEUERUNG_, MF_BYCOMMAND  | MF_STRING, ID_MODEL_PATTERN010_STEP001_START, L"Richtdiagramm über 10° mit 1° Schritten ausmessen");
			InsertMenu(hMenuAnst, ID_ANSTEUERUNG_, MF_BYCOMMAND  | MF_STRING, ID_MODEL_PATTERN010_STEP005_START, L"Richtdiagramm über 10° mit 5° Schritten ausmessen ausmessen");

			InsertMenu(hMenuAnst, ID_ANSTEUERUNG_, MF_BYCOMMAND  | MF_SEPARATOR, ID_MODEL_PATTERN_SEP_02, 0);

			InsertMenu(hMenuAnst, ID_ANSTEUERUNG_, MF_BYCOMMAND  | MF_STRING, ID_MODEL_PATTERN180_STEP001_START, L"Richtdiagramm über 180° mit 1° Schritten ausmessen");
			InsertMenu(hMenuAnst, ID_ANSTEUERUNG_, MF_BYCOMMAND  | MF_STRING, ID_MODEL_PATTERN180_STEP005_START, L"Richtdiagramm über 180° mit 5° Schritten ausmessen ausmessen");

			InsertMenu(hMenuAnst, ID_ANSTEUERUNG_, MF_BYCOMMAND  | MF_SEPARATOR, ID_MODEL_PATTERN_SEP_03, 0);

			InsertMenu(hMenuAnst, ID_ANSTEUERUNG_, MF_BYCOMMAND  | MF_STRING, ID_MODEL_PATTERN360_STEP001_START, L"Richtdiagramm über 360° mit 1° Schritten ausmessen");
			InsertMenu(hMenuAnst, ID_ANSTEUERUNG_, MF_BYCOMMAND  | MF_STRING, ID_MODEL_PATTERN360_STEP005_START, L"Richtdiagramm über 360° mit 5° Schritten ausmessen ausmessen");

			/* Remove place holder */
			RemoveMenu(hMenu, ID_ANSTEUERUNG_, MF_BYCOMMAND);

			_menuInfo.modelPatternAct = true;
		}
	}
	else {
		/* When not every type is any more connected deactivate Pattern Model */
		if (_menuInfo.modelPatternAct) {
			/* Remove menu items */
			RemoveMenu(hMenu, ID_MODEL_PATTERN_STOP, MF_BYCOMMAND);
			RemoveMenu(hMenu, ID_MODEL_PATTERN_REF_START, MF_BYCOMMAND);

			RemoveMenu(hMenu, ID_MODEL_PATTERN_SEP_01, MF_BYCOMMAND);

			RemoveMenu(hMenu, ID_MODEL_PATTERN010_STEP001_START, MF_BYCOMMAND);
			RemoveMenu(hMenu, ID_MODEL_PATTERN010_STEP005_START, MF_BYCOMMAND);

			RemoveMenu(hMenu, ID_MODEL_PATTERN_SEP_02, MF_BYCOMMAND);

			RemoveMenu(hMenu, ID_MODEL_PATTERN180_STEP001_START, MF_BYCOMMAND);
			RemoveMenu(hMenu, ID_MODEL_PATTERN180_STEP005_START, MF_BYCOMMAND);

			RemoveMenu(hMenu, ID_MODEL_PATTERN_SEP_03, MF_BYCOMMAND);

			RemoveMenu(hMenu, ID_MODEL_PATTERN360_STEP001_START, MF_BYCOMMAND);
			RemoveMenu(hMenu, ID_MODEL_PATTERN360_STEP005_START, MF_BYCOMMAND);

			EnableMenuItem(hMenu, ID_INSTRUMENTEN_DISCONNECT, MF_BYCOMMAND | MF_DISABLED);
			EnableMenuItem(hMenu, ID_CTRL_ALL_RESET, MF_BYCOMMAND | MF_DISABLED);

			_menuInfo.modelPatternAct = false;
		}
	}
	return FALSE;
}


/* Dataset handling */

FILETYPE_ENUM WinSrv::getFileType(wchar_t* filename)
{
	/* Sanity check */
	if (!filename) {
		return FILETYPE_UNKNOWN;
	}

	/* Cut out extension */
	wchar_t wcFileExt[16];
	wchar_t* pos = wcsrchr(filename, L'.');
	StrCpyNW(wcFileExt, pos + 1, (int)(lstrlenW(filename) - (pos - filename)));

	if (!StrCmpIW(wcFileExt, L"CSV")) {
		return FILETYPE_CSV;
	}
	else if (!StrCmpIW(wcFileExt, L"S1P")) {
		return FILETYPE_S1P;
	}
	else if (!StrCmpIW(wcFileExt, L"S2P")) {
		return FILETYPE_S2P;
	}
	else if (!StrCmpIW(wcFileExt, L"S3P")) {
		return FILETYPE_S3P;
	}
	else if (!StrCmpIW(wcFileExt, L"S4P")) {
		return FILETYPE_S4P;
	}
	else if (!StrCmpIW(wcFileExt, L"TXT")) {
		return FILETYPE_TXT;
	}

	/* File extension not known */
	return FILETYPE_UNKNOWN;
}

void WinSrv::saveCurrentDataset(void)
{
	if (g_instance && g_instance->isReady()) {
		MEASTYPE measType;

		/* Create last path and file name */
		wchar_t lineBuf[MAX_PATH] = { 0 };
		wchar_t* pos = lineBuf + lstrlenW(g_instance->cLastFilePath);
		StrCpyNW(lineBuf, g_instance->cLastFilePath, sizeof(lineBuf));
		StrCpyNW(pos, L"\\", 2);
		StrCpyNW(pos + 1, g_instance->cLastFileName, (int)(1 + lstrlenW(g_instance->cLastFileName)));
		
		/* Get local time */
		SYSTEMTIME systemTime;
		GetLocalTime(&systemTime);

		/* Get type of meassuremnt */
		getMeasType(&measType);

		/* Retrieve file type */
		FILETYPE_ENUM ft = getFileType(g_instance->cLastFileName);

		/* Decide which document type is chosen */
		switch (ft) {

		case FILETYPE_CSV:
		{
			/* Open or create new file */
			HANDLE fh = CreateFile(lineBuf,			// File name
				GENERIC_READ | GENERIC_WRITE,		// Read/Write
				0,									// No Sharing
				NULL,								// No Security
				CREATE_ALWAYS,					    // Wipe out old content of existing file, create new file when not exists
				0 /*FILE_FLAG_OVERLAPPED*/,	        // Non Overlapped I/O
				NULL);

			if (fh != INVALID_HANDLE_VALUE) {
				int status;
				DWORD len;
				DWORD lenWritten = 0;

				/* 1st Line */
				len = swprintf(lineBuf, sizeof(lineBuf), L"# Institut für Hochfrequenztechnik (HFT) - Zeitpunkt der Messung: %d-%02d-%02d  %02d:%02d:%02d\r\n", 
					systemTime.wYear, systemTime.wMonth, systemTime.wDay,
					systemTime.wHour, systemTime.wMinute, systemTime.wSecond);

				/* Length in bytes */
				len <<= 1;

				/* Write header line */
				status = WriteFile(fh,				// Handle to the Serial port
					lineBuf,						// Data to be written to the port
					len,							// Number of bytes to write
					&lenWritten,					// Bytes written
					NULL);


				/* 2nd line */
				len = swprintf(lineBuf, sizeof(lineBuf), L"# Antennen-Richtdiagramm\r\n");
				len <<= 1;

				/* Write header line */
				status = WriteFile(fh,				// Handle to the Serial port
					lineBuf,						// Data to be written to the port
					len,							// Number of bytes to write
					&lenWritten,					// Bytes written
					NULL);


				/* 3rd line */
				len = swprintf(lineBuf, sizeof(lineBuf), L"# Sende-Frequenz: %d.%03d %cHz\r\n",
					measType.measTxFreqHz_int, measType.measTxFreqHz_frac3, measType.meastxFreqHz_exp);
				len <<= 1;

				/* Write header line */
				status = WriteFile(fh,				// Handle to the Serial port
					lineBuf,						// Data to be written to the port
					len,							// Number of bytes to write
					&lenWritten,					// Bytes written
					NULL);


				/* 4th line */
				len = swprintf(lineBuf, sizeof(lineBuf), L"# Sende-Leistung: %d.%03d dBm\r\n",
					measType.measTxPowerDbm_int, measType.measTxPowerDbm_frac3);
				len <<= 1;

				/* Write header line */
				status = WriteFile(fh,				// Handle to the Serial port
					lineBuf,						// Data to be written to the port
					len,							// Number of bytes to write
					&lenWritten,					// Bytes written
					NULL);


				/* 5th line */
				len = swprintf(lineBuf, sizeof(lineBuf), L"Position, Empfangsleistung_mag, Empfangsleistung_phase\r\n");
				len <<= 1;

				/* Write header line */
				status = WriteFile(fh,				// Handle to the Serial port
					lineBuf,						// Data to be written to the port
					len,							// Number of bytes to write
					&lenWritten,					// Bytes written
					NULL);

				if (measType.measData->measVar == MEASDATA_SETUP__REFMEAS_GEN_SPEC) {
					len = swprintf(lineBuf, sizeof(lineBuf), L"%lf, %lf, %lf\r\n", 0.0, measType.measData->rxRefPwr, 0.0);
					len <<= 1;

					/* Write header line */
					status = WriteFile(fh,				// Handle to the Serial port
						lineBuf,						// Data to be written to the port
						len,							// Number of bytes to write
						&lenWritten,					// Bytes written
						NULL);
				}
				
				else if (measType.measData->measVar >  MEASDATA_SETUP__REFMEAS_GEN_SPEC) {
					/* Iterate over all meassured data */
					if (measType.measData->posDeg && measType.measData->rxPwrMag && measType.measData->rxPwrPhase) {
						std::list<double>::const_iterator itPos			= measType.measData->posDeg->begin();
						std::list<double>::const_iterator itPwrMag		= measType.measData->rxPwrMag->begin();
						std::list<double>::const_iterator itPwrPhase	= measType.measData->rxPwrPhase->begin();

						for (int lineNo = 1; lineNo <= measType.measData->entriesCount; lineNo++) {
							len = swprintf(lineBuf, sizeof(lineBuf), L"%lf, %lf, %lf\r\n", *itPos, *itPwrMag, *itPwrPhase);
							len <<= 1;

							/* Write header line */
							status = WriteFile(fh,				// Handle to the Serial port
								lineBuf,						// Data to be written to the port
								len,							// Number of bytes to write
								&lenWritten,					// Bytes written
								NULL);

							// Goto next row
							++itPos;
							++itPwrMag;
							++itPwrPhase;
						}
					}
				}

				/* Close file */
				CloseHandle(fh);

				if (g_winSrv) {
					/* Inform in status line */
					g_winSrv->reportStatus(L"Model: Pattern", L"Ready for new jobs", L"READY");
				}
			}
		}
		break;

		case FILETYPE_S1P:
		{

		}
		break;

		}
	}
}



/* Getters & Setters */
wchar_t* WinSrv::getLastFilePath(void)
{
	if (g_instance && g_instance->isReady()) {
		return g_instance->cLastFilePath;
	}
	else {
		return L"";
	}
}

wchar_t* WinSrv::getLastFileName(void)
{
	if (g_instance && g_instance->isReady()) {
		return g_instance->cLastFileName;
	}
	else {
		return L"";
	}
}

void WinSrv::setLastFilePath(wchar_t* s)
{
	if (g_instance && g_instance->isReady() && s) {
		if (lstrlenW(s)) {
			wchar_t* pos = wcsrchr(s, L'\\');
			StrCpyNW(g_instance->cLastFilePath, s, (int)(pos - s) + 1);
			StrCpyNW(g_instance->cLastFileName, pos + 1, (int)(lstrlenW(s) - (pos - s)));
		}
	}
}

void WinSrv::getMeasType(MEASTYPE* measType)
{
	if (g_instance && g_instance->isReady() && measType) {
		/* Wipe out old data */
		memset((void*)measType, 0, sizeof(MEASTYPE));

		/* Retrieve data */
		//measType->measSimuMode = agentModel::getSimuMode();
		agentModel::getMeasData(&measType->measData);

		/* Pretty print entities - Frequency */
		double frequency = measType->measData->txQrg;

		int frequencyExp = 0;
		while (frequency >= 1000.0) {
			frequency /= 1000.0;
			frequencyExp += 3;
		};

		measType->measTxFreqHz_int = (int)frequency;
		measType->measTxFreqHz_frac3 = ((int)(frequency * 1000.0)) % 1000;
		switch (frequencyExp) {

		default:
		case 0:
			measType->meastxFreqHz_exp = L' ';
			break;

		case 3:
			measType->meastxFreqHz_exp = L'k';
			break;

		case 6:
			measType->meastxFreqHz_exp = L'M';
			break;

		case 9:
			measType->meastxFreqHz_exp = L'G';
			break;

		}

		/* Pretty print entities - Power */
		double power = measType->measData->txPwr;
		measType->measTxPowerDbm_int = (int)power;
		measType->measTxPowerDbm_frac3 = ((int)(power * 1000.0)) % 1000;
	}
}
