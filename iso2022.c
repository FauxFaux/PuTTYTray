#include <string.h>
#include "putty.h"
#include "iso2022.h"

#define strlenu(s) strlen ((char *)s)

int iso2022_win95flag;

#ifdef _WINDOWS
#define UCS2CHAR WCHAR

static int
get_win95flag (void)
{
  OSVERSIONINFO ovi;

  ovi.dwOSVersionInfoSize = sizeof ovi;
  GetVersionEx (&ovi);
  return ovi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS;
}

static int
wchar_to_utf8 (UCS2CHAR *src, unsigned char *dest, int destlen, BOOL *err)
{
  return WideCharToMultiByte (CP_UTF8, 0, src, -1, (char *)dest, destlen, 0,
			      err);
}

static int
wchar_to_cp932 (UCS2CHAR *src, unsigned char *dest, int destlen, BOOL *err)
{
  return WideCharToMultiByte (932, 0, src, -1, (char *)dest, destlen, 0, err);
}

static int
wchar_to_cp950 (UCS2CHAR *src, unsigned char *dest, int destlen, BOOL *err)
{
  return WideCharToMultiByte (950, 0, src, -1, (char *)dest, destlen, 0, err);
}

static void
utf8_to_wchar (unsigned char *src, UCS2CHAR *dest, int destlen)
{
  MultiByteToWideChar (CP_UTF8, 0, (char *)src, -1, dest, destlen);
}

static void
cp932_to_wchar (unsigned char *src, UCS2CHAR *dest, int destlen)
{
  MultiByteToWideChar (932, 0, (char *)src, -1, dest, destlen);
}

static void
cp950_to_wchar (unsigned char *src, UCS2CHAR *dest, int destlen)
{
  MultiByteToWideChar (950, 0, (char *)src, -1, dest, destlen);
}
#else
#include <iconv.h>
#include <stdlib.h>
#define BOOL int
#define UCS2CHAR uint16_t

static int
get_win95flag (void)
{
  return 0;
}

static int
call_iconv (const char *fromcode, char *src, size_t srclen,
	    const char *tocode, char *dest, int destlen, int to_w, BOOL *err)
{
  iconv_t cd;
  size_t outbytesleft;

  outbytesleft = destlen - 1;
  if (to_w)
    outbytesleft--;
  cd = iconv_open (tocode, fromcode);
  if (cd == (iconv_t)-1)
    {
      char msg[100];

      snprintf (msg, 100, "iconv_open (\"%s\", \"%s\")", tocode, fromcode);
      perror (msg);
      exit (1);
    }
  if (err)
    *err = 0;
  if (iconv (cd, &src, &srclen, &dest, &outbytesleft) == -1 && err)
    *err = 1;
  iconv_close (cd);
  *dest++ = 0;
  if (to_w)
    *dest++ = 0;
  return destlen - outbytesleft;
}

static int
wchar_to_utf8 (UCS2CHAR *src, unsigned char *dest, int destlen, BOOL *err)
{
  size_t srclen;

  for (srclen = 0; src[srclen] != 0; srclen++);
  return call_iconv ("UCS-2LE", (char *)src, srclen * 2,
		     "UTF-8", (char *)dest, destlen, 0, err);
}

static int
wchar_to_cp932 (UCS2CHAR *src, unsigned char *dest, int destlen, BOOL *err)
{
  size_t srclen;

  for (srclen = 0; src[srclen] != 0; srclen++);
  return call_iconv ("UCS-2LE", (char *)src, srclen * 2,
		     "MS_KANJI", (char *)dest, destlen, 0, err);
}

static int
wchar_to_cp950 (UCS2CHAR *src, unsigned char *dest, int destlen, BOOL *err)
{
  size_t srclen;

  for (srclen = 0; src[srclen] != 0; srclen++);
  return call_iconv ("UCS-2LE", (char *)src, srclen * 2,
		     "CP950", (char *)dest, destlen, 0, err);
}

static void
utf8_to_wchar (unsigned char *src, UCS2CHAR *dest, int destlen)
{
  call_iconv ("UTF-8", (char *)src, strlen ((char *)src),
	      "UCS-2LE", (char *)dest, destlen * 2, 1, NULL);
}

static void
cp932_to_wchar (unsigned char *src, UCS2CHAR *dest, int destlen)
{
  /* "MS_KANJI" is better than "CP932" for avoiding mojibake. */
  call_iconv ("MS_KANJI", (char *)src, strlen ((char *)src),
	      "UCS-2LE", (char *)dest, destlen * 2, 1, NULL);
}

static void
cp950_to_wchar (unsigned char *src, UCS2CHAR *dest, int destlen)
{
  call_iconv ("CP950", (char *)src, strlen ((char *)src),
	      "UCS-2LE", (char *)dest, destlen * 2, 1, NULL);
}
#endif

static int
buflen (struct iso2022struct *p)
{
  return (p->buflen ? p->buflen - p->bufoff : 0) + p->inslen;
}

int
iso2022_buflen (struct iso2022_data *this)
{
  return buflen (&this->rcv);
}

int
iso2022_tbuflen (struct iso2022_data *this)
{
  return buflen (&this->trns);
}

void
iso2022_tbufclear (struct iso2022_data *this)
{
  this->trns.buflen = this->trns.bufoff = 0;
}

void
iso2022_settranschar (struct iso2022_data *this, int value)
{
  this->trns.transchar = value;
}

static unsigned char
getbuf (struct iso2022struct *p)
{
  if (p->inslen)
    {
      p->inslen--;
      p->width = *p->insw++;
      return *p->ins++;
    }
  return p->buf[p->bufoff++];
}

unsigned char
iso2022_getbuf (struct iso2022_data *this)
{
  return getbuf (&this->rcv);
}

unsigned char
iso2022_tgetbuf (struct iso2022_data *this)
{
  return getbuf (&this->trns);
}

int
iso2022_width_sub (struct iso2022_data *this, wchar_t c)
{
  if (this->rcv.width == 0)
    {
      if (this->rcv.gr->type == UTF8CJK)
	return -1;
      else if (this->rcv.gr->type == UTF8NONCJK)
	return -2;
      return 1;			/* This shouldn't happen */
    }
  else
    return this->rcv.width;
}

static int
utf8len (unsigned char firstbyte)
{
  int j = 0;
  unsigned char c = firstbyte;

  if ((c & 0x80) == 0)
    j = 1;
  else if ((c & 0xe0) == 0xc0)
    j = 2;
  else if ((c & 0xf0) == 0xe0)
    j = 3;
  else if ((c & 0xf8) == 0xf0)
    j = 4;
  else if ((c & 0xfc) == 0xf8)
    j = 5;
  else if ((c & 0xfe) == 0xfc)
    j = 6;
  return j;
}

static void
setg_94_n (struct iso2022struct *q, struct g *p)
{
  p->len = 2;
  p->type = UNKNOWN;
  if (q->bufoff <= 4)
    {
      switch (q->buf[q->bufoff - 1])
	{
	case '@': p->type = JISC6226_1978; break;
	case 'B':
	  if (q->jisx02081990flag)
	    q->jisx02081990flag = 0, p->type = JISX0208_1990;
	  else
	    p->type = JISX0208_1983;
	  break;
	case 'D': p->type = JISX0212_1990; break;
	case 'A': p->type = GB2312_80; break;
	case 'G': p->type = CSIC_SET1; break;
	case 'H': p->type = CSIC_SET2; break;
	case 'I': p->type = CSIC_SET3; break;
	case 'J': p->type = CSIC_SET4; break;
	case 'K': p->type = CSIC_SET5; break;
	case 'L': p->type = CSIC_SET6; break;
	case 'M': p->type = CSIC_SET7; break;
	case 'C': p->type = KSC5601_1987; break;
	case 'O': p->type = JISX0213_1; break;
	case 'P': p->type = JISX0213_2; break;
	case 'Q': p->type = JISX0213_2004_1; break;
	case 'E':
	  p->type = UNSUPPORTED;
	  break;
	}
    }
}

static void
setg_96_n (struct iso2022struct *q, struct g *p)
{
  p->type = UNKNOWN;
  p->len = 1;
}

