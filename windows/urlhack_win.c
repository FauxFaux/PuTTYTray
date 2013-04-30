#include <windows.h>
#include <string.h>
#include "misc.h"
#include "urlhack.h"

#define MAX_STR 4096
static wchar_t browser_app[MAX_STR] = L"";

static int starts_with(const wchar_t *thing, const wchar_t *prefix) {
    return 0 == wcsncmp(thing, prefix, wcslen(prefix));
}

void urlhack_launch_url(const char* app, const wchar_t *url)
{
	wchar_t *u;
	if (app) {
            wchar_t app_w[MAX_STR];
            size_t newlen;
            mbstowcs_s(&newlen, app_w, MAX_STR, app, MAX_STR);
	    ShellExecuteW(NULL, NULL, app_w, url, NULL, SW_SHOW);
	    return;
	}

	if (!wcslen(browser_app)) {
		#define SUFFIX L"\\shell\\open\\command"
		wchar_t str[MAX_STR] = L"";
		HKEY key;
		DWORD dwValue = MAX_STR - sizeof(SUFFIX) - 1;

		// first let the OS try...
		if ((long)ShellExecuteW(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL) > 32) {
			return;
		}

		// Find out the default app
		if (RegOpenKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice",
                        0, KEY_READ, &key) == ERROR_SUCCESS) {
		    if (RegQueryValueExW(key, L"Progid", NULL, NULL, (BYTE*)str, &dwValue) == ERROR_SUCCESS) {
			wcscat(str, SUFFIX);
		    }
		    RegCloseKey(key);
		}

		if (RegOpenKeyExW(HKEY_CLASSES_ROOT, wcslen(str) ? str : L"HTTP\\shell\\open\\command",
                    0, KEY_READ, &key) == ERROR_SUCCESS) {
			dwValue = MAX_STR;
			if (!RegQueryValueExW(key, NULL, NULL, NULL, (BYTE*)str, &dwValue) == ERROR_SUCCESS) {
				RegCloseKey(key);
				return;
			}
			RegCloseKey(key);
			
			// Drop all stuff from the path and leave only the executable and the path
			if (str[0] == '"') {
				wchar_t *p = wcschr(str, L'"');

				if (NULL != p)
					*p = 0;
				wcscpy(browser_app, str+1);
			}
			else {
				wchar_t *p = wcschr(str, L'"');
				if (NULL != p)
					*p = 0;
				wcscpy(browser_app, str);
			}
		}
		else {
			MessageBox(NULL, "Could not find your default browser.", "PuTTY Tray Error", MB_OK | MB_ICONINFORMATION);
		}
	}

	u = snewn(wcslen(url) + 10, wchar_t);
	wcscpy(u, url);

	if (!starts_with(url, L"http://") && !starts_with(url, L"https://") &&
	    !starts_with(url, L"ftp://") && !starts_with(url, L"ftps://")) {
	    if (wcsstr(url, L"ftp.")) {
		wcscpy(u, L"ftp://");
		wcscat(u, url);
	    } else {
		wcscpy(u, L"http://");
		wcscat(u, url);
	    }
	}

	ShellExecuteW(NULL, NULL, browser_app, u, NULL, SW_SHOW);
	free(u);
}

int urlhack_is_ctrl_pressed()
{
	return HIWORD(GetAsyncKeyState(VK_CONTROL));
}


void rtfm(const char *error)
{
    char std_msg[] = "The following error occured when compiling the regular expression\n" \
        "for the hyperlink support. Hyperlink detection is disabled during\n" \
        "this session (restart PuTTY Tray to try again).\n\n";

    char *full_msg = dupprintf("%s%s", std_msg, error);

    MessageBox(0, full_msg, "PuTTY Tray Error", MB_OK);
    free(full_msg);
}
