/* Source:		https://stackoverflow.com/questions/23862086/cdialogeventhandler-createinstance-identifier-not-found */
/* see also:	https://www.daniweb.com/programming/software-development/threads/394132/how-do-i-create-attach-an-event-handler-to-a-win7-style-ifilesavedialog */

#include "stdafx.h"

// COM-style
#include <shobjidl.h>



// Controls
#define CONTROL_GROUP           2000
#define CONTROL_RADIOBUTTONLIST 2
#define CONTROL_RADIOBUTTON1    1
#define CONTROL_RADIOBUTTON2    2       // It is OK for this to have the same IDas     CONTROL_RADIOBUTTONLIST,
// because it is a child control under CONTROL_RADIOBUTTONLIST

// IDs for the Task Dialog Buttons
#define IDC_BASICFILEOPEN                       100
#define IDC_ADDITEMSTOCUSTOMPLACES              101
#define IDC_ADDCUSTOMCONTROLS                   102
#define IDC_SETDEFAULTVALUESFORPROPERTIES       103
#define IDC_WRITEPROPERTIESUSINGHANDLERS        104
#define IDC_WRITEPROPERTIESWITHOUTUSINGHANDLERS 105

HWND ghMainWnd = 0;
HINSTANCE ghAppInst = 0;
RECT winRect;


void centerWnd(HWND parent_window);
void openDB();


class CDialogEventHandler : public IFileDialogEvents,
	public IFileDialogControlEvents
{
public:
	// IUnknown methods
	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv)
	{
		static const QITAB qit[] = {
			QITABENT(CDialogEventHandler, IFileDialogEvents),
			QITABENT(CDialogEventHandler, IFileDialogControlEvents),
			{ 0 },
		};
		return QISearch(this, qit, riid, ppv);
	}

	IFACEMETHODIMP_(ULONG) AddRef()
	{
		return InterlockedIncrement(&_cRef);
	}

	IFACEMETHODIMP_(ULONG) Release()
	{
		long cRef = InterlockedDecrement(&_cRef);
		if (!cRef)
			delete this;
		return cRef;
	}

	// IFileDialogEvents methods
	IFACEMETHODIMP OnFileOk(IFileDialog *) { return S_OK; };
	IFACEMETHODIMP OnFolderChange(IFileDialog *) { return S_OK; };
	IFACEMETHODIMP OnFolderChanging(IFileDialog *, IShellItem *) { return S_OK; };
	IFACEMETHODIMP OnHelp(IFileDialog *) { return S_OK; };
	IFACEMETHODIMP OnSelectionChange(IFileDialog *) { return S_OK; };
	IFACEMETHODIMP OnShareViolation(IFileDialog *, IShellItem *, FDE_SHAREVIOLATION_RESPONSE *) { return S_OK; };
	IFACEMETHODIMP OnTypeChange(IFileDialog *pfd);
	IFACEMETHODIMP OnOverwrite(IFileDialog *, IShellItem *, FDE_OVERWRITE_RESPONSE *) { return S_OK; };

	// IFileDialogControlEvents methods
	IFACEMETHODIMP OnItemSelected(IFileDialogCustomize *pfdc, DWORD dwIDCtl, DWORD dwIDItem);
	IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize *, DWORD) { return S_OK; };
	IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize *, DWORD, BOOL) { return S_OK; };
	IFACEMETHODIMP OnControlActivating(IFileDialogCustomize *, DWORD) { return S_OK; };

	CDialogEventHandler() : _cRef(1) { };
private:
	~CDialogEventHandler() { };
	long _cRef;
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_FILE_OPENDB:
			openDB();
			break;

		default:
			break;
		}

		break;
	case WM_LBUTTONDOWN:
		MessageBox(0, L"WM_LBUTTONDOWN message.", L"Msg", MB_OK);
		return 0;

	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			DestroyWindow(ghMainWnd);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR cmdLine, int showCmd)
{
	ghAppInst = hInstance;

	HMENU mMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1));

	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = ghAppInst;
	wc.hIcon = ::LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = ::LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) ::GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MyWndClassName";

	RegisterClass(&wc);

	centerWnd(GetDesktopWindow());

	ghMainWnd = ::CreateWindow("MyWndClassName", "Space Crusade Database Editor V1.0", WS_OVERLAPPEDWINDOW, winRect.left, winRect.top, 1280, 720, 0, mMenu, ghAppInst, 0);

	if (ghMainWnd == 0)
	{
		::MessageBox(0, "CreateWindow - Failed", 0, 0);
		return false;
	}

	ShowWindow(ghMainWnd, showCmd);
	UpdateWindow(ghMainWnd);

	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