static void
setg_96_1 (struct iso2022struct *q, struct g *p)
{
  p->len = 1;
  p->type = UNKNOWN;
  if (q->bufoff == 3)
    {
      switch (q->buf[q->bufoff - 1])
	{
	case 'A': p->type = ISO8859_1; break;
	case 'B': p->type = ISO8859_2; break;
	case 'C': p->type = ISO8859_3; break;
	case 'D': p->type = ISO8859_4; break;
	case 'L': p->type = ISO8859_5; break;
	case 'G': p->type = ISO8859_6; break;
	case 'F': p->type = ISO8859_7; break;
	case 'H': p->type = ISO8859_8; break;
	case 'M': p->type = ISO8859_9; break;
	case 'V': p->type = ISO8859_10; break;
	case 'Z':
	  p->type = UNSUPPORTED;
	  break;
	}
    }
}

static void
setg_94_1 (struct iso2022struct *q, struct g *p)
{
  p->len = 1;
  p->type = UNKNOWN;
  if (q->bufoff == 3)
    {
      switch (q->buf[q->bufoff - 1])
	{
	case 'I': p->type = JISX0201_KATAKANA; break;
	case 'J': p->type = JISX0201_ROMAN; break;
	case 'B': p->type = US_ASCII; break;
	case '0': p->type = VT100GRAPHICS; break;
	case '@': p->type = ISO646_1973IRV; break;
	case 'A': p->type = BS4730; break;
	case 'C': p->type = NATS_PRIMARY_FINLAND_SWEDEN; break;
	case 'E': p->type = NATS_PRIMARY_DENMARK_NORWAY; break;
	case 'K': p->type = DIN66003; break;
	case 'R': p->type = NFZ62010_1973; break;
	case 'Y': p->type = ISO646_ITALIAN; break;
	case 'Z': p->type = ISO646_SPANISH; break;
	case 'D':
	case 'F':
	case 'G':
	case 'H':
	case 'L':
	case 'N':
	case 'U':
	case 'X':
	case '[':
	case '\\':
	case 'M':
	case 'O':
	  p->type = UNSUPPORTED;
	  break;
	}
    }
}

static void
translate (struct iso2022struct *q, struct g *p)
{
  UCS2CHAR buf2[100];

  q->buflen = 0;
  q->width = p->len;
  switch (p->type)
    {
    case US_ASCII:
      buf2[0] = q->buf[0] & 0x7f;
      buf2[1] = 0;
      q->buflen = wchar_to_utf8 (buf2, q->buf, 100, NULL);
      if (q->buflen)
	q->buflen--;
      break;
    case JISX0201_ROMAN:
      buf2[0] = q->buf[0] & 0x7f;
      buf2[1] = 0;
      if (buf2[0] == 0x5c) buf2[0] = 0xa5;
      if (buf2[0] == 0x7e) buf2[0] = 0x203e;
      q->buflen = wchar_to_utf8 (buf2, q->buf, 100, NULL);
      if (q->buflen)
	q->buflen--;
      break;
    case JISX0201_KATAKANA:
      buf2[0] = q->buf[0] & 0x7f;
      buf2[0] += 0xff40;
      buf2[1] = 0;
      q->buflen = wchar_to_utf8 (buf2, q->buf, 100, NULL);
      if (q->buflen)
	q->buflen--;
      break;
#define A(a) q->buf[0] &= 0x7f; buf2[0] = q->buf[0] >= 0x20 ? a[q->buf[0] - 0x20] : 0; buf2[1] = 0; \
q->buflen = wchar_to_utf8 (buf2, q->buf, 100, NULL); \
if (q->buflen) q->buflen--;
    case ISO8859_1: A(iso_8859_1); break;
    case ISO8859_2: A(iso_8859_2); break;
    case ISO8859_3: A(iso_8859_3); break;
    case ISO8859_4: A(iso_8859_4); break;
    case ISO8859_5: A(iso_8859_5); break;
    case ISO8859_6: A(iso_8859_6); break;
    case ISO8859_7: A(iso_8859_7); break;
    case ISO8859_8: A(iso_8859_8); break;
    case ISO8859_9: A(iso_8859_9); break;
    case ISO8859_10: A(iso_8859_10); break;
    case ISO646_1973IRV: A(iso646_1973irv); break;
    case BS4730: A(bs4730); break;
    case NATS_PRIMARY_FINLAND_SWEDEN: A(nats_primary_finland_sweden); break;
    case NATS_PRIMARY_DENMARK_NORWAY: A(nats_primary_denmark_norway); break;
    case DIN66003: A(din66003); break;
    case NFZ62010_1973: A(nfz62010_1973); break;
    case ISO646_ITALIAN: A(iso646_italian); break;
    case ISO646_SPANISH: A(iso646_spanish); break;
    case VT100GRAPHICS:
      buf2[0] = q->buf[0] & 0x7f;
      buf2[0] |= 0xd900;        /* == CSET_LINEDRW (defined in putty.h) */
      buf2[1] = 0;
      q->buf[3] = 0;
      q->buf[2] = 0x80 | (buf2[0] & 0x3f), buf2[0] >>= 6;
      q->buf[1] = 0x80 | (buf2[0] & 0x3f), buf2[0] >>= 6;
      q->buf[0] = 0xe0 | (buf2[0] & 0xf);
      q->buflen = 3;
      break;
      q->buf[0] &= 0x7f; buf2[0] = q->buf[0] >= 0x20 ? q->buf[0] : 0;
      if (q->buf[0] >= 0x60 && q->buf[0] <= 0x7f)
        buf2[0] = unitab_xterm_std[q->buf[0] - 0x60];
      q->buflen = wchar_to_utf8 (buf2, q->buf, 100, NULL);
      if (q->buflen) q->buflen--;
      break;
#undef A
#define A(a) q->buf[0] &= 0x7f;q->buf[1] &= 0x7f;q->buflen = 0; \
if (q->buf[0] >= 0x21 && q->buf[0] <= 0x7e && q->buf[1] >= 0x21 && q->buf[1] <= 0x7e) \
{buf2[0] = a[q->buf[0] - 0x21][q->buf[1] - 0x21];buf2[1] = 0; \
q->buflen = wchar_to_utf8 (buf2, q->buf, 100, NULL); \
if (q->buflen)q->buflen--;}
/* This is a workaround for wave dash problem */
#define B(a) if((q->buf[0]&0x7f)>=0x30){A(a);}else{ \
q->buf[0]&=0x7f;q->buf[1]&=0x7f;q->buf[2]=q->buf[0]-0x21; \
q->buf[3]=q->buf[1]-0x21+((q->buf[2]%2)?94:0)+64; \
q->buf[0]=q->buf[2]/2+129;q->buf[1]=q->buf[3]+((q->buf[3]>=0x7f)?1:0); \
q->buf[2]=0; cp932_to_wchar (q->buf, buf2, 100); \
q->buflen = wchar_to_utf8 (buf2, q->buf, 100, NULL); \
if (q->buflen)q->buflen--;}
    case JISC6226_1978: B(jisc6226_1978); break;
    case JISX0208_1983: B(jisx0208_1983); break;
    case JISX0208_1990: B(jisx0208_1990); break;
    case JISX0212_1990: A(jisx0212_1990); break;
    case JISX0213_1: A(jisx0213_1); break;
    case JISX0213_2: A(jisx0213_2); break;
    case JISX0213_2004_1: A(jisx0213_2004_1); break;
    case GB2312_80: A(gb2312_80); break;
    case CSIC_SET1: A(csic_set1); break;
    case CSIC_SET2: A(csic_set2); break;
    case CSIC_SET3: A(csic_set3); break;
    case CSIC_SET4: A(csic_set4); break;
    case CSIC_SET5: A(csic_set5); break;
    case CSIC_SET6: A(csic_set6); break;
    case CSIC_SET7: A(csic_set7); break;
    case KSC5601_1987: A(ksc5601_1987); break;
#undef B
#undef A
    case MS_KANJI:
      q->width = (q->buf[0] >= 160 && q->buf[0] < 224) ? 1 : 2;
      q->buf[q->width] = 0;
      cp932_to_wchar (q->buf, buf2, 100);
      q->buflen = wchar_to_utf8 (buf2, q->buf, 100, NULL);
      break;
    case BIG5:
      q->width = 2;
      q->buf[q->width] = 0;
      cp950_to_wchar (q->buf, buf2, 100);
      q->buflen = wchar_to_utf8 (buf2, q->buf, 100, NULL);
      break;
    case UTF8CJK:
    case UTF8NONCJK:
    case UNKNOWN:
    case UNSUPPORTED:
      break;
    }
  if (!q->buflen)
    {
      q->buf[0] = '?';
      q->buf[1] = 0;
      q->buflen = 1;
      q->width = p->len;
    }
  q->bufoff = 0;
  if (q->ssl)
    q->gl = q->ssl, q->gr = q->ssr, q->ssl = 0, q->ssr = 0;
}

