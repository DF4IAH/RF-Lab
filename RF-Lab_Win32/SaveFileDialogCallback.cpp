/* Source:	https://www.daniweb.com/programming/software-development/threads/394132/how-do-i-create-attach-an-event-handler-to-a-win7-style-ifilesavedialog */

#include "stdafx.h"

// COM-style
#include <shobjidl.h>



class SaveFileDialogCallback : public IFileDialogEvents, public IFileDialogControlEvents {
public:
	SaveFileDialogCallback(FileDialog * pFileDialog) { mFileDialog = pFileDialog; }
	FileDialog * mFileDialog;
	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
		static const QITAB qit[] = {
			QITABENT(SaveFileDialogCallback, IFileDialogEvents),
			QITABENT(SaveFileDialogCallback, IFileDialogControlEvents),
			{ 0 },
		};
		return QISearch(this, qit, riid, ppv);
	}
	// This class makes special assumptions about how it is used, specifically
	// 1) This class will only reside on the stack.
	// 2) Components that consume this object have well-defined reference lifetimes.
	//    In this case, this is only consumed by the file dialog advise and unadvise.
	//    Unadvising will release the file dialog's only reference to this object.
	//
	// Do not do this for heap allocated objects.
	IFACEMETHODIMP_(ULONG) AddRef() { return 3; }
	IFACEMETHODIMP_(ULONG) Release() { return 2; }
	// IFileDialogEvents
	IFACEMETHODIMP OnFileOk(IFileDialog * /* pfd */) { return E_NOTIMPL; }
	IFACEMETHODIMP OnFolderChanging(IFileDialog * /* pfd */, IShellItem * /* psi */) { return E_NOTIMPL; }
	IFACEMETHODIMP OnFolderChange(IFileDialog * /* pfd */) { return E_NOTIMPL; }
	IFACEMETHODIMP OnSelectionChange(IFileDialog * /* pfd */) { return E_NOTIMPL; }
	IFACEMETHODIMP OnShareViolation(IFileDialog * /* pfd */, IShellItem * /* psi */, FDE_SHAREVIOLATION_RESPONSE * /* pResponse */) { return E_NOTIMPL; }
	IFACEMETHODIMP OnTypeChange(IFileDialog * /* pfd */) { return E_NOTIMPL; }
	IFACEMETHODIMP OnOverwrite(IFileDialog * /* pfd */, IShellItem * /* psi */, FDE_OVERWRITE_RESPONSE * /* pResponse */) { return E_NOTIMPL; }
	// IFileDialogControlEvents
	IFACEMETHODIMP OnItemSelected(IFileDialogCustomize * /* pfdc */, DWORD /* dwIDCtl */, DWORD /* dwIDItem */) { return E_NOTIMPL; }
	IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize * pFileDialogCustomize, DWORD pIDControl);
	IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize * /* pfdc */, DWORD /* dwIDCtl */, BOOL /* bChecked */) { return E_NOTIMPL; }
	IFACEMETHODIMP OnControlActivating(IFileDialogCustomize * /* pfdc */, DWORD /* dwIDCtl */) { return E_NOTIMPL; }
};

IFACEMETHODIMP SaveFileDialogCallback::OnButtonClicked(IFileDialogCustomize * pFileDialogCustomize, DWORD pIDControl) {
	if (pIDControl == cOptionsButton) {
		IFileDialog *pFileDialog;
		HRESULT result = pFileDialogCustomize->QueryInterface(&pFileDialog);
		if (SUCCEEDED(result)) {
			unsigned int fileTypeIndex;
			result = pFileDialog->GetFileTypeIndex(&fileTypeIndex);
			if (SUCCEEDED(result)) {
				HWND   hwndDialog;
				hwndDialog = ::GetParent((HWND)mFileDialog->GetHWND());
				HWND w = GetFocus();
				EnableWindow(hwndDialog, false);
				mFileDialog->ClickButton(fileTypeIndex - 1, true);
				EnableWindow(hwndDialog, true);
				SetFocus(w);
				return S_OK;
			}
		}
	}
	return S_FALSE;
}


