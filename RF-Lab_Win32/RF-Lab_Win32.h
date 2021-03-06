#pragma once

#include "resource.h"
#include "agentModelPattern.h"


#define C_BUFSIZE 256


BOOL CALLBACK RotorPosX_CB(	HWND   _hWnd,
							UINT   message,
							WPARAM wParam,
							LPARAM lParam);

BOOL CALLBACK AskTxSettings_CB(HWND   _hWnd,
	UINT   message,
	WPARAM wParam,
	LPARAM lParam);

BOOL CALLBACK AskRxSettings_CB(HWND   _hWnd,
	UINT   message,
	WPARAM wParam,
	LPARAM lParam);
