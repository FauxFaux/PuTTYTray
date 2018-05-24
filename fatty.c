#include "putty.h"

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
  char us[MAX_PATH];

  while (*cmdline && isspace(*cmdline))
    ++cmdline;

#define ARG_RUN(arg, method)                                                   \
  if (!strncmp(cmdline, arg, strlen(arg))) {                                   \
    return method(inst, prev, cmdline + strlen(arg), show);                    \
  }

  ARG_RUN("--as-gen", puttygen_main);
  ARG_RUN("--as-agent", pageant_main);
  ARG_RUN("--as-putty", putty_main);
#undef ARG_RUN

  if (GetModuleFileName(NULL, us, MAX_PATH)) {
    char *fn = strrchr(us, '\\');
    if (!fn)
      fn = us;
    else
      ++fn;

    if (!strncmp(fn, "puttygen", strlen("puttygen"))) {
      return puttygen_main(inst, prev, cmdline, show);
    }
    if (!strncmp(fn, "pageant", strlen("pageant"))) {
      return pageant_main(inst, prev, cmdline, show);
    }
  }

  return putty_main(inst, prev, cmdline, show);
}