HRESULT Win7SaveFileDialog(FileDialog * pFileDialog, OPENFILENAME & pFileDialogStruct, const wxString & pFilterStrings, const bool pDoingProject) {
	IFileSaveDialog * saveFileDialog;
	HRESULT result = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&saveFileDialog));
	FILEOPENDIALOGOPTIONS saveFileDialogOptions;
	result = saveFileDialog->GetOptions(&saveFileDialogOptions);
	if (SUCCEEDED(result)) {
		if (saveFileDialogOptions & FOS_OVERWRITEPROMPT) {
			saveFileDialogOptions -= FOS_OVERWRITEPROMPT;
			result = saveFileDialog->SetOptions(saveFileDialogOptions);
		}
	}
	if (FAILED(result)) {
		wxMessageBox(_("Could not CoCreateInstance for file save dialog"));
		return result;
	}
	else {
		wxString dialogTitle(pFileDialogStruct.lpstrTitle);
		if (!dialogTitle.IsEmpty())
			result = saveFileDialog->SetTitle(pFileDialogStruct.lpstrTitle);
		else
			result = saveFileDialog->SetTitle(_("Save"));
		unsigned int arraySize = 0;
		wxString defaultExtension = wxEmptyString;
		wxArrayString descriptions, extensions;
		COMDLG_FILTERSPEC * allowedTypes =
			AllowedSaveFileTypes(pFilterStrings, &arraySize, pDoingProject, defaultExtension, descriptions, extensions);
		if (allowedTypes) {
			result = saveFileDialog->SetFileTypes(arraySize, allowedTypes);
			delete[] allowedTypes;
		}
		if (SUCCEEDED(result)) result = saveFileDialog->SetOkButtonLabel(_("Save"));
		if (SUCCEEDED(result)) {
			if (pDoingProject) result = saveFileDialog->SetDefaultExtension(wxT("aup"));
			else result = saveFileDialog->SetDefaultExtension(pFileDialogStruct.lpstrDefExt);
		}
		if (SUCCEEDED(result)) {
			saveFileDialog->SetFileTypeIndex(pFileDialogStruct.nFilterIndex);
		}
		if (SUCCEEDED(result)) {
			wxString defaultFileName(pFileDialogStruct.lpstrFile);
			if (!defaultFileName.IsEmpty()) {
				wxFileName fileName(defaultFileName);;
				result = saveFileDialog->SetFileName(fileName.GetName());
			}
		}
		if (!pDoingProject && SUCCEEDED(result)) {
			IFileDialogCustomize * customize = NULL;
			result = saveFileDialog->QueryInterface(&customize);
			if (SUCCEEDED(result)) {
				customize->AddPushButton(cOptionsButton, _(" Export Format Settings "));
				saveFileDialog->Release();
			}
		}
		if (SUCCEEDED(result)) {
			SaveFileDialogCallback fileDialogCallback(pFileDialog);
			DWORD eventCookie;
			result = saveFileDialog->Advise(&fileDialogCallback, &eventCookie);
			if (SUCCEEDED(result)) {
				result = saveFileDialog->Show(pFileDialogStruct.hwndOwner);
				if (SUCCEEDED(result)) {
					IShellItem * shellItem;
					result = saveFileDialog->GetResult(&shellItem);
					if (SUCCEEDED(result)) {
						LPOLESTR shellItemName = NULL;
						result = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &shellItemName);
						if (SUCCEEDED(result)) {
							unsigned int fileTypeIndex;
							result = saveFileDialog->GetFileTypeIndex(&fileTypeIndex);
							if (SUCCEEDED(result)) pFileDialogStruct.nFilterIndex = fileTypeIndex;
							wxString itemName(shellItemName);
							wxStrncpy(pFileDialogStruct.lpstrFile, shellItemName, itemName.Len());
							CoTaskMemFree(shellItemName);
						}
					}
				}
				saveFileDialog->Unadvise(eventCookie);
			}
		}
	}
	return result;
}

