// CCL.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "resource.h"
#include "priv.h"
#include "ShellExt.h"
#include "YogoCopy.h"

#include <shlobj.h>
#include <shlguid.h>

#include "ShUtils.h"

/*

	What follows is our implementation of the IClassFactory interface.
	This code is basically unmodified from the SHELLEX example.

*/



/***************************************************************
	Function:
	CShellExtClassFactory::CShellExtClassFactory()

	Purpose:
	constructor

	Param			Type			Use
	-----			----			---

	Returns
	-------

***************************************************************/
CShellExtClassFactory::CShellExtClassFactory()
{
    ODS("CShellExtClassFactory::CShellExtClassFactory()\r\n");

    m_cRef = 0L;

    g_cRefThisDll++;	
}

/***************************************************************
	Function:
	CShellExtClassFactory::~CShellExtClassFactory()	

	Purpose:
	destructor

	Param			Type			Use
	-----			----			---

	Returns
	-------

***************************************************************/																
CShellExtClassFactory::~CShellExtClassFactory()				
{
    g_cRefThisDll--;
}

/***************************************************************
	Function:
	STDMETHODIMP CShellExtClassFactory::QueryInterface(REFIID riid,
                                                   LPVOID FAR *ppv)

	Purpose:
	Return a ptr to an interface

	Param			Type			Use
	-----			----			---
	riid			REFIID			interface to get
	ppv				LPVOID *		ptr to rec our interface

	Returns
	-------
	NOERROR on successz

***************************************************************/
STDMETHODIMP CShellExtClassFactory::QueryInterface(REFIID riid,
                                                   LPVOID FAR *ppv)
{
    ODS("CShellExtClassFactory::QueryInterface()\r\n");

    *ppv = NULL;

    // Any interface on this object is the object pointer

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
    {
        *ppv = (LPCLASSFACTORY)this;

        AddRef();

        return NOERROR;
    }

    return E_NOINTERFACE;
}	

/***************************************************************
	Function:
	STDMETHODIMP_(ULONG) CShellExtClassFactory::AddRef()

	Purpose:
	Increase our object's reference count

	Param			Type			Use
	-----			----			---

	Returns
	-------
	# of references

***************************************************************/
STDMETHODIMP_(ULONG) CShellExtClassFactory::AddRef()
{
    return ++m_cRef;
}

/***************************************************************
	Function:
	STDMETHODIMP_(ULONG) CShellExtClassFactory::Release()

	Purpose:
	Decrease our object's reference count

	Param			Type			Use
	-----			----			---

	Returns
	-------
	# of references

***************************************************************/

STDMETHODIMP_(ULONG) CShellExtClassFactory::Release()
{
    if (--m_cRef)
        return m_cRef;

    delete this;

    return 0L;
}

/***************************************************************
	Function:
	STDMETHODIMP CShellExtClassFactory::CreateInstance(LPUNKNOWN pUnkOuter,
                                                      REFIID riid,
                                                      LPVOID *ppvObj)

	Purpose:
	Create an instace of a particular interface

	Param			Type			Use
	-----			----			---
	pUnkOuter		LPUNKNOWN		outer IUnknown
	riid			REFIID			interface ID
	ppvObj			LPVOID *		ptr to rec our interface
	Returns
	-------
	NOERROR on success

***************************************************************/
STDMETHODIMP CShellExtClassFactory::CreateInstance(LPUNKNOWN pUnkOuter,
                                                      REFIID riid,
                                                      LPVOID *ppvObj)
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState( ));

   ODS("CShellExtClassFactory::CreateInstance()\r\n");

   *ppvObj = NULL;

   // Shell extensions typically don't support aggregation (inheritance)

   if (pUnkOuter)
      return CLASS_E_NOAGGREGATION;

   // Create the main shell extension object.  The shell will then call
   // QueryInterface with IID_IShellExtInit--this is how shell extensions are
   // initialized.

   LPCSHELLEXT pShellExt;
   try 
   {
      pShellExt = new CShellExt();  //Create the CShellExt object
   } catch (CMemoryException *e) {
      e->Delete();
      pShellExt=NULL;
   }


   if (NULL == pShellExt)
      return E_OUTOFMEMORY;

   return pShellExt->QueryInterface(riid, ppvObj);
}

/***************************************************************
	Function:
	STDMETHODIMP CShellExtClassFactory::LockServer(BOOL fLock)

	Purpose:
	Pretend we did something

	Param			Type			Use
	-----			----			---

	Returns
	-------
	NOERROR, always

***************************************************************/
STDMETHODIMP CShellExtClassFactory::LockServer(BOOL fLock)
{
    return NOERROR;
}