static int
try0 (struct g *p, UCS2CHAR buf2, unsigned char *buf)
{
  int i, j;

  switch (p->type)
    {
    case US_ASCII:
      if (buf2 >= 0x80)
	return 0;
      buf[0] = buf2 & 0x7f;
      buf[1] = 0;
      return 1;
    case JISX0201_ROMAN:
      if (buf2 == 0xa5)
	buf2 = 0x5c;
      if (buf2 == 0x203e)
	buf2 = 0x7e;
      if (buf2 >= 0x80)
	return 0;
      buf[0] = buf2 & 0x7f;
      buf[1] = 0;
      return 1;
    case JISX0201_KATAKANA:
      if (buf2 < 0xff61)
	return 0;
      if (buf2 > 0xff9f)
	return 0;
      buf[0] = buf2 - 0xff40;
      buf[1] = 0;
      return 1;
#define A(a) for(i=0;i<96;i++)if(a[i]==buf2)break;if(i==96)return 0; \
buf[0]=i+32;buf[1]=0;return 1;
    case ISO8859_1: A(iso_8859_1); break;
    case ISO8859_2: A(iso_8859_2); break;
    case ISO8859_3: A(iso_8859_3); break;
    case ISO8859_4: A(iso_8859_4); break;
    case ISO8859_5: A(iso_8859_5); break;
    case ISO8859_6: A(iso_8859_6); break;
    case ISO8859_7: A(iso_8859_7); break;
    case ISO8859_8: A(iso_8859_8); break;
    case ISO8859_9: A(iso_8859_9); break;
    case ISO8859_10: A(iso_8859_10); break;
    case ISO646_1973IRV: A(iso646_1973irv); break;
    case BS4730: A(bs4730); break;
    case NATS_PRIMARY_FINLAND_SWEDEN: A(nats_primary_finland_sweden); break;
    case NATS_PRIMARY_DENMARK_NORWAY: A(nats_primary_denmark_norway); break;
    case DIN66003: A(din66003); break;
    case NFZ62010_1973: A(nfz62010_1973); break;
    case ISO646_ITALIAN: A(iso646_italian); break;
    case ISO646_SPANISH: A(iso646_spanish); break;
#undef A
#define A(a) for(j=94,i=0;i<94&&j==94;i++)for(j=0;j<94;j++)if(a[i][j]==buf2)break; \
if(j!=94){buf[0]=i+0x20;buf[1]=j+0x21;buf[2]=0;return 1;}
#define B {UCS2CHAR a[]={buf2,0};BOOL b; \
 wchar_to_cp932 (a, buf, 5, &b); \
 if (buf[0]<129||buf[0]>=0xf0||(buf[0]>=160&&buf[0]<224))return 0; \
 if(!b){if(buf[0]>=224)buf[0]-=224-160;buf[0]-=129;if(buf[1]>=0x7f)buf[1]--; \
 buf[1]-=64;buf[0]=buf[0]*2+(buf[1]>=94);if(buf[1]>=94)buf[1]-=94;buf[2]=0; \
 buf[0]+=0x21;buf[1]+=0x21;return 1;}}

    case JISC6226_1978: A(jisc6226_1978); B; break;
    case JISX0208_1983: A(jisx0208_1983); B; break;
    case JISX0208_1990: A(jisx0208_1990); B; break;
    case JISX0212_1990: A(jisx0212_1990); break;
    case JISX0213_1: A(jisx0213_1); B; break;
    case JISX0213_2: A(jisx0213_2); break;
    case JISX0213_2004_1: A(jisx0213_2004_1); B; break;
    case GB2312_80: A(gb2312_80); break;
    case CSIC_SET1: A(csic_set1); break;
    case CSIC_SET2: A(csic_set2); break;
    case CSIC_SET3: A(csic_set3); break;
    case CSIC_SET4: A(csic_set4); break;
    case CSIC_SET5: A(csic_set5); break;
    case CSIC_SET6: A(csic_set6); break;
    case CSIC_SET7: A(csic_set7); break;
    case KSC5601_1987: A(ksc5601_1987); break;
    case MS_KANJI:
      {
	UCS2CHAR a[] = { buf2, 0 };
	BOOL b;

	wchar_to_cp932 (a, buf, 5, &b);
	if (!b)
	  return 1;
      }
      break;
    case BIG5:
      {
	UCS2CHAR a[] = { buf2, 0 };
	BOOL b;

	wchar_to_cp950 (a, buf, 5, &b);
	if (!b)
	  return 1;
      }
      break;
#undef B
#undef A
    case VT100GRAPHICS:         /* ? */
      for(i=0;i<32;i++)
        if(unitab_xterm_std[i]==buf2)
          break;
      if(i==32)
        return 0;
      buf[0]=i + 0x60;
      buf[1]=0;
      return 1;
    case UTF8CJK:
    case UTF8NONCJK:
    case UNKNOWN:
    case UNSUPPORTED:
      break;
    }
  return 0;
}

static void
clearesc (struct iso2022struct *q)
{
  q->esc = 0;
}

void
iso2022_clearesc (struct iso2022_data *this)
{
  clearesc (&this->rcv);
}

#define CONT_IF_SHORT(n) if (q->bufoff < n) goto cont

