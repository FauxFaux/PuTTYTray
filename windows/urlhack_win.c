#include <windows.h>
#include <string.h>
#include "misc.h"
#include "urlhack.h"

#define MAX_STR 4096
static char browser_app[MAX_STR] = "";

static int starts_with(const char *thing, const char *prefix) {
	return 0 == strncmp(thing, prefix, strlen(prefix));
}

void urlhack_launch_url(const char* app, const char *url)
{
	char *u;
	if (app) {
		ShellExecute(NULL, NULL, app, url, NULL, SW_SHOW);
		return;
	}

	if (!strlen(browser_app)) {
		#define SUFFIX "\\shell\\open\\command"
		char str[MAX_STR] = "";
		HKEY key;
		DWORD dwValue = MAX_STR - sizeof(SUFFIX) - 1;

		// first let the OS try...
		if ((long)ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL) > 32) {
			return;
		}

		// Find out the default app
		if (RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice", 0, KEY_READ, &key) == ERROR_SUCCESS) {
			if (RegQueryValueEx(key, "Progid", NULL, NULL, (BYTE*)str, &dwValue) == ERROR_SUCCESS)
			{
				strcat(str, SUFFIX);
			}
			RegCloseKey(key);
		}

		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, strlen(str) ? str : "HTTP\\shell\\open\\command", 0, KEY_READ, &key) == ERROR_SUCCESS) {
			dwValue = MAX_STR;
			if (!RegQueryValueEx(key, NULL, NULL, NULL, (BYTE*)str, &dwValue) == ERROR_SUCCESS) {
				RegCloseKey(key);
				return;
			}
			RegCloseKey(key);
			
			// Drop all stuff from the path and leave only the executable and the path
			if (str[0] == '"') {
				char *p = strchr(str, '"');

				if (NULL != p)
					*p = 0;
				strcpy(browser_app, str+1);
			}
			else {
				char *p = strchr(str, '"');
				if (NULL != p)
					*p = 0;
				strcpy(browser_app, str);
			}
		}
		else {
			MessageBox(NULL, "Could not find your default browser.", "PuTTY Tray Error", MB_OK | MB_ICONINFORMATION);
		}
	}

	u = malloc(strlen(url) + 10);
	strcpy(u, url);

	if (!starts_with(url, "http://") && !starts_with(url, "https://") &&
		!starts_with(url, "ftp://") && !starts_with(url, "ftps://")) {
		if (strstr(url, "ftp.")) {
			strcpy(u, "ftp://");
			strcat(u, url);
		} else {
			strcpy(u, "http://");
			strcat(u, url);
		}
	}

	ShellExecute(NULL, NULL, browser_app, u, NULL, SW_SHOW);
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
