#include <stdio.h>
#include <stdarg.h>

int get_l10n_setting(const char* keyname, char* buf, int size)
{
    return 0;
}

int xsprintf(char* buffer, const char* format, ...)
{
    int r;
    va_list args;
    va_start(args, format);
    r = vsprintf(buffer, format, args);
    va_end(args);
    return r;
}

#ifdef _WINDOWS
#undef _vsnprintf
#define vsnprintf _vsnprintf
#else
#undef vsnprintf
#endif
int xvsnprintf(char* buffer, int size, const char* format, va_list args)
{
  return vsnprintf(buffer, size, format, args);
}

char *l10n_dupstr (char *s) {
  return dupstr(s);
}