static void
put (struct iso2022struct *q, unsigned char c)
{
  int ul;
  unsigned int err_width;

  if (q->buflen)
    q->buflen = q->bufoff = 0;
  q->buf[q->bufoff] = c;
  q->bufoff++;
  if (q->esc)
    goto pass;
 loop:
  switch (q->buf[0])
    {
    default:
      if (q->buf[0] >= 0x20 && q->buf[0] <= 0x7f)
	goto gl;
      if (q->gr->type == MS_KANJI)
	{
	  if (q->buf[0] >= 0x80)
	    goto gr_mskanji;
	}
      if (q->gr->type == UTF8CJK || q->gr->type == UTF8NONCJK)
	{
	  if (q->buf[0] >= 0x80)
	    goto gr_utf8;
	}
      if (q->buf[0] >= 0xa0 && q->buf[0] <= 0xff)
	goto gr;
      if (q->buf[0] >= 0x80 && q->buf[0] <= 0x9f)
	{
	  q->buf[0] = 0x1b;
	  q->buf[1] = c - 0x80 + 0x40;
	  q->bufoff = 2;
	  goto loop;
	}
      goto pass;
    case 0xf:
      q->gl = &q->g0;
      goto clear;
    case 0xe:
      q->gl = &q->g1;
      goto clear;
    case 0x1b:
      CONT_IF_SHORT (2);
      switch (q->buf[1])
	{
	case 0x6e:
	  q->gl = &q->g2;
	  goto clear;
	case 0x6f:
	  q->gl = &q->g3;
	  goto clear;
	case 0x7e:
	  q->gr = &q->g1;
	  goto clear;
	case 0x7d:
	  q->gr = &q->g2;
	  goto clear;
	case 0x7c:
	  q->gr = &q->g3;
	  goto clear;
	case 0x4e:
	  q->ssl = q->gl;
	  q->ssr = q->gr;
	  q->gl = &q->g2;
	  q->gr = &q->g2;
	  goto clear;
	case 0x4f:
	  q->ssl = q->gl;
	  q->ssr = q->gr;
	  q->gl = &q->g3;
	  q->gr = &q->g3;
	  goto clear;
	case '(':
	  CONT_IF_SHORT (3);
	  setg_94_1 (q, &q->g0);
	  goto clear;
	case ')':
	  CONT_IF_SHORT (3);
	  setg_94_1 (q, &q->g1);
	  goto clear;
	case '*':
	  CONT_IF_SHORT (3);
	  setg_94_1 (q, &q->g2);
	  goto clear;
	case '+':
	  CONT_IF_SHORT (3);
	  setg_94_1 (q, &q->g3);
	  goto clear;
	case ',':		/* non-standard */
	  CONT_IF_SHORT (3);
	  setg_96_1 (q, &q->g0);
	  goto clear;
	case '-':
	  CONT_IF_SHORT (3);
	  setg_96_1 (q, &q->g1);
	  goto clear;
	case '.':
	  CONT_IF_SHORT (3);
	  setg_96_1 (q, &q->g2);
	  goto clear;
	case '/':
	  CONT_IF_SHORT (3);
	  setg_96_1 (q, &q->g3);
	  goto clear;
	case '$':
	  CONT_IF_SHORT (3);
	  switch (q->buf[2])
	    {
	    case '@':
	    case 'A':
	    case 'B':
	      setg_94_n (q, &q->g0);
	      goto clear;
	    case '(':
	      CONT_IF_SHORT (4);
	      setg_94_n (q, &q->g0);
	      goto clear;
	    case ')':
	      CONT_IF_SHORT (4);
	      setg_94_n (q, &q->g1);
	      goto clear;
	    case '*':
	      CONT_IF_SHORT (4);
	      setg_94_n (q, &q->g2);
	      goto clear;
	    case '+':
	      CONT_IF_SHORT (4);
	      setg_94_n (q, &q->g3);
	      goto clear;
	    case ',':		/* non-standard */
	      CONT_IF_SHORT (4);
	      setg_96_n (q, &q->g0);
	      goto clear;
	    case '-':
	      CONT_IF_SHORT (4);
	      setg_96_n (q, &q->g1);
	      goto clear;
	    case '.':
	      CONT_IF_SHORT (4);
	      setg_96_n (q, &q->g2);
	      goto clear;
	    case '/':
	      CONT_IF_SHORT (4);
	      setg_96_n (q, &q->g3);
	      goto clear;
	    }
	  goto esc;
	case '&':
	  CONT_IF_SHORT (3);
	  switch (q->buf[2])
	    {
	    case '@':
	      q->jisx02081990flag = 1;
	      goto clear;
	    }
	  goto esc;
	case '%':
	  CONT_IF_SHORT (3);
	  switch (q->buf[2])
	    {
	    case 'G':
	      q->switch_utf8 = SWITCH_UTF8_TO_UTF8;
	      goto clear;
	    case '@':
	      q->switch_utf8 = SWITCH_UTF8_FROM_UTF8;
	      goto clear;
	    }
	  goto esc;
	}
      goto esc;
    gl:				/* GL: bit7 = 0 */
      if ((c & 0x7f) < 0x20)
	{
	  err_width = q->bufoff - 1;
	  goto err;
	}
      if (q->gl->len == q->bufoff)
	translate (q, q->gl);
      break;
    gr:				/* GR: bit7 = 1 */
      if ((c & 0x7f) < 0x20)
	{
	  err_width = q->bufoff - 1;
	  goto err;
	}
      if (q->gr->len == q->bufoff)
	translate (q, q->gr);
      break;
    gr_mskanji:			/* MS_Kanji and bit7 = 1 */
      if (q->bufoff == 1 && (c < 129 || c > 252))
	{
	  err_width = 0;
	  goto err;
	}
      if (q->bufoff == 2 && (c < 0x40 || c == 0x7f || c > 252))
	{
	  err_width = 1;
	  goto err;
	}
      if (q->gr->len == q->bufoff ||
	  (q->buf[0] >= 160 && q->buf[0] < 224)) /* 1 byte katakana */
	translate (q, q->gr);
      break;
    gr_utf8:			/* UTF-8 and bit7 = 1 */
      ul = utf8len (q->buf[0]);
      if (ul < 2 ||
	  (q->bufoff >= 2 && (c & 0xc0) != 0x80))
	{
	  err_width = 1;	/* broken input, unknown width */
	  goto err;
	}
      if (ul == q->bufoff)
	{
	  q->width = 0;
	  goto pass;
	}
      break;
    esc:			/* to esc mode */
      q->esc = 1;
      goto pass;
    pass:			/* pass through the buffer */
      q->buf[q->buflen = q->bufoff] = 0;
      q->bufoff = 0;
      break;
    cont:		       /* parse after the next byte arrived */
      break;
    clear:			/* clear the buffer */
      q->bufoff = 0;
      break;
    err:			/* broken input */
      q->ins = (unsigned char *)"??";
      q->insw = (unsigned char *)"\1\1";
      q->inslen = (err_width > 2) ? 2 : err_width;
      if (q->bufoff >= 2)
	{
	  q->buf[0] = c;
	  q->bufoff = 1;
	  goto loop;
	}
      else
	goto clear;
    }
  if (q->bufoff >= 8)		/* avoid buffer overflow */
    {
      q->buf[q->buflen = q->bufoff] = 0;
      q->bufoff = 0;
    }
  if (q->lockgr && !q->ssl)
    q->gr = &q->lgr;
}

static int
try1 (struct iso2022struct *q, UCS2CHAR buf2, unsigned char *p,
      unsigned char *designate, unsigned char *invoke,
      unsigned char *beforec0, int bits)
{
  struct iso2022struct saveq;

  saveq = *q;
  if (try0 (q->gl, buf2, p))
    return 1;
  if (bits == 7)
    return 0;
  *q = saveq;
  if (!try0 (q->gr, buf2, p))
    return 0;
  if (q->gr->type != MS_KANJI && q->gr->type != BIG5)
    while (*p)
      *p++ |= 0x80;
  return 1;
}

static int
try2 (struct iso2022struct *q, UCS2CHAR buf2, unsigned char *p,
      unsigned char *designate, unsigned char *invoke,
      unsigned char *beforec0, int bits)
{
  struct iso2022struct saveq;
  unsigned char *pp, *ppp;

  saveq = *q;
  ppp = invoke;
  while (*ppp)
    {
      pp = p;
      q->esc = 0;
      q->buflen = 1;
      while (*ppp)
	put (q, *pp++ = *ppp++);
      ppp++;
      if (try1 (q, buf2, pp, designate, invoke, beforec0, bits))
	{
	  if (q->ssl)
	    {
	      q->gl = q->ssl, q->gr = q->ssr, q->ssl = 0, q->ssr = 0;
	      if (q->ssgr)
		while (*pp)
		  *pp++ |= 0x80;
	    }
	  return 1;
	}
      *q = saveq;
    }
  return 0;
}

static int
try3 (struct iso2022struct *q, UCS2CHAR buf2, unsigned char *p,
      unsigned char *designate, unsigned char *invoke,
      unsigned char *beforec0, int bits)
{
  struct iso2022struct saveq;
  unsigned char *pp, *ppp;

  saveq = *q;
  ppp = designate;
  while (*ppp)
    {
      pp = p;
      q->esc = 0;
      q->buflen = 1;
      while (*ppp)
	put (q, *pp++ = *ppp++);
      ppp++;
      if (try1 (q, buf2, pp, designate, invoke, beforec0, bits))
	{
	  if (q->ssl)
	    {
	      q->gl = q->ssl, q->gr = q->ssr, q->ssl = 0, q->ssr = 0;
	      if (q->ssgr)
		while (*pp)
		  *pp++ |= 0x80;
	    }
	  return 1;
	}
      *q = saveq;
    }
  return 0;
}

static int
try4 (struct iso2022struct *q, UCS2CHAR buf2, unsigned char *p,
      unsigned char *designate, unsigned char *invoke,
      unsigned char *beforec0, int bits)
{
  struct iso2022struct saveq;
  unsigned char *pp, *ppp;

  saveq = *q;
  ppp = designate;
  while (*ppp)
    {
      pp = p;
      q->esc = 0;
      q->buflen = 1;
      while (*ppp)
	put (q, *pp++ = *ppp++);
      ppp++;
      if (try2 (q, buf2, pp, designate, invoke, beforec0, bits))
	return 1;
      *q = saveq;
    }
  return 0;
}

