#include <windows.h>
#include <vector>
#include <sstream>
#include "urlhack.h"

static std::string browser_app;

void urlhack_launch_url(const char* app, const char *url)
{
	if (app) {
		ShellExecute(NULL, NULL, app, url, NULL, SW_SHOW);
		return;
	}

	if (browser_app.size() == 0) {
		// first let the OS try...
		if (reinterpret_cast<long>(ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL)) > 32) {
			return;
		}

		// Find out the default app
		HKEY key;
		DWORD dwValue;
		char *str;
		std::string lookup;

		if (RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice", 0, KEY_READ, &key) == ERROR_SUCCESS) {
			if (RegQueryValueEx(key, "Progid", NULL, NULL, NULL, &dwValue) == ERROR_SUCCESS)
			{
				str = new char[dwValue + 1];

				RegQueryValueEx(key, "Progid", NULL, NULL, (BYTE*)str, &dwValue);
				RegCloseKey(key);

				std::stringstream buffer;
				buffer << str << "\\shell\\open\\command";
				lookup = buffer.str();

				delete [] str;
			}
		}

		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, lookup.length() > 0 ? lookup.c_str() : "HTTP\\shell\\open\\command", 0, KEY_READ, &key) == ERROR_SUCCESS) {
			if (!RegQueryValueEx(key, NULL, NULL, NULL, NULL, &dwValue) == ERROR_SUCCESS) return;

			str = new char[dwValue + 1];

			RegQueryValueEx(key, NULL, NULL, NULL, (BYTE*)str, &dwValue);
			RegCloseKey(key);

			browser_app = str;
			delete[] str;

			// Drop all stuff from the path and leave only the executable and the path

			if (browser_app.at(0) == '"') {
				browser_app.erase(0, 1);
				
				if (browser_app.find('"') > 0)
					browser_app.resize(browser_app.find('"'));
			}
			else {
				if (browser_app.find(' ') > 0)
					browser_app.resize(browser_app.find(' '));
			}
		}
		else {
			MessageBox(NULL, "Could not find your default browser.", "PuTTY Tray Error", MB_OK | MB_ICONINFORMATION);
		}
	}

	std::string u = url;

	if (u.find("http://") == std::string::npos && u.find("https://") == std::string::npos &&
		u.find("ftp://") == std::string::npos && u.find("ftps://") == std::string::npos) {
		if (u.find("ftp.") != std::string::npos)
			u.insert(0, "ftp://");
		else
			u.insert(0, "http://");
	}

	ShellExecute(NULL, NULL, browser_app.c_str(), u.c_str(), NULL, SW_SHOW);
}

int urlhack_is_ctrl_pressed()
{
	return HIWORD(GetAsyncKeyState(VK_CONTROL));
}


void rtfm(const char *error)
{
	std::string error_msg = "The following error occured when compiling the regular expression\n" \
		                    "for the hyperlink support. Hyperlink detection is disabled during\n" \
							"this session (restart PuTTY Tray to try again).\n\n";

	std::string actual_error_msg = error_msg;

	actual_error_msg.append(error);

	MessageBox(0, actual_error_msg.c_str(), "PuTTY Tray Error", MB_OK);
}



