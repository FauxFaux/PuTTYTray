#include "stdafx.h"
#include "resource.h"
#include "ShUtils.h"
#include "errno.h"
#include <direct.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CRITICAL_SECTION g_critSectionBreak;

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
void ShMsgPump()
{
	// if we do MFC stuff in an exported fn, call this first!
	AFX_MANAGE_STATE(AfxGetStaticModuleState( ));

	DWORD dInitTime = GetTickCount();
	MSG m_msgCur;                   // current message
	CWinApp	*pWinApp = AfxGetApp();   
	while (::PeekMessage(&m_msgCur, NULL, NULL, NULL, PM_NOREMOVE)  &&
			(GetTickCount() - dInitTime < 200) )
	{
		pWinApp->PumpMessage();
	}
}

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
int WideCharToLocal(LPTSTR pLocal, LPWSTR pWide, DWORD dwChars)
{
	*pLocal = 0;

	#ifdef UNICODE
	lstrcpyn(pLocal, pWide, dwChars);
	#else
	WideCharToMultiByte( CP_ACP, 
						 0, 
						 pWide, 
						 -1, 
						 pLocal, 
						 dwChars, 
						 NULL, 
						 NULL);
	#endif

	return lstrlen(pLocal);
}

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
int LocalToWideChar(LPWSTR pWide, LPTSTR pLocal, DWORD dwChars)
{
	*pWide = 0;

	#ifdef UNICODE
	lstrcpyn(pWide, pLocal, dwChars);
	#else
	MultiByteToWideChar( CP_ACP, 
						 0, 
						 pLocal, 
						 -1, 
						 pWide, 
						 dwChars); 
	#endif

	return lstrlenW(pWide);
}

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
STDMETHODIMP CreateShellExtMenu(HMENU hMenu,
                                         UINT indexMenu,
                                         UINT idCmdFirst,
                                         UINT idCmdLast,
                                         UINT uFlags,
                                         HBITMAP hbmp)
{
   ODS("CShellExt::QueryContextMenu()\r\n");

   UINT idCmd = idCmdFirst;

   BOOL bAppendItems=TRUE;


   HINSTANCE hInst = AfxGetInstanceHandle();

   if ((uFlags & 0x000F) == CMF_NORMAL)  //Check == here, since CMF_NORMAL=0
   {
      ODS("CMF_NORMAL...\r\n");
   }
   else
   if (uFlags & CMF_VERBSONLY)
   {
      ODS("CMF_VERBSONLY...\r\n");
   }
   else
   if (uFlags & CMF_EXPLORE)
   {
      ODS("CMF_EXPLORE...\r\n");
   }
   else
   if (uFlags & CMF_DEFAULTONLY)
   {
      ODS("CMF_DEFAULTONLY...\r\n");
      bAppendItems = FALSE;
   }
   else
   {
      char szTemp[32];

      wsprintf(szTemp, "uFlags==>%d\r\n", uFlags);
      ODS("CMF_default...\r\ne");
      ODS(szTemp);
      bAppendItems = FALSE;
   }

   BOOL bPopUp = TRUE;

   if (bAppendItems)
   {
      HMENU hParentMenu;

      // add popup
      if (bPopUp)
      {
         hParentMenu = ::CreateMenu();
      }
      else
      {
         // or not
         hParentMenu = hMenu;
      }

      if (hParentMenu) 
      {
         // pop-up title
         if (bPopUp)
         {        
            InsertMenu(hMenu, indexMenu++, MF_POPUP|MF_BYPOSITION, (UINT)hParentMenu, SHELLEXNAME);
            SetMenuItemBitmaps(hMenu, indexMenu-1, MF_BITMAP | MF_BYPOSITION, hbmp, hbmp);
         }

         InsertMenu(hParentMenu, indexMenu++, MF_STRING|MF_BYPOSITION, idCmd++, "Operation 1 using "SHELLEXNAME);
         InsertMenu(hParentMenu, indexMenu++, MF_STRING|MF_BYPOSITION, idCmd++, "Operation 2 using "SHELLEXNAME);
      }
                       
      return ResultFromShort(idCmd-idCmdFirst); //Must return number of menu
      //items we added.
   }
   return NOERROR;
}

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
STDMETHODIMP GetSelectedFiles(LPCITEMIDLIST pIDFolder,
                                   LPDATAOBJECT &pDataObj,
                                   CStringArray &csaPaths)
{
   // get these paths into a CStringArray
   csaPaths.RemoveAll();

   // fetch all of the file names we're supposed to operate on
   if (pDataObj) 
   {
	   pDataObj->AddRef();

	   STGMEDIUM medium;
	   FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};


	   HRESULT hr = pDataObj->GetData (&fe, &medium);
	   if (FAILED (hr))
	   {
		   return E_FAIL;
	   }

	   // buffer to receive filenames
	   char path[MAX_PATH];

	   // how many are there?
	   UINT fileCount = DragQueryFile((HDROP)medium.hGlobal, 0xFFFFFFFF,
	   path, MAX_PATH);

	   if (fileCount>0)
	   {
		   // avoid wasting mem when this thing gets filled in
		   csaPaths.SetSize(fileCount);

		   // stash the paths in our CStringArray
		   for (UINT i=0;i<fileCount;i++) 
         {
			   // clear old path
			   memset(path, 0, MAX_PATH);
			   // fetch new path
			   if (DragQueryFile((HDROP)medium.hGlobal, i, path, MAX_PATH)) 
            {
 		         csaPaths.SetAt(i, path);
		      }
	      }

         csaPaths.FreeExtra();
      }

      // free our path memory - we have the info in our CStringArray
      ReleaseStgMedium(&medium);
   }

   return NOERROR;
}
