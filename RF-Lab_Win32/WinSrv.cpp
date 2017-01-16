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
#include "agentCom.h"

#include "WinSrv.h"


WinSrv* g_instance = nullptr;


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
				 , pFactory(nullptr)
				 , pRenderTarget(nullptr)
				 , pBrush(nullptr)
				 , size(D2D1_SIZE_F())
				 , pAgtModel(nullptr)
				 , pAgtCom { nullptr, nullptr,nullptr }
				 , ready(FALSE)
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
	for (int i = 0; i < C_AGT_COM__COUNT; ++i) {
		if (pAgtCom[i]) {
			SafeRelease(&pAgtCom[i]);
		}
	}

	SafeRelease(&pBrush);
	SafeRelease(&pRenderTarget);
	SafeRelease(&pFactory);

	CoUninitialize();
}


void WinSrv::threadsStart()
{
	unbounded_buffer<agentModelReq> ub_agtModel_req;
	overwrite_buffer<agentModelRsp> ob_agtModel_rsp;

	// start the antenna measure model
	pAgtModel  = new agentModel(ub_agtModel_req, ob_agtModel_rsp);
	pAgtModel->start();
}

void WinSrv::threadsStop()
{
	// shutdown the antenna measure model
	if (pAgtModel) {
		pAgtModel->shutdown();
		agent::wait(pAgtModel);
	}

	// shutdown each communication interface
	for (int i = 0; i < C_AGT_COM__COUNT; ++i) {
		if (pAgtCom[i]) {
			agent::wait(pAgtCom[i]);
		}
	}
}

bool WinSrv::isReady()
{
	return ready ?  TRUE : FALSE;
}


LRESULT WinSrv::setWindow(HWND hWnd)
{
	ready = FALSE;

	this->hWnd = hWnd;
	if (this->hWnd) {
		if (SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
		{
			if (SUCCEEDED(createGraphicsResources())) {
				ready = TRUE;
				return 0;
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

		pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::SkyBlue));
		pRenderTarget->DrawLine(D2D1::Point2F(0, 0), D2D1::Point2F(size.width, size.height), pBrush);
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
		calculateLayout();
		InvalidateRect(hWnd, NULL, FALSE);
	}
}


void WinSrv::calculateLayout()
{
	if (pRenderTarget != NULL)
	{
		size = D2D1_SIZE_F(pRenderTarget->GetSize());
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
