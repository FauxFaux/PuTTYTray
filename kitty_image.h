#ifdef IMAGEPORT
extern HDC textdc;
extern HBITMAP textbm;
extern HBITMAP backgroundbm;
extern HDC backgroundblenddc;
extern HBITMAP backgroundblendbm;

extern COLORREF colorinpixel;
extern HDC colorinpixeldc;
extern HBITMAP colorinpixelbm;
extern HDC backgrounddc;
extern BOOL bBgRelToTerm;

extern int resizing;
extern RECT size_before;

void color_blend(HDC destDc, int x, int y, int width, int height, COLORREF alphacolor, int opacity) ;
void paint_term_edges(HDC hdc, LONG paint_left, LONG paint_top, LONG paint_right, LONG paint_bottom)  ;
void init_dc_blend(void);
#endif
