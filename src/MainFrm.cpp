////////////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include "stdafx.h"
#include <map>
#include <math.h>

#include "MP3_Recorder.h"
#include "MainFrm.h"
#include "MainFrm_Helpers.h"
#include "common.h"
#include "BASS_Functions.h"
#include "RecordingSourceDlg.h"
#include <basswasapi.h>

//#include "FilterChain.h"
#include "WasapiRecorder.h"
#include "WasapiRecorderMulti.h"
#include "VASFilter.h"
#include "Encoder_MP3.h"
#include "FilterFileWriter.h"
#include "FilterFileWriterWAV.h"
#include "Debug.h"
#include "MySheet.h"
#include "ShellUtils.h"

HSTREAM m_bassPlaybackHandle = 0;   // Playback
//HSTREAM g_update_handle = 0;   // Graph window update (used by callback func)
//HSTREAM g_loopback_handle = 0; // Handle for Loopback stream in Vista

//HRECORD g_record_handle = 0; 
//HRECORD g_monitoring_handle = 0;

//static CWasapiRecorder* g_wasapi_recorder = NULL;
//static FileWriter* g_fileWriter = NULL;
//static CEncoder_MP3* g_mp3Encoder = NULL;

////////////////////////////////////////////////////////////////////////////////
// shared data
#pragma data_seg(".SHARED")

TCHAR g_command_line[MAX_PATH] = {0};

#pragma data_seg()

#pragma comment(linker, "/section:.SHARED,RWS")
// end shared data


////////////////////////////////////////////////////////////////////////////////
bool IsPlaying(const ProgramState& a_state)
{
	return (a_state == PLAY_STATE || a_state == PAUSEPLAY_STATE);
};

bool IsRecording(const ProgramState& a_state)
{
	return (a_state == RECORD_STATE || a_state == PAUSEREC_STATE);
};

////////////////////////////////////////////////////////////////////////////////
#define WM_ICON_NOTIFY   WM_USER+10
#define WM_FILTER_NOTIFY WM_USER+11

static const UINT UWM_ARE_YOU_ME = ::RegisterWindowMessage(
	_T("UWM_ARE_YOU_ME-{B87861B4-8BE0-4dc7-A952-E8FFEEF48FD3}"));

static const UINT UWM_PARSE_LINE = ::RegisterWindowMessage(
	_T("UWM_PARSE_LINE-{FE0907E6-B77E-46da-8D2B-15F41F32F440}"));

LRESULT CMainFrame::OnAreYouMe(WPARAM, LPARAM)
{
	return UWM_ARE_YOU_ME;
}

LRESULT CMainFrame::OnParseLine(WPARAM, LPARAM)
{
	CString l_cmd_line = g_command_line;
	CString l_file_cmd = _T("/file=\"");
	CString l_file_name = _T("{Desktop}/{Autoname}.mp3");
	bool l_cmd_record = false;

	if (l_cmd_line.Find(_T("/close")) != -1)
	{
		// Using IDM_TRAY_EXIT to correctly close if options dialog is opened.
		PostMessage(WM_COMMAND, MAKEWPARAM(IDM_TRAY_EXIT, 0), 0);
		return UWM_PARSE_LINE;
	}
	if (l_cmd_line.Find(_T("/record")) != -1)
	{
		l_cmd_record = true;
	}
	if (l_cmd_line.Find(l_file_cmd) != -1)
	{
		// Getting last position of a file name
		int l_start_pos = l_cmd_line.Find(l_file_cmd) + l_file_cmd.GetLength();
		int l_end_pos = l_start_pos;
		for (; l_end_pos < l_cmd_line.GetLength(); l_end_pos++)
		{
			if (_T("\"") == l_cmd_line.Mid(l_end_pos, 1))
				break;
		}
		//Extracting file name
		l_file_name = l_cmd_line.Mid(l_start_pos, l_end_pos - l_start_pos);
	}

	l_file_name = ParseFileName(l_file_name);
	if (l_cmd_record)
	{
		// Change file name if it is currently exist
		int l_number = 2;
		DWORD l_error_code = 0;
		while (!Helpers::IsSuitableForRecording(l_file_name, &l_error_code))
		{
			if (ERROR_PATH_NOT_FOUND == l_error_code)
			{
				AfxMessageBox(IDS_ERROR_DIRECTORY, MB_OK|MB_ICONWARNING);
				return UWM_PARSE_LINE;
			}

			CString l_addition;

			// Removing old addition.
			l_addition.Format(_T("[%d]"), l_number - 1);
			l_file_name.Replace(l_addition, _T(""));

			// Adding new addition, [N] like.
			l_addition.Format(_T("[%d]"), l_number++);
			int l_dot_pos = l_file_name.ReverseFind(_T('.'));
			l_dot_pos = (l_dot_pos != -1) ? l_dot_pos : l_file_name.GetLength();
			l_file_name.Insert(l_dot_pos, l_addition);
		}
	}
	
	OpenFile(l_file_name);
	if (l_cmd_record)
	{
		PostMessage(WM_COMMAND, MAKEWPARAM(IDM_SOUND_REC, 0), NULL);
	}
	return UWM_PARSE_LINE;
}

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
	ON_BN_CLICKED(IDB_BTNSTOP, OnBtnSTOP)
	ON_BN_CLICKED(IDB_BTNREC,  OnBtnREC)
	ON_BN_CLICKED(IDB_MIX_SEL, OnBtnMIX_SEL)
	ON_WM_LBUTTONDOWN()
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
	ON_COMMAND(IDA_SOUND_PLAY, OnSoundPlayA)
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
	ON_COMMAND(IDM_SOUND_REC, OnBtnREC)
	ON_COMMAND(IDM_SOUND_STOP, OnBtnSTOP)
	ON_COMMAND(IDM_SOUND_PLAY, OnBtnPLAY)
	ON_WM_MOUSEWHEEL()
	//}}AFX_MSG_MAP
	ON_REGISTERED_MESSAGE(UWM_ARE_YOU_ME, OnAreYouMe)
	ON_REGISTERED_MESSAGE(UWM_PARSE_LINE, OnParseLine)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnToolTipNotify)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipNotify)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_SOUND_PLAY, IDM_SOUND_END, OnUpdateSoundPlay)
	ON_MESSAGE(WM_ICON_NOTIFY, OnTrayNotification)
	ON_MESSAGE(WM_FILTER_NOTIFY, OnFilterNotify)
	ON_MESSAGE(WM_RECSOURCE_DLGCLOSED, OnRecSourceDialogClosed)
	ON_MESSAGE(WM_RECSOURCE_CHANGED, OnRecSourceChanged)
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////

float CMainFrame::PeaksCallback_Wasapi(int channel, void* userData)
{
	IWasapiRecorder* recorder = static_cast<IWasapiRecorder*>(userData);
	return (recorder != NULL) ? recorder->GetPeakLevel(channel) : 0;
}
//------------------------------------------------------------------------------

