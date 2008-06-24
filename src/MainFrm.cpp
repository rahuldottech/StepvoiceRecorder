////////////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include <winuser.h>
#include "stdafx.h"
#include "Interface\Buttons\BitmapBtn.h"

#include "MP3_Recorder.h"
#include "MainFrm.h"
#include "common.h"

#include "system.h"
#include <math.h>

HSTREAM g_stream_handle = 0;
HRECORD g_record_handle = 0;
HRECORD g_monitoring_handle = 0;

////////////////////////////////////////////////////////////////////////////////
/// Check if a file is suitable for recording (not exist or length = 0).
bool IsSuitableForRecording(CString a_filename)
{
	HANDLE l_file_handle = CreateFile(
		a_filename.GetBuffer(a_filename.GetLength()),
		GENERIC_READ,
		FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (l_file_handle == INVALID_HANDLE_VALUE)
	{
		DWORD l_error = ::GetLastError();
		return (l_error && l_error != ERROR_FILE_NOT_FOUND) ? false: true;
	}
	DWORD l_size = GetFileSize(l_file_handle, NULL);
	CloseHandle(l_file_handle);
	return (l_size == 0);
}

////////////////////////////////////////////////////////////////////////////////
#define WM_ICON_NOTIFY WM_USER+10
static const UINT UWM_ARE_YOU_ME = ::RegisterWindowMessage(
	_T("UWM_ARE_YOU_ME-{B87861B4-8BE0-4dc7-A952-E8FFEEF48FD3}"));

////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_COMMAND(IDM_OPT_EM, OnOptEm)
	ON_COMMAND(IDM_OPT_TOP, OnOptTop)
	ON_BN_CLICKED(IDB_BTNOPEN, OnBtnOPEN)
	ON_BN_CLICKED(IDB_BTNPLAY, OnBtnPLAY)
	ON_BN_CLICKED(IDB_BTNSTOP,  OnBtnSTOP)
	ON_BN_CLICKED(IDB_BTNREC,  OnBtnREC)
	ON_BN_CLICKED(IDB_MIX_REC,  OnBtnMIX_REC)
	ON_BN_CLICKED(IDB_MIX_PLAY, OnBtnMIX_PLAY)
	ON_BN_CLICKED(IDB_MIX_INV, OnBtnMIX_INV)
	ON_BN_CLICKED(IDB_MIX_SEL, OnBtnMIX_SEL)
	ON_WM_LBUTTONDOWN()
	ON_COMMAND(IDM_MIX_REC, OnMixRec)
	ON_COMMAND(IDM_MIX_PLAY, OnMixPlay)
	ON_COMMAND(IDA_MIX_REC, OnMixRecA)
	ON_COMMAND(IDA_MIX_PLAY, OnMixPlayA)
	ON_WM_HSCROLL()
	ON_WM_MOVING()
	ON_WM_NCLBUTTONDOWN()
	ON_COMMAND(IDM_OPT_COM, OnOptCom)
	ON_COMMAND(IDM_FILE_FINDFILE, OnFileFindfile)
	ON_COMMAND(IDM_FILE_CLOSE, OnFileClose)
	ON_WM_TIMER()
	ON_UPDATE_COMMAND_UI(IDM_SOUND_REC, OnUpdateSoundRec)
	ON_UPDATE_COMMAND_UI(IDM_OPT_TOP, OnUpdateOptTop)
	ON_UPDATE_COMMAND_UI(IDM_OPT_EM, OnUpdateOptEm)
	ON_COMMAND(IDA_SOUND_REC, OnSoundRecA)
	ON_COMMAND(IDA_SOUND_STOP, OnSoundStopA)
	ON_COMMAND(IDA_SOUND_STOP2, OnSoundStopA)
	ON_COMMAND(IDA_SOUND_PLAY, OnSoundPlayA)
	ON_COMMAND(IDA_SOUND_BEGIN, OnSoundBeginA)
	ON_COMMAND(IDA_SOUND_REW, OnSoundRewA)
	ON_COMMAND(IDA_SOUND_FF, OnSoundFfA)
	ON_COMMAND(IDA_SOUND_END, OnSoundEndA)
	ON_COMMAND(IDA_FILE_CREATEOPEN, OnFileCreateopenA)
	ON_COMMAND(IDM_SOUND_BEGIN, OnSoundBegin)
	ON_COMMAND(IDM_SOUND_REW, OnSoundRew)
	ON_COMMAND(IDM_SOUND_FF, OnSoundFf)
	ON_COMMAND(IDM_SOUND_END, OnSoundEnd)
	ON_WM_DROPFILES()
	ON_COMMAND(IDM_FILE_DELETE, OnFileDelete)
	ON_COMMAND(IDM_FILE_CLEAR, OnFileClear)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONUP()
	ON_COMMAND(IDM_OPT_SNDDEV, OnOptSnddev)
	ON_WM_SIZE()
	ON_UPDATE_COMMAND_UI(IDM_TRAY_PLAY, OnUpdateTrayPlay)
	ON_UPDATE_COMMAND_UI(IDM_TRAY_REC, OnUpdateTrayRec)
	ON_COMMAND(ID_STAT_PREF, OnStatPref)
	ON_COMMAND(IDA_VOL_UP, OnVolUpA)
	ON_COMMAND(IDA_VOL_DOWN, OnVolDownA)
	ON_COMMAND(IDM_FILE_CREATEOPEN, OnBtnOPEN)
	ON_COMMAND(IDA_FILE_FINDFILE, OnFileFindfile)
	ON_COMMAND(IDA_OPT_COM, OnOptCom)
	ON_COMMAND(IDA_OPT_EM, OnOptEm)
	ON_COMMAND(IDA_OPT_TOP, OnOptTop)
	ON_COMMAND(IDM_SOUND_REC, OnBtnREC)
	ON_COMMAND(IDM_SOUND_STOP, OnBtnSTOP)
	ON_COMMAND(IDM_SOUND_PLAY, OnBtnPLAY)
	ON_COMMAND(IDA_FILE_CLEAR, OnFileClear)
	ON_WM_MOUSEWHEEL()
	//}}AFX_MSG_MAP
	ON_REGISTERED_MESSAGE(UWM_ARE_YOU_ME, OnAreYouMe)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnToolTipNotify)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipNotify)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_SOUND_PLAY, IDM_SOUND_END,
							   OnUpdateSoundPlay)
	ON_MESSAGE(WM_ICON_NOTIFY, OnTrayNotification)
	ON_COMMAND_RANGE(ID_MIXITEM_REC0, ID_MIXITEM_REC0+50, OnRecMixMenuSelect)
	ON_COMMAND_RANGE(ID_MIXITEM_PLAY0, ID_MIXITEM_PLAY0+3, OnPlayMixMenuSelect)
END_MESSAGE_MAP()

//====================================================================
LRESULT CMainFrame::OnAreYouMe(WPARAM, LPARAM)
{
    return UWM_ARE_YOU_ME;
} // CMainFrame::OnAreYouMe

////////////////////////////////////////////////////////////////////////////////
double GetMaxPeakDB(HRECORD a_handle)
{
	BASS_CHANNELINFO l_ci;
	BOOL l_result = BASS_ChannelGetInfo(a_handle, &l_ci);

	int l_max_possible = 0xFFFF / 2; // 16-bit as default
	if (l_ci.flags & BASS_SAMPLE_8BITS)
	{
		l_max_possible = 0xFF / 2;
	}
	else if (l_ci.flags & BASS_SAMPLE_FLOAT)
	{
		l_max_possible = 1;
	}
	DWORD l_ch_level = BASS_ChannelGetLevel(a_handle);
	DWORD l_level = (LOWORD(l_ch_level) > HIWORD(l_ch_level)) ?
		LOWORD(l_ch_level) : HIWORD(l_ch_level);
	if (l_level < 1)
	{
		l_level = 1;
	}
	return 20 * log10(float(l_level)/l_max_possible);
}