/*
	What follows is our implementation of several interfaces :
			IContextMenu
			IShellExtInit 
			IExtractIcon 
			IPersistFile 
			IShellPropSheetExt
			ICopyHook

	These represent the services we can provide for Explorer.
	Not all of these need to be implemented.

	This code is unmodified from SHELLEX
*/
// *********************** CShellExt *************************

/***************************************************************
	Function:
	CShellExt::CShellExt()

	Purpose:
	contructor

	Param			Type			Use
	-----			----			---

	Returns
	-------

***************************************************************/
CShellExt::CShellExt()
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState( ));

   ODS("CShellExt::CShellExt()\r\n");

   m_cRef = 0L;
   m_pDataObj = NULL;

   g_cRefThisDll++;   

   m_menuBmp.LoadBitmap(IDB_MENU_BMP);

}

/***************************************************************
	Function:
	CShellExt::~CShellExt()

	Purpose:
	destructor

	Param			Type			Use
	-----			----			---

	Returns
	-------

***************************************************************/
CShellExt::~CShellExt()
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState( ));

   if (m_pDataObj)
      m_pDataObj->Release();

   g_cRefThisDll--;

   if (m_menuBmp.GetSafeHandle())
      m_menuBmp.DeleteObject();
}

/***************************************************************
	Function:
	STDMETHODIMP CShellExt::QueryInterface(REFIID riid, LPVOID FAR *ppv)

	Purpose:
	See if we support a given interface

	Param			Type			Use
	-----			----			---
	riid			REFIID			interface ID
	ppv				LPVOID *		ptr to rec our interface

	Returns
	-------
	NOERROR on success

***************************************************************/
STDMETHODIMP CShellExt::QueryInterface(REFIID riid, LPVOID FAR *ppv)
{
   *ppv = NULL;

   if (IsEqualIID(riid, IID_IShellExtInit) || IsEqualIID(riid, IID_IUnknown))
   {
      ODS("CShellExt::QueryInterface()==>IID_IShellExtInit\r\n");

      *ppv = (LPSHELLEXTINIT)this;
   }
   else if (IsEqualIID(riid, IID_IContextMenu))
   {
      ODS("CShellExt::QueryInterface()==>IID_IContextMenu\r\n");

      *ppv = (LPCONTEXTMENU)this;
   }

   if (*ppv)
   {
      AddRef();
      return NOERROR;
   }

   ODS("CShellExt::QueryInterface()==>Unknown Interface!\r\n");

	return E_NOINTERFACE;
}

/***************************************************************
	Function:
	STDMETHODIMP_(ULONG) CShellExt::AddRef()

	Purpose:
	increase our reference count

	Param			Type			Use
	-----			----			---

	Returns
	-------
	# of references

***************************************************************/
STDMETHODIMP_(ULONG) CShellExt::AddRef()
{
   ODS("CShellExt::AddRef()\r\n");

   return ++m_cRef;
}

/***************************************************************
	Function:
	STDMETHODIMP_(ULONG) CShellExt::Release()

	Purpose:
	decrease our reference count, clean up

	Param			Type			Use
	-----			----			---

	Returns
	-------
	# of references

***************************************************************/
STDMETHODIMP_(ULONG) CShellExt::Release()
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState( ));

   ODS("CShellExt::Release()\r\n");

   if (--m_cRef)
      return m_cRef;

   delete this;

   return 0L;
}


//
//  FUNCTION: CShellExt::Initialize(LPCITEMIDLIST, LPDATAOBJECT, HKEY)
//
//  PURPOSE: Called by the shell when initializing a context menu or property
//           sheet extension.
//
//  PARAMETERS:
//    pIDFolder - Specifies the parent folder
//    pDataObj  - Spefifies the set of items selected in that folder.
//    hRegKey   - Specifies the type of the focused item in the selection.
//
//  RETURN VALUE:
//
//    NOERROR in all cases.
//
//  COMMENTS:   Note that at the time this function is called, we don't know 
//              (or care) what type of shell extension is being initialized.  
//              It could be a context menu or a property sheet.
//

STDMETHODIMP CShellExt::Initialize(LPCITEMIDLIST pIDFolder,
                                   LPDATAOBJECT pDataObj,
                                   HKEY hRegKey)
{

   AFX_MANAGE_STATE(AfxGetStaticModuleState( ));

   ODS("CShellExtFld::Initialize()\r\n");

   // this can be called more than once
   if (m_pDataObj)
	   m_pDataObj->Release();

   // duplicate the object pointer and registry handle
   if (pDataObj)
   {
	   m_pDataObj = pDataObj;
	   pDataObj->AddRef();
   } 
   else
   {
	   return E_FAIL;
   }

   return GetSelectedFiles(pIDFolder, m_pDataObj, m_csaPaths);
}
