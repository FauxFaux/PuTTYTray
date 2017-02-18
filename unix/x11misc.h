/*
 * x11misc.h: header file for functions that need to refer to Xlib
 * data types. Has to be separate from unix.h so that we can include
 * it only after including the X headers, which in turn has to be done
 * after putty.h has told us whether NOT_X_WINDOWS is defined.
 */

#ifndef NOT_X_WINDOWS

/*
 * x11misc.c.
 */
void x11_ignore_error(Display *disp, unsigned char errcode);

#endif
