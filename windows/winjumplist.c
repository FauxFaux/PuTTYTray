#include "winjumplist.h"
#define COBJMACROS

// this must compile with UNICODE or it mysteriously doesn't work. >.<
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <propsys.h>
#include <propkey.h>

#define NAME L"PuttyJump"

void jumplist_startup() {
	CoInitialize(NULL);
	SetCurrentProcessExplicitAppUserModelID(NAME);
}

void propogate(const char *name) {
	IShellLink *link = NULL;
	WCHAR path_to_app[MAX_PATH];
	IPropertyStore* prop_store;
	SHARDAPPIDINFOLINK appinfo;

	HRESULT hr;

	hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLink, (LPVOID*)&link);
	if (FAILED(hr)) {
		return;
	}

	GetModuleFileName(NULL, path_to_app, MAX_PATH);
	IShellLinkW_SetPath(link, path_to_app);
	IShellLinkW_SetIconLocation(link, path_to_app, 0);

	{
		WCHAR cmdline[MAX_PATH] = {0};
		WCHAR bits[MAX_PATH] = {0};
		mbstowcs(bits, name, MAX_PATH);
		wcscat_s(cmdline, MAX_PATH, L"-load \"");
		wcscat_s(cmdline, MAX_PATH, bits);
		wcscat_s(cmdline, MAX_PATH, L"\"");
		IShellLinkW_SetArguments(link, cmdline);
	}

	hr = IUnknown_QueryInterface(link, &IID_IPropertyStore, &prop_store);

	if(SUCCEEDED(hr)) {
		PROPVARIANT pv;
		PROPERTYKEY PKEY_Title;

		pv.vt=VT_LPSTR;
		pv.pszVal = name;

		CLSIDFromString(L"{F29F85E0-4FF9-1068-AB91-08002B27B3D9}", &(PKEY_Title.fmtid));
		PKEY_Title.pid=2;

		hr = IPropertyStore_SetValue(prop_store, &PKEY_Title, &pv); // THIS is where the displayed title is actually set
		if (FAILED(hr)) {
			MessageBox(0, L"Can't set value.", NULL, 0);
			return;
		}

		IPropertyStore_Commit(prop_store);
		IPropertyStore_Release(prop_store);
	}

	appinfo.pszAppID = NAME;
	appinfo.psl = link;
	SHAddToRecentDocs(SHARD_APPIDINFOLINK, &appinfo);
	IUnknown_Release(link);
}