////////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK CMainFrame::NewRecordProc(HRECORD a_handle, void* a_buffer,
										DWORD a_length, void* a_user)
{
	static char pBufOut[10240]; //10k buffer
	char* pBufIn = (char*)a_buffer;
	int	  nBufInSize = a_length;
	int   nBufOutSize= 0;

	CMainFrame* l_main_window = (CMainFrame *)a_user;

	if (l_main_window->m_vas.IsRunning())
	{
		double l_peak_db = GetMaxPeakDB(a_handle);
		l_main_window->m_vas.ProcessThreshold(l_peak_db);

		if (l_main_window->m_vas.CheckVAS()) 
		{
			l_main_window->ProcessVAS(true);
			return TRUE;
		}
		else
		{
			l_main_window->ProcessVAS(false);
		}
	}

	while (l_main_window->m_pEncoder->EncodeChunk(pBufIn, nBufInSize,
		pBufOut, nBufOutSize))
	{
		try
		{
			l_main_window->m_record_file.Write(pBufOut, nBufOutSize);
		}
		catch (CFileException *)
		{	// Possible, the disk is full.
			AfxMessageBox(IDS_REC_NOSPACE, MB_OK|MB_ICONSTOP);			
			l_main_window->PostMessage(WM_COMMAND, IDB_BTNSTOP,
				LONG(l_main_window->m_BtnSTOP.m_hWnd));
			return FALSE;
		}
		if (pBufIn)
		{	// Clearing pointer for iterations.
			pBufIn = NULL;
			nBufInSize = 0;
		}
	}
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
CMainFrame* CMainFrame::m_pMainFrame = NULL;

void CMainFrame::_RecordProc(char* pBuf, int dwBufSize)
{
	static char pBufOut[10000];
	char* pBufIn     = pBuf;
	int	  nBufInSize = dwBufSize, nBufOutSize = 0;
	DWORD dwBytesWritten;

	// ������ ������
	if(!m_pMainFrame->IsIconic())
		m_pMainFrame->m_GraphWnd.Update(pBuf, dwBufSize);

	// ������� ���� �������� ���������� (��� ������)
	if(m_pMainFrame->IsMonitoringOnly())
		return;

	// ��������� ���� �� VAS
	if(m_pMainFrame->m_vas.IsRunning())
	{
		double dParam1, dParam2, dResult;
		dParam1 = m_pMainFrame->m_GraphWnd.GetMaxPeakdB(pBuf, dwBufSize, 0);
		dParam2 = m_pMainFrame->m_GraphWnd.GetMaxPeakdB(pBuf, dwBufSize, 1);
		dResult = (dParam1 > dParam2) ? dParam1 : dParam2;

		m_pMainFrame->m_vas.ProcessThreshold(dResult);
		if(m_pMainFrame->m_vas.CheckVAS()) 
		{
			m_pMainFrame->ProcessVAS(true);
			return;
		}
		else
			m_pMainFrame->ProcessVAS(false);
	}

	// ����������� ������ � ������ �� ����
	while(m_pMainFrame->m_pEncoder->EncodeChunk(pBufIn, nBufInSize,
		pBufOut, nBufOutSize))
	{
		dwBytesWritten = m_pMainFrame->m_pSndFile->Write(pBufOut,
			nBufOutSize);
		// ��������� �� ����� �����
		if((int)dwBytesWritten < nBufOutSize)
		{
			AfxMessageBox(IDS_REC_NOSPACE, MB_OK|MB_ICONSTOP);
			m_pMainFrame->PostMessage(WM_COMMAND, IDB_BTNSTOP,
				LONG(m_pMainFrame->m_BtnSTOP.m_hWnd));
			break;
		}
		// �������� ��������� � ������ ��� ���������� ��������
		if(pBufIn)
		{
			pBufIn     = NULL;
			nBufInSize = 0;
		}
	}

}

//====================================================================
bool CMainFrame::_PlaybkProc(char* pBuf, int& nBufSize, int nBytesToShow)
{
	static char  cMp3Buffer[8192], *pMp3Buffer = NULL;
	static int   nMp3BufSize = 0;
	static bool  bNeedToRead = true;

	// ������ ������
	if(!m_pMainFrame->IsIconic())
		m_pMainFrame->m_GraphWnd.Update(pBuf, nBytesToShow);

read_again:
	if(bNeedToRead)
	{
		DWORD dwBytesRead = m_pMainFrame->m_pSndFile->Read(cMp3Buffer, 8192);
		// ��������� �� ����� �����
		if(dwBytesRead == 0)
		{
			m_pMainFrame->PostMessage(WM_COMMAND, IDB_BTNSTOP,
				LONG(m_pMainFrame->m_BtnSTOP.m_hWnd));
			// ������������� ��������� �� ������ �����
			//m_pMainFrame->PostMessage(WM_HSCROLL,  MAKEWPARAM(SB_THUMBPOSITION, 0),
			//	(LPARAM)m_pMainFrame->m_SliderTime.m_hWnd);
			//m_pMainFrame->m_SliderTime.SetCurPos(0);
			return false;			
		}
		// �������� ������ - �������������� �����
		pMp3Buffer  = cMp3Buffer;
		nMp3BufSize = dwBytesRead;
	}
	// ����������
	int nTemp = nBufSize;
	bNeedToRead = !m_pMainFrame->m_pDecoder->DecodeChunk(pMp3Buffer,
		nMp3BufSize, pBuf, nBufSize);
	if(!bNeedToRead)
	{
		pMp3Buffer  = NULL;
		nMp3BufSize = 0;
	}
	else
	{
		nBufSize = nTemp;
		goto read_again; // ���������� ������� ����� ������
	}

	return true;
}

//====================================================================
// ����������� / ����������
//====================================================================
CMainFrame::CMainFrame()
{	// �����
	m_pWaveIn	= NULL;
	m_pWaveOut	= NULL;
	m_pEncoder	= NULL;
	m_pDecoder	= NULL;
	m_pSndFile	= NULL;
	m_title		= NULL;

	m_nState	= STOP_STATE;
	m_strDir	= "";

	m_pOptDialog = NULL;

	//m_bOwnerCreated = false;

	m_pMainFrame = this;
	m_bAutoMenuEnable = false;
	LoadFrame(IDR_MAINFRAME, WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX);

	// init window snapping
	m_szMoveOffset.cx = 0;
	m_szMoveOffset.cy = 0;
	m_nSnapPixels = 7;
	//::GetWindowRect(::GetDesktopWindow(), &m_rDesktopRect);
	::SystemParametersInfo(SPI_GETWORKAREA, 0, &m_rDesktopRect, 0);

	m_nActiveMixerID = 0;
}

//====================================================================
CMainFrame::~CMainFrame()
{
	SAFE_DELETE(m_title);

	CloseMixerWindows();
}

/////////////////////////////////////////////////////////////////////////////
#pragma optimize("", off)
int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_bMonitoringBtn = false;

	// � ������������������ ������ ������� ���� "����������������"
	REG_CRYPT_BEGIN;
	#ifndef _DEBUG
		CMenu* pMenu = this->GetMenu();
		pMenu->RemoveMenu(4, MF_BYPOSITION);
		CMenu* pHelpSubMenu = pMenu->GetSubMenu(3);
		pHelpSubMenu->EnableMenuItem(5, MF_GRAYED|MF_BYPOSITION);
		this->DrawMenuBar();
	#endif
	REG_CRYPT_END;

	// ������������ ������������ ������ ����
	int nMenuHeight = GetSystemMetrics(SM_CYMENU);
	int nCaptionHeight = GetSystemMetrics(SM_CYCAPTION);
	RECT rW;
	CPoint p(0, 0);
	this->GetWindowRect(&rW);
	this->ClientToScreen(&p);

	int nDifference = p.y - rW.top - nMenuHeight - nCaptionHeight - 3;
	rW.bottom += nDifference;

	this->SetWindowPos(&wndTop, rW.left, rW.top, rW.right-rW.left, rW.bottom-rW.top,
		SWP_NOZORDER | SWP_NOMOVE);

	// �������������� ���� ���������
	m_IcoWnd.Create(NULL, NULL, WS_CHILD|WS_VISIBLE, CRect(1, 0, 24, 25), this, IDW_ICO);
	m_TimeWnd.Create(NULL, NULL, WS_CHILD|WS_VISIBLE, CRect(24, 0, 106, 25), this, IDW_TIME);
	m_GraphWnd.Create(NULL, NULL, WS_CHILD|WS_VISIBLE, CRect(1, 24, 106, 77), this, IDW_GRAPH);
	m_StatWnd.Create(NULL, NULL, WS_CHILD|WS_VISIBLE, CRect(106, 0, 282, 50), this, IDW_STAT);
	
	m_IcoWnd.ShowWindow(SW_SHOW);   m_IcoWnd.UpdateWindow();
	m_TimeWnd.ShowWindow(SW_SHOW);  m_TimeWnd.UpdateWindow();
	m_GraphWnd.ShowWindow(SW_SHOW); m_GraphWnd.UpdateWindow();
	m_StatWnd.ShowWindow(SW_SHOW);  m_StatWnd.UpdateWindow();

	// ������������� ��� �������
	m_GraphWnd.SetDisplayMode(m_conf.GetConfProg()->nGraphType);
	m_GraphWnd.SetMaxpeaks(m_conf.GetConfProg()->bGraphMaxpeaks != 0);

	// �������������� �������� ������� � �������
	CRect rT(106, 55, 286, 78);
	CRgn rgnT;
	rgnT.CreateRectRgn(0+6, 0, rT.right-rT.left-6, 26/*rT.bottom-rT.top*/);
	m_SliderTime.Create(WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS|TBS_BOTH,
		rT, this, IDS_SLIDERTIME);
	m_SliderTime.SetWindowRgn(rgnT, false);
	m_SliderTime.SetRange(0, 1000);
	//m_SliderTime.ShowThumb(false);	// ������� �������� ��������

	m_SliderFrame.Create(_T("Static"), "", WS_CHILD|WS_VISIBLE|SS_ETCHEDFRAME,
		CRect(109, 53, 283, 82), this, IDC_STATIC);

	CRect rV(185, 84, 185+70, 84+23);
	CRgn rgnV;
	rgnV.CreateRectRgn(0+6, 0, rV.right-rV.left-6, 26);
	m_SliderVol.Create(WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS|TBS_BOTH,
		rV, this, IDS_SLIDERVOL);
	m_SliderVol.SetWindowRgn(rgnV, false);
	m_SliderVol.SetRange(0, 100);
	m_SliderVol.SetPageSize(5);
	m_SliderVol.SetLineSize(1);
	
	// �������������� ������ ����������
	int	iX = 6, iY = 85, idx = 45, idy = 23;
	m_BtnOPEN.Create(CRect(iX, iY, iX+idx, iY+idy), this, IDB_BTNOPEN); iX += idx + 1;
	m_BtnREC.Create(CRect(iX, iY, iX+idx, iY+idy), this, IDB_BTNREC);  iX += idx + 1;
	m_BtnSTOP.Create(CRect(iX, iY, iX+idx, iY+idy), this, IDB_BTNSTOP); iX += idx + 1;
	m_BtnPLAY.Create(CRect(iX, iY, iX+idx, iY+idy), this, IDB_BTNPLAY); iX = 226; idx = 25;

	//iX = 243; idx = 25;
	//m_BtnMIX_INV.Create(CRect(iX, iY, iX+idx, iY+idy), this, IDB_MIX_INV);
	//iX += idx+0; idx = 10;
	iX = 251; idx = 27;
	m_BtnMIX_SEL.Create(CRect(iX, iY, iX+idx, iY+idy), this, IDB_MIX_SEL);

	m_BtnOPEN.SetIcon(IDI_OPEN);
	m_BtnPLAY.SetIcon(IDI_PLAY);
	m_BtnSTOP.SetIcon(IDI_STOP);
	m_BtnREC.SetIcon(IDI_REC);

	m_BtnMIX_SEL.SetIcon(IDI_MIC);

	m_BtnFrame.Create(_T("Static"), "", WS_CHILD|WS_VISIBLE|SS_ETCHEDFRAME,
		CRect(1, 80, 283, 113), this, IDC_STATIC);

	// ������� ���������
	EnableToolTips(true);

	// ��������� ����� "��������� �� �������"
	OnOptTop(); OnOptTop();	// ��������� �� ����������

	// ����������� ������
	m_RecMixer.Open(WAVE_MAPPER, GetSafeHwnd());
	m_PlayMixer.Open(WAVE_MAPPER, GetSafeHwnd());
	if(m_RecMixer.GetLinesNum() != 0)
		m_SliderVol.SetPos(m_RecMixer.GetVol(m_RecMixer.GetCurLine()));
	else
		m_SliderVol.EnableWindow(false);

	// Force updating button image
	CMP3_RecorderApp* l_app = (CMP3_RecorderApp* )AfxGetApp();
	if (l_app->IsVistaOS())
	{
		PostMessage(MM_MIXM_LINE_CHANGE, 0, 0);
	}
	// ���������� ��������� �������� ���������
	//m_nSysVolume = 0;
	//m_nSysVolume = m_PlayMixer.GetVol (m_PlayMixer.GetCurLine());
	//m_PlayMixer.SetVol (m_conf.GetConfProg()->nPlayVolume);

	// ������������� ��������� ��� ����
	m_title = new CTitleText(this->GetSafeHwnd());

	// �������� ���������� ���������
	m_strDir = GetProgramDir();

	// ������������ ����� ����������
	CString strName = "";
	switch (m_conf.GetConfDialGen()->nLoader)
	{
		case 1:		// �������� ������ ����� - �������
			//strName = GetAutoName(CString("")) + ".mp3";
			strName = GetAutoName(CString(""));
			break;
		case 2:		// �������� ���������� �����
			strName = m_conf.GetConfProg()->strLastFileName;
			break;
	}
	OnFileClose();
	if (!strName.IsEmpty())
	{
		CString strPath = m_conf.GetConfProg()->strLastFilePath;
		if (strPath.IsEmpty())
		{
			strPath = GetProgramDir();
		}
		OpenFile(strPath + "\\" + strName);
	}

	// ������� ������ � ����
	m_TrayIcon.Create(this, WM_ICON_NOTIFY, "StepVoice Recorder",
		LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_TRAY_STOP)), IDR_TRAY_MENU);
	if(m_conf.GetConfDialGen()->bTrayIcon)
		m_TrayIcon.ShowIcon();
	else
		m_TrayIcon.HideIcon();
	UpdateTrayText();

	// Setting up Monitoring
	if (m_conf.GetConfProg()->bMonitoring)
	{
		m_bMonitoringBtn = false;
		OnBtnMonitoring();
	}

	// Setting up Scheduler
	m_sched2.SetCallbackFunc(Scheduler2Function);
	if (m_conf.GetConfDialSH2()->bIsEnabled)
	{
		m_conf.GetConfDialSH2()->bIsEnabled = false;
		OnBtnSched();
	}

	// Setting up Voice Activation System
	if (m_conf.GetConfDialVAS()->bEnable)
	{
		OnBtnVas();
	}

	return 0;
}
#pragma optimize("", on)

/////////////////////////////////////////////////////////////////////////////
BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;

	cs.cx = 290;//285;//318;//255;
	cs.cy = 120 + GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYMENU);
	
	// ������������� ��������� ����
	int nScreenCX = GetSystemMetrics(SM_CXSCREEN);
	int nScreenCY = GetSystemMetrics(SM_CYSCREEN);
	cs.x  = m_conf.GetConfProg()->nXcoord;
	cs.y  = m_conf.GetConfProg()->nYcoord;
	if(cs.x >= (nScreenCX-40) || cs.y >= (nScreenCY-40))
	{
		cs.x  = (nScreenCX - cs.cx)/2;
		cs.y  = (nScreenCY - cs.cy)/2;
	}

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;

	//cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS,
	//	::LoadCursor(NULL, IDC_ARROW), (HBRUSH)16/*(HBRUSH)COLOR_BACKGROUND+1*/,
	//	AfxGetApp()->LoadIcon(IDR_MAINFRAME));

	cs.lpszClass	= "SvRec";
	WNDCLASS wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.hInstance	= AfxGetInstanceHandle();
	wc.lpfnWndProc	= ::DefWindowProc;
	wc.style		= CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS;
	wc.hCursor		= ::LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= HBRUSH(16);
	wc.hIcon		= ::LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME));
	wc.lpszClassName= "SvRec";
	AfxRegisterClass(&wc);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
