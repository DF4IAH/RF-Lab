#include "stdafx.h"
#include "Srv.h"

#include <shobjidl.h>

Srv* g_instance = nullptr;


Srv::Srv()
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr)) {
		m_ready = TRUE;
	}
}


Srv::~Srv()
{
	CoUninitialize();
}

void Srv::paint(HWND hWnd, LPPAINTSTRUCT ps, HDC hdc)
{
	FillRect(hdc, &ps->rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
	
}

bool Srv::isReady()
{
	return m_ready ? TRUE : FALSE;
}


// Statische Methoden folgen

// Startet den Visualisierungsserver an
void Srv::SrvStart()
{
	if (!g_instance) {
		g_instance = new Srv();
	}
}


// Hält den Visualisierungsserver an
void Srv::SrvStop()
{
	if (g_instance) {
		delete g_instance;
		g_instance = nullptr;
	}
}


void Srv::SrvPaint(HWND hWnd, LPPAINTSTRUCT ps, HDC hdc)
{
	if (g_instance || g_instance->isReady()) {
		g_instance->paint(hWnd, ps, hdc);
	}
}
