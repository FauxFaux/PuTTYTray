/*
 * HACK: PuttyTray / Session Icon
 * Added this file
 */ 

#ifndef _SELECTICON_H_
#define _SELECTICON_H_

BOOL SelectIconW(HWND hWndParent, LPWSTR lpszFilename, DWORD dwBufferSize, DWORD * pdwIndex);
BOOL SelectIconA(HWND hWndParent, LPSTR lpszFilename, DWORD dwBufferSize, DWORD * pdwIndex);

#ifdef _UNICODE
#define SelectIcon SelectIconW
#else
#define SelectIcon SelectIconA
#endif


#endif