static void
transmit (struct iso2022_data *this, struct iso2022struct *q, unsigned char c)
{
  if (q->buflen)
    q->buflen = q->bufoff = 0;
  q->buf[q->bufoff++] = c;
  if (q->gr->type == UTF8CJK || q->gr->type == UTF8NONCJK)
    {
      q->buflen = q->bufoff;
      q->bufoff = 0;
      return;
    }
  if ((q->bufoff == 1 && (q->buf[0] & 0x80) == 0) ||
      (q->bufoff == 2 && (q->buf[0] & 0xe0) == 0xc0) ||
      (q->bufoff == 3 && (q->buf[0] & 0xf0) == 0xe0) ||
      (q->bufoff == 4 && (q->buf[0] & 0xf8) == 0xf0) ||
      (q->bufoff == 5 && (q->buf[0] & 0xfc) == 0xf8) ||
      (q->bufoff == 6 && (q->buf[0] & 0xfe) == 0xfc))
    {
      UCS2CHAR buf2[100];
      unsigned char *p1, *p2, *p3, *p4, buf3[100];
      int bits;

      p1 = this->initstring;
      p1 += strlenu (p1) + 1;
      p1 += strlenu (p1) + 1;
      p2 = p1;
      while (strlenu (p2))
	p2 += strlenu (p2) + 1;
      p2++;
      p3 = p2;
      while (strlenu (p3))
	p3 += strlenu (p3) + 1;
      p3++;
      p4 = p3;
      while (strlenu (p4))
	p4 += strlenu (p4) + 1;
      p4++;
      bits = *p4;
      if (q->buf[0] >= 0x20 && q->buf[0] != 0x7f)
	{
	  q->buf[q->bufoff] = 0;
	  utf8_to_wchar (q->buf, buf2, 100);
	  for (;;)
	    {
	      if (!try1 (q, buf2[0], buf3, p1, p2, p3, bits))
		if (!try2 (q, buf2[0], buf3, p1, p2, p3, bits))
		  if (!try3 (q, buf2[0], buf3, p1, p2, p3, bits))
		    if (!try4 (q, buf2[0], buf3, p1, p2, p3, bits))
		      {
			if (buf2[0] == L'?')
			  buf3[0] = '?', buf3[1] = 0;
			else
			  {
			    buf2[0] = L'?';
			    continue;
			  }
		      }
	      break;
	    }
	  strcpy ((char *)q->buf, (char *)buf3);
	  q->buflen = strlenu (q->buf);
	  q->bufoff = 0;
	}
      else
	{
	  unsigned char c = q->buf[0];
	  struct iso2022struct saveq;
	  unsigned char *pp, *ppp, *p5;

	  ppp = p3;
	  pp = q->buf;
	  while (*ppp)
	    {
	      p5 = pp;
	      saveq = *q;
	      q->esc = 0;
	      q->buflen = 1;
	      while (*ppp)
		put (q, *pp++ = *ppp++);
	      ppp++;
	      if (!memcmp (&q->g0, &saveq.g0, sizeof (q->g0)) &&
		  !memcmp (&q->g1, &saveq.g1, sizeof (q->g1)) &&
		  !memcmp (&q->g2, &saveq.g2, sizeof (q->g2)) &&
		  !memcmp (&q->g3, &saveq.g3, sizeof (q->g3)) &&
		  q->gl == saveq.gl &&
		  q->gr == saveq.gr)
		*q = saveq, pp = p5;
	    }
	  *pp++ = c;
	  *pp = 0;
	  q->buflen = strlenu (q->buf);
	  q->bufoff = 0;
	  if (!c)
	    q->buflen++;
	}
    }
}

static void
transmit2 (struct iso2022_data *this, struct iso2022struct *q, unsigned char c)
{
  if (q->transchar) {
	transmit (this, q, c);
	return;
  }
  if (q->buflen)
    q->buflen = q->bufoff = 0;
  if (c < 0x20 || c >= 0x7f)
    {
      unsigned char *p1, *p2, *p3;

      p1 = this->initstring;
      p1 += strlenu (p1) + 1;
      p1 += strlenu (p1) + 1;
      p2 = p1;
      while (strlenu (p2))
	p2 += strlenu (p2) + 1;
      p2++;
      p3 = p2;
      while (strlenu (p3))
	p3 += strlenu (p3) + 1;
      p3++;
        {
	  struct iso2022struct saveq;
	  unsigned char *pp, *ppp, *p5;

	  ppp = p3;
	  pp = q->buf;
	  while (*ppp)
	    {
	      p5 = pp;
	      saveq = *q;
	      q->esc = 0;
	      q->buflen = 1;
	      while (*ppp)
		put (q, *pp++ = *ppp++);
	      ppp++;
	      if (!memcmp (&q->g0, &saveq.g0, sizeof (q->g0)) &&
		  !memcmp (&q->g1, &saveq.g1, sizeof (q->g1)) &&
		  !memcmp (&q->g2, &saveq.g2, sizeof (q->g2)) &&
		  !memcmp (&q->g3, &saveq.g3, sizeof (q->g3)) &&
		  q->gl == saveq.gl &&
		  q->gr == saveq.gr)
		*q = saveq, pp = p5;
	    }
	  *pp++ = c;
	  *pp = 0;
	  q->buflen = strlenu (q->buf);
	  q->bufoff = 0;
	  if (!c)
	    q->buflen++;
	}
    }
  else {
	q->buf[0] = c;
	q->buflen = 1;
  }
}

enum autodetect_result {
  AD_IGNORE,
  AD_GOOD,
  AD_BAD,
};

static enum autodetect_result
autodetect_jp_eucjp (struct iso2022_autodetect_jp *p, unsigned char c)
{
  if (p->buflen < 0)
    {
      if (c >= 0x80)
	goto next;
      p->buflen = 0;
      return AD_BAD;
    }
  if (p->buflen < AUTODETECT_BUFLEN)
    {
      p->buf[p->buflen++] = c;
      if (p->buf[0] < 0x80)
	goto ignore;
      if (p->buf[0] == 0x8e)
	{
	  if (p->buflen >= 2)
	    {
	      if (p->buf[1] >= 0xa1 && p->buf[1] <= 0xdf)
		goto good;
	      else
		goto bad;
	    }
	  else
	    goto next;
	}
      if (p->buf[0] == 0x8f)
	{
	  if (p->buflen >= 2)
	    {
	      if (p->buf[1] >= 0xa1 && p->buf[1] <= 0xfe)
		{
		  if (p->buflen >= 3)
		    {
		      if (p->buf[2] >= 0xa1 && p->buf[2] <= 0xfe)
			goto good;
		      else
			goto bad;
		    }
		  else
		    goto next;
		}
	      else
		goto bad;
	    }
	  else
	    goto next;
	}
      if (p->buf[0] >= 0xa1 && p->buf[0] <= 0xfe)
	{
	  if (p->buflen >= 2)
	    {
	      if (p->buf[1] >= 0xa1 && p->buf[1] <= 0xfe)
		goto good;
	      else
		goto bad;
	    }
	  else
	    goto next;
	}
      else
	goto bad;
    }
  else
    goto bad;
 ignore:
  p->buflen = 0;
  return AD_IGNORE;
 next:
  return AD_IGNORE;
 good:
  p->buflen = 0;
  return AD_GOOD;
 bad:
  p->buflen = -1;
  return AD_BAD;
}

static enum autodetect_result
autodetect_jp_mskanji (struct iso2022_autodetect_jp *p, unsigned char c)
{
  if (p->buflen < 0)
    {
      if (c >= 0x80)
	goto next;
      p->buflen = 0;
      return AD_BAD;
    }
  if (p->buflen < AUTODETECT_BUFLEN)
    {
      p->buf[p->buflen++] = c;
      if (p->buf[0] < 0x80)
	goto ignore;
      if (p->buf[0] >= 160 && p->buf[0] <= 223)
	goto good;
      if ((p->buf[0] >= 129 && p->buf[0] <= 159) ||
	  (p->buf[0] >= 224 && p->buf[0] <= 252))
	{
	  if (p->buflen >= 2)
	    {
	      if ((p->buf[1] >= 0x40 && p->buf[1] <= 0x7e) ||
		  (p->buf[1] >= 0x80 && p->buf[1] <= 252))
		goto good;
	      else
		goto bad;
	    }
	  else
	    goto next;
	}
      else
	goto bad;
    }
  else
    goto bad;
 ignore:
  p->buflen = 0;
  return AD_IGNORE;
 next:
  return AD_IGNORE;
 good:
  p->buflen = 0;
  return AD_GOOD;
 bad:
  p->buflen = -1;
  return AD_BAD;
}

