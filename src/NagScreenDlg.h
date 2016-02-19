#pragma once
#include "UrlWnd.h"

class CNagScreenDlg : public CDialog
{
	enum { IDD = IDD_NAGSCREEN };

public:
	CNagScreenDlg(CWnd* pParent = NULL);

private:
	virtual BOOL OnInitDialog();
	afx_msg void OnEntercode();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	DECLARE_MESSAGE_MAP()

	CUrlWnd	m_wndOrder;
	CFont m_boldFont;
};
