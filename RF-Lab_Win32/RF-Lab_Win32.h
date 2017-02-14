#pragma once

#include "resource.h"

#define C_BUFSIZE 256


static int AskRotorPosX(HINSTANCE hInst, HWND hWnd);

BOOL CALLBACK RotorPosX_CB(	HWND   hWnd,
							UINT   message,
							WPARAM wParam,
							LPARAM lParam);


static int AskTxSettings(HINSTANCE hInst, HWND hWnd);

BOOL CALLBACK AskTxSettings_CB(HWND   hWnd,
	UINT   message,
	WPARAM wParam,
	LPARAM lParam);