static enum autodetect_result
autodetect_jp_utf8cjk (struct iso2022_autodetect_jp *p, unsigned char c)
{
  int len;
  int i;

  if (p->buflen < 0)
    {
      if (c >= 0x80)
	goto next;
      p->buflen = 0;
      return AD_BAD;
    }
  if (p->buflen < AUTODETECT_BUFLEN)
    {
      p->buf[p->buflen++] = c;
      if (p->buf[0] < 0x80)
	goto ignore;
      len = utf8len (p->buf[0]);
      if (len == 0)
	goto bad;
      if (len >= 4)		/* 32bit code is not supported */
	goto bad;
      for (i = 1; i < p->buflen; i++)
	{
	  if ((p->buf[i] & 0xc0) != 0x80)
	    goto bad;
	}
      if (len == 3 && p->buf[0] == 0xe0 &&
	  p->buflen >= 2 && p->buf[1] < 0xa0)
	goto bad;
      if (p->buflen >= len)
	goto good;
      else
	goto next;
    }
  else
    goto bad;
 ignore:
  p->buflen = 0;
  return AD_IGNORE;
 next:
  return AD_IGNORE;
 good:
  p->buflen = 0;
  return AD_GOOD;
 bad:
  p->buflen = -1;
  return AD_BAD;
}

static void
autodetect_jp (struct iso2022_data *this, unsigned char *buf, int nchars)
{
  struct {
    int good;
    int bad;
  } eucjp, mskanji, utf8cjk;
  enum autodetect_result r;
  int i;

  eucjp.good = eucjp.bad = 0;
  mskanji.good = mskanji.bad = 0;
  utf8cjk.good = utf8cjk.bad = 0;
#define DO_AUTODETECT(ENCODING) do { \
    if (this->autodetect.jp.ENCODING.e) \
      r = autodetect_jp_##ENCODING (&this->autodetect.jp.ENCODING, buf[i]); \
    else \
      r = AD_BAD; \
    if (r == AD_GOOD) \
      ENCODING.good++; \
    if (r == AD_BAD) \
      ENCODING.bad++; \
} while (0)
  for (i = 0; i < nchars; i++)
    {
      DO_AUTODETECT (eucjp);
      DO_AUTODETECT (mskanji);
      DO_AUTODETECT (utf8cjk);
    }
#undef DO_AUTODETECT
  if (utf8cjk.good > 0 && utf8cjk.bad == 0)
    {
      if (this->rcv.lgr.type != UTF8CJK)
	{
	  this->rcv.lgr.type = UTF8CJK;
	  this->rcv.lgr.len = 0;
	  this->rcv.gr = &this->rcv.lgr;
	  this->rcv.buflen = this->rcv.bufoff = 0;
	}
    }
  else if (utf8cjk.bad > 0 && eucjp.good > 0 && eucjp.bad == 0)
    {
      if (this->rcv.lgr.type != JISX0208_1990)
	{
	  this->rcv.lgr.type = JISX0208_1990;
	  this->rcv.lgr.len = 2;
	  this->rcv.g2.type = JISX0201_KATAKANA;
	  this->rcv.g2.len = 1;
	  this->rcv.g3.type = JISX0212_1990;
	  this->rcv.g3.len = 2;
	  this->rcv.gr = &this->rcv.lgr;
	  this->rcv.buflen = this->rcv.bufoff = 0;
	}
    }
  else if (eucjp.bad > 0 && mskanji.good > 0 && mskanji.bad == 0)
    {
      if (this->rcv.lgr.type != MS_KANJI)
	{
	  this->rcv.lgr.type = MS_KANJI;
	  this->rcv.lgr.len = 2;
	  this->rcv.gr = &this->rcv.lgr;
	  this->rcv.buflen = this->rcv.bufoff = 0;
	}
    }
}

#define IS_CJK(x) ( \
  (x) == JISX0201_ROMAN || \
  (x) == JISX0201_KATAKANA || \
  (x) == JISC6226_1978 || \
  (x) == JISX0208_1983 || \
  (x) == JISX0208_1990 || \
  (x) == JISX0212_1990 || \
  (x) == JISX0213_1 || \
  (x) == JISX0213_2 || \
  (x) == JISX0213_2004_1 || \
  (x) == MS_KANJI || \
  (x) == GB2312_80 || \
  (x) == KSC5601_1987 || \
  (x) == BIG5 || \
  (x) == UTF8CJK || \
  0 )

void
iso2022_put (struct iso2022_data *this, unsigned char c)
{
  put (&this->rcv, c);
  if (this->rcv.switch_utf8 != SWITCH_UTF8_NONE)
    {
      if (this->rcv.switch_utf8 == SWITCH_UTF8_TO_UTF8)
	{
	  if (!this->rcv.usgr)
	    {
	      this->rcv.usgr = this->rcv.gr;
	      this->rcv.uslgr = this->rcv.lgr;
	      this->rcv.uslockgr = this->rcv.lockgr;
	      this->trns.usgr = this->trns.gr;
	      this->trns.uslgr = this->trns.lgr;
	      this->trns.uslockgr = this->trns.lockgr;
	      if (IS_CJK (this->rcv.usgr->type))
		this->rcv.lgr.type = UTF8CJK;
	      else
		this->rcv.lgr.type = UTF8NONCJK;
	      this->rcv.lgr.len = 0;
	      this->rcv.gr = &this->rcv.lgr;
	      this->rcv.lockgr = 1;
	      this->trns.lgr = this->rcv.lgr;
	      this->trns.gr = &this->trns.lgr;
	    }
	}
      else if (this->rcv.switch_utf8 == SWITCH_UTF8_FROM_UTF8)
	{
	  if (this->rcv.usgr)
	    {
	      this->rcv.gr = this->rcv.usgr;
	      this->rcv.lgr = this->rcv.uslgr;
	      this->rcv.lockgr = this->rcv.uslockgr;
	      this->trns.gr = this->trns.usgr;
	      this->trns.lgr = this->trns.uslgr;
	      this->trns.lockgr = this->trns.uslockgr;
	      this->rcv.usgr = 0;
	    }
	}
      this->rcv.switch_utf8 = SWITCH_UTF8_NONE;
    }
}

void
iso2022_autodetect_put (struct iso2022_data *this, unsigned char *buf,
			int nchars)
{
  if (this->autodetect.n)
    {
      if (this->autodetect.jp.n)
	autodetect_jp (this, buf, nchars);
    }
}

void
iso2022_transmit (struct iso2022_data *this, unsigned char c)
{
  transmit2 (this, &this->trns, c);
}

static void
init (struct iso2022struct *q)
{
  q->g0.len = 1, q->g0.type = US_ASCII;
  q->g1.len = 1, q->g1.type = US_ASCII;
  q->g2.len = 1, q->g2.type = US_ASCII;
  q->g3.len = 1, q->g3.type = US_ASCII;
#if 0
  if (!strcmp (p, "EUC-JP"))
    {
      q->g1.len = 2, q->g1.type = JISX0208_1983;
      q->g2.len = 1, q->g2.type = JISX0201_KATAKANA;
      q->g3.len = 2, q->g3.type = JISX0212_1990;
    }
#endif
  q->gl = &q->g0;
  q->gr = &q->g1;
  q->ssl = 0;
  q->ssr = 0;
  q->usgr = 0;
  q->jisx02081990flag = 0;
  q->buflen = q->bufoff = 0;
  q->esc = 0;
  q->lockgr = 0;
  q->ssgr = 0;
  q->transchar = 0;
  q->inslen = 0;
}

/*
  examples
  euc-jp:
  > 1b2842 1b242942 1b2a49 1b242b44 0f 1b7e 00
  > 1b2842 1b242942 1b2a49 1b242b44 0f 1b7e 00
  > 00
  > 8e 00 8f 00 00
  > 00
  > 08
  iso-2022-jp:
  > 1b2842 1b2949 0f 1b7e 00
  > 1b2842 1b2949 0f 1b7e 00
  > 1b2842 00 1b242842 00 1b 24 28 44 00 00
  > 0f 00 0e 00 00
  > 1b2842 00 0f 00 00
  > 07
 */
/* mode
   0: always initialize
   1: initialize if initstring changed
   2: only check whether initstring is correct or not
   return value
   0: success
   -1: error
*/

