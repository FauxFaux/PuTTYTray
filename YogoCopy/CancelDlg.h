#if !defined(AFX_CANCELDLG_H__7F813CE2_9AFB_11D1_8159_444553540000__INCLUDED_)
#define AFX_CANCELDLG_H__7F813CE2_9AFB_11D1_8159_444553540000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// CancelDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CCancelDlg dialog

class CCancelDlg : public CDialog
{
// Construction
public:
	CCancelDlg(CWnd* pParent = NULL);   // standard constructor

	UINT		m_curIcon;
	CWnd *		m_parent;

	void		NextIcon();

	void		ShutDown();

	void 		SetProgText(CString txt);

	void 		SetExpText(CString txt);

	void		SetPathText(CString csText);

	BOOL		m_bCancel;

   void     SetItemPercentDone(int iPct);

   void     SetTotalItems(int iItems);

   void     StepIt();

// Dialog Data
	//{{AFX_DATA(CCancelDlg)
	enum { IDD = IDD_CANCEL_DLG };
	CButton	m_cancelBtn;
	CStatic	m_cancelText;
	CStatic	m_aniIcon;
	CStatic m_pathWnd;
	CProgressCtrl   m_itemProgress;
   CProgressCtrl	m_progress;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCancelDlg)
	public:
	virtual BOOL Create(CWnd* pParentWnd);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CCancelDlg)
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()


};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CANCELDLG_H__7F813CE2_9AFB_11D1_8159_444553540000__INCLUDED_)