BOOL CMainFrame::ShowWindow()
{	
	// process show window, called by MP3_RecorderApp
	int nCmdShow = (m_conf.GetConfDialGen()->bMinimized) ? SW_MINIMIZE : SW_SHOW;

	if(nCmdShow == SW_MINIMIZE && m_conf.GetConfDialGen()->bTrayMin)
	{
		nCmdShow = SW_HIDE;
		m_TrayIcon.ShowIcon();
	}

	return CFrameWnd::ShowWindow(nCmdShow);
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers
/////////////////////////////////////////////////////////////////////////////
LRESULT CMainFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	switch(message)
	{
	// ������������ �����. ��������� �������� ������ ��� snapping
	case WM_DISPLAYCHANGE:
		//::GetWindowRect(::GetDesktopWindow(),&m_rDesktopRect);
		::SystemParametersInfo(SPI_GETWORKAREA, 0, &m_rDesktopRect, 0);
		break;
	case MM_MIXM_CONTROL_CHANGE:
	case MM_MIXM_LINE_CHANGE:
		{
			//if(m_nActiveMixerID == 1)
			//	OnPlayMixMenuSelect(ID_MIXITEM_PLAY0 + m_PlayMixer.GetCurLine());
			//else
			//	OnRecMixMenuSelect(ID_MIXITEM_REC0 + m_RecMixer.GetCurLine());

			if(m_nActiveMixerID == 1)
			{
				m_SliderVol.SetPos(m_PlayMixer.GetVol(m_PlayMixer.GetCurLine()));
			}
			else
			{
				m_SliderVol.SetPos(m_RecMixer.GetVol(m_RecMixer.GetCurLine()));
				// ������ ������ � ����������� �� ���� �����
				const int ICON_ID[] = { IDI_MIXLINE00, IDI_MIXLINE01, IDI_MIXLINE02,
										IDI_MIXLINE03, IDI_MIXLINE04, IDI_MIXLINE05,
										IDI_MIXLINE06, IDI_MIXLINE07, IDI_MIXLINE08,
										IDI_MIXLINE09, IDI_MIXLINE10 };
				int i = m_RecMixer.GetLineType(m_RecMixer.GetCurLine());
				m_BtnMIX_SEL.SetIcon(ICON_ID[i]);
			}
		}
		break;
	}

	return CFrameWnd::WindowProc(message, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if (m_conf.GetConfProg()->bEasyMove)
	{
		CPoint scrPoint;
		GetCursorPos(&scrPoint);
		PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(scrPoint.x, scrPoint.y));
	}
	CFrameWnd::OnLButtonDown(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnMoving(UINT fwSide, LPRECT pRect) 
{
	CFrameWnd::OnMoving(fwSide, pRect);
	
	// calculate window size
	SIZE szSize = {pRect->right - pRect->left, pRect->bottom - pRect->top};
	POINT pt;
	GetCursorPos(&pt);
	pRect->left  = pt.x - m_szMoveOffset.cx;
	pRect->top   = pt.y - m_szMoveOffset.cy;
	pRect->right = pRect->left + szSize.cx;
	pRect->bottom= pRect->top  + szSize.cy;
	// snap window to screen edges
	if(pRect->top < m_nSnapPixels && pRect->top > -m_nSnapPixels)
	{// begin snap to top
		pRect->top = 0;
		pRect->bottom = szSize.cy;
	}// end snap to top
	if(pRect->left < m_nSnapPixels && pRect->left > -m_nSnapPixels)
	{// begin snap to left
		pRect->left = 0;
		pRect->right = szSize.cx;
	}// end snap to left
	if(pRect->right < m_rDesktopRect.right+m_nSnapPixels && pRect->right > m_rDesktopRect.right-m_nSnapPixels)
	{// begin snap to right
		pRect->right = m_rDesktopRect.right;
		pRect->left = m_rDesktopRect.right-szSize.cx;
	}// end snap to right
	if(pRect->bottom < m_rDesktopRect.bottom+m_nSnapPixels && pRect->bottom > m_rDesktopRect.bottom-m_nSnapPixels)
	{// begin snap to bottom
		pRect->bottom = m_rDesktopRect.bottom;
		pRect->top = m_rDesktopRect.bottom-szSize.cy;
	}// end snap to bottom
}// end OnMoving

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnNcLButtonDown(UINT nHitTest, CPoint point) 
{
	if (nHitTest == HTCAPTION)
	{//begin save mouse coord
		// calculate drag offset
		RECT rRect;
		GetWindowRect(&rRect);
		m_szMoveOffset.cx = point.x - rRect.left;
		m_szMoveOffset.cy = point.y - rRect.top;
	}// end save mouse coord
	CFrameWnd::OnNcLButtonDown(nHitTest, point);
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnDestroy() 
{
	CFrameWnd::OnDestroy();

	OnFileClose();

	if (m_bMonitoringBtn)
	{
		OnBtnMonitoring();
		m_conf.GetConfProg()->bMonitoring = true;
	}
	if (!IsIconic())
	{
		CRect r; GetWindowRect(&r);
		m_conf.GetConfProg()->nXcoord = r.left;
		m_conf.GetConfProg()->nYcoord = r.top;
	}

	m_conf.GetConfProg()->nGraphType = m_GraphWnd.GetDisplayMode();
	m_conf.GetConfProg()->bGraphMaxpeaks = (int)m_GraphWnd.GetMaxpeaks();

	// ����. ��������� �����. � ��������������� ��������� ���������
	//m_conf.GetConfProg()->nPlayVolume = m_PlayMixer.GetVol (
	//	m_PlayMixer.GetCurLine ());
	//m_PlayMixer.SetVol (m_nSysVolume);	
}


//===========================================================================
// ���� : File
//===========================================================================
void CMainFrame::OnFileClose() 
{
	OnBtnSTOP();
	m_GraphWnd.StopUpdate();

	BASS_Free();
	BASS_RecordFree();
	g_stream_handle = 0;
	g_record_handle = 0;

	if (m_bMonitoringBtn)
	{
		MonitoringStart();
	}
	if (m_record_file.m_hFile != CFile::hFileNull)
	{
		m_record_file.Flush();
		m_record_file.Close();
	}
	SAFE_DELETE(m_pEncoder);
	SAFE_DELETE(m_pSndFile);

	// ������������� ����� �� ���������: <no file> - StepVoice Recorder
	CString strTitle, strNoFile((LPCSTR)IDS_NOFILE);
	AfxFormatString1(strTitle, IDS_FILETITLE, strNoFile);
	m_title->SetTitleText(strTitle);

	m_SliderTime.SetCurPos(0);
	//m_SliderTime.ShowThumb(false);	// ������� �������� ��������
	m_TimeWnd.Reset();
	UpdateStatWindow();
	UpdateTrayText();

	///@bug Copied from OnBtnSTOP - refactoring needed!
	UpdateButtonState(IDB_BTNPLAY);
	UpdateButtonState(IDB_BTNREC);
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnFileClear() 
{
	if (!g_record_handle && !g_stream_handle &&
		(m_record_file.m_hFile == CFile::hFileNull))
	{
		return;
	}
	CString& l_last_path = m_conf.GetConfProg()->strLastFilePath;
	CString& l_last_name = m_conf.GetConfProg()->strLastFileName;

	CString l_str_warning  = "";
	CString l_str_filename = l_last_path + '\\' + l_last_name;

	AfxFormatString1(l_str_warning, IDS_CLEAR_ASK, l_str_filename);
	if (AfxMessageBox(l_str_warning, MB_YESNO|MB_ICONWARNING) == IDYES)
	{
		OnFileClose();
		m_record_file.Open(l_str_filename, CFile::modeCreate, NULL);
		m_record_file.Close();
		OpenFile(l_str_filename);
	}
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnFileDelete() 
{
	if (!g_record_handle && !g_stream_handle &&
		(m_record_file.m_hFile == CFile::hFileNull))
	{
		return;
	}
	CString& l_last_path = m_conf.GetConfProg()->strLastFilePath;
	CString& l_last_name = m_conf.GetConfProg()->strLastFileName;

	CString l_str_warning  = "";
	CString l_str_filename = l_last_path + '\\' + l_last_name;

	AfxFormatString1(l_str_warning, IDS_DEL_ASK, l_str_filename);
	if (AfxMessageBox(l_str_warning, MB_YESNO|MB_ICONWARNING) == IDYES)
	{
		OnFileClose();
		if (!DeleteFile(l_str_filename))
		{
			AfxFormatString1(l_str_warning, IDS_DEL_UNABLE, l_str_filename);
			AfxMessageBox(l_str_warning, MB_OK|MB_ICONWARNING);
		}
	}
}

//=======�� �� ����, �� �������� :)===========================================
void CMainFrame::OpenFile(CString& str)
{
	OnFileClose();

	// Storing last opened file name and path.
	CString& l_last_path = m_conf.GetConfProg()->strLastFilePath;
	CString& l_last_name = m_conf.GetConfProg()->strLastFileName;

	int l_slash_pos = str.ReverseFind('\\');
	if (l_slash_pos == -1)
	{
		l_last_name = str;
	}
	else
	{	
		l_last_path = str.Left(l_slash_pos);
		l_last_name = str.Right(str.GetLength() - l_slash_pos - 1);
	}

	if (IsSuitableForRecording(str))
	{
		if (!m_record_file.Open(str, CFile::modeCreate|CFile::modeWrite|
			CFile::typeBinary|CFile::shareDenyWrite, NULL))
		{
			return;
		}
	}
	else
	{
		if (!BASS_Init(-1, 44100, 0, GetSafeHwnd(), NULL))
		{
			return;
		}
		g_stream_handle = BASS_StreamCreateFile(false, str, 0, 0, 0);
		if (!g_stream_handle)
		{
			BASS_Free();
			return;
		}
	}

	// Modifying window caption to "<FILE> - StepVoice Recorder".
	CString l_filename = str;
	if (l_filename.ReverseFind('\\') != -1)
	{
		l_filename = l_filename.Right(l_filename.GetLength() -
			l_filename.ReverseFind('\\') - 1);
	}
	CString l_new_caption;
	AfxFormatString1(l_new_caption, IDS_FILETITLE, l_filename);
	m_title->SetTitleText(l_new_caption);
	
	UpdateStatWindow();
	UpdateTrayText();

	/*
	if(m_pSndFile)
		OnFileClose();

	// ���������� ��������� � �������� �����
	CString strLoad((LPCSTR)IDS_LOADING);
	m_title->SetTitleText(strLoad);
	this->BeginWaitCursor();

	SOUND_INFO si;
	si.nBitrate	= m_conf.GetConfDialMp3()->nBitrate;
	si.nFreq	= m_conf.GetConfDialMp3()->nFreq;
	si.nStereo	= m_conf.GetConfDialMp3()->nStereo;
	si.nBits	= 16;

	m_pSndFile = new CSndFile_MP3;
	bool bRes = m_pSndFile->Open(str, &si);

	this->EndWaitCursor();
	if(bRes == false)
	{
		SAFE_DELETE(m_pSndFile);
		OnFileClose();
		// ������ ������
		CString strRes;
		AfxFormatString1(strRes, IDS_OPEN_ERR, str);
		AfxMessageBox(strRes, MB_OK|MB_ICONWARNING);
		return;
	}

	// ���������� ��� � ������ ���� �����
	CString& strPath = m_conf.GetConfProg()->strLastFilePath;
	CString& strName = m_conf.GetConfProg()->strLastFileName;
	int nSlash = str.ReverseFind('\\');
	if(nSlash == -1)
		strName = str;
	else
	{	// !!!!!!
		strPath = str.Left(nSlash);	// ��������� ����, ���� � ��. �����
		strName = str.Right(str.GetLength() - nSlash - 1);
	}

	// ��������� ���� ����������
	m_SliderTime.SetCurPos(0);
	//if(m_pSndFile->GetFileSize(IN_SECONDS) > 0)
	//	m_SliderTime.ShowThumb(true);
	m_TimeWnd.Reset();
	UpdateStatWindow();

	// ������������� ���������: "<FILE> - StepVoice Recorder"
	CString strTitle;
	AfxFormatString1(strTitle, IDS_FILETITLE, strName);
	m_title->SetTitleText(strTitle);

	UpdateTrayText();
	*/
}

//===========================================================================
void CMainFrame::OnFileFindfile() 
{
	CString filePath = m_conf.GetConfProg()->strLastFilePath + "\\" +
		m_conf.GetConfProg()->strLastFileName;
	
	ShellExecute(NULL, "open", "explorer", "/select, """+filePath+"""",
		NULL, SW_NORMAL);
	//this->SetForegroundWindow();
}

//===========================================================================
// ���� : Sound (����������� �� �����)
//===========================================================================
void CMainFrame::OnSoundBegin()
{
	double l_all_seconds = 0;

	if (g_stream_handle)
	{
		l_all_seconds = BASS_ChannelBytes2Seconds(g_stream_handle,
			BASS_ChannelGetLength(g_stream_handle, BASS_POS_BYTE));

		// Moving to the beginning on file
		BOOL l_result = BASS_ChannelSetPosition(g_stream_handle,
			BASS_ChannelSeconds2Bytes(g_stream_handle, 0), BASS_POS_BYTE);
		ASSERT(l_result);

		m_TimeWnd.SetTime(0);
		m_SliderTime.SetCurPos(0);
	}

	char l_str_alltime[20] = {0};
	Convert((UINT)l_all_seconds, l_str_alltime, sizeof(l_str_alltime));

	CString l_str_caption;
	l_str_caption.Format(IDS_SEEKTO, _T("0:00:00"), l_str_alltime, 0);
	m_title->SetTitleText(l_str_caption, 1200);
}

//===========================================================================
void CMainFrame::OnSoundRew() 
{
	double l_allsec = 0;
	double l_cursec = 0;

	if (g_stream_handle)
	{
		l_cursec = BASS_ChannelBytes2Seconds(g_stream_handle,
			BASS_ChannelGetPosition(g_stream_handle, BASS_POS_BYTE));
		l_allsec = BASS_ChannelBytes2Seconds(g_stream_handle,
			BASS_ChannelGetLength(g_stream_handle, BASS_POS_BYTE));

		l_cursec = ((l_cursec - 5) > 0) ? l_cursec - 5 : 0;

		BOOL l_result = BASS_ChannelSetPosition(
			g_stream_handle,
			BASS_ChannelSeconds2Bytes(g_stream_handle, l_cursec),
			BASS_POS_BYTE);
		ASSERT(l_result);

		m_TimeWnd.SetTime((UINT)l_cursec);
		m_SliderTime.SetCurPos(int(l_cursec/l_allsec*1000));
	}

	char szCurSec[20] = {0};
	char szAllSec[20] = {0};
	Convert((UINT)l_cursec, szCurSec, sizeof(szCurSec));
	Convert((UINT)l_allsec, szAllSec, sizeof(szAllSec));

	CString strRes;
	strRes.Format(IDS_SEEKTO, szCurSec, szAllSec, int(l_cursec/l_allsec*100));
	m_title->SetTitleText(strRes, 1200);
}

//===========================================================================
void CMainFrame::OnSoundFf() 
{
	double l_allsec = 0;
	double l_cursec = 0;

	if (g_stream_handle)
	{
		l_cursec = BASS_ChannelBytes2Seconds(g_stream_handle,
			BASS_ChannelGetPosition(g_stream_handle, BASS_POS_BYTE));
		l_allsec = BASS_ChannelBytes2Seconds(g_stream_handle,
			BASS_ChannelGetLength(g_stream_handle, BASS_POS_BYTE));

		l_cursec = ((l_cursec + 5) < l_allsec) ? l_cursec + 5 : l_allsec;

		BOOL l_result = BASS_ChannelSetPosition(
			g_stream_handle,
			BASS_ChannelSeconds2Bytes(g_stream_handle, l_cursec),
			BASS_POS_BYTE);
		ASSERT(l_result);

		m_TimeWnd.SetTime((UINT)l_cursec);
		m_SliderTime.SetCurPos(int(l_cursec/l_allsec*1000));
	}

	char szCurSec[20] = {0};
	char szAllSec[20] = {0};
	Convert((UINT)l_cursec, szCurSec, sizeof(szCurSec));
	Convert((UINT)l_allsec, szAllSec, sizeof(szAllSec));

	CString strRes;
	strRes.Format(IDS_SEEKTO, szCurSec, szAllSec, int(l_cursec/l_allsec*100));
	m_title->SetTitleText(strRes, 1200);
}

//===========================================================================
void CMainFrame::OnSoundEnd() 
{
	double l_allsec = 0;

	if (g_stream_handle)
	{
		l_allsec = BASS_ChannelBytes2Seconds(g_stream_handle,
			BASS_ChannelGetLength(g_stream_handle, BASS_POS_BYTE));

		BOOL l_result = BASS_ChannelSetPosition(
			g_stream_handle,
			BASS_ChannelSeconds2Bytes(g_stream_handle, l_allsec - 0.3),
			BASS_POS_BYTE);
		ASSERT(l_result);

		m_TimeWnd.SetTime((UINT)l_allsec);
		m_SliderTime.SetCurPos(1000);
	}

	char l_str_alltime[20] = {0};
	Convert((UINT)l_allsec, l_str_alltime, sizeof(l_str_alltime));

	CString l_str_caption;
	l_str_caption.Format(IDS_SEEKTO, l_str_alltime, l_str_alltime, 100);
	m_title->SetTitleText(l_str_caption, 1200);
}

//===========================================================================
// ���� : Options
//===========================================================================
void CMainFrame::OnStatPref() 
{
	//m_conf.GetConfProg()->nDialogIndex = -1; // ������ �� ����� �������
	OnOptCom();
}

void CMainFrame::OnOptCom()
{
	// ��������� ����� ���� ������� "General" ��� "Record" :)
	int nDialogIndex = m_conf.GetConfProg()->nDialogIndex;
	//nDialogIndex = (nDialogIndex == -1) ? 1 : 0;

	//CMySheet optDlg("Preferences", this, m_conf.GetConfProg()->nDialogIndex);
	CMySheet optDlg("Preferences", this, nDialogIndex);
	// ����������� ��� ������� � ��������� ��� ���������
	optDlg.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	//optDlg.m_psh.dwFlags |= PSH_HASHELP;

	optDlg.SetConfig( m_conf.GetConfDialGen() );
	optDlg.SetConfig( m_conf.GetConfDialMp3() );
	optDlg.SetConfig( m_conf.GetConfDialSH2()  );
	optDlg.SetConfig( m_conf.GetConfDialVAS() );
	optDlg.SetConfig( m_conf.GetConfDialAN()  );
	// !!!
	m_pOptDialog = &optDlg;

	// �������� ��������� ��������
	bool bOldSchedState = m_conf.GetConfDialSH2()->bIsEnabled != 0;
	bool bNewSchedState = bOldSchedState;

	if(optDlg.DoModal() == IDOK)
	{
		m_conf.saveConfig();		
		//m_conf.RegisterWrite(); // ��������� ��������� ��������

		if(m_pSndFile)
		{
			SOUND_INFO si;
			si.nBitrate	= m_conf.GetConfDialMp3()->nBitrate;
			si.nFreq	= m_conf.GetConfDialMp3()->nFreq;
			si.nStereo	= m_conf.GetConfDialMp3()->nStereo;
			si.nBits	= 16;
			// �������� �������� ��������� ����� (��� ������� ���������)
			m_pSndFile->ChangeSoundInfo(si);
		}
		UpdateStatWindow();

		// ��������� ����� ����
		if(m_conf.GetConfDialGen()->bTrayIcon) //Taskbar only
			m_TrayIcon.ShowIcon();
		else
			m_TrayIcon.HideIcon();

		// !!!!! �������� ��������� �������� (�������� SHR)
		bNewSchedState = m_conf.GetConfDialSH2()->bIsEnabled != 0;
		if(bNewSchedState != bOldSchedState)
		{	// ����������� ��������� �������� (�������� ������� �� ������)
			m_conf.GetConfDialSH2()->bIsEnabled = !bNewSchedState;
			OnBtnSched();
		}
		else if(bNewSchedState) {	// ������������� �������
			OnBtnSched(); // ����.
			OnBtnSched(); // ���.
		}

		// �������� VAS
		CONF_DIAL_VAS* pConfig = m_conf.GetConfDialVAS();
		if((pConfig->bEnable != 0) != m_vas.IsRunning())
			OnBtnVas();
		if(m_vas.IsRunning())
		{
			m_vas.InitVAS(pConfig->nThreshold, pConfig->nWaitTime);
			m_GraphWnd.ShowVASMark(pConfig->nThreshold);
		}
	}

	m_conf.GetConfProg()->nDialogIndex = optDlg.m_nPageIndex;

	// !!!
	m_pOptDialog = NULL;
}

//===========================================================================
void CMainFrame::OnOptEm() 
{	// ��������� "������� �����������"
	m_conf.GetConfProg()->bEasyMove = m_conf.GetConfProg()->bEasyMove ? 0 : 1;
}

//===========================================================================
void CMainFrame::OnOptTop() 
{
	// Link to the window top option
	int& l_top = m_conf.GetConfProg()->bAlwaysOnTop;
	l_top = l_top ? 0 : 1;

	const CWnd* pWndType = (l_top) ? &wndTopMost : &wndNoTopMost;
	SetWindowPos(pWndType, 0, 0, 0, 0, SWP_FRAMECHANGED|SWP_NOSIZE|SWP_NOMOVE);
}

//===========================================================================
void CMainFrame::OnMixPlay() 
{
	CMP3_RecorderApp* l_app = (CMP3_RecorderApp* )AfxGetApp();
	if (l_app->IsVistaOS())
	{
		WinExec("sndvol", SW_SHOW);
	}
	else
	{
		CloseMixerWindows();
		WinExec("sndvol32", SW_SHOW);
	}
}

//===========================================================================
void CMainFrame::OnMixRec() 
{
	CMP3_RecorderApp* l_app = (CMP3_RecorderApp* )AfxGetApp();
	if (l_app->IsVistaOS())
	{
		WinExec("control MMSYS.CPL,,1", SW_SHOW);
	}
	else
	{
		CloseMixerWindows();
		WinExec("sndvol32 -r", SW_SHOW);
	}
}


//===========================================================================
// ������
//===========================================================================
void CMainFrame::OnBtnOPEN()
{
	this->SetFocus(); // ������� ����� � ������

	CString strTemp;
	strTemp.LoadString(IDS_FILEFILTER);			// ������ - MPEG audio files

	CString strName = GetAutoName(CString(""));	// ��� ���� 08jun_05.mp3
	CString strLastPath = m_conf.GetConfProg()->strLastFilePath;

	if (!strLastPath.IsEmpty())
	{	
		strLastPath = strLastPath + "\\";
	}

	// Creating a standard file open dialog
	CFileDialog NewOpenFileDialog(true, "mp3",
		strName.GetBuffer(strName.GetLength()),
		OFN_HIDEREADONLY | OFN_EXPLORER, strTemp);

	strTemp.LoadString(IDS_FILENEWOPENTITLE);
	NewOpenFileDialog.m_ofn.lpstrTitle= strTemp.GetBuffer(strTemp.GetLength());
	NewOpenFileDialog.m_ofn.lpstrInitialDir = strLastPath;	

	if (NewOpenFileDialog.DoModal() == IDOK)
	{	
		OpenFile(NewOpenFileDialog.GetPathName());
		return;
	}
	/*
	OnFileClose();

	if (IsSuitableForRecording(NewOpenFileDialog.GetPathName()))
	{
		if (!BASS_RecordInit(-1))
		{
			return;
		}
		if (!m_record_file.Open(NewOpenFileDialog.GetPathName(),
			CFile::modeCreate|CFile::modeWrite|CFile::typeBinary, NULL))
		{
			BASS_RecordFree();
			return;
		}
	}
	else
	{
		if (!BASS_Init(-1, 44100, 0, GetSafeHwnd(), NULL))
		{
			return;
		}
		g_stream_handle = BASS_StreamCreateFile(false,
			NewOpenFileDialog.GetPathName(), 0, 0, 0);
		if (!g_stream_handle)
		{
			BASS_Free();
			return;
		}
	}


	UpdateStatWindow();

	// Modifying window caption to "<FILE> - StepVoice Recorder".
	CString l_filename = NewOpenFileDialog.GetPathName();
	if (l_filename.ReverseFind('\\') != -1)
	{
		l_filename = l_filename.Right(l_filename.GetLength() -
			l_filename.ReverseFind('\\') - 1);
	}
	CString l_new_caption;
	AfxFormatString1(l_new_caption, IDS_FILETITLE, l_filename);
	m_title->SetTitleText(l_new_caption);
	*/

	/*if (NewOpenFileDialog.DoModal() == IDOK)
	{
		OpenFile(NewOpenFileDialog.GetPathName());
	}*/

	/*OPENFILENAME ofn;       // common dialog box structure
	char szFile[260];       // buffer for file name
	CString strFilter, strTitle, strName, strLastPath;

	strFilter.LoadString(IDS_FILEFILTER);	// ������ - MPEG audio files
	strTitle.LoadString(IDS_FILENEWOPENTITLE);
	strName = GetAutoName(CString(""));		// ��� ���� 08jun_05.mp3
	strLastPath = m_conf.GetConfProg()->strLastFilePath;

	strcpy(szFile, strName.GetBuffer(strName.GetLength()));

	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	//ofn.hwndOwner = this->GetSafeHwnd();
	ofn.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrDefExt = "mp3";
	ofn.lpstrFilter = "MPEG audio files (*.MP3)\0*.mp3\0All files (*.*)\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrTitle = strTitle;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.Flags = OFN_HIDEREADONLY | OFN_EXPLORER;
	ofn.lpstrInitialDir = NULL;
	if(!strLastPath.IsEmpty()) {	
		strLastPath = strLastPath + "\\";
		ofn.lpstrInitialDir = strLastPath;	
	}

	// Display the Open dialog box. 
	if (GetOpenFileName(&ofn) == TRUE) {
		strName = ofn.lpstrFile;
		OpenFile(strName);
	}*/
}

//===========================================================================
void CMainFrame::OnBtnPLAY()
{
	SetFocus();

	if (m_record_file.m_hFile != CFile::hFileNull)
	{
		return;
	}
	if (!g_stream_handle)
	{
		OnBtnOPEN();
		if (!g_stream_handle)
		{
			return;
		}
	}

	if (m_bMonitoringBtn)
	{
		MonitoringStop();
	}
	const DWORD CHANNEL_STATE = BASS_ChannelIsActive(g_stream_handle);
	switch (CHANNEL_STATE)
	{
	case BASS_ACTIVE_PLAYING:
		m_GraphWnd.StopUpdate();
		BASS_ChannelPause(g_stream_handle);
		KillTimer(1);
		m_BtnPLAY.SetIcon(IDI_PLAY);
		m_TrayIcon.SetIcon(IDI_TRAY_PAUSE);
		break;

	case BASS_ACTIVE_PAUSED:
	case BASS_ACTIVE_STOPPED:
		BASS_ChannelPlay(g_stream_handle, false);
		SetTimer(1, 1000, NULL);
		m_BtnPLAY.SetIcon(IDI_PAUSE);
		m_TrayIcon.SetIcon(IDI_TRAY_PLAY);
		m_GraphWnd.StartUpdate(g_stream_handle);
	}
	UpdateIcoWindow();
	UpdateButtonState(IDB_BTNREC);
	OnPlayMixMenuSelect(ID_MIXITEM_PLAY0 + m_PlayMixer.GetCurLine());
	return;
	/*
	/// @note Not going here!!
	SOUND_INFO si;
	int nRes;

	SetFocus();
	switch(m_nState)
	{
	case STOP_STATE:
		// ���������, ������ �� ����
		if(!m_pSndFile)
		{	OnBtnOPEN();
			if(!m_pSndFile)
				return;
		}
		m_pSndFile->GetSoundInfo(si);

		// �������������� �������
		m_pDecoder = new CDecoder_MP3(m_strDir);
		if(!m_pDecoder->InitDecoder(NULL))
		{
			SAFE_DELETE(m_pDecoder);
			// ������ ��������� �� ������
			CString strRes;
			AfxFormatString2(strRes, IDS_DEC_INIT, m_strDir, "mpglib.dll");
			AfxMessageBox(strRes, MB_OK|MB_ICONSTOP);
			return;
		}

		// ������������� ���������� ����� ����������������
		if(m_bMonitoringBtn)
		{
			//MonitoringStop();
			OnBtnMonitoring();
		}

		// �������������� �����.
		m_pWaveOut = new CWaveOut(_PlaybkProc);
		nRes = m_pWaveOut->Open(si.nFreq, si.nBits, si.nStereo);
		if(nRes)
		{	// ��������� ������ ��������
			SAFE_DELETE(m_pWaveOut);
			// ������ ��������� �� ������
			if(nRes != ERR_BUFFER)
				AfxMessageBox(IDS_WAVEOUT_OPEN, MB_OK|MB_ICONWARNING);
			return;
		}

		// ��������� �����.
		if(!m_pWaveOut->Start())
		{
			SAFE_DELETE(m_pWaveOut);
			// ������ ��������� �� ������
			AfxMessageBox(IDS_WAVEOUT_START, MB_OK|MB_ICONWARNING);
			return;
		}

		// ������. ���. ��������
		m_nState = PLAY_STATE;
		SetTimer(1, 1000, NULL);
		// ��������� ��� ����
		//m_GraphWnd.SetConfig(&si);
		m_BtnPLAY.SetIcon(IDI_PAUSE);
		//m_BtnREC.ModifyStyle(0, WS_DISABLED);
		//m_BtnREC.Invalidate(false);
		// ��������� ������ � ����
		m_TrayIcon.SetIcon(IDI_TRAY_PLAY);
		break;

	case PLAY_STATE:
		m_pWaveOut->Pause();
		// ������ ���. ��������
		m_nState = PAUSEPLAY_STATE;
		KillTimer(1);
		m_BtnPLAY.SetIcon(IDI_PLAY);
		m_TimeWnd.Blink(true);
		// ��������� ������ � ����
		m_TrayIcon.SetIcon(IDI_TRAY_PAUSE);
		break;

	case PAUSEPLAY_STATE:
		m_pWaveOut->Start();
		// ������ ���. ��������
		m_nState = PLAY_STATE;
		SetTimer(1, 1000, NULL);
		m_BtnPLAY.SetIcon(IDI_PAUSE);
		m_TimeWnd.Blink(false);
		// ��������� ������ � ����
		m_TrayIcon.SetIcon(IDI_TRAY_PLAY);
		break;
	}

	UpdateIcoWindow();
	UpdateButtonState(IDB_BTNREC);

	// ������ ����� �������
	OnPlayMixMenuSelect(ID_MIXITEM_PLAY0 + m_PlayMixer.GetCurLine());
	*/
}

//===========================================================================
void CMainFrame::OnBtnSTOP()
{
	this->SetFocus();

	if (g_stream_handle)
	{
		m_GraphWnd.StopUpdate();

		KillTimer(1);
		BOOL l_result = BASS_ChannelStop(g_stream_handle);
		ASSERT(l_result);

		l_result = BASS_ChannelSetPosition(g_stream_handle,
			BASS_ChannelSeconds2Bytes(g_stream_handle, 0), BASS_POS_BYTE);
		ASSERT(l_result);
	}
	if (g_record_handle)
	{
		m_GraphWnd.StopUpdate();

		KillTimer(2);
		BOOL l_result = BASS_ChannelStop(g_record_handle);
		ASSERT(l_result);

		g_record_handle = 0;
		BASS_RecordFree();
		CString l_recorded_file = m_record_file.GetFilePath();
		m_record_file.Close();
		OpenFile(l_recorded_file);
	}
	PostMessage(WM_HSCROLL,  MAKEWPARAM(SB_THUMBPOSITION, 0),
		(LPARAM)m_pMainFrame->m_SliderTime.m_hWnd);
	m_SliderTime.SetCurPos(0);
	UpdateIcoWindow();
	UpdateStatWindow();
	//m_GraphWnd.StopUpdate();
	m_GraphWnd.Clear();
	m_BtnREC.SetIcon(IDI_REC);
	m_BtnPLAY.SetIcon(IDI_PLAY);
	m_TrayIcon.SetIcon(IDI_TRAY_STOP);

	if (m_bMonitoringBtn)
	{
		MonitoringStart();
	}
	UpdateButtonState(IDB_BTNPLAY);
	UpdateButtonState(IDB_BTNREC);
	OnRecMixMenuSelect(ID_MIXITEM_REC0 + m_RecMixer.GetCurLine());
	return;
	/*
	///@note Not going here!
	SetFocus();
	switch(m_nState)
	{
	case RECORD_STATE:
	case PAUSEREC_STATE:

		SAFE_DELETE(m_pWaveIn);
		SAFE_DELETE(m_pEncoder);
		// �������������� ��������
		KillTimer(2);
		m_BtnREC.SetIcon(IDI_REC);
		//m_BtnPLAY.ModifyStyle(WS_DISABLED, 0);
		//m_BtnPLAY.Invalidate(false);
		break;

	case PLAY_STATE:
	case PAUSEPLAY_STATE:

		SAFE_DELETE(m_pWaveOut);
		SAFE_DELETE(m_pDecoder);
		// �������������� ��������
		KillTimer(1);
		m_BtnPLAY.SetIcon(IDI_PLAY);
		//m_BtnREC.ModifyStyle(WS_DISABLED, 0);
		//m_BtnREC.Invalidate(false);
		break;
	}

	m_nState = STOP_STATE;
	// ��������� ����������
	if(m_bMonitoringBtn)
		MonitoringStart();

	// ������������� ��������� �� ������ �����
	PostMessage(WM_HSCROLL,  MAKEWPARAM(SB_THUMBPOSITION, 0),
		(LPARAM)m_pMainFrame->m_SliderTime.m_hWnd);
	m_SliderTime.SetCurPos(0);
	//if(m_pSndFile)
	//	m_pSndFile->Seek(0, SND_FILE_BEGIN);
	//m_SliderTime.SetCurPos(0);

	//if(m_pSndFile)
	//{
	//	if(m_pSndFile->GetFileSize(IN_SECONDS) > 0)
	//		m_SliderTime.ShowThumb(true);
	//}

	// �������� ���� ������������ � ����������
	UpdateIcoWindow();
	UpdateStatWindow();
	ProcessSliderTime(SB_THUMBPOSITION, 1000);
	m_TimeWnd.Blink(false);
	//m_TimeWnd.Reset();
	m_GraphWnd.Clear();

	UpdateButtonState(IDB_BTNPLAY);
	UpdateButtonState(IDB_BTNREC);

	// ��������� ������ � ����
	m_TrayIcon.SetIcon(IDI_TRAY_STOP);

	// ������ ����� �������
	OnRecMixMenuSelect(ID_MIXITEM_REC0 + m_RecMixer.GetCurLine());
	*/
}


//===========================================================================
void CMainFrame::OnBtnREC()
{
	SetFocus();

	// ������ ��� ������ ��������
	bool bIsSchedEnabled= m_conf.GetConfDialSH2()->bIsEnabled  != 0;
	bool bSchedStart	= m_conf.GetConfDialSH2()->bSchedStart != 0;
	SHR_TIME* ptRec		= &m_conf.GetConfDialSH2()->t_rec;
	SHR_TIME* ptStop	= &m_conf.GetConfDialSH2()->t_stop;

	if (m_record_file.m_hFile == CFile::hFileNull)
	{
		OnBtnOPEN();
		if (m_record_file.m_hFile == CFile::hFileNull)
		{
			return;
		}
	}
	if (m_bMonitoringBtn)
	{
		MonitoringStop();
	}
	if (!g_record_handle)
	{
		if (!BASS_RecordInit(-1))
		{
			return;
		}
		CONF_DIAL_MP3 l_conf_mp3;
		memcpy(&l_conf_mp3, m_conf.GetConfDialMp3(), sizeof(CONF_DIAL_MP3));

		SAFE_DELETE(m_pEncoder);
		m_pEncoder = new CEncoder_MP3(m_strDir);

		if (m_pEncoder->InitEncoder(&l_conf_mp3) != CEncoder::ENC_NOERROR)
		{
			SAFE_DELETE(m_pEncoder);
			CString strRes;
			AfxFormatString2(strRes, IDS_ENC_ERR_DLL, m_strDir, "lame_enc.dll");
			AfxMessageBox(strRes, MB_OK|MB_ICONSTOP);
			return;
		}
		g_record_handle = BASS_RecordStart(
			l_conf_mp3.nFreq,
			l_conf_mp3.nStereo+1,
			BASS_RECORD_PAUSE,
			(RECORDPROC *)&NewRecordProc,
			this);
		if (FALSE == g_record_handle)
		{
			SAFE_DELETE(m_pEncoder);
			g_record_handle = 0;
			return;
		}
	}

	const DWORD CHANNEL_STATE = BASS_ChannelIsActive(g_record_handle);
	switch (CHANNEL_STATE)
	{
	case BASS_ACTIVE_PLAYING:
		m_GraphWnd.StopUpdate();
		m_BtnREC.SetIcon(IDI_REC);
		m_TrayIcon.SetIcon(IDI_TRAY_PAUSE);
		BASS_ChannelPause(g_record_handle);
		KillTimer(2);
		break;

	case BASS_ACTIVE_PAUSED:
	case BASS_ACTIVE_STOPPED:
		BASS_ChannelPlay(g_record_handle, false);
		SetTimer(2, 1000, NULL);
		m_GraphWnd.StartUpdate((HSTREAM)g_record_handle);
		m_BtnREC.SetIcon(IDI_PAUSE);
		m_TrayIcon.SetIcon(IDI_TRAY_REC);

		// ��������� �������
		if (bIsSchedEnabled && (!bSchedStart))
		{
			if (m_conf.GetConfDialSH2()->nStopByID == 1)
			{
				m_sched2.SetRecTime(ptRec, NULL);
			}
			else
			{
				m_sched2.SetStopTime(ptStop, NULL);
			}
			TRACE0("==> OnBtnREC: trying to run the scheduler ...\n");
			m_sched2.Start();
		}
		break;
	}
	UpdateIcoWindow();
	UpdateButtonState(IDB_BTNPLAY);
	return;

	/*
	SOUND_INFO    si;
	CONF_DIAL_MP3 confMp3;
	int nResult;

	CString strTest;

	// ������ ��� ������ ��������
	bool bIsSchedEnabled= m_conf.GetConfDialSH2()->bIsEnabled  != 0;
	bool bSchedStart	= m_conf.GetConfDialSH2()->bSchedStart != 0;
	SHR_TIME* ptRec		= &m_conf.GetConfDialSH2()->t_rec;
	SHR_TIME* ptStop	= &m_conf.GetConfDialSH2()->t_stop;

	SetFocus();
	switch(m_nState)
	{
	case STOP_STATE:
		// ���� ���� �� ������ - �������� ������ ��������
		if(!m_pSndFile)
		{	
			TRACE0("==> OnBtnREC: trying to create/open a file ... \n");
			OnBtnOPEN();
			if(!m_pSndFile)
				return;
			TRACE0("==> OnBtnREC: file created/opened!\n");
		}

		// �������������� �����
		m_pSndFile->GetSoundInfo(si);
		confMp3.nBitrate = si.nBitrate;
		confMp3.nFreq	 = si.nFreq;
		confMp3.nStereo  = si.nStereo;

		ASSERT(!m_pEncoder);
		m_pEncoder = new CEncoder_MP3(m_strDir);
		nResult = m_pEncoder->InitEncoder(&confMp3);
		if(nResult != CEncoder::ENC_NOERROR)
		{
			SAFE_DELETE(m_pEncoder);

			CString strRes;
			if(nResult == CEncoder::ENC_ERR_DLL || nResult == CEncoder::ENC_ERR_UNK)
			{	// ����������� ������
				AfxFormatString2(strRes, IDS_ENC_ERR_DLL, m_strDir, "lame_enc.dll");
				AfxMessageBox(strRes, MB_OK|MB_ICONSTOP);
			}
			else if(nResult == CEncoder::ENC_ERR_INIT)
			{
				strRes.LoadString(IDS_ENC_ERR_INIT);
				AfxMessageBox(strRes, MB_OK | MB_ICONWARNING);
			}
			return;
		}

		// ������������� ���������� ����� �������
		if(m_bMonitoringBtn)
		{
			//MonitoringStop();
			OnBtnMonitoring();
		}

		// �������������� ������
		m_pWaveIn = new CWaveIn(_RecordProc);
		if(m_pWaveIn->Open(si.nFreq, si.nBits, si.nStereo))
		{
			SAFE_DELETE(m_pWaveIn);
			AfxMessageBox(IDS_WAVEIN_OPEN, MB_OK|MB_ICONWARNING);
			return;
		}

		// ������� �������� ����� ��������� ��� ������ ������ � ���. VAS
		//m_GraphWnd.SetConfig(&si);

		// ��������� ������
		if(!m_pWaveIn->Start())
		{
			SAFE_DELETE(m_pWaveIn);
			AfxMessageBox(IDS_WAVEIN_START, MB_OK|MB_ICONWARNING);
			return;
		}

		// �������������� ��������
		m_nState = RECORD_STATE;
		SetTimer(2, 1000, NULL);
		// ��������� ����
		m_BtnREC.SetIcon(IDI_PAUSE);
		//m_BtnPLAY.ModifyStyle(0, WS_DISABLED);
		//m_BtnPLAY.Invalidate(false);
		// ��������� ������ � ����
		m_TrayIcon.SetIcon(IDI_TRAY_REC);
		
		// ��������� �������
		if(bIsSchedEnabled && (!bSchedStart)) {
			if(m_conf.GetConfDialSH2()->nStopByID == 1)
				m_sched2.SetRecTime(ptRec, NULL);
			else
				m_sched2.SetStopTime(ptStop, NULL);
			TRACE0("==> OnBtnREC: trying to run the scheduler ...\n");
			m_sched2.Start();
		}

		// ������������� VAS
		if(m_vas.IsRunning())
			m_vas.ResetVAS();
		break;

	case RECORD_STATE:
		m_pWaveIn->Pause();
		// �������������� ��������
		m_nState = PAUSEREC_STATE;
		KillTimer(2);
		m_BtnREC.SetIcon(IDI_REC);
		m_TimeWnd.Blink(true);
		// ��������� ������ � ����
		m_TrayIcon.SetIcon(IDI_TRAY_PAUSE);
		break;

	case PAUSEREC_STATE:
		m_pWaveIn->Start();
		// �������������� ��������
		m_nState = RECORD_STATE;
		SetTimer(2, 1000, NULL);
		m_BtnREC.SetIcon(IDI_PAUSE);
		m_TimeWnd.Blink(false);
		// ��������� ������ � ����
		m_TrayIcon.SetIcon(IDI_TRAY_REC);
		// ������������� VAS
		if(m_vas.IsRunning())
			m_vas.ResetVAS();
		break;
	}
	m_nStartSec = m_pSndFile->GetFilePos(IN_SECONDS);
	UpdateIcoWindow();
	UpdateButtonState(IDB_BTNPLAY);
	*/
}

//===========================================================================
void CMainFrame::OnBtnMIX_REC()
{
	SetFocus();
	OnMixRec();
}

//===========================================================================
void CMainFrame::OnBtnMIX_PLAY()
{
	SetFocus();
	OnMixPlay();
}

////////////////////////////////////////////////////////////////////////////////
// Seek and volume slider handlers
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	if (pScrollBar->m_hWnd == m_SliderTime.m_hWnd)
	{
		ProcessSliderTime(nSBCode, nPos);
	}
	else if (pScrollBar->m_hWnd == m_SliderVol.m_hWnd)
	{
		ProcessSliderVol(nSBCode, nPos);
	}
	CFrameWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

//------------------------------------------------------------------------------
void CMainFrame::ProcessSliderTime(UINT nSBCode, UINT nPos)
{
	double l_seconds_pos = 0;
	double l_seconds_all = 0;

	if (g_stream_handle)
	{
		l_seconds_all = BASS_ChannelBytes2Seconds(g_stream_handle,
			BASS_ChannelGetLength(g_stream_handle, BASS_POS_BYTE));
		l_seconds_pos = l_seconds_all * nPos / 1000;
	}

	if (SB_THUMBTRACK == nSBCode)
	{
		// Converting seconds to HHMMSS format
		char l_str_curtime[20] = {0};
		char l_str_alltime[20] = {0};
		Convert((UINT)l_seconds_pos, l_str_curtime, sizeof(l_str_curtime));
		Convert((UINT)l_seconds_all, l_str_alltime, sizeof(l_str_alltime));

		CString l_seek_msg;
		l_seek_msg.Format(IDS_SEEKTO, l_str_curtime, l_str_alltime, nPos/10);
		m_title->SetTitleText(l_seek_msg, 10000);
	}
	else if (SB_THUMBPOSITION == nSBCode)
	{
		m_title->Restore();
		if (g_stream_handle)
		{
			if (l_seconds_pos >= l_seconds_all - 0.3)
			{
				l_seconds_pos = l_seconds_all - 0.3;
			}
			BOOL l_result = BASS_ChannelSetPosition(
				g_stream_handle,
				BASS_ChannelSeconds2Bytes(g_stream_handle, l_seconds_pos),
				BASS_POS_BYTE);
			ASSERT(l_result);
			m_TimeWnd.SetTime((UINT)l_seconds_pos);
		}
		else
		{
			m_SliderTime.SetCurPos(0);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// WM_TIMER message handler
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnTimer(UINT nIDEvent) 
{
	CFrameWnd::OnTimer(nIDEvent);

	HSTREAM l_handle = 0;
	switch (nIDEvent)
	{
	case 1:
		l_handle = g_stream_handle;
		break;
	case 2:
		l_handle = (HSTREAM)g_record_handle;
		break;
	default:
		return;
	}

	double l_seconds_all = BASS_ChannelBytes2Seconds(l_handle,
		BASS_ChannelGetLength(l_handle, BASS_POS_BYTE));
	double l_seconds_cur = BASS_ChannelBytes2Seconds(l_handle,
		BASS_ChannelGetPosition(l_handle, BASS_POS_BYTE));

	if (l_handle == g_record_handle)
	{
		int l_stream_rate = m_conf.GetConfDialMp3()->nBitrate;
		QWORD l_size_bytes = (QWORD)m_record_file.GetLength();
		l_seconds_cur = (double)l_size_bytes / (l_stream_rate * 125);
	}
	if (!m_SliderTime.IsDragging())
	{
		m_SliderTime.SetCurPos(int(l_seconds_cur / l_seconds_all * 1000));
	}
	m_TimeWnd.SetTime((UINT)l_seconds_cur);
	UpdateStatWindow();

	if (BASS_ACTIVE_STOPPED == BASS_ChannelIsActive(l_handle))
	{
		PostMessage(WM_COMMAND, IDB_BTNSTOP, LONG(this->m_BtnSTOP.m_hWnd));
	}
}


//===========================================================================
// ������ �������������� ���������� ������ � ������ ���� "�:��:��"
//===========================================================================
void CMainFrame::Convert(UINT nCurSec, char *pszTime, int nStrSize)
{
	const char* szPattern[] = {"%d", "0%d"};
	char szMin[3] = "", szSec[3] = "";
	int  iHour, iMin, iSec;

	iHour= nCurSec/3600;
	iMin =(nCurSec - iHour*3600)/60;
	iSec = nCurSec - iHour*3600 - iMin*60;

	sprintf_s(szMin, sizeof(szMin), szPattern[iMin<10], iMin);
	sprintf_s(szSec, sizeof(szSec), szPattern[iSec<10], iSec);
	sprintf_s(pszTime, nStrSize, "%d:%s:%s", iHour, szMin, szSec);
}


//===========================================================================
// ������ ���������� ���� ������������ � ����������
//===========================================================================
void CMainFrame::UpdateIcoWindow()
{
	HSTREAM l_handle = (g_record_handle) ? g_record_handle : g_stream_handle;
	const DWORD CHANNEL_STATE = BASS_ChannelIsActive(l_handle);

	switch (CHANNEL_STATE)
	{
	case BASS_ACTIVE_PLAYING:
		m_IcoWnd.SetNewIcon((g_record_handle) ? ICON_REC : ICON_PLAY);
		break;
	case BASS_ACTIVE_PAUSED:
		m_IcoWnd.SetNewIcon((g_record_handle) ? ICON_PAUSER : ICON_PAUSEP);
		break;
	case BASS_ACTIVE_STOPPED:
		m_IcoWnd.SetNewIcon(ICON_STOP);
		break;
	default:
		ASSERT(false);
		break;
	}
}

//------------------------------------------------------------------------------
void CMainFrame::UpdateStatWindow()
{
	int l_stream_rate = m_conf.GetConfDialMp3()->nBitrate;
	int l_stream_freq = m_conf.GetConfDialMp3()->nFreq;
	int l_stream_mode = m_conf.GetConfDialMp3()->nStereo;

	QWORD l_size_bytes = 0;
	double l_size_seconds = 0;

	if (g_stream_handle)
	{
		BASS_CHANNELINFO l_channel_info;
		BASS_ChannelGetInfo(g_stream_handle, &l_channel_info);

		l_size_bytes = BASS_StreamGetFilePosition(g_stream_handle,
			BASS_FILEPOS_END);

		l_size_seconds = BASS_ChannelBytes2Seconds(g_stream_handle,
			BASS_ChannelGetLength(g_stream_handle, BASS_POS_BYTE));

		l_stream_rate = int(l_size_bytes / (125* l_size_seconds) + 0.5); //Kbps
		l_stream_freq = l_channel_info.freq;
		l_stream_mode = (l_channel_info.chans > 1) ? 1 : 0;
	}
	else if (g_record_handle)
	{
		l_size_bytes   = (QWORD)m_record_file.GetLength();
		l_size_seconds = (double)l_size_bytes / (l_stream_rate * 125);
		//l_size_seconds = BASS_ChannelBytes2Seconds(g_record_handle,
		//	BASS_ChannelGetPosition(g_record_handle, BASS_POS_BYTE));
	}
	char l_time_str[20] = {0};	// Time string in Hours:Minutes:Seconds format
	Convert((UINT)l_size_seconds, l_time_str, sizeof(l_time_str));

	m_StatWnd.Set(l_stream_freq, l_stream_rate, l_stream_mode);
	m_StatWnd.Set((UINT)l_size_bytes/1024, l_time_str);
}

//------------------------------------------------------------------------------
void CMainFrame::UpdateTrayText()
{
	CString wndText;

	GetWindowText(wndText);
	m_TrayIcon.SetTooltipText(wndText);
}

////////////////////////////////////////////////////////////////////////////////
// Main menu update handlers.
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateSoundRec(CCmdUI* pCmdUI) 
{
	bool l_enabled = !g_stream_handle;
	pCmdUI->Enable(l_enabled);
}

//------------------------------------------------------------------------------
void CMainFrame::OnUpdateSoundPlay(CCmdUI* pCmdUI)
{
	bool l_enabled = (m_record_file.m_hFile == CFile::hFileNull);
	pCmdUI->Enable(l_enabled);
}

//------------------------------------------------------------------------------
void CMainFrame::OnUpdateOptTop(CCmdUI* pCmdUI) 
{
	int enable = m_conf.GetConfProg()->bAlwaysOnTop;
	pCmdUI->SetCheck(enable);
}

//------------------------------------------------------------------------------
void CMainFrame::OnUpdateOptEm(CCmdUI* pCmdUI) 
{
	int enable = m_conf.GetConfProg()->bEasyMove;
	pCmdUI->SetCheck(enable);
}

//===========================================================================
// ������������
//===========================================================================
void CMainFrame::OnFileCreateopenA() 
{
	m_BtnOPEN.Press();
	OnBtnOPEN();
}

//===========================================================================
void CMainFrame::OnSoundRecA() 
{
	if (BASS_ChannelIsActive(g_stream_handle) != BASS_ACTIVE_STOPPED)
	{
		return;
	}
	m_BtnREC.Press();
	OnBtnREC();
}

//===========================================================================
void CMainFrame::OnSoundStopA() 
{
	m_BtnSTOP.Press();
	OnBtnSTOP();
}

//===========================================================================
void CMainFrame::OnSoundPlayA() 
{
	if (m_record_file.m_hFile != CFile::hFileNull)
	{
		return;
	}
	m_BtnPLAY.Press();
	OnBtnPLAY();
}

//===========================================================================
void CMainFrame::OnSoundBeginA() 
{
	OnSoundBegin();
}

void CMainFrame::OnSoundRewA() 
{
	OnSoundRew();
}

void CMainFrame::OnSoundFfA() 
{
	OnSoundFf();
}

void CMainFrame::OnSoundEndA() 
{
	OnSoundEnd();
}

//===========================================================================
void CMainFrame::OnMixRecA()
{
	m_BtnMIX_REC.Press();
	OnMixRec();
}

//---------------------------------------------------------------------
void CMainFrame::OnMixPlayA()
{
	m_BtnMIX_PLAY.Press();
	OnMixPlay();
}

//===========================================================================
// ��������� Drag-and-Drop
//===========================================================================
void CMainFrame::OnDropFiles(HDROP hDropInfo) 
{
	// ������ ���� ��������
	this->SetForegroundWindow();

	// �������� ��� �����
	TCHAR szFileName[_MAX_PATH];
	::DragQueryFile(hDropInfo, 0, szFileName, _MAX_PATH);

	// �������� ��� ������� �������� �����
	CString str(szFileName);
	OpenFile(str);

	::DragFinish(hDropInfo);
}

/////////////////////////////////////////////////////////////////////////////
CString CMainFrame::GetProgramDir()
{
	CString RtnVal;
    char    FileName[1000];

    GetModuleFileName(AfxGetInstanceHandle(), FileName, 1000);
    RtnVal = FileName;
    RtnVal = RtnVal.Left(RtnVal.ReverseFind('\\'));

    return RtnVal;
}

/////////////////////////////////////////////////////////////////////////////
CString CMainFrame::GetAutoName( CString& strPattern )
{
	// ������� ��� ����� "%" � ����� ������.
	if( strPattern.Right(1) == "%")
		strPattern.TrimRight('%');

	CTime t = CTime::GetCurrentTime();
	CString s = t.Format( m_conf.GetConfDialAN()->strAutoName );
	s += ".mp3";

	if( s == ".mp3" )
		s = "Empty.mp3";

	return s;
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	OnStatPref();
	CFrameWnd::OnLButtonDblClk(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnRButtonUp(UINT nFlags, CPoint point) 
{
	CRect r;
	CMenu StatMenu, *pStatMenu;

	ClientToScreen(&point);
	StatMenu.LoadMenu(IDR_STATMENU);
	pStatMenu = StatMenu.GetSubMenu(0);
	pStatMenu->TrackPopupMenu(TPM_LEFTALIGN|TPM_LEFTBUTTON, point.x,
		point.y, this);
	
	CFrameWnd::OnRButtonUp(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnOptSnddev() 
{
	//WinExec("control mmsys.cpl,,0", SW_SHOW);
	//WinExec("control desk.cpl,,1", SW_SHOW);

	CMP3_RecorderApp* l_app = (CMP3_RecorderApp* )AfxGetApp();
	if (l_app->IsVistaOS())
	{
		WinExec("control MMSYS.CPL", SW_SHOW);
	}
	else
	{
		WinExec("rundll32.exe MMSYS.CPL,ShowAudioPropertySheet", SW_SHOW);
	}
}

/////////////////////////////////////////////////////////////////////////////
BOOL CMainFrame::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
	if(!m_conf.GetConfDialGen()->bToolTips)
		return true;

	// need to handle both ANSI and UNICODE versions of the message
	TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
	TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;
	CString strTipText;

	// �������� ������������� ������� � ������ �� ����
	if(pNMHDR->code == TTN_NEEDTEXTA && (pTTTA->uFlags & TTF_IDISHWND) ||
	   pNMHDR->code == TTN_NEEDTEXTW && (pTTTW->uFlags & TTF_IDISHWND))
	{	// idFrom is actually the HWND of the tool
		UINT nID = ::GetDlgCtrlID((HWND)pNMHDR->idFrom);
		switch(nID)
		{
		case IDB_BTNOPEN:		nID = IDS_TT_OPEN; break;
		case IDB_BTNREC:		nID = IDS_TT_REC;  break;
		case IDB_BTNSTOP:		nID = IDS_TT_STOP; break;
		case IDB_BTNPLAY:		nID = IDS_TT_PLAY; break;
		case IDB_MIX_REC:		nID = IDS_TT_MIXREC;  break;
		case IDB_MIX_PLAY:		nID = IDS_TT_MIXPLAY; break;
		case IDS_SLIDERTIME:	nID = IDS_TT_SEEKBAR; break;
		case IDW_GRAPH:			nID = IDS_TT_WAVEWND; break;
		case IDW_TIME:			nID = IDS_TT_TIMEWND; break;
		case IDW_STAT:			nID = IDS_TT_STATWND; break;
		case IDB_MIX_SEL:		nID = IDS_TT_MIXSEL; break;
		case IDS_SLIDERVOL:		nID = IDS_TT_VOLBAR; break;
		case IDB_BTN_SCHED:		nID = IDS_TT_SCHEDULER; break;
		case IDB_BTN_VAS:		nID = IDS_TT_VAS;		break;
		case IDB_BTN_MON:		nID = IDS_TT_MONITORING;break;
		default:
			return true;
		}
		strTipText = CString((LPCSTR)nID);
		if(nID == IDS_TT_VOLBAR)
		{
			CString strCurLine;
			if(m_nActiveMixerID == 1)
				strCurLine = m_PlayMixer.GetLineName(m_PlayMixer.GetCurLine());
			else
				strCurLine = m_RecMixer.GetLineName(m_RecMixer.GetCurLine());
			strTipText = strTipText + strCurLine;
		}
	}

	// �������� ������ ��������� � ���������
	if (pNMHDR->code == TTN_NEEDTEXTA)
	  lstrcpyn(pTTTA->szText, strTipText, sizeof(pTTTA->szText));
	else
	  _mbstowcsz(pTTTW->szText, strTipText, sizeof(pTTTW->szText));
	*pResult = 0;

	return true;
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::CloseMixerWindows()
{
	// ��������� ��� ���� �������
	HWND hMixWnd = ::FindWindow("Volume Control", NULL);
	while(hMixWnd)
	{
		::SendMessage(hMixWnd, WM_CLOSE, 0, 0);
		hMixWnd = ::FindWindow("Volume Control", NULL);
	}
}


/////////////////////////////////////////////////////////////////////////////
LRESULT CMainFrame::OnTrayNotification(WPARAM wParam, LPARAM lParam)
{
	// Delegate all the work back to the default implementation in CSystemTray.
	return m_TrayIcon.OnTrayNotification(wParam, lParam);
}


void CMainFrame::OnSize(UINT nType, int cx, int cy) 
{
	CFrameWnd::OnSize(nType, cx, cy);
	
	if(nType==SIZE_MINIMIZED && m_conf.GetConfDialGen()->bTrayMin)
	{
		if(!m_conf.GetConfDialGen()->bTrayIcon)
			m_TrayIcon.ShowIcon();
		::ShowWindow(this->GetSafeHwnd(), SW_HIDE);
	}
	else if(nType==SIZE_RESTORED && !m_conf.GetConfDialGen()->bTrayIcon)
	{
		m_TrayIcon.HideIcon();
	}

}

BOOL CMainFrame::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	switch (wParam)
	{
	case IDM_TRAY_SHOW:
		{
			LONG wndStyle = GetWindowLong(GetSafeHwnd(), GWL_STYLE);
			int nCmdShow = (wndStyle&WS_MINIMIZE || !(wndStyle&WS_VISIBLE))
				? SW_RESTORE : SW_MINIMIZE;

			if (nCmdShow == SW_MINIMIZE && m_conf.GetConfDialGen()->bTrayMin)
			{
				nCmdShow = SW_HIDE;
			}
			::ShowWindow(this->GetSafeHwnd(), nCmdShow);
		}
		break;
	case IDM_TRAY_REC:
		OnBtnREC();
		break;
	case IDM_TRAY_STOP:
		OnBtnSTOP();
		break;
	case IDM_TRAY_PLAY:
		OnBtnPLAY();
		break;
	case IDM_TRAY_ABOUT:
		{
			CMP3_RecorderApp* my_app = (CMP3_RecorderApp *)AfxGetApp();
			my_app->OnAppAbout();
		}
		break;
	case IDM_TRAY_EXIT:
		// ���� ������ ������ �������� - ����� ��� ������� �� ������. �����.
		if (m_pOptDialog)
		{
			m_pOptDialog->PostMessage(WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
			m_pOptDialog = NULL;
		}
		PostMessage(WM_CLOSE);
		break;
	}
		
	return CFrameWnd::OnCommand(wParam, lParam);
}

void CMainFrame::OnUpdateTrayPlay(CCmdUI* pCmdUI) 
{
	bool l_enabled = (m_record_file.m_hFile == CFile::hFileNull);
	pCmdUI->Enable(l_enabled);
}

void CMainFrame::OnUpdateTrayRec(CCmdUI* pCmdUI) 
{
	bool l_enabled = BASS_ChannelIsActive(g_stream_handle)==BASS_ACTIVE_STOPPED;
	pCmdUI->Enable(l_enabled);
}

/*
BOOL CMainFrame::ShowTaskBarButton(BOOL bVisible)
{
	if (!m_bOwnerCreated) return FALSE; 

	::ShowWindow(GetSafeHwnd(), SW_HIDE);

	if (bVisible) 
		ModifyStyleEx(0, WS_EX_APPWINDOW); 
	else 
		ModifyStyleEx(WS_EX_APPWINDOW, 0);

	::ShowWindow(GetSafeHwnd(), SW_SHOW);

	return TRUE; 
}
*/

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnBtnMIX_INV()
{
	this->SetFocus();

	m_nActiveMixerID = (m_nActiveMixerID == 1) ? 0 : 1;
	if(m_nActiveMixerID == 1)
		OnPlayMixMenuSelect(ID_MIXITEM_PLAY0 + m_PlayMixer.GetCurLine());
	else
		OnRecMixMenuSelect(ID_MIXITEM_REC0 + m_RecMixer.GetCurLine());
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnBtnMIX_SEL()
{
	this->SetFocus();

	static bool bShowMenu = true;
	if (!bShowMenu)
	{
		bShowMenu = true;
		return;
	}
	/*
	int l_input[] = {
		BASS_INPUT_TYPE_DIGITAL,
		BASS_INPUT_TYPE_LINE,
		BASS_INPUT_TYPE_MIC,
		BASS_INPUT_TYPE_SYNTH,
		BASS_INPUT_TYPE_CD,
		BASS_INPUT_TYPE_PHONE,
		BASS_INPUT_TYPE_SPEAKER,
		BASS_INPUT_TYPE_WAVE,
		BASS_INPUT_TYPE_AUX,
		BASS_INPUT_TYPE_ANALOG,
		BASS_INPUT_TYPE_UNDEF
	};
	{
		int count=0;
		BASS_DEVICEINFO info;
		for (int device_id=0; BASS_RecordGetDeviceInfo(device_id, &info); device_id++)
		{
			if (info.flags&BASS_DEVICE_ENABLED) // device is enabled
			{
				BASS_RecordInit(device_id);
				const char *name;
				for (int n=-1; name=BASS_RecordGetInputName(n); n++)
				{
					float vol;
					int s=BASS_RecordGetInput(n, &vol);
					printf("%s [%s : %g]\n", name, s&BASS_INPUT_OFF?"off":"on", vol);

					if ((s&BASS_INPUT_TYPE_MASK) == BASS_INPUT_TYPE_MIC)
					{
						bool l_found_mic = true;
					}
				}
				BASS_RecordFree();
				int l_error = BASS_ErrorGetCode();
				count++; // count it 
			}
		}
		count += 0;
	}
	*/
	// ������������� ������
	m_RecMixer.Open(WAVE_MAPPER, GetSafeHwnd());
	m_PlayMixer.Open(WAVE_MAPPER, GetSafeHwnd());
	if(m_nActiveMixerID == 1)
		OnPlayMixMenuSelect(ID_MIXITEM_PLAY0 + m_PlayMixer.GetCurLine());
	else
		OnRecMixMenuSelect(ID_MIXITEM_REC0 + m_RecMixer.GetCurLine());

	CRect r;
	m_BtnMIX_SEL.GetWindowRect(&r);

	CMenu mixMenu;
	mixMenu.CreatePopupMenu();

	for(int j = 0; j < m_PlayMixer.GetLinesNum(); j++)
	{
		mixMenu.AppendMenu(MF_STRING, ID_MIXITEM_PLAY0 + j, m_PlayMixer.GetLineName(j));
	}
	mixMenu.AppendMenu(MF_SEPARATOR);
	for(int i = 0; i < m_RecMixer.GetLinesNum(); i++)
	{
		mixMenu.AppendMenu(MF_STRING, ID_MIXITEM_REC0 + i, m_RecMixer.GetLineName(i));
	}

	if(m_nActiveMixerID == 1) // play_mixer
	{
		//mixMenu.CheckMenuItem(ID_MIXITEM_PLAY0 + m_PlayMixer.GetCurLine(),
		//	MF_CHECKED | MF_BYCOMMAND);
	}
	else
	{
		mixMenu.CheckMenuItem(ID_MIXITEM_REC0 + m_RecMixer.GetCurLine(),
			MF_CHECKED | MF_BYCOMMAND);
	}

	//������� ���� �� ����� � ������������� ���� ������
	int nItemID = mixMenu.TrackPopupMenu(TPM_VCENTERALIGN|TPM_LEFTBUTTON|
		TPM_RETURNCMD, r.right, r.top+(r.bottom-r.top)/2, this);
	if(!nItemID)
	{
		POINT p;
		::GetCursorPos(&p);
		RECT r;
		m_BtnMIX_SEL.GetWindowRect(&r);
		bShowMenu = (PtInRect(&r, p)) ? false : true;
	}
	else
	{
		this->PostMessage(WM_COMMAND, MAKEWPARAM(nItemID, 0), 0);
	}
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnRecMixMenuSelect(UINT nID)
{
	if(m_RecMixer.GetLinesNum() == 0)
	{	m_SliderVol.EnableWindow(false);
		return;
	}

	m_SliderVol.EnableWindow(true);
	m_RecMixer.SetLine(nID - ID_MIXITEM_REC0);
	m_SliderVol.SetPos(m_RecMixer.GetVol(m_RecMixer.GetCurLine()));
	m_nActiveMixerID = 0;

	// Force updating button image
	CMP3_RecorderApp* l_app = (CMP3_RecorderApp* )AfxGetApp();
	if (l_app->IsVistaOS())
	{
		PostMessage(MM_MIXM_LINE_CHANGE, 0, 0);
	}

	//m_BtnMIX_INV.SetIcon(IDI_MIC);
	//m_BtnMIX_SEL.SetIcon(IDI_MIC);

	// ������ ������ � ����������� �� ���� �����
	/*const int ICON_ID[] = { IDI_MIXLINE00, IDI_MIXLINE01, IDI_MIXLINE02,
							IDI_MIXLINE03, IDI_MIXLINE04, IDI_MIXLINE05,
							IDI_MIXLINE06, IDI_MIXLINE07, IDI_MIXLINE08,
							IDI_MIXLINE09, IDI_MIXLINE10 };
	int i = m_RecMixer.GetLineType(m_RecMixer.GetCurLine());
	m_BtnMIX_SEL.SetIcon(ICON_ID[i]);*/
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnPlayMixMenuSelect(UINT nID)
{
	if(m_PlayMixer.GetLinesNum() == 0)
	{	m_SliderVol.EnableWindow(false);
		return;
	}

	m_SliderVol.EnableWindow(true);
	m_PlayMixer.SetLine(nID - ID_MIXITEM_PLAY0);
	m_SliderVol.SetPos(m_PlayMixer.GetVol(m_PlayMixer.GetCurLine()));
	m_nActiveMixerID = 1;
	//m_BtnMIX_INV.SetIcon(IDI_DIN);
	m_BtnMIX_SEL.SetIcon(IDI_MIXLINE);
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::ProcessSliderVol(UINT nSBCode, UINT nPos)
{
   // Get the minimum and maximum scroll-bar positions.
   int minpos, maxpos, curpos;
   m_SliderVol.GetRange(minpos, maxpos);
   curpos = m_SliderVol.GetPos();

/*   // Determine the new position of scroll box.
   switch (nSBCode)
   {
   case SB_LEFT:
      curpos = minpos;
      break;
   case SB_RIGHT:
      curpos = maxpos;
      break;
   case SB_LINELEFT:
      if (curpos < minpos)
         curpos--;
      break;
   case SB_LINERIGHT:
      if (curpos > maxpos)
         curpos++;
      break;
   case SB_PAGELEFT:
	  if (curpos > minpos)
		curpos = max(minpos, curpos - m_SliderVol.GetPageSize());
	  break;
   case SB_PAGERIGHT:
      if (curpos < maxpos)
         curpos = min(maxpos, curpos + m_SliderVol.GetPageSize());
      break;
   case SB_THUMBTRACK:
   case SB_THUMBPOSITION:
      curpos = nPos;
      break;
   default:
	   return;
   }
   // Set the new position of the thumb (scroll box).
   m_SliderVol.SetPos(curpos);
*/
	// ����������� ������������
	int nPercent = int(100.0 * curpos / (maxpos - minpos));

	CString strTitle;
	if(m_nActiveMixerID == 1)
	{
		if(m_PlayMixer.GetLinesNum() == 0)
			return;
		strTitle.Format(IDS_VOLUME_TITLE, nPercent,
			m_PlayMixer.GetLineName(m_PlayMixer.GetCurLine()));
		m_PlayMixer.SetVol(nPercent);
	}
	else
	{
		if(m_RecMixer.GetLinesNum() == 0)
			return;
		strTitle.Format(IDS_VOLUME_TITLE, nPercent,
			m_RecMixer.GetLineName(m_RecMixer.GetCurLine()));
		m_RecMixer.SetVol(nPercent);
	}

	// ������������ ��������� ��������
	switch (nSBCode)
	{
	case SB_THUMBTRACK:
		m_title->SetTitleText(strTitle, 10000);
		break;
	case SB_THUMBPOSITION:
		m_title->Restore();
		break;
	case SB_PAGELEFT:
	case SB_PAGERIGHT:
	case SB_LINELEFT:
	case SB_LINERIGHT:
		//m_title->SetTitleText(strTitle, 1200);
		break;
	}
}
void CMainFrame::OnVolUpA() 
{
	// ��������� �� ����������� ����� ��������
	if(m_nActiveMixerID == 0 && m_RecMixer.GetLinesNum() == 0)
		return;
	if(m_nActiveMixerID == 1 && m_PlayMixer.GetLinesNum() == 0)
		return;

	int nPos = min(m_SliderVol.GetRangeMax(),
		m_SliderVol.GetPos() + m_SliderVol.GetPageSize());
	m_SliderVol.SetPos(nPos);

	this->PostMessage(WM_HSCROLL, MAKEWPARAM(0, SB_PAGERIGHT),
		(LPARAM)m_SliderVol.GetSafeHwnd());
}

void CMainFrame::OnVolDownA() 
{
	// ��������� �� ����������� ����� ��������
	if(m_nActiveMixerID == 0 && m_RecMixer.GetLinesNum() == 0)
		return;
	if(m_nActiveMixerID == 1 && m_PlayMixer.GetLinesNum() == 0)
		return;

	int nPos = max(m_SliderVol.GetRangeMin(),
		m_SliderVol.GetPos() - m_SliderVol.GetPageSize());
	m_SliderVol.SetPos(nPos);

	this->PostMessage(WM_HSCROLL, MAKEWPARAM(0, SB_PAGELEFT),
		(LPARAM)m_SliderVol.GetSafeHwnd());
}


/////////////////////////////////////////////////////////////////////////////
// �������
/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnBtnSched()
{
#ifndef _DEBUG
	// ������ ����������� ����� ���������� ���������� �������
	if(fsProtect_GetDaysLeft() <= 0)
		return;
#endif

	bool bIsEnabled		= m_conf.GetConfDialSH2()->bIsEnabled  != 0;
	bool bSchedStart	= m_conf.GetConfDialSH2()->bSchedStart != 0;
	SHR_TIME* ptStart	= &m_conf.GetConfDialSH2()->t_start;
	SHR_TIME* ptRec		= &m_conf.GetConfDialSH2()->t_rec;
	SHR_TIME* ptStop	= &m_conf.GetConfDialSH2()->t_stop;

	if (!bIsEnabled)
	{	// ���� ����� ������ �� ������, �� ������� ������� ��� ������
		if (bSchedStart)
		{
			if (m_conf.GetConfDialSH2()->nStopByID == 1)
			{
				m_sched2.SetRecTime(ptRec, ptStart);
				TRACE("==> OnBtnSched: SetRecTime(%d:%d:%d, %d:%d:%d)\n",
					ptRec->h, ptRec->m, ptRec->s, ptStart->h, ptStart->m, ptStart->s);
			}
			else
			{
				m_sched2.SetStopTime(ptStop, ptStart);
				TRACE("==> OnBtnSched: SetStopTime(%d:%d:%d, %d:%d:%d)\n",
					ptStop->h, ptStop->m, ptStop->s, ptStart->h, ptStart->m, ptStart->s);
			}
			if (!m_sched2.Start())
			{
				return;
			}
		}
		// ���� ��������� ��������� - ������, �� ���. ���
		else if (BASS_ACTIVE_PLAYING == BASS_ChannelIsActive(g_record_handle))
		{
			if (m_conf.GetConfDialSH2()->nStopByID == 1)
			{
				m_sched2.SetRecTime(ptRec, NULL);
			}
			else
			{
				m_sched2.SetStopTime(ptStop, NULL);
			}
			if (!m_sched2.Start())
			{
				return;
			}
		}
		m_StatWnd.m_btnSched.SetState(BTN_PRESSED);
	}
	else
	{
		m_sched2.Stop();
		m_StatWnd.m_btnSched.SetState(BTN_NORMAL);
	}

	m_conf.GetConfDialSH2()->bIsEnabled = !bIsEnabled;
	// ��������� ��������� ������ "������" ��� ���������� ��������
	UpdateButtonState(IDB_BTNREC);

	/*
	// ��������� ���� �������
	int nRecTime = 0;
	if (m_conf.GetConfDialSH2()->bIsEnabled)
	{
		nRecTime = m_sched2.GetRecTimeInSec();
	}
	m_TimeWnd.SetTime(nRecTime);
	*/
}

////////////////////////////////////////////////////////////////////////////////
// IN: nAction - ��������������� ��������
//		0 - ����� ������
//		1 - ���� ������
void Scheduler2Function(int nAction)
{
	//CMainFrame* pMainWnd = (CMainFrame *)AfxGetMainWnd();
	CMainFrame* pMainWnd = CMainFrame::m_pMainFrame;

	if (nAction == 0)		// ������������ ����� ������
	{
		if (pMainWnd->m_conf.GetConfDialSH2()->bRunExternal)
		{
			ShellExecute(0, NULL, pMainWnd->m_conf.GetConfDialSH2()->strFileName,
				NULL, NULL, SW_SHOWNORMAL);
		}

		if (pMainWnd->m_record_file.m_hFile == CFile::hFileNull)
		{
			CString strName = pMainWnd->GetAutoName(CString(""));
			CString strPath = pMainWnd->m_conf.GetConfProg()->strLastFilePath;
			if (strPath.IsEmpty())
			{
				strPath = pMainWnd->m_strDir;
			}
			strName = strPath + "\\" + strName;
			TRACE1("==> Scheduler2Function: trying to open file: %s\n", strName);
			pMainWnd->OpenFile(strName);
		}
		
		if (BASS_ChannelIsActive(g_record_handle) != BASS_ACTIVE_PLAYING)
		{
			TRACE0("==> Scheduler2Function: call OnBtnREC to start recording\n");
			//pMainWnd->OnBtnSTOP();
			pMainWnd->OnBtnREC();
			pMainWnd->UpdateButtonState(IDB_BTNREC);
		}
	}
	else if (nAction == 1)	// ������������ ���� ������
	{
		TRACE0("==> Scheduler2Function: call OnBtnSTOP\n");
		pMainWnd->OnBtnSTOP();

		if (pMainWnd->m_conf.GetConfDialSH2()->action.shutDown)
		{
			CTimerDlg dlg;
			dlg.m_tdi.strDlgText = "Recording complete. Shutdown computer?";
			if (dlg.DoModal() == IDOK)
			{
				ShutDownComputer();
			}
			return;
		}
		else if (pMainWnd->m_conf.GetConfDialSH2()->action.closeAll)
		{
			CTimerDlg dlg;
			dlg.m_tdi.strDlgText = "Recording complete. Close the program?";
			if (dlg.DoModal() == IDOK)
			{
				CloseAllPrograms();
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Processing sound level monitoring
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnBtnMonitoring()
{
#ifndef _DEBUG
	// ������ ����������� ����� ���������� ���������� �������
	if(fsProtect_GetDaysLeft() <= 0)
		return;
#endif

	if (!m_bMonitoringBtn)
	{
		// If not recording and not playing, then start monitoring.
		if (!g_record_handle && (BASS_ChannelIsActive(g_stream_handle) ==
			BASS_ACTIVE_STOPPED))
		{
			if (!MonitoringStart())
			{
				AfxMessageBox("Monitoring error!", MB_OK);
				return;
			}
		}
		m_bMonitoringBtn = true;
		m_StatWnd.m_btnMon.SetState(BTN_PRESSED);
	}
	else
	{
		MonitoringStop();
		m_bMonitoringBtn = false;
		m_StatWnd.m_btnMon.SetState(BTN_NORMAL);
	}
	m_conf.GetConfProg()->bMonitoring = m_bMonitoringBtn;
}

//------------------------------------------------------------------------------
bool CMainFrame::IsMonitoringOnly()
{
	return m_bMonitoringBtn && (m_nState == STOP_STATE);
}

//------------------------------------------------------------------------------
bool CMainFrame::MonitoringStart()
{
	if (!BASS_RecordInit(-1))
	{
		return false;
	}
	BASS_SetConfig(BASS_CONFIG_REC_BUFFER, 1000);
	g_monitoring_handle = BASS_RecordStart(44100, 2, MAKELONG(0, 10), NULL, NULL);
	if (!g_monitoring_handle)
	{
		return false;
	}
	m_GraphWnd.StartUpdate((HSTREAM)g_monitoring_handle);
	return true;
}

//------------------------------------------------------------------------------
void CMainFrame::MonitoringStop()
{
	if (!g_monitoring_handle)
	{
		return;
	}
	m_GraphWnd.StopUpdate();
	m_GraphWnd.Clear();
	BASS_ChannelStop(g_monitoring_handle);
	BASS_RecordFree();
	g_monitoring_handle = 0;
}

////////////////////////////////////////////////////////////////////////////////
// VAS 
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnBtnVas()
{
#ifndef _DEBUG
	// ������ ����������� ����� ���������� ���������� �������
	if(fsProtect_GetDaysLeft() <= 0)
		return;
#endif

	CONF_DIAL_VAS* pConfig = m_conf.GetConfDialVAS();

	if(!m_vas.IsRunning())
	{
		m_vas.InitVAS(pConfig->nThreshold, pConfig->nWaitTime);
		m_StatWnd.m_btnVas.SetState(BTN_PRESSED);
		m_GraphWnd.ShowVASMark(pConfig->nThreshold);
	}
	else
	{
		m_vas.StopVAS();
		m_StatWnd.m_btnVas.SetState(BTN_NORMAL);
		UpdateIcoWindow();
		if(m_nState == RECORD_STATE)
			m_TrayIcon.SetIcon(IDI_TRAY_REC);
		m_GraphWnd.HideVASMark();
	}
	pConfig->bEnable = m_vas.IsRunning();
}

////////////////////////////////////////////////////////////////////////////////
void CMainFrame::ProcessVAS(bool bVASResult)
{
	static bool bOldVASResult = false;

	if (bVASResult == true)
	{
		switch (m_conf.GetConfDialVAS()->nAction)
		{
		case 0:
			m_IcoWnd.SetNewIcon(ICON_RECVAS);
			m_TrayIcon.SetIcon(IDI_TRAY_PAUSE);
			break;
		case 1:
			this->PostMessage(WM_COMMAND, IDB_BTNSTOP,
				LONG(this->m_BtnSTOP.m_hWnd));
			break;
		}
	}
	else if ((bVASResult == false) && (bOldVASResult == true))
	{	// ����� �� ������� VAS
		m_IcoWnd.SetNewIcon(ICON_REC);
		m_TrayIcon.SetIcon(IDI_TRAY_REC);
	}

	bOldVASResult = bVASResult;
}

/////////////////////////////////////////////////////////////////////////////
BOOL CMainFrame::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
{
	if (zDelta > 0)
	{
		OnVolUpA();
	}
	else if (zDelta < 0)
	{
		OnVolDownA();
	}
	return CFrameWnd::OnMouseWheel(nFlags, zDelta, pt);
}

////////////////////////////////////////////////////////////////////////////////
void CMainFrame::UpdateButtonState(UINT nID)
{
	CButton* pBtn = (CButton *)GetDlgItem(nID);
	ASSERT(pBtn);

	pBtn->ModifyStyle(WS_DISABLED, 0);	// Resetting button state

	if (IDB_BTNREC == nID)
	{
		bool bSchedStart   = m_conf.GetConfDialSH2()->bSchedStart != 0;
		bool bSchedEnabled = m_conf.GetConfDialSH2()->bIsEnabled != 0;

		if (bSchedEnabled && bSchedStart &&
			(BASS_ChannelIsActive(g_record_handle) == BASS_ACTIVE_STOPPED))
		{
			pBtn->ModifyStyle(0, WS_DISABLED);
		}
		// �������� ������ "������" ��� ���������������
		if (BASS_ChannelIsActive(g_stream_handle) != BASS_ACTIVE_STOPPED)
		{
			pBtn->ModifyStyle(0, WS_DISABLED);
		}
	}
	else if (IDB_BTNPLAY == nID)
	{
		// �������� ������ "���������������" ��� ������
		if (m_record_file.m_hFile != CFile::hFileNull)
		{
			pBtn->ModifyStyle(0, WS_DISABLED);
		}
	}
	pBtn->Invalidate(false); // ��������� ������
}

/////////////////////////////////////////////////////////////////////////////