int
iso2022_init (struct iso2022_data *this, char *p, int mode)
{
  int i, f, j, k = 0;
  int tmp_lockgr, tmp_mskanji, tmp_big5, tmp_win95flag, tmp_ssgr, tmp_utf8cjk;
  int tmp_utf8noncjk, tmp_autojp_eucjp, tmp_autojp_mskanji, tmp_autojp_utf8;
  unsigned char *init_string = this->initstring;

  if (!stricmp (p, "euc-jp"))
    p = "iso2022 lockgr euc-jp";
  else if (!stricmp (p, "iso-2022-jp"))
    p = "iso2022 "
      "1b28420f00 1b28420f00 1b2842001b2442001b2428440000 00 1b28420000 07";
  else if (!stricmp (p, "MS_Kanji") || !stricmp (p, "Shift_JIS"))
    p = "iso2022 lockgr MS_Kanji";
  else if (!stricmp (p, "big5"))
    p = "iso2022 lockgr big5";
  else if (!stricmp (p, "euc-kr"))
    p = "iso2022 lockgr euc-kr";
  else if (!stricmp (p, "euc-cn"))
    p = "iso2022 lockgr euc-cn";
  else if (!stricmp (p, "euc-tw"))
    p = "iso2022 lockgr euc-tw";
  else if (!stricmp (p, "utf-8 (cjk)"))
    p = "iso2022 lockgr utf-8-cjk";
  else if (!stricmp (p, "utf-8 (non-cjk)"))
    p = "iso2022 lockgr utf-8-noncjk";
  else if (!stricmp (p, "euc-jp/auto-detect japanese"))
    p = "iso2022 autojp-eucjp autojp-mskanji autojp-utf8 lockgr euc-jp";
  else if (!stricmp (p, "MS_Kanji/auto-detect japanese"))
    p = "iso2022 autojp-eucjp autojp-mskanji autojp-utf8 lockgr MS_Kanji";
  else if (!stricmp (p, "Shift_JIS/auto-detect japanese"))
    p = "iso2022 autojp-eucjp autojp-mskanji autojp-utf8 lockgr MS_Kanji";
  else if (!stricmp (p, "utf-8/auto-detect japanese"))
    p = "iso2022 autojp-eucjp autojp-mskanji autojp-utf8 lockgr utf-8-cjk";
  if (strnicmp (p, "iso2022 ", 8))
    return -1;
  p += 8;
  {
    unsigned char initstring[512];

    tmp_win95flag = get_win95flag ();
    tmp_lockgr = tmp_mskanji = tmp_big5 = tmp_ssgr = tmp_utf8cjk = tmp_utf8noncjk = 0;
    tmp_autojp_eucjp = tmp_autojp_mskanji = tmp_autojp_utf8 = 0;
    for (;;)
      {
	if (*p == ' ')
	  {
	    p++;
	    continue;
	  }
	if (!strnicmp (p, "w95", 3))
	  {
	    tmp_win95flag = 1;
	    p += 3;
	    if (!strnicmp (p, "off", 3))
	      {
		tmp_win95flag = 0;
		p += 3;
	      }
	    continue;
	  }
	if (!strnicmp (p, "lockgr", 6))
	  {
	    tmp_lockgr = 1;
	    p += 6;
	    continue;
	  }
	if (!strnicmp (p, "ms_kanji", 8))
	  {
	    tmp_mskanji = 1;
	    p += 8;
	    continue;
	  }
	if (!strnicmp (p, "big5", 4))
	  {
	    tmp_big5 = 1;
	    p += 4;
	    continue;
	  }
	if (!strnicmp (p, "ssgr", 4))
	  {
	    tmp_ssgr = 1;
	    p += 4;
	    continue;
	  }
	if (!strnicmp (p, "utf-8-noncjk", 12))
	  {
	    tmp_utf8noncjk = 1;
	    p += 12;
	    continue;
	  }
	if (!strnicmp (p, "utf-8-cjk", 9))
	  {
	    tmp_utf8cjk = 1;
	    p += 9;
	    continue;
	  }
	if (!strnicmp (p, "autojp-eucjp", 12))
	  {
	    tmp_autojp_eucjp = 1;
	    p += 12;
	    continue;
	  }
	if (!strnicmp (p, "autojp-mskanji", 14))
	  {
	    tmp_autojp_mskanji = 1;
	    p += 14;
	    continue;
	  }
	if (!strnicmp (p, "autojp-utf8", 11))
	  {
	    tmp_autojp_utf8 = 1;
	    p += 11;
	    continue;
	  }
	if (!stricmp (p, "euc-jp"))
	  {
	    p =
	      /* receiving */
	      "1b2842"		/* G0: US ASCII */
	      /*"1b242942"*/	/* G1: JIS X0208-1983 */
	      "1b26401b242942"	/* G1: JIS X0208-1990 */
	      "1b2a49"		/* G2: JIS X0201 KATAKANA */
	      "1b242b44"	/* G3: JIS X0212-1990 */
	      "0f"		/* GL: G0 */
	      "1b7e"		/* GR: G1 */
	      "00"
	      /* transmitting */
	      "1b2842"		/* G0: US ASCII */
	      /*"1b242942"*/	/* G1: JIS X0208-1983 */
	      "1b26401b242942"	/* G1: JIS X0208-1990 */
	      "1b2a49"		/* G2: JIS X0201 KATAKANA */
	      "1b242b44"	/* G3: JIS X0212-1990 */
	      "0f"		/* GL: G0 */
	      "1b7e"		/* GR: G1 */
	      "00"
	      /* designating */
	      "00"
	      /* invoking */
	      "8e00"		/* SS2 */
	      "8f00"		/* SS3 */
	      "00"
	      /* before C0 control characters */
	      "00"
	      /* 7bit/8bit */
	      "08";
	    tmp_ssgr = 1;
	    break;
	  }
	if (!stricmp (p, "euc-kr"))
	  {
	    p =
	      /* receiving */
	      "1b2842"		/* G0: US ASCII */
	      "1b242943"	/* G1: KS C 5601-1987 */
	      "0f"		/* GL: G0 */
	      "1b7e"		/* GR: G1 */
	      "00"
	      /* transmitting */
	      "1b2842"		/* G0: US ASCII */
	      "1b242943"	/* G1: KS C 5601-1987 */
	      "0f"		/* GL: G0 */
	      "1b7e"		/* GR: G1 */
	      "00"
	      /* designating */
	      "00"
	      /* invoking */
	      "8e00"		/* SS2 */
	      "8f00"		/* SS3 */
	      "00"
	      /* before C0 control characters */
	      "00"
	      /* 7bit/8bit */
	      "08";
	    break;
	  }
	if (!stricmp (p, "euc-cn"))
	  {
	    p =
	      /* receiving */
	      "1b2842"		/* G0: US ASCII */
	      "1b242941"	/* G1: GB 2312-80 */
	      "0f"		/* GL: G0 */
	      "1b7e"		/* GR: G1 */
	      "00"
	      /* transmitting */
	      "1b2842"		/* G0: US ASCII */
	      "1b242941"	/* G1: GB 2312-80 */
	      "0f"		/* GL: G0 */
	      "1b7e"		/* GR: G1 */
	      "00"
	      /* designating */
	      "00"
	      /* invoking */
	      "8e00"		/* SS2 */
	      "8f00"		/* SS3 */
	      "00"
	      /* before C0 control characters */
	      "00"
	      /* 7bit/8bit */
	      "08";
	    break;
	  }
	if (!stricmp (p, "euc-tw"))
	  {
	    p =
	      /* receiving */
	      "1b2842"		/* G0: US ASCII */
	      "1b242947"	/* G1: CSIC SET 1 */
	      "1b242b48"	/* G3: CSIC SET 2 */
	      "0f"		/* GL: G0 */
	      "1b7e"		/* GR: G1 */
	      "00"
	      /* transmitting */
	      "1b2842"		/* G0: US ASCII */
	      "1b242947"	/* G1: CSIC SET 1 */
	      "1b242b48"	/* G3: CSIC SET 2 */
	      "0f"		/* GL: G0 */
	      "1b7e"		/* GR: G1 */
	      "00"
	      /* designating */
	      "00"
	      /* invoking */
	      "8e00"		/* SS2 */
	      "8f00"		/* SS3 */
	      "00"
	      /* before C0 control characters */
	      "00"
	      /* 7bit/8bit */
	      "08";
	    break;
	  }
	break;
      }
    for (f = 0, i = 0 ; *p ; p++)
      {
	j = -1;
	if ('0' <= *p && *p <= '9')
	  j = *p - '0';
	if ('A' <= *p && *p <= 'F')
	  j = *p - 'A' + 10;
	if ('a' <= *p && *p <= 'f')
	  j = *p - 'a' + 10;
	if (j >= 0)
	  {
	    if (f)
	      {
		if (i >= 500)
		  return -1;
		k += j;
		initstring[i++] = k;
		f = 0;
	      }
	    else
	      {
		k = j * 16;
		f = 1;
	      }
	  }
      }
    if (tmp_mskanji && tmp_big5)
      return -1;
    if ((tmp_utf8cjk || tmp_utf8noncjk) && (tmp_mskanji || tmp_big5))
      return -1;
    if (!tmp_mskanji && !tmp_big5 && !tmp_utf8cjk && !tmp_utf8noncjk)
      for (j = 0;;)
	{
	  if (j >= i)
	    return -1;
	  if (!initstring[j++])
	    break;
	}
    if (mode == 2)
      return 0;
    if (this == NULL)
      return -1;
    initstring[i++] = 0;
    initstring[i++] = 0;
    initstring[i++] = 0;
    initstring[i++] = 0;
    initstring[i++] = 0;
    initstring[i++] = 0;
    initstring[i++] = 0;
    if (mode == 1)
      {
	if ((tmp_mskanji || tmp_big5 || tmp_utf8cjk || tmp_utf8noncjk ||
	     !memcmp (init_string, initstring, i))
	    && tmp_win95flag == this->win95flag
	    && tmp_lockgr == this->rcv.lockgr
	    && tmp_ssgr == this->rcv.ssgr
	    && tmp_mskanji == (this->rcv.lgr.type == MS_KANJI && this->rcv.gr == &this->rcv.lgr)
	    && tmp_big5 == (this->rcv.lgr.type == BIG5 && this->rcv.gr == &this->rcv.lgr)
	    && tmp_utf8cjk == (this->rcv.lgr.type == UTF8CJK && this->rcv.gr == &this->rcv.lgr)
	    && tmp_utf8noncjk == (this->rcv.lgr.type == UTF8NONCJK && this->rcv.gr == &this->rcv.lgr)
	    && tmp_autojp_eucjp == this->autodetect.jp.eucjp.e
	    && tmp_autojp_mskanji == this->autodetect.jp.mskanji.e
	    && tmp_autojp_utf8 == this->autodetect.jp.utf8cjk.e)
	  return 0;
      }
    memcpy (init_string, initstring, i);
  }
  iso2022_win95flag = this->win95flag = tmp_win95flag;
  init (&this->rcv);
  init (&this->trns);
  if (tmp_mskanji)
    {
      this->rcv.lgr.type = MS_KANJI;
      this->rcv.lgr.len = 2;
      this->rcv.gr = &this->rcv.lgr;
      this->trns.lgr = this->rcv.lgr;
      this->trns.gr = &this->trns.lgr;
    }
  if (tmp_big5)
    {
      this->rcv.lgr.type = BIG5;
      this->rcv.lgr.len = 2;
      this->rcv.gr = &this->rcv.lgr;
      this->trns.lgr = this->rcv.lgr;
      this->trns.gr = &this->trns.lgr;
    }
  if (tmp_utf8noncjk)
    {
      this->rcv.lgr.type = UTF8NONCJK;
      this->rcv.lgr.len = 0;
      this->rcv.width = 1;
      this->rcv.gr = &this->rcv.lgr;
      this->trns.lgr = this->rcv.lgr;
      this->trns.gr = &this->trns.lgr;
    }
  if (tmp_utf8cjk)
    {
      this->rcv.lgr.type = UTF8CJK;
      this->rcv.lgr.len = 0;
      this->rcv.gr = &this->rcv.lgr;
      this->trns.lgr = this->rcv.lgr;
      this->trns.gr = &this->trns.lgr;
    }
  for (i = 0 ; this->initstring[i] ; i++)
    put (&this->rcv, this->initstring[i]);
  for (i++ ; this->initstring[i] ; i++)
    put (&this->trns, this->initstring[i]);
  if (tmp_lockgr)
    {
      this->rcv.lgr = *this->rcv.gr;
      this->rcv.gr = &this->rcv.lgr;
      this->rcv.lockgr = 1;
    }
  if (tmp_ssgr)
    this->trns.ssgr = 1;
  this->autodetect.n = 0;
  this->autodetect.jp.n = 0;
  this->autodetect.jp.eucjp.e = 0;
  this->autodetect.jp.mskanji.e = 0;
  this->autodetect.jp.utf8cjk.e = 0;
  if (tmp_autojp_eucjp)
    {
      this->autodetect.jp.eucjp.e = 1;
      this->autodetect.jp.eucjp.buflen = 0;
      this->autodetect.jp.n++;
      this->autodetect.n++;
    }
  if (tmp_autojp_mskanji)
    {
      this->autodetect.jp.mskanji.e = 1;
      this->autodetect.jp.mskanji.buflen = 0;
      this->autodetect.jp.n++;
      this->autodetect.n++;
    }
  if (tmp_autojp_utf8)
    {
      this->autodetect.jp.utf8cjk.e = 1;
      this->autodetect.jp.utf8cjk.buflen = 0;
      this->autodetect.jp.n++;
      this->autodetect.n++;
    }
#undef A
  return 0;
}

