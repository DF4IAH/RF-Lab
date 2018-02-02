// RF-Lab_Win32.cpp : Definiert den Einstiegspunkt für die Anwendung.
//

#include "stdafx.h"

#include <list>


// COM-style
#include <shobjidl.h>

// STL-enhancements
//#include <atlbase.h>

// D2Draw-style
//#include <d2d1.h>
//#pragma comment(lib, "d2d1")


#include "RF-Lab_Win32.h"
#include "WinSrv.h"
#include "agentModel_InstList.h"



#define MAX_LOADSTRING 100


// Globale Variablen:
HINSTANCE			g_hInst							= nullptr;	// Aktuelle Instanz
WCHAR				g_szTitle[MAX_LOADSTRING]		= { 0 };	// Titelleistentext
WCHAR				g_szWindowClass[MAX_LOADSTRING] = { 0 };	// Klassenname des Hauptfensters
int					g_iCbValue						= 0;
agentModel		   *g_am							= nullptr;

bool				g_am_InstList_locked			= false;	// List of Instruments is locked
am_InstList_t		g_am_InstList;								// List of Instruments (rotors, TX, RX)


// Vorwärtsdeklarationen der in diesem Codemodul enthaltenen Funktionen:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
static int			AskRotorPosX(HINSTANCE g_hInst, HWND hWnd);
static int			AskTxSettings(HINSTANCE g_hInst, HWND hWnd);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

#if 0
	// init global values
	g_hInst					= nullptr;
	*g_szTitle				= nullptr;
	*g_szWindowClass		= nullptr;
	g_iCbValue				= 0;
#endif

	// Windows-Kommunikationsserver anstarten
	WinSrv::srvStart();

    // Globale Zeichenfolgen initialisieren
    LoadStringW(hInstance, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_RFLAB_WIN32, g_szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Anwendungsinitialisierung ausführen:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_RFLAB_WIN32));

    MSG msg;

    // Hauptnachrichtenschleife:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

	return (int) msg.wParam;
}


//
//  FUNKTION: MyRegisterClass()
//
//  ZWECK: Registriert die Fensterklasse.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RFLAB_WIN32));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_RFLAB_WIN32);
    wcex.lpszClassName  = g_szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}


//
//   FUNKTION: InitInstance(HINSTANCE, int)
//
//   ZWECK: Speichert das Instanzenhandle und erstellt das Hauptfenster.
//
//   KOMMENTARE:
//
//        In dieser Funktion wird das Instanzenhandle in einer globalen Variablen gespeichert, und das
//        Hauptprogrammfenster wird erstellt und angezeigt.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   g_hInst = hInstance; // Instanzenhandle in der globalen Variablen speichern

   HWND hWnd = CreateWindowW(g_szWindowClass, g_szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}