int CMainFrame::LinesCallback_Wasapi(int channel, float* buffer, int bufferSize, void* userData)
{
	IWasapiRecorder* recorder = static_cast<IWasapiRecorder*>(userData);
	return (recorder != NULL) ? recorder->GetChannelData(channel, buffer, bufferSize) : 0;
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//CMainFrame* CMainFrame::m_pMainFrame = NULL;

//------------------------------------------------------------------------------
float CMainFrame::PeaksCallback(int channel, void* userData)
{
	CMainFrame* mainFrame = static_cast<CMainFrame*>(userData);

	DWORD peakLevel = BASS_ChannelGetLevel(mainFrame->m_bassPlaybackHandle);
	if (peakLevel == -1 || channel < 0 || channel > 1)
		return 0;

	peakLevel = (channel == 0) ? LOWORD(peakLevel) : HIWORD(peakLevel);
	return float(peakLevel) / 32768;
}

//------------------------------------------------------------------------------
int CMainFrame::LinesCallback(int channel, float* buffer, int bufferSize, void* userData)
{
	CMainFrame* mainFrame = static_cast<CMainFrame*>(userData);

	float* l_buffer_ptr = buffer + channel;
	const int l_bytes_2_copy = (bufferSize - channel) * sizeof(float);

	const int l_bytes_copied = BASS_ChannelGetData(
		mainFrame->m_bassPlaybackHandle, l_buffer_ptr, (l_bytes_2_copy | BASS_DATA_FLOAT));

	return (l_bytes_copied == -1) ? 0 : l_bytes_copied / sizeof(float);
}

////////////////////////////////////////////////////////////////////////////////
CMainFrame::CMainFrame()
	//:m_vista_loopback(NULL)
	//,m_visualization_data(NULL)
	//,m_loopback_hdsp(0)
	//,m_mute_hdsp(0)
	:m_playback_volume(1.0) //full volume
	,m_recording_volume(1.0)
	,m_monitoringChain(HandleFilterNotification, this)
	,m_recordingChain(HandleFilterNotification, this)
	,m_bassPlaybackHandle(0)
	,m_destroyingMainWindow(false)
{
	m_pEncoder	= NULL;
	m_title		= NULL;

	m_nState    = STOP_STATE;

	//m_pMainFrame = this;
	m_bAutoMenuEnable = false;

	// Init window snapping
	m_szMoveOffset.cx = 0;
	m_szMoveOffset.cy = 0;
	m_nSnapPixels = 7;
	::SystemParametersInfo(SPI_GETWORKAREA, 0, &m_rDesktopRect, 0);

	BASS_SetConfig(BASS_CONFIG_FLOATDSP, TRUE);
	BASS_SetConfig(BASS_CONFIG_REC_BUFFER, 500);

	// Note: calls OnCreate handler.
	LoadFrame(IDR_MAINFRAME, WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX);
}

//====================================================================
CMainFrame::~CMainFrame()
{
	//if (g_wasapi_recorder != NULL)
	//	g_wasapi_recorder->Stop();
	//if (g_mp3Encoder != NULL)
	//	g_mp3Encoder->WriteVBRHeader(_T("d:\\test.mp3"));

	//SAFE_DELETE(g_wasapi_recorder);
	//SAFE_DELETE(g_fileWriter);
	//SAFE_DELETE(g_mp3Encoder);

	//!!!test
	BASS_WASAPI_Stop(TRUE);
	BASS_WASAPI_Free();

	SAFE_DELETE(m_title);
	//SAFE_DELETE(m_visualization_data);
}

/////////////////////////////////////////////////////////////////////////////
#pragma optimize("", off)
int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	//m_bMonitoringBtn = false;

	// Removing the "Register" menu in registered mode
	REG_CRYPT_BEGIN;
	#ifndef _DEBUG
		CMenu* pMenu = this->GetMenu();
		pMenu->RemoveMenu(4, MF_BYPOSITION);
		CMenu* pHelpSubMenu = pMenu->GetSubMenu(3);
		pHelpSubMenu->EnableMenuItem(5, MF_GRAYED|MF_BYPOSITION);
		this->DrawMenuBar();
	#endif
	REG_CRYPT_END;

	// Adjusting vertical window size
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

	// Creating interface windows
	m_IcoWnd.Create(NULL, NULL, WS_CHILD|WS_VISIBLE, CRect(1, 0, 24, 25), this, IDW_ICO);
	m_TimeWnd.Create(NULL, NULL, WS_CHILD|WS_VISIBLE, CRect(24, 0, 106, 25), this, IDW_TIME);
	m_GraphWnd.Create(NULL, NULL, WS_CHILD|WS_VISIBLE, CRect(1, 24, 106, 77), this, IDW_GRAPH);
	m_StatWnd.Create(NULL, NULL, WS_CHILD|WS_VISIBLE, CRect(106, 0, 282, 50), this, IDW_STAT);
	
	m_IcoWnd.ShowWindow(SW_SHOW);   m_IcoWnd.UpdateWindow();
	m_TimeWnd.ShowWindow(SW_SHOW);  m_TimeWnd.UpdateWindow();
	m_GraphWnd.ShowWindow(SW_SHOW); m_GraphWnd.UpdateWindow();
	m_StatWnd.ShowWindow(SW_SHOW);  m_StatWnd.UpdateWindow();

	// Setting graphs display mode
	//const int graphType = RegistryConfig::GetOption(_T("General\\Graph Type"), 0);
	//const bool maxPeaks = RegistryConfig::GetOption(_T("General\\Show max peaks"), 1);
	//m_GraphWnd.SetDisplayMode(CGraphWnd::DisplayMode(graphType));
	//m_GraphWnd.ShowMaxpeaks(maxPeaks);

	// Adjusting the position and volume sliders
	CRect rT(106, 55, 286, 78);
	CRgn rgnT;
	rgnT.CreateRectRgn(0+6, 0, rT.right-rT.left-6, 26/*rT.bottom-rT.top*/);
	m_SliderTime.Create(WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS|TBS_BOTH,
		rT, this, IDS_SLIDERTIME);
	m_SliderTime.SetWindowRgn(rgnT, false);
	m_SliderTime.SetRange(0, 1000);

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
	
	// Creating the control buttons
	int	iX = 6, iY = 85, idx = 45, idy = 23;
	m_BtnOPEN.Create(CRect(iX, iY, iX+idx, iY+idy), this, IDB_BTNOPEN); iX += idx + 1;
	m_BtnREC.Create(CRect(iX, iY, iX+idx, iY+idy), this, IDB_BTNREC);  iX += idx + 1;
	m_BtnSTOP.Create(CRect(iX, iY, iX+idx, iY+idy), this, IDB_BTNSTOP); iX += idx + 1;
	m_BtnPLAY.Create(CRect(iX, iY, iX+idx, iY+idy), this, IDB_BTNPLAY); iX = 226; idx = 25;

	iX = 251; idx = 27;
	m_BtnMIX_SEL.Create(CRect(iX, iY, iX+idx, iY+idy), this, IDB_MIX_SEL);

	m_BtnOPEN.SetIcon(IDI_OPEN);
	m_BtnPLAY.SetIcon(IDI_PLAY);
	m_BtnSTOP.SetIcon(IDI_STOP);
	m_BtnREC.SetIcon(IDI_REC);

	m_BtnFrame.Create(_T("Static"), "", WS_CHILD|WS_VISIBLE|SS_ETCHEDFRAME,
		CRect(1, 80, 283, 113), this, IDC_STATIC);

	EnableToolTips(true);


	m_title = new CTitleText(this->GetSafeHwnd());

	OnFileClose();


	// Creating icon for the system tray
	m_TrayIcon.Create(this, WM_ICON_NOTIFY, "Stepvoice Recorder",
		LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_TRAY_STOP)), IDR_TRAY_MENU);
	UpdateTrayText();
	if(RegistryConfig::GetOption(_T("General\\Show icon in tray"), 0))
		m_TrayIcon.ShowIcon();

	// Setting up Monitoring
	const bool monEnabled = RegistryConfig::GetOption(_T("General\\Sound Monitor"), 0);
	m_StatWnd.m_btnMon.SetCheck(monEnabled);

	//if (m_conf.GetConfProg()->bMonitoring)
	//{
	//	m_bMonitoringBtn = false;
	//	OnBtnMonitoring();
	//}

	// Setting up Voice Activation System
	const bool vasEnabled = RegistryConfig::GetOption(_T("Tools\\VAS\\Enable"), 0);
	m_StatWnd.m_btnVas.SetCheck(vasEnabled);

	CMenu* pRecDeviceMenu = this->GetMenu()->GetSubMenu(2);
	ASSERT(pRecDeviceMenu);
	pRecDeviceMenu->RemoveMenu(0, MF_BYPOSITION); // Removing loopback device menu

	return 0;
}
#pragma optimize("", on)

/////////////////////////////////////////////////////////////////////////////
BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;

	cs.cx = 290;
	cs.cy = 120 + GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYMENU);
	
	int nScreenCX = GetSystemMetrics(SM_CXSCREEN);
	int nScreenCY = GetSystemMetrics(SM_CYSCREEN);
	cs.x  = RegistryConfig::GetOption(_T("General\\Xcoord"), 300);
	cs.y  = RegistryConfig::GetOption(_T("General\\Ycoord"), 200);
	if (cs.x >= (nScreenCX-40) || cs.y >= (nScreenCY-40))
	{
		cs.x  = (nScreenCX - cs.cx)/2;
		cs.y  = (nScreenCY - cs.cy)/2;
	}

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.lpszClass = "SvRec";

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
/*
/////////////////////////////////////////////////////////////////////////////
BOOL CMainFrame::ShowWindow()
{	
	///@note Called by MP3_RecorderApp

	int nCmdShow = (m_conf.GetConfDialGen()->bMinimized) ? SW_MINIMIZE : SW_SHOW;
	if (nCmdShow == SW_MINIMIZE && m_conf.GetConfDialGen()->bTrayMin)
	{
		nCmdShow = SW_HIDE;
		m_TrayIcon.ShowIcon();
	}

	return CFrameWnd::ShowWindow(nCmdShow);
}
*/
/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers
/////////////////////////////////////////////////////////////////////////////
LRESULT CMainFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if (message == WM_DISPLAYCHANGE)
		::SystemParametersInfo(SPI_GETWORKAREA, 0, &m_rDesktopRect, 0);

	return CFrameWnd::WindowProc(message, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if (RegistryConfig::GetOption(_T("General\\Easy Move"), 1))
	{
		CPoint pt;
		GetCursorPos(&pt);
		PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(pt.x, pt.y));
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
	m_destroyingMainWindow = true;
	CFrameWnd::OnDestroy();

	OnFileClose();
	MonitoringStop();
	BASS_Free();

	if (!IsIconic())
	{
		CRect r; GetWindowRect(&r);
		RegistryConfig::SetOption(_T("General\\Xcoord"), r.left);
		RegistryConfig::SetOption(_T("General\\Ycoord"), r.top);
	}
	//RegistryConfig::SetOption(_T("General\\Graph Type"), m_GraphWnd.GetDisplayMode());
	//RegistryConfig::SetOption(_T("General\\Show max peaks"), m_GraphWnd.MaxpeaksVisible());
	RegistryConfig::SetOption(_T("General\\Playback volume"), int(m_playback_volume * 10000));
}

