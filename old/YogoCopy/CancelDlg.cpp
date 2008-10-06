// CancelDlg.cpp : implementation file
//
				  
#include "stdafx.h"
#include "resource.h"
#include "CancelDlg.h"

#include "SHUtils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCancelDlg dialog

#define ICONS 9
HICON gIconArray[ICONS];				    


CCancelDlg::CCancelDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CCancelDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CCancelDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	gIconArray[0] = AfxGetApp()->LoadIcon(IDI_ANI_1);
	gIconArray[1] = AfxGetApp()->LoadIcon(IDI_ANI_2);
	gIconArray[2] = AfxGetApp()->LoadIcon(IDI_ANI_3);
	gIconArray[3] = AfxGetApp()->LoadIcon(IDI_ANI_4);
	gIconArray[4] = AfxGetApp()->LoadIcon(IDI_ANI_5);
	gIconArray[5] = AfxGetApp()->LoadIcon(IDI_ANI_6);
	gIconArray[6] = AfxGetApp()->LoadIcon(IDI_ANI_7);
   gIconArray[7] = AfxGetApp()->LoadIcon(IDI_ANI_8);
   gIconArray[8] = AfxGetApp()->LoadIcon(IDI_ANI_9);

	m_curIcon=0;

}


void CCancelDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCancelDlg)
	DDX_Control(pDX, IDC_ITEM_PROGRESS, m_itemProgress);
	DDX_Control(pDX, IDCANCEL, m_cancelBtn);
	DDX_Control(pDX, IDC_CANCEL_TEXT, m_cancelText);
	DDX_Control(pDX, IDC_ANI_ICON, m_aniIcon);
   DDX_Control(pDX, IDC_PATH_TEXT, m_pathWnd);
	DDX_Control(pDX, IDC_PROGRESS, m_progress);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CCancelDlg, CDialog)
	//{{AFX_MSG_MAP(CCancelDlg)
	ON_WM_TIMER()
	ON_WM_SHOWWINDOW()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCancelDlg message handlers

BOOL CCancelDlg::Create(CWnd* pParentWnd) 
{
	m_parent=pParentWnd;
	return CDialog::Create(IDD, pParentWnd);
}

void CCancelDlg::OnTimer(UINT nIDEvent) 
{
	NextIcon();	
	CDialog::OnTimer(nIDEvent);

}

void CCancelDlg::PostNcDestroy() 
{
	// modeless dialogs delete themselves
	delete this;

}

void CCancelDlg::OnShowWindow(BOOL bShow, UINT nStatus) 
{

	if (bShow==TRUE) 
   {
      // update the icon every 1/2 second
		SetTimer(10, 500, NULL);
		m_curIcon=0;
		m_aniIcon.SetIcon(gIconArray[m_curIcon]);
	} 
   else 
   {
		KillTimer(10);
		m_curIcon=0;
	}

	CDialog::OnShowWindow(bShow, nStatus);	
}
void CCancelDlg::ShutDown()
{
	DestroyWindow();
}

void CCancelDlg::NextIcon()
{
	m_aniIcon.SetIcon(gIconArray[m_curIcon]);
	m_curIcon++;
	if (m_curIcon > ICONS)
		m_curIcon=0;
	
}

void CCancelDlg::OnCancel() 
{
	m_cancelBtn.EnableWindow(FALSE);

	m_cancelText.SetWindowText("Waiting for operation to complete.");

	SET_SAFE(m_bCancel, TRUE);
}

BOOL CCancelDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
   SetWindowText(SHELLEXNAME" Is Busy...");

	m_cancelText.SetWindowText("");

	CenterWindow();

	SET_SAFE(m_bCancel, FALSE);

	m_itemProgress.SetRange(0, 101);
	m_itemProgress.SetPos(0);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


void CCancelDlg::SetProgText(CString csText)
{
	m_cancelText.SetWindowText(csText);
}

void CCancelDlg::SetPathText(CString csText)
{
	m_pathWnd.SetWindowText(csText);
}

void CCancelDlg::SetTotalItems(int iItems)
{
	m_progress.SetRange(0, iItems);
	m_progress.SetStep(1);
	m_progress.SetPos(0);
}

void CCancelDlg::StepIt()
{
	m_progress.StepIt();
}

void CCancelDlg::SetItemPercentDone(int i)
{
   m_itemProgress.SetPos(i);
}
