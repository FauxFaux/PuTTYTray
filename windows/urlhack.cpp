/*
 * HACK: PuttyTray / Nutty
 * Hyperlink stuff: CORE FILE! Don't forget to COPY IT TO THE NEXT VERSION
 */
#include <windows.h>
#include <vector>
#include <sstream>
#include "urlhack.h"

extern int urlhack_mouse_old_x = -1, urlhack_mouse_old_y = -1, urlhack_current_region = -1;

static std::vector<text_region> link_regions;
static std::string browser_app;

extern const char* urlhack_default_regex = "(((https?|ftp):\\/\\/)|www\\.)(([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)|localhost|([a-zA-Z0-9\\-]+\\.)*[a-zA-Z0-9\\-]+\\.(com|net|org|info|biz|gov|name|edu|[a-zA-Z][a-zA-Z]))(:[0-9]+)?((\\/|\\?)[^ \"]*[^ ,;\\.:\">)])?";


void urlhack_clear_link_regions()
{
	link_regions.clear();
}



int urlhack_is_in_link_region(int x, int y)
{
	std::vector<text_region>::iterator i = link_regions.begin();
	int loopCounter = 1;

	while (i != link_regions.end()) {
		text_region r = *i;

		if ((r.y0 == r.y1 && y == r.y0 && y == r.y1 && x >= r.x0 && x < r.x1) ||
			(r.y0 != r.y1 && ((y == r.y0 && x >= r.x0) || (y == r.y1 && x < r.x1) || (y > r.y0 && y < r.y1))))
			return loopCounter; //Changed: was return true

		loopCounter++;
		i++;
	}
	
	return false;
}



int urlhack_is_in_this_link_region(text_region r, int x, int y)
{
  	if ((r.y0 == r.y1 && y == r.y0 && y == r.y1 && x >= r.x0 && x < r.x1) || (r.y0 != r.y1 && ((y == r.y0 && x >= r.x0) || (y == r.y1 && x < r.x1) || (y > r.y0 && y < r.y1)))) {
		return true;
	}
	
	return false;
}



text_region urlhack_get_link_bounds(int x, int y)
{
	std::vector<text_region>::iterator i = link_regions.begin();

	while (i != link_regions.end()) {
		text_region r = *i;

		if ((r.y0 == r.y1 && y == r.y0 && y == r.y1 && x >= r.x0 && x < r.x1) ||
			(r.y0 != r.y1 && ((y == r.y0 && x >= r.x0) || (y == r.y1 && x < r.x1) || (y > r.y0 && y < r.y1))))
			return *i;

		i++;
	}

	text_region region;
	region.x0 = region.y0 = region.x1 = region.y1 = -1;
	return region;
}



text_region urlhack_get_link_region(int index)
{
	text_region region;

	if (index < 0 || index >= (int)link_regions.size()) {
		region.x0 = region.y0 = region.x1 = region.y1 = -1;
		return region;
	}
	else {
		return link_regions.at(index);
	}
}



void urlhack_add_link_region(int x0, int y0, int x1, int y1)
{
	text_region region;

	region.x0 = x0;
	region.y0 = y0;
	region.x1 = x1;
	region.y1 = y1;

	link_regions.insert(link_regions.end(), region);
}



void urlhack_launch_url(const char* app, const char *url)
{
	if (app) {
		ShellExecute(NULL, NULL, app, url, NULL, SW_SHOW);
		return;
	}

	if (browser_app.size() == 0) {
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



// Regular expression stuff

static int urlhack_disabled = 0;
static int is_regexp_compiled = 0;
static regexp* urlhack_rx;
static std::string text_mass;



void urlhack_reset()
{
	text_mass.clear();
}



void urlhack_putchar(char ch)
{
	char r00fles[2] = { ch, 0 };
	text_mass.append(r00fles);
}



static void rtfm(char *error)
{
	std::string error_msg = "The following error occured when compiling the regular expression\n" \
		                    "for the hyperlink support. Hyperlink detection is disabled during\n" \
							"this session (restart PuTTY Tray to try again).\n\n";

	std::string actual_error_msg = error_msg;

	actual_error_msg.append(error);

	MessageBox(0, actual_error_msg.c_str(), "PuTTY Tray Error", MB_OK);
}



void urlhack_set_regular_expression(const char* expression)
{
	is_regexp_compiled = 0;
	urlhack_disabled = 0;

	set_regerror_func(rtfm);
	urlhack_rx = regcomp(const_cast<char*>(expression));

	if (urlhack_rx == 0) {
		urlhack_disabled = 1;
	}

	is_regexp_compiled = 1;
}



void urlhack_go_find_me_some_hyperlinks(int screen_width)
{
	if (urlhack_disabled != 0) return;

	if (is_regexp_compiled == 0) {
		urlhack_set_regular_expression(urlhack_default_regex);
	}

	urlhack_clear_link_regions();


	char* text = const_cast<char*>(text_mass.c_str());
	char* text_pos = text;

	while (regexec(urlhack_rx, text_pos) == 1) {
		char* start_pos = *urlhack_rx->startp[0] == ' ' ? urlhack_rx->startp[0] + 1: urlhack_rx->startp[0];

		int x0 = (start_pos - text) % screen_width;
		int y0 = (start_pos - text) / screen_width;
		int x1 = (urlhack_rx->endp[0] - text) % screen_width;
		int y1 = (urlhack_rx->endp[0] - text) / screen_width;

		if (x0 >= screen_width) x0 = screen_width - 1;
		if (x1 >= screen_width) x1 = screen_width - 1;

		urlhack_add_link_region(x0, y0, x1, y1);

		text_pos = urlhack_rx->endp[0] + 1;
	}
}