//===========================================================================
// MENU : File
//===========================================================================
void CMainFrame::OnFileClose() 
{
	OnBtnSTOP();

	//Recording and playback parameters should be mutually exclusive.
	//But, calling OnFileClose with nothing opened, should not assert.

	if (!m_recordingFileName.IsEmpty())
	{
		m_recordingFileName.Empty();
	}
	if (m_bassPlaybackHandle != NULL)
	{
		BASS_StreamFree(m_bassPlaybackHandle);
		m_bassPlaybackHandle = 0;
	}

	// Setting the default window text: <no file> - Stepvoice Recorder
	CString strTitle, strNoFile((LPCSTR)IDS_NOFILE);
	AfxFormatString1(strTitle, IDS_FILETITLE, strNoFile);
	m_title->SetTitleText(strTitle);

	m_TimeWnd.Reset();
	UpdateStatWindow();
	UpdateTrayText();
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnFileClear() 
{
	if (m_recordingFileName.IsEmpty() && !m_bassPlaybackHandle)
		return;

	const CString lastFilePath = RegistryConfig::GetOption(_T("General\\LastFile"), CString());
	if (lastFilePath.IsEmpty())
		return;

	CString msgWarning;
	AfxFormatString1(msgWarning, IDS_CLEAR_ASK, lastFilePath);
	if (AfxMessageBox(msgWarning, MB_YESNO|MB_ICONWARNING) != IDYES)
		return;

	OnFileClose();
	try
	{
		CFile file(lastFilePath, CFile::modeCreate);
	}
	catch (CException *e)
	{
		e->ReportError();
		e->Delete();
		return;
	}
	OpenFile(lastFilePath);
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnFileDelete() 
{
	if (m_recordingFileName.IsEmpty() && !m_bassPlaybackHandle)
		return;

	const CString lastFilePath = RegistryConfig::GetOption(_T("General\\LastFile"), CString());
	if (lastFilePath.IsEmpty())
		return;

	//CString msgWarning;
	//AfxFormatString1(msgWarning, IDS_DEL_ASK, lastFilePath);
	//if (AfxMessageBox(msgWarning, MB_YESNO|MB_ICONWARNING) != IDYES)
	//	return;

	OnFileClose();

	TCHAR fromBuffer[MAX_PATH+2]; //String must be double-null terminated.
	ZeroMemory(fromBuffer, sizeof(fromBuffer));
	lstrcpy(fromBuffer, lastFilePath);

	//Removing file to RecycleBin.
	SHFILEOPSTRUCT fileOp;
	ZeroMemory(&fileOp, sizeof(fileOp));
	fileOp.wFunc = FO_DELETE;
	fileOp.pFrom = fromBuffer;
	fileOp.fFlags = FOF_ALLOWUNDO | FOF_SILENT;
	
	SHFileOperation(&fileOp);
}

//=======not from menu, but useful one :)===========================================
void CMainFrame::OpenFile(CString fileName)
{
	OnFileClose(); //updates m_recordingFileName and m_bassPlaybackHandle.

	if (Helpers::IsSuitableForRecording(fileName))
	{
		m_recordingFileName = fileName;
	}
	else
	{
		//WriteDbg() << __FUNCTION__ << " ::3 (BASS_Init)";

		BOOL result = BASS_Init(-1, 44100, 0, GetSafeHwnd(), NULL);
		if (!result)
		{
			//DWORD currentDeviceID = BASS_GetDevice();
			//bool debug = true;
			//Bass::ShowErrorFrom(_T("BASS_Init"));
			//return;
		}
		m_bassPlaybackHandle = BASS_StreamCreateFile(false, fileName, 0, 0, 0);
		if (!m_bassPlaybackHandle)
		{
			DWORD currentDeviceID = BASS_GetDevice();
			Bass::ShowErrorFrom(_T("BASS_StreamCreateFile"));
			BASS_Free();
			return;
		}
	}

	RegistryConfig::SetOption(_T("General\\LastFile"), fileName);

	/*
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

	if (Helpers::IsSuitableForRecording(str))
	{
		UINT l_open_flags = CFile::modeCreate | CFile::modeReadWrite |
			CFile::typeBinary | CFile::shareDenyWrite;
		if (!m_record_file.Open(str, l_open_flags, NULL))
			return;
	}
	else
	{
		if (BASS_GetDevice() == -1)
		{
			int deviceID = m_conf.GetConfDialGen()->nPlayDevice + 1; // BASS starts devices from 1
			BASS_Init(deviceID <= 0 ? -1 : deviceID, 44100, 0, GetSafeHwnd(), NULL);
		}

		m_bassPlaybackHandle = BASS_StreamCreateFile(false, str, 0, 0, 0);
		if (!m_bassPlaybackHandle)
		{
			// Not calling BASS_Free, because it can be used by monitoring
			//BASS_Free();
			return;
		}
	}
	*/

	// Modifying window caption to "<FILE> - StepVoice Recorder".

	const int slashPos = fileName.ReverseFind('\\');
	const CString shortName = (slashPos == -1) ? fileName
		: fileName.Right(fileName.GetLength() - slashPos - 1);

	CString newCaption;
	AfxFormatString1(newCaption, IDS_FILETITLE, shortName);
	m_title->SetTitleText(newCaption);
	
	UpdateStatWindow();
	UpdateTrayText();
}

//===========================================================================
void CMainFrame::OnFileFindfile() 
{
	const CString lastFilePath = RegistryConfig::GetOption(_T("General\\LastFile"), CString());
	ShellExecute(NULL, _T("open"), _T("explorer"), _T("/select, \"")+lastFilePath+_T("\""),
		NULL, SW_NORMAL);
}

//===========================================================================
// MENU : Sound
//===========================================================================
void CMainFrame::OnSoundBegin()
{
	double l_seconds_all = 0;

	if (m_bassPlaybackHandle)
	{
		l_seconds_all = BASS_ChannelBytes2Seconds(m_bassPlaybackHandle,
			BASS_ChannelGetLength(m_bassPlaybackHandle, BASS_POS_BYTE));

		// Moving to the beginning on file
		BOOL l_result = BASS_ChannelSetPosition(m_bassPlaybackHandle,
			BASS_ChannelSeconds2Bytes(m_bassPlaybackHandle, 0), BASS_POS_BYTE);
		ASSERT(l_result);

		m_TimeWnd.SetTime(0);
		m_SliderTime.SetCurPos(0);
	}

	UpdateWindowTitle_SeekTo(0.0, l_seconds_all, 1200);
}

//===========================================================================
void CMainFrame::OnSoundRew() 
{
	double l_allsec = 0;
	double l_cursec = 0;

	if (m_bassPlaybackHandle)
	{
		l_cursec = BASS_ChannelBytes2Seconds(m_bassPlaybackHandle,
			BASS_ChannelGetPosition(m_bassPlaybackHandle, BASS_POS_BYTE));
		l_allsec = BASS_ChannelBytes2Seconds(m_bassPlaybackHandle,
			BASS_ChannelGetLength(m_bassPlaybackHandle, BASS_POS_BYTE));

		l_cursec = ((l_cursec - 5) > 0) ? l_cursec - 5 : 0;

		BOOL l_result = BASS_ChannelSetPosition(
			m_bassPlaybackHandle,
			BASS_ChannelSeconds2Bytes(m_bassPlaybackHandle, l_cursec),
			BASS_POS_BYTE);
		ASSERT(l_result);

		m_TimeWnd.SetTime((UINT)l_cursec);
		m_SliderTime.SetCurPos(int(l_cursec/l_allsec*1000));
	}

	UpdateWindowTitle_SeekTo(l_cursec, l_allsec, 1200);
}

//===========================================================================
void CMainFrame::OnSoundFf() 
{
	double l_allsec = 0;
	double l_cursec = 0;

	if (m_bassPlaybackHandle)
	{
		l_cursec = BASS_ChannelBytes2Seconds(m_bassPlaybackHandle,
			BASS_ChannelGetPosition(m_bassPlaybackHandle, BASS_POS_BYTE));
		l_allsec = BASS_ChannelBytes2Seconds(m_bassPlaybackHandle,
			BASS_ChannelGetLength(m_bassPlaybackHandle, BASS_POS_BYTE));

		l_cursec = ((l_cursec + 5) < l_allsec) ? l_cursec + 5 : l_allsec;

		BOOL l_result = BASS_ChannelSetPosition(
			m_bassPlaybackHandle,
			BASS_ChannelSeconds2Bytes(m_bassPlaybackHandle, l_cursec),
			BASS_POS_BYTE);
		ASSERT(l_result);

		m_TimeWnd.SetTime((UINT)l_cursec);
		m_SliderTime.SetCurPos(int(l_cursec/l_allsec*1000));
	}

	UpdateWindowTitle_SeekTo(l_cursec, l_allsec, 1200);
}

//===========================================================================
void CMainFrame::OnSoundEnd() 
{
	double l_allsec = 0;

	if (m_bassPlaybackHandle)
	{
		l_allsec = BASS_ChannelBytes2Seconds(m_bassPlaybackHandle,
			BASS_ChannelGetLength(m_bassPlaybackHandle, BASS_POS_BYTE));

		BOOL l_result = BASS_ChannelSetPosition(
			m_bassPlaybackHandle,
			BASS_ChannelSeconds2Bytes(m_bassPlaybackHandle, l_allsec - 0.3),
			BASS_POS_BYTE);
		ASSERT(l_result);

		m_TimeWnd.SetTime((UINT)l_allsec);
		m_SliderTime.SetCurPos(1000);
	}

	UpdateWindowTitle_SeekTo(l_allsec, l_allsec, 1200);
}

//===========================================================================
// MENU : Options
//===========================================================================
void CMainFrame::OnStatPref() 
{
	//m_conf.GetConfProg()->nDialogIndex = -1; // Selecting tab sheet for display
	OnOptCom();
}

void CMainFrame::OnOptCom()
{
	CMySheet optDlg;
	if (optDlg.DoModal() != IDOK)
		return

	UpdateStatWindow();

	// Checking tray options

	if(RegistryConfig::GetOption(_T("General\\Show icon in tray"), 0))
		m_TrayIcon.ShowIcon();
	else
		m_TrayIcon.HideIcon();

	//Updating GUI and filter with new VAS parameters.

	const bool isEnabled = RegistryConfig::GetOption(_T("Tools\\VAS\\Enable"), 0);
	if (isEnabled && m_StatWnd.m_btnVas.IsChecked())
	{
		//Button already pressed and VAS is running. Its
		//parameters could be updated in options dialog.
		//We should update VAS filter (from OnBtnVas handler).
		m_StatWnd.m_btnVas.SetCheck(false);
		m_StatWnd.m_btnVas.SetCheck(true);
	}
	else
		m_StatWnd.m_btnVas.SetCheck(isEnabled);
}

//===========================================================================
void CMainFrame::OnOptEm() 
{
	//Reverting feature of dragging window from any point
	int curEM = RegistryConfig::GetOption(_T("General\\Easy Move"), 1);
	RegistryConfig::SetOption(_T("General\\Easy Move"), curEM ? 0 : 1);
}

//===========================================================================
void CMainFrame::OnOptTop() 
{
	//Reverting feature
	int curOnTop = RegistryConfig::GetOption(_T("General\\Always on Top"), 0);
	curOnTop = curOnTop ? 0 : 1;
	RegistryConfig::SetOption(_T("General\\Always on Top"), curOnTop);

	const CWnd* pWndType = curOnTop ? &wndTopMost : &wndNoTopMost;
	SetWindowPos(pWndType, 0, 0, 0, 0, SWP_FRAMECHANGED|SWP_NOSIZE|SWP_NOMOVE);
}

//===========================================================================
// BUTTONS
//===========================================================================

void CMainFrame::OnBtnOPEN()
{
	const CString generatedName = Helpers::GetMp3AutonameFromConfig();
	
	const CString lastFileName = RegistryConfig::GetOption(_T("General\\LastFile"), CString());
	const int slashPos = lastFileName.ReverseFind(_T('\\'));
	const CString lastFilePath = (slashPos != -1) ? lastFileName.Left(slashPos) : CString();

	const CString outputFolder = RegistryConfig::GetOption(_T("General\\OutputFolder"), CString());
	const bool storeInOutputFolder = RegistryConfig::GetOption(_T("General\\StoreInOutputFolder"), 0);

	// Creating a standard file open dialog

	CString dialogTitle, filesFilter;
	dialogTitle.LoadString(IDS_FILENEWOPENTITLE);
	filesFilter.LoadString(IDS_FILEFILTER);

	CFileDialog openDialog(true, _T("mp3"), generatedName, OFN_HIDEREADONLY | OFN_EXPLORER, filesFilter);
	openDialog.m_ofn.lpstrTitle= dialogTitle;
	openDialog.m_ofn.lpstrInitialDir = storeInOutputFolder ? outputFolder : lastFilePath;

	if (openDialog.DoModal() == IDOK)
	{	
		OpenFile(openDialog.GetPathName());
		return;
	}
}

//===========================================================================
void CMainFrame::OnBtnPLAY()
{
	if (!this->CanPlay())
		return;

	if (!m_bassPlaybackHandle)
	{
		OnBtnOPEN();
		if (!m_bassPlaybackHandle)
			return;
	}

	MonitoringStop();

	BASS_ChannelSetAttribute(m_bassPlaybackHandle, BASS_ATTRIB_VOL, m_playback_volume);
	const DWORD CHANNEL_STATE = BASS_ChannelIsActive(m_bassPlaybackHandle);
	switch (CHANNEL_STATE)
	{
	case BASS_ACTIVE_PLAYING:
		BASS_ChannelPause(m_bassPlaybackHandle);
		KillTimer(1);
		m_nState = PAUSEPLAY_STATE;
		break;

	case BASS_ACTIVE_STOPPED:
	case BASS_ACTIVE_PAUSED:
		BASS_ChannelPlay(m_bassPlaybackHandle, false);
		SetTimer(1, 1000, NULL);
		m_nState = PLAY_STATE;
		break;
	}
}

//===========================================================================
void CMainFrame::OnBtnSTOP()
{
	if (m_nState == STOP_STATE)
		return;

	//Setting stop state in the method beginning, to avoid extra OnBtnSTOP
	//calls from OpenFile below (after recording finished).
	m_nState = STOP_STATE;

	if (m_bassPlaybackHandle)
	{
		ASSERT(m_recordingFileName.IsEmpty());
		KillTimer(1);

		BOOL result = BASS_ChannelStop(m_bassPlaybackHandle);
		ASSERT(result);
		result = BASS_ChannelSetPosition(m_bassPlaybackHandle,
			BASS_ChannelSeconds2Bytes(m_bassPlaybackHandle, 0), BASS_POS_BYTE);
		ASSERT(result);
	}

	if (!m_recordingChain.IsEmpty())
	{
		ASSERT(!m_recordingFileName.IsEmpty());
		KillTimer(2);
		m_GraphWnd.StopUpdate(); //need it here, before recorder chain is destroyed

		m_recordingChain.GetFilter<IWasapiRecorder>()->Stop();
		m_recordingChain.GetFilter<FileWriter>()->ForceClose();
		m_recordingChain.GetFilter<CEncoder_MP3>()->WriteVBRHeader(m_recordingFileName);
		m_recordingChain.Empty();

		OpenFile(m_recordingFileName);
	}

	//TODO: refactor. This thing resets playback time in time wnd.
	PostMessage(WM_HSCROLL,  MAKEWPARAM(SB_THUMBPOSITION, 0),
		(LPARAM)m_SliderTime.m_hWnd);

	//if (m_monitoringChain.IsEmpty()) //Removes multiple restart on stop processing.
		MonitoringRestart();

	/*
	m_nState = STOP_STATE;
	if (m_bassPlaybackHandle)
	{
		KillTimer(1);
		BOOL l_result = BASS_ChannelStop(m_bassPlaybackHandle);
		ASSERT(l_result);

		l_result = BASS_ChannelSetPosition(m_bassPlaybackHandle,
			BASS_ChannelSeconds2Bytes(m_bassPlaybackHandle, 0), BASS_POS_BYTE);
		ASSERT(l_result);
	}
	if (g_record_handle)
	{
		KillTimer(2);
		BOOL l_result = BASS_ChannelStop(g_record_handle);
		ASSERT(l_result);

		l_result = BASS_ChannelStop(g_loopback_handle);
		ASSERT(l_result);

		g_loopback_handle = 0;
		g_record_handle = 0;
		m_loopback_hdsp = 0;
		m_mute_hdsp = 0;
		BASS_Free();
		
		BASS_RecordFree();

		SAFE_DELETE(m_vista_loopback);
		SAFE_DELETE(m_visualization_data);

		CString l_recorded_file = m_record_file.GetFilePath();
		m_record_file.Flush();
		m_record_file.Close();

		m_pEncoder->WriteVBRHeader(l_recorded_file);
		SAFE_DELETE(m_pEncoder);
		OpenFile(l_recorded_file);
	}

	PostMessage(WM_HSCROLL,  MAKEWPARAM(SB_THUMBPOSITION, 0),
		(LPARAM)m_pMainFrame->m_SliderTime.m_hWnd);

	if (m_bMonitoringBtn)
		MonitoringStart();
	*/
}

//===========================================================================
void CMainFrame::OnBtnREC()
{
	if (m_recordingFileName.IsEmpty())
	{
		OnBtnOPEN();
		if (m_recordingFileName.IsEmpty())
			return;
	}

	MonitoringStop();

	if (m_recordingChain.IsEmpty())
	{
		const bool vasEnabled = RegistryConfig::GetOption(_T("Tools\\VAS\\Enable"), 0);
		const int vasThresholdDB = RegistryConfig::GetOption(_T("Tools\\VAS\\Threshold"), -30);
		const int vasWaitTimeMS = RegistryConfig::GetOption(_T("Tools\\VAS\\WaitTime"), 2000);

		int bitrate = RegistryConfig::GetOption(_T("File types\\MP3\\Bitrate"), 128);
		int frequency = RegistryConfig::GetOption(_T("File types\\MP3\\Freq"), 44100);
		int channels = RegistryConfig::GetOption(_T("File types\\MP3\\Stereo"), 1) + 1;

		WasapiHelpers::DevicesArray selectedDevices = CRecordingSourceDlg::GetInstance()->GetSelectedDevices();
		CWasapiRecorderMulti* recorder = new CWasapiRecorderMulti(selectedDevices, frequency, channels);
		//CWasapiRecorder* recorder = new CWasapiRecorder(selectedDevices[0].first, frequency, channels);
		recorder->SetVolume(m_recording_volume);

		frequency = recorder->GetActualFrequency();
		channels = recorder->GetActualChannelCount();

		m_recordingChain.AddFilter(recorder);
		m_recordingChain.AddFilter(new VasFilter(vasThresholdDB, vasWaitTimeMS, vasEnabled));
		m_recordingChain.AddFilter(new CEncoder_MP3(bitrate, frequency, channels));
		m_recordingChain.AddFilter(new FileWriter(m_recordingFileName));
		//m_recordingChain.AddFilter(new FileWriterWAV(m_recordingFileName, frequency, channels));
	}

	IWasapiRecorder* recorder = m_recordingChain.GetFilter<IWasapiRecorder>();
	if (recorder->IsStarted())
	{
		recorder->Pause();
		KillTimer(2);
		m_nState = PAUSEREC_STATE;
	}
	else if (recorder->IsPaused() || recorder->IsStopped())
	{
		m_recordingChain.GetFilter<VasFilter>()->ResetDetection();
		recorder->Start();
		SetTimer(2, 1000, NULL);
		m_nState = RECORD_STATE;
	}
}

////////////////////////////////////////////////////////////////////////////////
// WM_TIMER message handler
////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnTimer(UINT nIDEvent) 
{
	CFrameWnd::OnTimer(nIDEvent);

	double curSeconds = 0.0;
	double allSeconds = 0.0;

	if (nIDEvent == 1)
	{
		ASSERT(m_bassPlaybackHandle != NULL);

		curSeconds = BASS_ChannelBytes2Seconds(m_bassPlaybackHandle,
			BASS_ChannelGetPosition(m_bassPlaybackHandle, BASS_POS_BYTE));
		allSeconds = BASS_ChannelBytes2Seconds(m_bassPlaybackHandle,
			BASS_ChannelGetLength(m_bassPlaybackHandle, BASS_POS_BYTE));

		//Autostop on file end.
		if (BASS_ChannelIsActive(m_bassPlaybackHandle) == BASS_ACTIVE_STOPPED)
			PostMessage(WM_COMMAND, IDB_BTNSTOP, LONG(this->m_BtnSTOP.m_hWnd));
	}
	else if (nIDEvent == 2)
	{
		ASSERT(!m_recordingChain.IsEmpty());
		const int kbitSec = RegistryConfig::GetOption(_T("File types\\MP3\\Bitrate"), 128);
		const int bytesSec = kbitSec * (1000/8);
		const ULONGLONG fileSize = m_recordingChain.GetFilter<FileWriter>()->GetFileLength();

		curSeconds = double(fileSize / bytesSec);
		allSeconds = curSeconds;
	}

	if (!m_SliderTime.IsDragging())
		m_SliderTime.SetCurPos(int(curSeconds/allSeconds * 1000));

	m_TimeWnd.SetTime((UINT)curSeconds);
	UpdateStatWindow();

	/*
	HSTREAM l_handle = 0;
	switch (nIDEvent)
	{
	case 1:
		l_handle = m_bassPlaybackHandle;
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
	*/
}

////////////////////////////////////////////////////////////////////////////////
void CMainFrame::UpdateStatWindow()
{
	int bitrate = RegistryConfig::GetOption(_T("File types\\MP3\\Bitrate"), 128);
	int frequency = RegistryConfig::GetOption(_T("File types\\MP3\\Freq"), 44100);
	int stereo = RegistryConfig::GetOption(_T("File types\\MP3\\Stereo"), 1);
	ULONGLONG fileSize = 0;
	double fileSeconds = 0.0;

	if (m_bassPlaybackHandle != NULL)
	{
		BASS_CHANNELINFO channelInfo;
		BASS_ChannelGetInfo(m_bassPlaybackHandle, &channelInfo);

		fileSize = BASS_StreamGetFilePosition(m_bassPlaybackHandle, BASS_FILEPOS_END);
		fileSeconds = BASS_ChannelBytes2Seconds(m_bassPlaybackHandle,
			BASS_ChannelGetLength(m_bassPlaybackHandle, BASS_POS_BYTE));

		bitrate = int(fileSize / (125*fileSeconds) + 0.5); //to kbit/sec
		frequency = channelInfo.freq;
		stereo = (channelInfo.chans > 1) ? 1 : 0;
	}
	else if (!m_recordingChain.IsEmpty())
	{
		IWasapiRecorder* recorder = m_recordingChain.GetFilter<IWasapiRecorder>();
		frequency = recorder->GetActualFrequency();
		stereo = recorder->GetActualChannelCount() > 1;

		const int bytesSec = bitrate * (1000/8); //bitrate is kbit/sec
		fileSize = m_recordingChain.GetFilter<FileWriter>()->GetFileLength();
		fileSeconds = double(fileSize / bytesSec);
	}

	const CString strFileSeconds = Helpers::ToString_HMMSS(fileSeconds);
	m_StatWnd.Set(frequency, bitrate, stereo);
	m_StatWnd.Set(fileSize/1024, strFileSeconds);

	/*
	int l_stream_rate = m_conf.GetConfDialMp3()->nBitrate;
	int l_stream_freq = m_conf.GetConfDialMp3()->nFreq;
	int l_stream_mode = m_conf.GetConfDialMp3()->nStereo;

	QWORD l_size_bytes = 0;
	double l_size_seconds = 0;

	if (m_bassPlaybackHandle)
	{
		BASS_CHANNELINFO l_channel_info;
		BASS_ChannelGetInfo(m_bassPlaybackHandle, &l_channel_info);

		l_size_bytes = BASS_StreamGetFilePosition(m_bassPlaybackHandle,
			BASS_FILEPOS_END);

		l_size_seconds = BASS_ChannelBytes2Seconds(m_bassPlaybackHandle,
			BASS_ChannelGetLength(m_bassPlaybackHandle, BASS_POS_BYTE));

		l_stream_rate = int(l_size_bytes / (125* l_size_seconds) + 0.5); //Kbps
		l_stream_freq = l_channel_info.freq;
		l_stream_mode = (l_channel_info.chans > 1) ? 1 : 0;
	}
	else if (g_record_handle)
	{
		l_size_bytes   = (QWORD)m_record_file.GetLength();
		l_size_seconds = (double)l_size_bytes / (l_stream_rate * 125);
	}

	const CString allSeconds = Helpers::ToString_HMMSS(l_size_seconds);
	m_StatWnd.Set(l_stream_freq, l_stream_rate, l_stream_mode);
	m_StatWnd.Set((UINT)l_size_bytes/1024, allSeconds);
	*/
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
	pCmdUI->Enable(!IsPlaying(m_nState));
}

//------------------------------------------------------------------------------
void CMainFrame::OnUpdateSoundPlay(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(!IsRecording(m_nState));
}

//------------------------------------------------------------------------------
void CMainFrame::OnUpdateOptTop(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(RegistryConfig::GetOption(_T("General\\Always on Top"), 0));
}

//------------------------------------------------------------------------------
void CMainFrame::OnUpdateOptEm(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(RegistryConfig::GetOption(_T("General\\Easy Move"), 1));
}

//===========================================================================
// ACCELERATORS
//===========================================================================
void CMainFrame::OnSoundRecA() 
{
	if (CanRecord())
	{
		m_BtnREC.Press();
		OnBtnREC();
	}
}

//===========================================================================
void CMainFrame::OnSoundPlayA() 
{
	if (CanPlay())
	{
		m_BtnPLAY.Press();
		OnBtnPLAY();
	}
}

//===========================================================================
// Drag-and-Drop support
//===========================================================================
void CMainFrame::OnDropFiles(HDROP hDropInfo) 
{
	this->SetForegroundWindow();

	TCHAR szFileName[_MAX_PATH];
	::DragQueryFile(hDropInfo, 0, szFileName, _MAX_PATH);

	OpenFile(CString(szFileName));

	::DragFinish(hDropInfo);
}
/*
////////////////////////////////////////////////////////////////////////////////
CString CMainFrame::GetAutoName( CString& strPattern )
{
	// Removing all '%' symbols from the end of string
	if( strPattern.Right(1) == "%")
		strPattern.TrimRight('%');

	CTime t = CTime::GetCurrentTime();
	CString s = t.Format( m_conf.GetConfDialAN()->strAutoName );
	s += ".mp3";

	if( s == ".mp3" )
		s = "Empty.mp3";

	return s;
}
*/
/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	OnStatPref();
	CFrameWnd::OnLButtonDblClk(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnRButtonUp(UINT nFlags, CPoint point) 
{
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
	LPCSTR l_old_exec = "rundll32.exe MMSYS.CPL,ShowAudioPropertySheet";
	LPCSTR l_new_exec = "control MMSYS.CPL";

	CMP3_RecorderApp* l_app = (CMP3_RecorderApp* )AfxGetApp();
	WinExec(l_app->IsVistaOS() ? l_new_exec : l_old_exec, SW_SHOW);
}

/////////////////////////////////////////////////////////////////////////////
BOOL CMainFrame::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
	//if(!m_conf.GetConfDialGen()->bToolTips)
	//	return true;

	// need to handle both ANSI and UNICODE versions of the message
	TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
	TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;
	CString strTipText;

	// Getting an element ID and a string from it
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
		case IDS_SLIDERTIME:	nID = IDS_TT_SEEKBAR; break;
		case IDW_GRAPH:			nID = IDS_TT_WAVEWND; break;
		case IDW_TIME:			nID = IDS_TT_TIMEWND; break;
		case IDW_STAT:			nID = IDS_TT_STATWND; break;
		case IDB_MIX_SEL:		nID = IDS_TT_MIXSEL; break;
		case IDS_SLIDERVOL:		nID = IDS_TT_VOLBAR; break;
		case IDB_BTN_VAS:		nID = IDS_TT_VAS;	break;
		case IDB_BTN_MON:		nID = IDS_TT_MONITORING;break;
		default:
			return true;
		}
		strTipText = CString((LPCSTR)nID);
	}

	// Copying hint string into the structure
	if (pNMHDR->code == TTN_NEEDTEXTA)
	  lstrcpyn(pTTTA->szText, strTipText, sizeof(pTTTA->szText));
	else
	  _mbstowcsz(pTTTW->szText, strTipText, sizeof(pTTTW->szText));
	*pResult = 0;

	return true;
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

	const bool showIconInTray = RegistryConfig::GetOption(_T("General\\Show icon in tray"), 0);
	const bool minimizeToTray = RegistryConfig::GetOption(_T("General\\Minimize to tray"), 0);
	if (nType==SIZE_MINIMIZED && minimizeToTray)
	{
		if (!showIconInTray)
			m_TrayIcon.ShowIcon();
		ShowWindow(SW_HIDE);
	}
	else if (nType==SIZE_RESTORED && !showIconInTray)
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

			const bool minimizeToTray = RegistryConfig::GetOption(_T("General\\Minimize to tray"), 0);
			if (nCmdShow == SW_MINIMIZE && minimizeToTray)
				nCmdShow = SW_HIDE;

			ShowWindow(nCmdShow);
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
		{
			CWnd* wndFound = this->FindWindow(NULL, CMySheet::GetWindowTitle());
			if (wndFound != NULL)
			{
				CMySheet* optionsDialog = dynamic_cast<CMySheet*>(wndFound);
				optionsDialog->SendMessage(WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
			}
		}
		PostMessage(WM_CLOSE);
		break;
	}
		
	return CFrameWnd::OnCommand(wParam, lParam);
}

