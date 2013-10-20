/*
 * windefs.c: default settings that are specific to Windows.
 */

#include "putty.h"

#include <commctrl.h>

FontSpec *platform_default_fontspec(const char *name)
{
    FontSpec *ret;
    FontSpec tmp;
    tmp.name = snewn(128, char);
    if (!strcmp(name, "Font")) {
	if (!get_l10n_setting("_DEFAULTFONTNAME_", tmp.name, 128))
            strcpy(tmp.name, "Courier New");
	tmp.isbold = 0;
#ifdef ANSI_CHARSET
        {
            int charset;
            char buf[32];
            if (!get_l10n_setting("_DEFAULTFONTCHARSET_", buf, sizeof (buf)))
                charset = ANSI_CHARSET;
            else if (!strcmp(buf, "ANSI_CHARSET"))
                charset = ANSI_CHARSET;
#ifdef DEFAULT_CHARSET
            else if (!strcmp(buf, "DEFAULT_CHARSET"))
                charset = DEFAULT_CHARSET;
#endif // DEFAULT_CHARSET
#ifdef SYMBOL_CHARSET
            else if (!strcmp(buf, "SYMBOL_CHARSET"))
                charset = SYMBOL_CHARSET;
#endif // SYMBOL_CHARSET
#ifdef SHIFTJIS_CHARSET
            else if (!strcmp(buf, "SHIFTJIS_CHARSET")) 
                charset = SHIFTJIS_CHARSET;
#endif // SHIFTJIS_CHARSET
#ifdef HANGEUL_CHARSET
            else if (!strcmp(buf, "HANGEUL_CHARSET")) 
                charset = HANGEUL_CHARSET;
#endif // HANGEUL_CHARSET
#ifdef HANGUL_CHARSET
            else if (!strcmp(buf, "HANGUL_CHARSET")) 
                charset = HANGUL_CHARSET;
#endif // HANGUL_CHARSET
#ifdef GB2312_CHARSET
            else if (!strcmp(buf, "GB2312_CHARSET")) 
                charset = GB2312_CHARSET;
#endif // GB2312_CHARSET
#ifdef CHINESEBIG5_CHARSET
            else if (!strcmp(buf, "CHINESEBIG5_CHARSET")) 
                charset = CHINESEBIG5_CHARSET;
#endif // CHINESEBIG5_CHARSET
#ifdef OEM_CHARSET
            else if (!strcmp(buf, "OEM_CHARSET")) 
                charset = OEM_CHARSET;
#endif // OEM_CHARSET
#ifdef JOHAB_CHARSET
            else if (!strcmp(buf, "JOHAB_CHARSET")) 
                charset = JOHAB_CHARSET;
#endif // JOHAB_CHARSET
#ifdef HEBREW_CHARSET
            else if (!strcmp(buf, "HEBREW_CHARSET")) 
                charset = HEBREW_CHARSET;
#endif // HEBREW_CHARSET
#ifdef ARABIC_CHARSET
            else if (!strcmp(buf, "ARABIC_CHARSET")) 
                charset = ARABIC_CHARSET;
#endif // ARABIC_CHARSET
#ifdef GREEK_CHARSET
            else if (!strcmp(buf, "GREEK_CHARSET")) 
                charset = GREEK_CHARSET;
#endif // GREEK_CHARSET
#ifdef TURKISH_CHARSET
            else if (!strcmp(buf, "TURKISH_CHARSET")) 
                charset = TURKISH_CHARSET;
#endif // TURKISH_CHARSET
#ifdef VIETNAMESE_CHARSET
            else if (!strcmp(buf, "VIETNAMESE_CHARSET")) 
                charset = VIETNAMESE_CHARSET;
#endif // VIETNAMESE_CHARSET
#ifdef THAI_CHARSET
            else if (!strcmp(buf, "THAI_CHARSET")) 
                charset = THAI_CHARSET;
#endif // THAI_CHARSET
#ifdef EASTEUROPE_CHARSET
            else if (!strcmp(buf, "EASTEUROPE_CHARSET")) 
                charset = EASTEUROPE_CHARSET;
#endif // EASTEUROPE_CHARSET
#ifdef RUSSIAN_CHARSET
            else if (!strcmp(buf, "RUSSIAN_CHARSET")) 
                charset = RUSSIAN_CHARSET;
#endif // RUSSIAN_CHARSET
#ifdef MAC_CHARSET
            else if (!strcmp(buf, "MAC_CHARSET")) 
                charset = MAC_CHARSET;
#endif // MAC_CHARSET
#ifdef BALTIC_CHARSET
            else if (!strcmp(buf, "BALTIC_CHARSET")) 
                charset = BALTIC_CHARSET;
#endif // BALTIC_CHARSET
            else
                charset = ANSI_CHARSET;
	    tmp.charset = charset;
        }
#else // ANSI_CHARSET
	tmp.charset = 0;
#endif// ANSI_CHARSET
	tmp.height = 10;
    } else {
	tmp.name[0] = '\0';
	tmp.height = 0;
	tmp.charset = 0;
    }
    ret = fontspec_new(tmp.name, tmp.isbold, tmp.height, tmp.charset);
    sfree(tmp.name);
    return ret;
}

Filename *platform_default_filename(const char *name)
{
    if (!strcmp(name, "LogFileName"))
	return filename_from_str("putty.log");
    else
	return filename_from_str("");
}

char *platform_default_s(const char *name)
{
    if (!strcmp(name, "SerialLine"))
	return dupstr("COM1");
    return NULL;
}

int platform_default_i(const char *name, int def)
{
    return def;
}
