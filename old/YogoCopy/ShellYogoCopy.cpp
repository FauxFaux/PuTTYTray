#include "stdafx.h"
#include "resource.h"
#include "priv.h"
#include "ShellExt.h"
#include "YogoCopy.h"

#include <shlobj.h>
#include <shlguid.h>
#include "io.h"

// utilities
#include "ShUtils.h"
#include "FileProcess.h"
#include "CancelDlg.h"              

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/*----------------------------------------------------------------

  FUNCTION: CShellExt::InvokeCommand(LPCMINVOKECOMMANDINFO)

  PURPOSE: Called by the shell after the user has selected on of the
           menu items that was added in QueryContextMenu(). This is 
           the function where you get to do your real work!!!

  PARAMETERS:
    lpcmi - Pointer to an CMINVOKECOMMANDINFO structure

  RETURN VALUE:


  COMMENTS:
 
----------------------------------------------------------------*/

STDMETHODIMP CShellExt::InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi)
{
	// look at all the MFC stuff in here! call this to allow us to 
	// not blow up when we try to use it.
	AFX_MANAGE_STATE(AfxGetStaticModuleState( ));

	HINSTANCE hInst = AfxGetInstanceHandle();

    ODS("CShellExt::InvokeCommand()\r\n");

	HWND hParentWnd = lpcmi->hwnd;
                                      
	HRESULT hr = NOERROR;

    //If HIWORD(lpcmi->lpVerb) then we have been called programmatically
    //and lpVerb is a command that should be invoked.  Otherwise, the shell
    //has called us, and LOWORD(lpcmi->lpVerb) is the menu ID the user has
    //selected.  Actually, it's (menu ID - idCmdFirst) from QueryContextMenu().
	UINT idCmd = 0;

   if (!HIWORD(lpcmi->lpVerb)) 
   {
      idCmd = LOWORD(lpcmi->lpVerb);

		// process it
		switch (idCmd) 
      {
		default:
		case 0: // operation 1
		case 1: // operation 2
			{
            // get a CWnd for Explorer
				CWnd *pParentWnd = NULL;
				if (hParentWnd!=NULL)
					pParentWnd = CWnd::FromHandle(hParentWnd);

            // disable explorer
				::PostMessage(hParentWnd, WM_ENABLE, (WPARAM)FALSE, (LPARAM)0);

            // create our process/cancel dialog
				CCancelDlg *pCancel;
				try 
				{
					pCancel = new CCancelDlg;
				} 
				catch (CMemoryException *e) 
				{
					e->ReportError();
					e->Delete();
					pCancel=NULL;
               return hr;
				}

            // show it
				pCancel->Create(pParentWnd);
				if (::IsWindow(pCancel->m_hWnd)) 
				{
					pCancel->ShowWindow(SW_SHOW);
				}

            int iFiles = m_csaPaths.GetSize();

            // process files in m_csaPaths
            pCancel->SetTotalItems(iFiles);

            // wrap this whole thing in a nice try/catch
				try 
            {
					// do it!
					for (int i=0; i < iFiles; i++) 
               {
                  // get the current file. this array is filled in CShellExt::Initialize
                  CString csFile = m_csaPaths.GetAt(i);

                  // update progress dialog
						CString f; f.Format("Processing file %d of %d", i + 1, iFiles);
                  pCancel->SetProgText(f);
						pCancel->SetPathText(csFile);
                  pCancel->StepIt();

                  // move some Windows messages around
						ShMsgPump();

                  // this struct carries info to and from the thread
                  ThreadInfo is;
						is.bDone = FALSE;
						is.bStop = FALSE;
						is.csFile = csFile;

						//////////////////////////////////////////////////////
						// start the worker thread
						CWinThread *pThread = AfxBeginThread(FileProcessThreadFunc, 
																	(LPVOID)&is,
																	THREAD_PRIORITY_NORMAL,
																	0,
																	CREATE_SUSPENDED);

						//////////////////////////////////////////////////////
						// did we create OK?
						if (pThread ==NULL) 
						{
							ODS("Thread creation falied\n");
							break;
						}

						//////////////////////////////////////////////////////
						// start doing some real work

                  // this will start the thread for real. 
						if (pThread->ResumeThread()==-1) 
						{
							ODS("Resume thread failure\n");
							break;
						}
						
                  BOOL bCancel = FALSE;
						BOOL bDone = FALSE;
                  int iCurPercentDone = 0;
                  int iLastPercentDone = -1;

						//////////////////////////////////////////////////////
                  // now, we just sit back and wait
						// for the thread to signal finished

						while (!bDone) 
                  {
							// test for cancel signal from the progress dialod
							bCancel = GET_SAFE(pCancel->m_bCancel);
							if (bCancel) 
							{
								ODS("bCancel (main)\n");
								SET_SAFE(is.bStop, TRUE);
							}

							// test for thread finished
							bDone = GET_SAFE(is.bDone);

                     // get progress
                     iCurPercentDone = GET_SAFE(is.iPercentDone);

                     // update the progress meter
                     if (iCurPercentDone!=iLastPercentDone)
                     {
                        pCancel->SetItemPercentDone(iCurPercentDone);
                        iLastPercentDone = iCurPercentDone;
                     }

                     // let the dialog breathe
							ShMsgPump();
						}

						ODS("bDone (main)\n");

                  // operation cancelled. abort the whole thing...
						if (bCancel)
						{
							ODS("cancelled (main)\n");
							break;
						}

                  // handle any errors that may have popped up
                  int iErr = GET_SAFE(is.iErr);

                  if (iErr!=0)
                  {
                     CString msg;
                     
                     pCancel->ShowWindow(SW_HIDE);
                     
                     msg.Format("Error : %d", iErr);


                     UINT res = AfxMessageBox(SHELLEXNAME + msg, MB_OKCANCEL);

                     // if the user presses 'cancel', abort
                     if (res==IDCANCEL)
                     {
                        break;
                     }

                     // bring back our progress dialog
                     pCancel->ShowWindow(SW_SHOW);

                  } // err

               } // file loop
				} 
				catch (CException *e) 
				{
					e->ReportError();
					e->Delete();
				}

				pCancel->ShowWindow(SW_HIDE);
				pCancel->ShutDown();

				::PostMessage(hParentWnd, WM_ENABLE, (WPARAM)TRUE, (LPARAM)0);
			}
			break;
		}	// switch on command
	}

   return hr;
}