//
//  FUNKTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ZWECK:  Verarbeitet Meldungen vom Hauptfenster.
//
//  WM_COMMAND  - Verarbeiten des Anwendungsmenüs
//  WM_PAINT    - Darstellen des Hauptfensters
//  WM_DESTROY  - Ausgeben einer Beendenmeldung und zurückkehren
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int argInt = 0;
	
	switch (message)
    {
    case WM_COMMAND:
        {
			int wmId = LOWORD(wParam);

			// Menüauswahl bearbeiten:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;

			case IDM_EXIT:
                DestroyWindow(hWnd);
                break;

			case ID_ROTOR_GOTO_0:
				WinSrv::srvWmCmd(hWnd, wmId);
				break;

			case ID_ROTOR_GOTO_X:
				argInt = AskRotorPosX(g_hInst, hWnd);
				if (argInt != MAXINT16) {  // Position nur verändern, wenn gültiger Wert vorliegt
					WinSrv::srvWmCmd(hWnd, wmId, &argInt);
				}
				break;

			case ID_TX_SETTINGS:
				AskTxSettings(g_hInst, hWnd);
				break;

			case ID_CTRL_ALL_RESET:
			case ID_MODEL_PATTERN_STOP:
			case ID_MODEL_PATTERN_180_START:
			case ID_MODEL_PATTERN_360_START:
				WinSrv::srvWmCmd(hWnd, wmId);
				break;

			default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

	case WM_PAINT:
		// Windows-Kommunikationsserver Zeichen-Code, der hdc verwendet
		WinSrv::srvPaint();
        break;
	case WM_SIZE:
		WinSrv::srvResize();
		break;

	case WM_CREATE:
		return WinSrv::srvSetWindow(hWnd);
	case WM_DESTROY:
		// Windows - Kommunikationsserver abmelden
		WinSrv::srvStop();

        PostQuitMessage(0);
        break;

	default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


// Meldungshandler für Infofeld.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


// Dialog for Rotor Position to go to
// returns milli-degrees
static int AskRotorPosX(HINSTANCE g_hInst, HWND hWnd)
{
	int lastPos = agentModel::requestPos() / 800;
	g_iCbValue = lastPos;

	// MessageBox(NULL, L"Rotor postition to go to: ° ?\n", L"Rotor position\n", MB_ICONQUESTION);
	if (IDOK == DialogBox(g_hInst,
							MAKEINTRESOURCE(IDD_ROTOR_POS_X),
							hWnd,
							(DLGPROC) RotorPosX_CB)) {
		if (g_iCbValue != MAXINT16) {
			return g_iCbValue * 1000;
		}
	}

	// Fail - no value to return
	return MAXINT16;
}

BOOL CALLBACK RotorPosX_CB(	HWND   hWnd,
							UINT   message,
							WPARAM wParam,
							LPARAM lParam)
{
	wchar_t szIdcRotorPosXCurrent[C_BUFSIZE] = { 0 };
	wchar_t szIdcRotorPosXNew[C_BUFSIZE] = { 0 };

	switch (message) {
	case WM_INITDIALOG:
		if (g_iCbValue < -180) {
			g_iCbValue = -180;
		}
		else if (g_iCbValue > 180) {
			g_iCbValue = 180;
		}

		swprintf_s(szIdcRotorPosXCurrent, L"%d", g_iCbValue);
		SetDlgItemText(hWnd, IDC_ROTOR_POS_X_CURRENT_EDIT_RO, szIdcRotorPosXCurrent);
		SetDlgItemText(hWnd, IDC_ROTOR_POS_X_NEW_EDIT, szIdcRotorPosXCurrent);
		SendMessage(GetDlgItem(hWnd, IDC_ROTOR_POS_X_NEW_SLIDER), TBM_SETRANGEMIN, FALSE,   0);
		SendMessage(GetDlgItem(hWnd, IDC_ROTOR_POS_X_NEW_SLIDER), TBM_SETRANGEMAX, FALSE, 360);
		SendMessage(GetDlgItem(hWnd, IDC_ROTOR_POS_X_NEW_SLIDER), TBM_SETTICFREQ,     45,   0);
		SendMessage(GetDlgItem(hWnd, IDC_ROTOR_POS_X_NEW_SLIDER), TBM_SETLINESIZE,     0,   1);
		SendMessage(GetDlgItem(hWnd, IDC_ROTOR_POS_X_NEW_SLIDER), TBM_SETPAGESIZE,     0,  45);
		SendMessage(GetDlgItem(hWnd, IDC_ROTOR_POS_X_NEW_SLIDER), TBM_SETPOS,       TRUE, 180 + g_iCbValue);
		return (INT_PTR)TRUE;
		break;

	case WM_HSCROLL:
		switch (LOWORD(wParam)) {
		case TB_THUMBTRACK:
			g_iCbValue = HIWORD(wParam) - 180;
			break;

		default:
			g_iCbValue = SendMessage(GetDlgItem(hWnd, IDC_ROTOR_POS_X_NEW_SLIDER), TBM_GETPOS, 0, 0) - 180;
		}

		swprintf_s(szIdcRotorPosXCurrent, L"%d", g_iCbValue);
		SetDlgItemText(hWnd, IDC_ROTOR_POS_X_NEW_EDIT, szIdcRotorPosXCurrent);
		break;

	case WM_KEYDOWN:
		if (!GetDlgItemText(hWnd, IDC_ROTOR_POS_X_NEW_EDIT, szIdcRotorPosXCurrent, sizeof(szIdcRotorPosXCurrent) - 1)) {
			g_iCbValue = MAXINT16;
		} else {
			// Process input
			if (swscanf_s(szIdcRotorPosXNew, L"%d", &g_iCbValue)) {
				SendMessage(GetDlgItem(hWnd, IDC_ROTOR_POS_X_NEW_SLIDER), TBM_SETPOS, TRUE, 180 + g_iCbValue);
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			if (!GetDlgItemText(hWnd, IDC_ROTOR_POS_X_NEW_EDIT, szIdcRotorPosXNew, C_BUFSIZE - 1))
			{
				*szIdcRotorPosXNew = 0;
				g_iCbValue = MAXINT16;
			}
			else {
				// Process input
				swscanf_s(szIdcRotorPosXNew, L"%d", &g_iCbValue);
			}
			// Fall-through.
		case IDCANCEL:
			EndDialog(hWnd, wParam);
			return TRUE;
			break;
		}
		break;
	}
	return FALSE;
}


// Dialog für TX Einstellungen
// am HF Generator
static int AskTxSettings(HINSTANCE g_hInst, HWND hWnd)
{
	if (IDOK == DialogBox(g_hInst,
		MAKEINTRESOURCE(IDD_TX_SETTINGS),
		hWnd,
		(DLGPROC)AskTxSettings_CB)) {

	}
	return g_iCbValue;
}

BOOL CALLBACK AskTxSettings_CB(HWND hWnd,
	UINT   message,
	WPARAM wParam,
	LPARAM lParam)
{
	double lfIdcTxSettingsFrequency = 0.;
	double lfIdcTxSettingsPower = 0.;
	wchar_t szIdcTxSettingsFrequency[C_BUFSIZE] = { 0 };
	wchar_t szIdcTxSettingsPower[C_BUFSIZE] = { 0 };

	switch (message) {
	case WM_INITDIALOG:
		CheckDlgButton(hWnd, IDC_TX_SETTINGS_ON_CHECK, agentModel::getTxOnState());

		swprintf_s(szIdcTxSettingsFrequency, L"%.3f", agentModel::getTxFrequencyValue());
		SetDlgItemText(hWnd, IDC_TX_SETTINGS_F_EDIT, szIdcTxSettingsFrequency);
		
		swprintf_s(szIdcTxSettingsPower, L"%.1f", agentModel::getTxPwrValue());
		SetDlgItemText(hWnd, IDC_TX_SETTINGS_PWR_EDIT, szIdcTxSettingsPower);

		return (INT_PTR)TRUE;
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			agentModel::setTxOnState(BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_TX_SETTINGS_ON_CHECK));

			if (GetDlgItemText(hWnd, IDC_TX_SETTINGS_F_EDIT, szIdcTxSettingsFrequency, sizeof(szIdcTxSettingsFrequency) - 1)) {
				// Process input
				swscanf_s(szIdcTxSettingsFrequency, L"%lf", &lfIdcTxSettingsFrequency);
				agentModel::setTxFrequencyValue(lfIdcTxSettingsFrequency);
				agentModel::setRxFrequencyValue(lfIdcTxSettingsFrequency);
			}

			if (GetDlgItemText(hWnd, IDC_TX_SETTINGS_PWR_EDIT, szIdcTxSettingsPower, sizeof(szIdcTxSettingsPower) - 1)) {
				// Process input
				swscanf_s(szIdcTxSettingsPower, L"%lf", &lfIdcTxSettingsPower);
				agentModel::setTxPwrValue(lfIdcTxSettingsPower);
				agentModel::setRxLevelMaxValue(lfIdcTxSettingsPower);
			}
			// Fall-through.
		case IDCANCEL:
			EndDialog(hWnd, wParam);
			return TRUE;
			break;
		}
		break;
	}
	return FALSE;
}
