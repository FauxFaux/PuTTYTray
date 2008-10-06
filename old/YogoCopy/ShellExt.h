#ifndef SHELLEXTDEFSH
#define SHELLEXTDEFSH

// this class factory object creates context menu handlers for Windows 95 shell
class CShellExtClassFactory : public IClassFactory
{
protected:
	ULONG        m_cRef;


public:
	CShellExtClassFactory();
	~CShellExtClassFactory();

	//IUnknown members
	STDMETHODIMP			QueryInterface(REFIID, LPVOID FAR *);
	STDMETHODIMP_(ULONG)	AddRef();
	STDMETHODIMP_(ULONG)	Release();

	//IClassFactory members
	STDMETHODIMP		CreateInstance(LPUNKNOWN, REFIID, LPVOID FAR *);
	STDMETHODIMP		LockServer(BOOL);

};
typedef CShellExtClassFactory *LPCSHELLEXTCLASSFACTORY;

// this is the actual OLE Shell context menu handler
class CShellExt : public IContextMenu, 
                         IShellExtInit
{
public:
  

protected:
	ULONG			   m_cRef;
	LPDATAOBJECT	m_pDataObj;

   // CShellExt::Initialize fills this with the paths to the
   // files that were selected when the user chose our menu item.
	CStringArray	m_csaPaths;

   // our menu bitmap
   CBitmap        m_menuBmp;
	//////////////////////////////////////////////////////////////////

protected:

public:
	// generic shell ext functions
	CShellExt();
	~CShellExt();

	//IUnknown members
	STDMETHODIMP			QueryInterface(REFIID, LPVOID FAR *);
	STDMETHODIMP_(ULONG)	AddRef();
	STDMETHODIMP_(ULONG)	Release();

   //IShellExtInit methods
	STDMETHODIMP		    Initialize(LPCITEMIDLIST pIDFolder, 
                                          LPDATAOBJECT pDataObj, 
                                          HKEY hKeyID);

	//IShell members

   //////////////////////////////////////////////////////////////
   //
   // These are the only functions you really need to modify. They
   // are all in ShellYogoCopy.cpp
   
   //
   // called to get menu item strings for Explorer
   //
	STDMETHODIMP			QueryContextMenu(HMENU hMenu,
                                          UINT indexMenu, 
                                          UINT idCmdFirst,
                                          UINT idCmdLast, 
                                          UINT uFlags);
   //
   // called after a menu item has been selected
   //
	STDMETHODIMP			InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi);

   //
   // fetch various strings for Explorer
   //
	STDMETHODIMP			GetCommandString(UINT idCmd, 
                                          UINT uFlags, 
                                          UINT FAR *reserved, 
                                          LPSTR pszName, 
                                          UINT cchMax);

   //
   //
   //////////////////////////////////////////////////////////////
   
};
typedef CShellExt *LPCSHELLEXT;


#endif