////////////////////////////////////////////////////////////////////////
//
//  FUNCTION: CShellExt::GetCommandString(...)
//
//  PURPOSE: Retrieve various text strinsg associated with the context menu
//
//	Param			Type			Use
//	-----			----			---
//	idCmd			UINT			ID of the command
//	uFlags			UINT			which type of info are we requesting
//	reserved		UINT *			must be NULL
//	pszName			LPSTR			output buffer
//	cchMax			UINT			max chars to copy to pszName
//
////////////////////////////////////////////////////////////////////////

STDMETHODIMP CShellExt::GetCommandString(UINT idCmd,
                                         UINT uFlags,
                                         UINT FAR *reserved,
                                         LPSTR pszName,
                                         UINT cchMax)
{
   ODS("CShellExt::GetCommandString()\r\n");
	
	AFX_MANAGE_STATE(AfxGetStaticModuleState( ));

	HINSTANCE hInst = AfxGetInstanceHandle();

	switch (uFlags) 
   {
	case GCS_HELPTEXT:		// fetch help text for display at the bottom of the 
							// explorer window
		switch (idCmd)
		{
			case 0:
            strncpy(pszName, "Do something to the selected file(s)", cchMax);
				break;
			case 1:
            strncpy(pszName, "Do something else to the selected file(s)", cchMax);
				break;
         default:
            strncpy(pszName, SHELLEXNAME, cchMax);
				break;
		}
		break;

	case GCS_VALIDATE:
		break;

	case GCS_VERB:			// language-independent command name for the menu item 
		switch (idCmd)
		{
			case 0:
            strncpy(pszName, "Operation1", cchMax);
				break;
			case 1:
            strncpy(pszName, "Operation2", cchMax);
				break;
         default:
            strncpy(pszName, SHELLEXNAME, cchMax);
				break;
		}

		break;
	}
    return NOERROR;
}

///////////////////////////////////////////////////////////////////////////
//
//  FUNCTION: CShellExt::QueryContextMenu(HMENU, UINT, UINT, UINT, UINT)
//
//  PURPOSE: Called by the shell just before the context menu is displayed.
//           This is where you add your specific menu items.
//
//  PARAMETERS:
//    hMenu      - Handle to the context menu
//    indexMenu  - Index of where to begin inserting menu items
//    idCmdFirst - Lowest value for new menu ID's
//    idCmtLast  - Highest value for new menu ID's
//    uFlags     - Specifies the context of the menu event
//
//  RETURN VALUE:
//
//
//  COMMENTS:
//
///////////////////////////////////////////////////////////////////////////

STDMETHODIMP CShellExt::QueryContextMenu(HMENU hMenu,
                                         UINT indexMenu,
                                         UINT idCmdFirst,
                                         UINT idCmdLast,
                                         UINT uFlags)
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState( ));

   return CreateShellExtMenu(hMenu, 
                              indexMenu, 
                              idCmdFirst, 
                              idCmdLast, 
                              uFlags, 
                              (HBITMAP)m_menuBmp.GetSafeHandle());
}