void CMainFrame::OnUpdateTrayPlay(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(!IsRecording(m_nState));
}

void CMainFrame::OnUpdateTrayRec(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(!IsPlaying(m_nState));
}

/////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnBtnMIX_SEL()
{
	CRect r;
	m_BtnMIX_SEL.GetWindowRect(&r);
	CRecordingSourceDlg::GetInstance()->Execute(CPoint(r.right, r.top));
}

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

	if (m_bassPlaybackHandle)
	{
		l_seconds_all = BASS_ChannelBytes2Seconds(m_bassPlaybackHandle,
			BASS_ChannelGetLength(m_bassPlaybackHandle, BASS_POS_BYTE));
		l_seconds_pos = l_seconds_all * nPos / 1000;
	}

	if (SB_THUMBTRACK == nSBCode)
	{
		UpdateWindowTitle_SeekTo(l_seconds_pos, l_seconds_all, 10000);
	}
	else if (SB_THUMBPOSITION == nSBCode)
	{
		m_title->Restore();
		if (m_bassPlaybackHandle)
		{
			if (l_seconds_pos >= l_seconds_all - 0.3)
			{
				l_seconds_pos = max(l_seconds_all - 0.3, 0);
			}
			BOOL l_result = BASS_ChannelSetPosition(
				m_bassPlaybackHandle,
				BASS_ChannelSeconds2Bytes(m_bassPlaybackHandle, l_seconds_pos),
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

//------------------------------------------------------------------------------
void CMainFrame::ProcessSliderVol(UINT nSBCode, UINT nPos)
{

   // Get the minimum and maximum scroll-bar positions.
   int minpos, maxpos, curpos;
   m_SliderVol.GetRange(minpos, maxpos);
   curpos = m_SliderVol.GetPos();

	// Informing user
	const int nPercent = int(100.0 * curpos / (maxpos - minpos));

	CString strTitle;
	if (!m_recordingFileName.IsEmpty() || m_nState == STOP_STATE)
	{
		strTitle.Format(IDS_VOLUME_TITLE, nPercent, CString(_T("recording")));
		m_recording_volume = (float)nPercent / 100;
		if (!m_recordingChain.IsEmpty())
			m_recordingChain.GetFilter<IWasapiRecorder>()->SetVolume(m_recording_volume);
		if (!m_monitoringChain.IsEmpty())
			m_monitoringChain.GetFilter<IWasapiRecorder>()->SetVolume(m_recording_volume);
	}
	else if (m_bassPlaybackHandle != NULL)
	{
		strTitle.Format(IDS_VOLUME_TITLE, nPercent, CString(_T("playback")));
		m_playback_volume = (float)nPercent / 100;
		BASS_ChannelSetAttribute(m_bassPlaybackHandle, BASS_ATTRIB_VOL, m_playback_volume);
	}

	// Processing slider messages
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
		m_title->SetTitleText(strTitle, 1200);
		break;
	}
}

//------------------------------------------------------------------------------
BOOL CMainFrame::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
{
	(zDelta > 0) ?  OnVolUpA() : OnVolDownA();

	return CFrameWnd::OnMouseWheel(nFlags, zDelta, pt);
}

//------------------------------------------------------------------------------
void CMainFrame::OnVolUpA() 
{
	if (m_SliderVol.IsWindowEnabled())
	{
		int nPos = min(m_SliderVol.GetRangeMax(),
			m_SliderVol.GetPos() + m_SliderVol.GetPageSize());
		m_SliderVol.SetPos(nPos);

		this->PostMessage(WM_HSCROLL, MAKEWPARAM(0, SB_PAGERIGHT),
			(LPARAM)m_SliderVol.GetSafeHwnd());
	}
}

//------------------------------------------------------------------------------
void CMainFrame::OnVolDownA()
{
	if (m_SliderVol.IsWindowEnabled())
	{
		int nPos = max(m_SliderVol.GetRangeMin(),
			m_SliderVol.GetPos() - m_SliderVol.GetPageSize());
		m_SliderVol.SetPos(nPos);

		this->PostMessage(WM_HSCROLL, MAKEWPARAM(0, SB_PAGELEFT),
			(LPARAM)m_SliderVol.GetSafeHwnd());
	}
}
//------------------------------------------------------------------------------

void CMainFrame::OnBtnMonitoring()
{
#ifndef _DEBUG
	// Button is disabled after the trial period is over
	if(fsProtect_GetDaysLeft() <= 0)
		return;
#endif

	const bool monitoringButtonChecked = m_StatWnd.m_btnMon.IsChecked();
	RegistryConfig::SetOption(_T("General\\Sound Monitor"), monitoringButtonChecked);

	if (!monitoringButtonChecked)
	{
		MonitoringStop();
	}
	else if ((m_nState == STOP_STATE) && !MonitoringStart())
	{
		AfxMessageBox("Monitoring error!", MB_OK);
		m_StatWnd.m_btnMon.SetCheck(false);
	}
}
//------------------------------------------------------------------------------

//bool CMainFrame::IsMonitoringOnly()
//{
//	return m_bMonitoringBtn && (m_nState == STOP_STATE);
//}
//------------------------------------------------------------------------------

bool CMainFrame::MonitoringRestart()
{
	if (m_StatWnd.m_btnMon.IsChecked() && m_nState == STOP_STATE && !m_destroyingMainWindow)
	{
		MonitoringStop();
		return MonitoringStart();
	}
	return false;
}
//------------------------------------------------------------------------------

bool CMainFrame::MonitoringStart()
{
	ASSERT(m_monitoringChain.IsEmpty());
	if (!m_monitoringChain.IsEmpty())
		MonitoringStop();

	WasapiHelpers::DevicesArray selectedDevices = CRecordingSourceDlg::GetInstance()->GetSelectedDevices();
	CWasapiRecorderMulti* recorder = new CWasapiRecorderMulti(selectedDevices, 44100, 2);
	//CWasapiRecorder* recorder = new CWasapiRecorder(selectedDevices[0].first, 44100, 2);
	recorder->SetVolume(m_recording_volume);

	m_monitoringChain.AddFilter(recorder);
	if (recorder->Start())
	{
		m_GraphWnd.StartUpdate(PeaksCallback_Wasapi, LinesCallback_Wasapi, recorder);
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------
void CMainFrame::MonitoringStop()
{
	if (!m_monitoringChain.IsEmpty())
	{
		m_GraphWnd.StopUpdate();
		m_monitoringChain.GetFilter<IWasapiRecorder>()->Stop();
		m_monitoringChain.Empty();
	}


	/*
	if (g_monitoring_handle)
	{
		m_GraphWnd.StopUpdate();

		BOOL l_result = BASS_ChannelStop(g_monitoring_handle);
		ASSERT(l_result);
		l_result = BASS_ChannelStop(g_loopback_handle);
		ASSERT(l_result);

		SAFE_DELETE(m_vista_loopback);
		SAFE_DELETE(m_visualization_data);
		g_loopback_handle = 0;
		g_monitoring_handle = 0;
		m_loopback_hdsp = 0;
		m_mute_hdsp = 0;

		if (!m_bassPlaybackHandle && !g_loopback_handle)
			BASS_Free();
		BASS_RecordFree();
	}
	*/
}
//------------------------------------------------------------------------------

void CMainFrame::OnBtnVas()
{
#ifndef _DEBUG
	// Button is disabled after the trial period is over
	if(fsProtect_GetDaysLeft() <= 0)
		return;
#endif

	const bool vasEnabled = m_StatWnd.m_btnVas.IsChecked();
	RegistryConfig::SetOption(_T("Tools\\VAS\\Enable"), vasEnabled);

	const int vasThresholdDB = RegistryConfig::GetOption(_T("Tools\\VAS\\Threshold"), -30);
	const int vasDurationMS = RegistryConfig::GetOption(_T("Tools\\VAS\\WaitTime"), 2000);

	m_GraphWnd.ShowVASMark(vasEnabled, vasThresholdDB);
	if (!m_recordingChain.IsEmpty())
	{
		m_recordingChain.GetFilter<VasFilter>()->Enable(vasEnabled);
		m_recordingChain.GetFilter<VasFilter>()->SetTreshold(vasThresholdDB);
		m_recordingChain.GetFilter<VasFilter>()->SetDuration(vasDurationMS);
	}
}

////////////////////////////////////////////////////////////////////////////////
void CMainFrame::UpdateButtonState(UINT nID)
{
	CButton* pBtn = (CButton *)GetDlgItem(nID);
	ASSERT(pBtn);

	pBtn->ModifyStyle(WS_DISABLED, 0);	// Resetting button state
	if (IDB_BTNREC == nID)
	{
		if (IsPlaying(m_nState))
			pBtn->ModifyStyle(0, WS_DISABLED);
	}
	else if (IDB_BTNPLAY == nID)
	{
		if (IsRecording(m_nState))
			pBtn->ModifyStyle(0, WS_DISABLED);
	}
	pBtn->Invalidate(false);
}

////////////////////////////////////////////////////////////////////////////////
void CMainFrame::UpdateInterface()
{
	static int g_currentState = -1;
	if (g_currentState == m_nState)
		return;

	switch (m_nState)
	{
	case PLAY_STATE:
		///@bug Testing new functionality
		//g_update_handle = m_bassPlaybackHandle;
		m_GraphWnd.StartUpdate(PeaksCallback, LinesCallback, this);

		m_BtnPLAY.SetIcon(IDI_PAUSE);
		m_TrayIcon.SetIcon(IDI_TRAY_PLAY);
		m_IcoWnd.SetNewIcon(ICON_PLAY);
		m_BtnMIX_SEL.SetIcon(IDI_MIXLINE);

		if (!m_SliderVol.IsDragging())
			UpdateVolumeSlider(m_SliderVol, m_playback_volume);
		break;

	case PAUSEPLAY_STATE:
		m_GraphWnd.StopUpdate();
		m_BtnPLAY.SetIcon(IDI_PLAY);
		m_TrayIcon.SetIcon(IDI_TRAY_PAUSE);
		m_IcoWnd.SetNewIcon(ICON_PAUSEP);
		break;

	case RECORD_STATE:
		m_GraphWnd.StartUpdate(PeaksCallback_Wasapi, LinesCallback_Wasapi,
			m_recordingChain.GetFilter<IWasapiRecorder>());

		///@bug Testing new functionality
		//g_update_handle = g_record_handle;
		//m_GraphWnd.StartUpdate(PeaksCallback, LinesCallback, this);

		m_BtnREC.SetIcon(IDI_PAUSE);
		m_TrayIcon.SetIcon(IDI_TRAY_REC);
		m_IcoWnd.SetNewIcon(ICON_REC);
		break;

	case PAUSEREC_STATE:
		m_GraphWnd.StopUpdate();
		m_BtnREC.SetIcon(IDI_REC);
		m_TrayIcon.SetIcon(IDI_TRAY_PAUSE);
		m_IcoWnd.SetNewIcon(ICON_PAUSER);
		break;

	case STOP_STATE:
	default:
		if (!m_StatWnd.m_btnMon.IsChecked())
			m_GraphWnd.StopUpdate();

		m_SliderTime.SetCurPos(0);
		m_BtnREC.SetIcon(IDI_REC);
		m_BtnPLAY.SetIcon(IDI_PLAY);
		m_TrayIcon.SetIcon(IDI_TRAY_STOP);
		m_IcoWnd.SetNewIcon(ICON_STOP);
		m_BtnMIX_SEL.SetIcon(IDI_MIXLINE03);

		if (!m_SliderVol.IsDragging())
			UpdateVolumeSlider(m_SliderVol, m_recording_volume);

		UpdateStatWindow();
		break;
	}

	UpdateButtonState(IDB_BTNPLAY);
	UpdateButtonState(IDB_BTNREC);
	g_currentState = m_nState;
}
//------------------------------------------------------------------------------

void CMainFrame::UpdateVolumeSlider(CSliderVol& slider, float volumeLevel)
{
	const int rangeMin = slider.GetRangeMin();
	const int rangeMax = slider.GetRangeMax();

	ASSERT(volumeLevel >= 0 && volumeLevel <= 1);
	const int volumePos = rangeMin + (rangeMax-rangeMin) * volumeLevel;
	slider.SetPos(volumePos);
}
//------------------------------------------------------------------------------

void CMainFrame::UpdateWindowTitle_SeekTo(
	double curSeconds, double allSeconds, int displayTimeMs)
{
	const int percent = int(curSeconds/allSeconds)*100;
	const CString strCurSec = Helpers::ToString_HMMSS(curSeconds);
	const CString strAllSec = Helpers::ToString_HMMSS(allSeconds);

	CString title;
	title.Format(IDS_SEEKTO, strCurSec, strAllSec, percent);
	m_title->SetTitleText(title, displayTimeMs);
}
//------------------------------------------------------------------------------

CString CMainFrame::ParseFileName(CString a_file_name)
{
	// Writing real values instead of patterns

	TCHAR l_path[2 * MAX_PATH] = {0};
	std::map<CString, CString> l_patterns;

	CString l_auto_name = Helpers::GetMp3AutonameFromConfig();
	l_auto_name.Replace(_T(".mp3"), _T(""));	// Removing extension from name
	l_patterns[_T("{Autoname}")] = l_auto_name;

	SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT, l_path);
	l_patterns[_T("{Desktop}")] = l_path;
	SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS , NULL, SHGFP_TYPE_CURRENT, l_path);
	l_patterns[_T("{MyDocuments}")] = l_path;

	std::map<CString, CString>::iterator i;
	for (i = l_patterns.begin(); i != l_patterns.end(); i++)
	{
		std::pair<CString, CString> l_pair = *i;
		if (a_file_name.Find(l_pair.first) != -1)
		{
			a_file_name.Replace(l_pair.first, l_pair.second);
		}
	}

	a_file_name.Replace(_T('/'), _T('\\'));
	return a_file_name;
}

////////////////////////////////////////////////////////////////////////////////
/*
void CALLBACK CMainFrame::LoopbackStreamDSP(HDSP a_handle, DWORD a_channel,
	void *a_buffer, DWORD a_length, void *a_user)
{
	// Loopback is a decoding stream and have same parameters (freq, channels)
	// as a main recording stream, for direct copying.

	//1. Fill buffer with required length from Loopback stream (a_user)
	const int BUFFER_LENGTH = 256 * 1024;
	ASSERT(a_length <= BUFFER_LENGTH);
	ASSERT(a_user);

	static char l_src_buffer[BUFFER_LENGTH] = {0}; //256k buffer
	HSTREAM l_src_stream = *((HSTREAM*)a_user);

	DWORD l_length = BASS_ChannelGetData(l_src_stream, l_src_buffer, a_length);
	if (l_length == -1)
	{
		int l_error_code = BASS_ErrorGetCode();
		return;
	}
	ASSERT(l_length == a_length);

	//2. Replace data in the recording buffer (assuming we have float samples).
	float* l_dst_buffer = (float*)a_buffer;
	float* l_src_buffer_float = (float*)l_src_buffer;
	for (int i = 0; i < a_length / sizeof(float); i++)
		l_dst_buffer[i] = max(-1.0, min((l_dst_buffer[i] + l_src_buffer_float[i]), 1.0));

	/*
	char* l_dst_buffer = (char*)a_buffer;
	for (DWORD i = 0; i < a_length; i++)
		l_dst_buffer[i] = l_src_buffer[i];
	*/
//}
//*/

//------------------------------------------------------------------------------
bool CMainFrame::CanPlay() const
{
	return m_recordingFileName.IsEmpty();
	//return (m_record_file.m_hFile == CFile::hFileNull);
}

//------------------------------------------------------------------------------
bool CMainFrame::CanRecord() const
{
	return !m_recordingFileName.IsEmpty();
	//return BASS_ChannelIsActive(m_bassPlaybackHandle)==BASS_ACTIVE_STOPPED;
}
//------------------------------------------------------------------------------

LRESULT CMainFrame::OnRecSourceDialogClosed(WPARAM wParam, LPARAM lParam)
{
	MonitoringRestart();

	/*
	FilterChain chain(NULL, NULL);
	chain.AddFilter(new FileWriter(_T("d:\\test.mp3")));
	chain.AddFilter(new CEncoder_MP3(128, 44100, 2));

	FileWriter* curWriter = chain.GetFilter<FileWriter>();
	FileWriterWAV* curWriterWav = chain.GetFilter<FileWriterWAV>();
	CEncoder_MP3* curEncoder = chain.GetFilter<CEncoder_MP3>();
	CWasapiRecorder* curRecorder = chain.GetFilter<CWasapiRecorder>();

	bool debug = true;
	*/

	//NEW version, commented.
	/*
	OutputDebugString(__FUNCTION__"\n");
	m_GraphWnd.StopUpdate();

	WasapiHelpers::DevicesArray selectedDevices = CRecordingSourceDlg::GetInstance()->GetSelectedDevices();
	ASSERT(!selectedDevices.empty());
	const DWORD deviceID = selectedDevices[0].first;

	try
	{
		SAFE_DELETE(g_wasapi_recorder);
		SAFE_DELETE(g_fileWriter);
		SAFE_DELETE(g_mp3Encoder);

		g_wasapi_recorder = new CWasapiRecorder(deviceID, 44100, 2);//, NULL, NULL);
		const int actualFreq = g_wasapi_recorder->GetActualFrequency();
		const int actualChannels = g_wasapi_recorder->GetActualChannelCount();

		g_mp3Encoder = new CEncoder_MP3(128, actualFreq, actualChannels);
		g_wasapi_recorder->SetChildFilter(g_mp3Encoder);

		g_fileWriter = new FileWriter(_T("d:\\test.mp3"));
		g_mp3Encoder->SetChildFilter(g_fileWriter);
	}
	catch (CException* e)
	{
		SAFE_DELETE(g_wasapi_recorder);
		SAFE_DELETE(g_fileWriter);
		SAFE_DELETE(g_mp3Encoder);
		throw;
	}
	catch (...)
	{
		AfxMessageBox(_T("Unknown exception occured."));
	}

	if (!g_wasapi_recorder->Start())
		return 0;

	m_GraphWnd.StartUpdate(PeaksCallback_Wasapi, LinesCallback_Wasapi, g_wasapi_recorder);
	*/


	/*
	BOOL result = BASS_WASAPI_Stop(TRUE);
	result = BASS_WASAPI_Free();
	m_GraphWnd.StopUpdate();
	SAFE_DELETE(m_visualization_data);

	//graph_wnd start update with our test stream.
	
	WasapiHelpers::DevicesArray selectedDevices = CRecordingSourceDlg::GetInstance()->GetSelectedDevices();
	ASSERT(!selectedDevices.empty());
	const DWORD deviceID = selectedDevices[0].first;
	DWORD errorCode = 0;

	result = BASS_WASAPI_Init(deviceID, 44100, 2, BASS_WASAPI_AUTOFORMAT, 0.5, 0, WasapiRecordingProc, this);
	if (result)
	{
		OutputDebugString(__FUNCTION__" ::SUCCESS!\n");
		result = BASS_WASAPI_Start();
	}
	else
	{
		OutputDebugString(__FUNCTION__" ::ERROR!\n");
		errorCode = BASS_ErrorGetCode();
		return 0;
	}

	m_visualization_data = new VisualizationData(44100, 2);
	m_GraphWnd.StartUpdate(PeaksCallback_Wasapi, LinesCallback_Wasapi);
	*/
	return 0;
}
//------------------------------------------------------------------------------

LRESULT CMainFrame::OnRecSourceChanged(WPARAM wParam, LPARAM lParam)
{
	OutputDebugString(__FUNCTION__"\n");
	return 0;
}
//------------------------------------------------------------------------------

void CMainFrame::HandleFilterNotification(
	const Filter* /*fromFilter*/, const Parameter& parameter, void* userData)
{
	//Request notification processing in main thread.
	CMainFrame* mainFrame = static_cast<CMainFrame*>(userData);
	mainFrame->m_filterNotifications.push_back(parameter);
	mainFrame->PostMessage(WM_FILTER_NOTIFY, NULL, NULL);
}
//------------------------------------------------------------------------------

LRESULT CMainFrame::OnFilterNotify(WPARAM wParam, LPARAM lParam)
{
	for (size_t i = 0; i < m_filterNotifications.size(); i++)
	{
		const Parameter& param = m_filterNotifications[i];
		if (param.name == _T("VAS.HandleSilence"))
		{
			const int vasAction = RegistryConfig::GetOption(_T("Tools\\VAS\\Action"), 0);
			const bool stopOnSilence = vasAction == 1;
			const bool isSilence = param.valueInt == 1;

			if (isSilence && stopOnSilence)
				OnBtnSTOP();
			else
			{
				const int icoWndIcon = isSilence ? ICON_RECVAS : ICON_REC;
				const int trayIcon = isSilence ? IDI_TRAY_PAUSE : IDI_TRAY_REC;
				m_IcoWnd.SetNewIcon(icoWndIcon);
				m_TrayIcon.SetIcon(trayIcon);
			}
			continue;
		}
		if (param.name == _T("Recorder.Stopped"))
		{
			OnBtnSTOP();
			continue;
		}

		WriteDbg() << "UNHANDLED PARAMETER: " << param;
	}
	m_filterNotifications.clear();
	return 0;
}
//------------------------------------------------------------------------------