void centerWnd(HWND parent_window)
{
	GetClientRect(parent_window, &winRect);
	winRect.left = (winRect.right / 2) - (1280 / 2);
	winRect.top = (winRect.bottom / 2) - (720 / 2);
}

void openDB()
{
	//Cocreate the file open dialog object
	IFileDialog *pfd = NULL;

	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

	if (SUCCEEDED(hr))
	{
		//Stuff needed for later
		const COMDLG_FILTERSPEC rgFExt[] = { { L"SQLite3 Database (*.sqlite)", L"*.sqlite" } };
		WCHAR fPath[MAX_PATH] = {};

		//Create event handling
		IFileDialogEvents *pfde = NULL;
		hr = CDialogEventHandler_CreateInstance(IID_PPV_ARGS(&pfde));

		if (SUCCEEDED(hr))
		{
			//Hook the event handler
			DWORD dwCookie;

			hr = pfd->Advise(pfde, &dwCookie);

			if (SUCCEEDED(hr))
			{
				//Set options for the dialog
				DWORD dwFlags;

				//Get options first so we do not override
				hr = pfd->GetOptions(&dwFlags);

				if (SUCCEEDED(hr))
				{
					//Get shell items only
					hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);

					if (SUCCEEDED(hr))
					{
						//Types of files to display (not default)
						hr = pfd->SetFileTypes(ARRAYSIZE(rgFExt), rgFExt);

						if (SUCCEEDED(hr))
						{
							//Set default file type to display
							hr = pfd->SetDefaultExtension(L"sqlite");

							if (SUCCEEDED(hr))
							{
								//Show dialog
								hr = pfd->Show(NULL);

								if (SUCCEEDED(hr))
								{
									//Get the result once the user clicks on open
									IShellItem *result;

									hr = pfd->GetResult(&result);

									if (SUCCEEDED(hr))
									{
										//Print out the file name
										PWSTR fName = NULL;

										hr = result->GetDisplayName(SIGDN_FILESYSPATH, &fName);

										if (SUCCEEDED(hr))
										{
											TaskDialog(NULL, NULL, L"File Name", fName, NULL, TDCBF_OK_BUTTON, TD_INFORMATION_ICON, NULL);
											CoTaskMemFree(fName);
										}

										result->Release();
									}
								}
							}
						}
					}
				}
			}

			pfd->Unadvise(dwCookie);
		}

		pfde->Release();
	}

	pfd->Release();
}


HRESULT CDialogEventHandler_CreateInstance(REFIID riid, void **ppv)
{
	*ppv = NULL;
	CDialogEventHandler *pDialogEventHandler = new (std::nothrow) CDialogEventHandler();
	HRESULT hr = pDialogEventHandler ? S_OK : E_OUTOFMEMORY;
	if (SUCCEEDED(hr))
	{
		hr = pDialogEventHandler->QueryInterface(riid, ppv);
		pDialogEventHandler->Release();
	}
	return hr;
}

HRESULT CDialogEventHandler::OnTypeChange(IFileDialog *pfd)
{
	IFileSaveDialog *pfsd;
	HRESULT hr = pfd->QueryInterface(&pfsd);
	if (SUCCEEDED(hr))
	{
		UINT uIndex;
		hr = pfsd->GetFileTypeIndex(&uIndex);   // index of current file-type
		if (SUCCEEDED(hr))
		{
			IPropertyDescriptionList *pdl = NULL;


		}
		pfsd->Release();
	}
	return hr;
}

// IFileDialogControlEvents
// This method gets called when an dialog control item selection happens (radio-button selection. etc).
// For sample sake, let's react to this event by changing the dialog title.
HRESULT CDialogEventHandler::OnItemSelected(IFileDialogCustomize *pfdc, DWORD dwIDCtl, DWORD dwIDItem)
{
	IFileDialog *pfd = NULL;
	HRESULT hr = pfdc->QueryInterface(&pfd);
	if (SUCCEEDED(hr))
	{
		if (dwIDCtl == CONTROL_RADIOBUTTONLIST)
		{
			switch (dwIDItem)
			{
			case CONTROL_RADIOBUTTON1:
				hr = pfd->SetTitle(L"Longhorn Dialog");
				break;

			case CONTROL_RADIOBUTTON2:
				hr = pfd->SetTitle(L"Vista Dialog");
				break;
			}
		}
		pfd->Release();
	}
	return hr;
}