int
iso2022_init_test (char *p)
{
  return iso2022_init (NULL, p, 2);
}

#ifdef _WINDOWS
int
xMultiByteToWideChar (UINT a1, DWORD a2, LPCSTR a3, int a4, LPWSTR a5, int a6)
{
  static int (WINAPI *f)(UINT, DWORD, LPCSTR, int, LPWSTR, int) = 0;
  int i, j, k;
  unsigned char c;
  unsigned long w;

  if (!f)
    f = GetProcAddress (GetModuleHandle ("kernel32"), "MultiByteToWideChar");
  if (a1 != CP_UTF8)
    return f (a1, a2, a3, a4, a5, a6);
  if (a4 == -1)
    a4 = lstrlenA (a3);
  k = 0;
  a6--;
  for (i = 0 ; i < a4 ; i++)
    {
      c = a3[i];
      if ((c & 0x80) == 0)
	j = 1, w = c;
      else if ((c & 0xe0) == 0xc0)
	j = 2, w = c & 0x1f;
      else if ((c & 0xf0) == 0xe0)
	j = 3, w = c & 0x0f;
      else if ((c & 0xf8) == 0xf0)
	j = 4, w = c & 0x07;
      else if ((c & 0xfc) == 0xf8)
	j = 5, w = c & 0x03;
      else if ((c & 0xfe) == 0xfc)
	j = 6, w = c & 0x01;
      else
	continue;
      if (i + j <= a4)
	{
	  while (--j)
	    w = (w << 6) | (a3[++i] & 0x3f);
	  if (w >= 0x10000UL)
	    w = (unsigned long)L'?';
	  if (k < a6)
	    a5[k] = w;
	  else if (k == a6)
	    break;
	  k++;
	}
      else
	break;
    }
  if (k <= a6)
    a5[k] = 0;
  return k + 1;
}

int
xWideCharToMultiByte (UINT a1, DWORD a2, LPCWSTR a3, int a4, LPSTR a5, int a6,
		     LPCSTR a7, LPBOOL a8)
{
  static int (WINAPI *f)(UINT, DWORD, LPCWSTR, int, LPSTR, int, LPCSTR,
			 LPBOOL) = 0;
  int i, j, k;
  WCHAR w;
  unsigned char b[10];

  if (!f)
    f = GetProcAddress (GetModuleHandle ("kernel32"), "WideCharToMultiByte");
  if (a1 != CP_UTF8)
    return f (a1, a2, a3, a4, a5, a6, a7, a8);
  if (a4 == -1)
    a4 = lstrlenW (a3);
  k = 0;
  a6--;
  for (i = 0 ; i < a4 ; i++)
    {
      w = a3[i];
      if (w < 0x80)
	j = 1, b[0] = w;
      else if (w < 0x800)
	j = 2, b[1] = 0x80 | (w & 0x3f), b[0] = 0xc0 | ((w >> 6) & 0x1f);
      else
	j = 3, b[2] = 0x80 | (w & 0x3f),
	  b[1] = 0x80 | ((w >> 6) & 0x3f), b[0] = 0xe0 | ((w >> 12) & 0x0f);
      if (k + j <= a6 || a6 == -1)
	{
	  if (a6 == -1)
	    k += j;
	  else
	    {
	      a5[k++] = b[0];
	      if (j >= 2)
		a5[k++] = b[1];
	      if (j == 3)
		a5[k++] = b[2];
	    }
	}
      else
	break;
    }
  if (k <= a6)
    a5[k] = '\0';
  return k + 1;
}
#endif
