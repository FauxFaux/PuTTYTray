// YogoCopy.h : main header file for the CTXMENU DLL
//

#if !defined(AFX_CTXMENU_H__982905C7_3928_11D3_A09E_00500402F30B__INCLUDED_)
#define AFX_CTXMENU_H__982905C7_3928_11D3_A09E_00500402F30B__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CYogoCopyApp
// See YogoCopy.cpp for the implementation of this class
//

class CYogoCopyApp : public CWinApp
{
public:
	CYogoCopyApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CYogoCopyApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CYogoCopyApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

extern UINT      g_cRefThisDll;

/////////////////////////////////////////////////////////////
// use GUIDGEN to generate a GUID for your shell extension
// ...
//use GUIDGEN to generate a GUID for your shell extension. call
//it "CLSID_MyFileYogoCopyID"
//ex. (don't use this GUID!!)
//DEFINE_GUID(CLSID_MyFileYogoCopyID, 
//0xc14f7681, 0x33d8, 0x11d3, 0xa0, 0x9b, 0x0, 0x50, 0x4, 0x2, 0xf3, 0xb);
DEFINE_GUID(CLSID_MyFileYogoCopyID, 
0xc14f7681, 0x33d8, 0x11d3, 0xa0, 0x9b, 0x0, 0x50, 0x4, 0x2, 0xf3, 0xb);


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CTXMENU_H__982905C7_3928_11D3_A09E_00500402F30B__INCLUDED_)
