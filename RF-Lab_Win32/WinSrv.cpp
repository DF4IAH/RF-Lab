#include "stdafx.h"

// COM-style
#include <shobjidl.h>

// STL-enhancements
#include <atlbase.h>

// D2Draw-style
//#include <d2d1.h>
#pragma comment(lib, "d2d1")

// sub-module agents
#include "agentModel.h"
#include "agentModelPattern.h"
#include "agentCom.h"

#include "WinSrv.h"


WinSrv* g_instance = nullptr;
extern HINSTANCE g_hInst;                                // Aktuelle Instanz


template <class T>  void SafeRelease(T **ppT)
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


WinSrv::WinSrv() : hWnd(nullptr)
				 , hWndStatus(nullptr)
				 , pFactory(nullptr)
				 , pRenderTarget(nullptr)
				 , pBrush(nullptr)
				 , _size(D2D1_SIZE_F())
				 , _PD({ 0 })
				 , pAgtModel(nullptr)
				 , pAgtCom { nullptr, nullptr,nullptr }
				 , _winExitReceived(FALSE)
				 , _ready(FALSE)
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr)) {
		threadsStart();
	}
}


WinSrv::~WinSrv()
{
	threadsStop();

	SafeRelease(&pAgtModel);
	for (int i = 0; i < C_COMINST__COUNT; ++i) {
		if (pAgtCom[i]) {
			SafeRelease(&pAgtCom[i]);
		}
	}

	//SafeRelease(&hwndStatus);

	SafeRelease(&pBrush);
	SafeRelease(&pRenderTarget);
	SafeRelease(&pFactory);

	CoUninitialize();
}


void WinSrv::threadsStart()
{
	// start the master model with the antenna pattern model variant
	pAgtModel  = new agentModel(
		&_ub_agtModel_req,
		&_ob_agtModel_rsp,
		this,
		agentModel::AGENT_MODEL_PATTERN,
		(AGENT_ALL_SIMUMODE_t) (AGENT_ALL_SIMUMODE_NO_RX | AGENT_ALL_SIMUMODE_NO_TX | AGENT_ALL_SIMUMODE_RUN_BARGRAPH)
	);

	// starting agent
	pAgtModel->start();
}

void WinSrv::threadsStop()
{
	// shutdown the antenna measure model
	if (pAgtModel) {
		//_ub_agtModel_req.send();
		//pAgtModel->shutdown();
		agent::wait(pAgtModel);
	}

	// shutdown each communication interface
	for (int i = 0; i < C_COMINST__COUNT; ++i) {
		if (pAgtCom[i]) {
			agent::wait(pAgtCom[i]);
		}
	}
}


void WinSrv::srvWinExit()
{
	if (g_instance) {
		g_instance->_winExitReceived = TRUE;

		if (g_instance->pAgtModel) {
			g_instance->pAgtModel->shutdown();
		}
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
				this->hWndStatus = DoCreateStatusBar(this->hWnd, 63, g_hInst, StatusBarParts);
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
	SafeRelease(&pBrush);
	SafeRelease(&pRenderTarget);
}

void WinSrv::wmCmd(HWND hWnd, int wmId, LPVOID arg)
{
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
HWND WinSrv::DoCreateStatusBar(HWND hwndParent, int idStatus, HINSTANCE	hinst, int cParts)
{
	HWND hWndStatus;
	RECT rcClient;
	HLOCAL hloc;
	PINT paParts;
	int i, nWidth;

	// Ensure that the common control DLL is loaded.
	//InitCommonControls();

	// Create the status bar.
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

	// Allocate an array for holding the right edge coordinates.
	hloc = LocalAlloc(LHND, sizeof(int) * cParts);
	paParts = (PINT)LocalLock(hloc);

	// Calculate the right edge coordinate for each part, and
	// copy the coordinates to the array.
	nWidth = rcClient.right / cParts;
	int rightEdge = nWidth;
	for (i = 0; i < cParts; i++) {
		paParts[i] = rightEdge;
		rightEdge += nWidth;
	}

	// Tell the status bar to create the window parts.
	SendMessage(hWndStatus, SB_SETPARTS, (WPARAM)cParts, (LPARAM)paParts);

	/* Status line information */
	SendMessage(hWndStatus, SB_SETTEXT, (WPARAM)0x0000, (LPARAM)L"Status 1");
	SendMessage(hWndStatus, SB_SETTEXT, (WPARAM)0x0001, (LPARAM)L"Status 2");
	SendMessage(hWndStatus, SB_SETTEXT, (WPARAM)0x0002, (LPARAM)L"Status 3");

	// Free the array, and return.
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
		while (agentModel::isRunning()) {
			Sleep(10);
		}

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

void WinSrv::reportStatus(LPVOID modelVariant, LPVOID modelStatus)
{
	if (modelVariant) {
		SendMessage(hWndStatus, SB_SETTEXT, (WPARAM)0x0000, (LPARAM)modelVariant);
	}

	if (modelStatus) {
		SendMessage(hWndStatus, SB_SETTEXT, (WPARAM)0x0001, (LPARAM)modelStatus);
	}

	//InvalidateRect(hWndStatus, NULL, FALSE);
	//InvalidateRect(hWnd, NULL, FALSE);
	//SendMessage(hWndStatus, WM_SIZE, 0, 0);
	//SendMessage(hWnd, WM_SIZE, 0, 0);
	//SendMessage(hWndStatus, WM_DISPLAYCHANGE, 0, 0);
}
