#include <winsock2.h>
#include <windows.h>
#include <string.h>
#include "misc.h"
#include "urlhack.h"

enum
{
  MAX_STR = 4096
};

static int starts_with(const wchar_t *thing, const wchar_t *prefix)
{
  return 0 == wcsncmp(thing, prefix, wcslen(prefix));
}

void urlhack_launch_url(const char *app, const wchar_t *url)
{
  wchar_t *u;
  if (app) {
    wchar_t app_w[MAX_STR];
    size_t newlen;
    mbstowcs_s(&newlen, app_w, MAX_STR, app, MAX_STR);
    ShellExecuteW(NULL, NULL, app_w, url, NULL, SW_SHOW);
    return;
  }

  if ((long)ShellExecuteW(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL) > 32) {
    return;
  }

  // if the OS couldn't launch it, munge it towards a plausible url, then launch
  // that instead:
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

  if (!!wcscmp(url, u)) {
    ShellExecuteW(NULL, NULL, u, NULL, NULL, SW_SHOWNORMAL);
  }

  free(u);
}

int urlhack_is_ctrl_pressed()
{
  return HIWORD(GetAsyncKeyState(VK_CONTROL));
}

void rtfm(const char *error)
{
  char std_msg[] =
      "The following error occured when compiling the regular expression\n"
      "for the hyperlink support. Hyperlink detection is disabled during\n"
      "this session (restart PuTTY Tray to try again).\n\n";

  char *full_msg = dupprintf("%s%s", std_msg, error);

  MessageBox(0, full_msg, "PuTTY Tray Error", MB_OK);
  free(full_msg);
}
