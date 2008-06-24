///////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "MP3_Recorder.h"
#include "GraphWnd.h"

#include <mmsystem.h>
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

///////////////////////////////////////////////////////////////////////////////
class CMyLock
{
public:
	CMyLock(LPCRITICAL_SECTION a_sync_ptr)
		:m_sync_ptr(a_sync_ptr)
	{
		EnterCriticalSection(m_sync_ptr);
	}
	~CMyLock()
	{
		LeaveCriticalSection(m_sync_ptr);
	}
private:
	CMyLock(const CMyLock &);
	const CMyLock& operator=(const CMyLock &);
private:
	LPCRITICAL_SECTION m_sync_ptr;
};

///////////////////////////////////////////////////////////////////////////////
const int MAXPEAK_TIMER_ID = 123;
const int PLAY_BUFFER_SIZE = 2048;

float g_play_buffer[2 * PLAY_BUFFER_SIZE];

enum Channels {LEFT_CHANNEL, RIGHT_CHANNEL};
enum VisTypes {DRAW_PEAKS, DRAW_PEAKS_DB, DRAW_LINES, DRAW_NONE};
enum PeakMode {LINEAR, LOGARITHMIC};

///////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CGraphWnd, CWnd)
	//{{AFX_MSG_MAP(CGraphWnd)
	ON_WM_PAINT()
	ON_WM_CREATE()
	ON_WM_RBUTTONDOWN()
	ON_WM_LBUTTONDOWN()
	ON_WM_TIMER()
	ON_COMMAND(ID_GRAPH_MAXPEAKS, OnGraphMaxpeaks)
	//}}AFX_MSG_MAP
	ON_COMMAND_RANGE(ID_GRAPH_PEAKMETER, ID_GRAPH_NONE, OnGraphMenu)
	ON_UPDATE_COMMAND_UI_RANGE(ID_GRAPH_PEAKMETER, ID_GRAPH_NONE, OnUpdateGraphMenu)
END_MESSAGE_MAP()

///////////////////////////////////////////////////////////////////////////////
CGraphWnd::CGraphWnd()
	:m_bShowVASMark(false)
	,m_bMaxpeaks(true)
	,m_timer_id(0)
	,m_display_mode(0)
	,m_stream_handle(0)
{
	InitializeCriticalSection(&m_sync_object);
}

//-----------------------------------------------------------------------------
CGraphWnd::~CGraphWnd()
{
	DeleteCriticalSection(&m_sync_object);
}

//-----------------------------------------------------------------------------
int CGraphWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
	{
		return -1;
	}
	m_wndsize.cx = lpCreateStruct->cx;
	m_wndsize.cy = lpCreateStruct->cy;

	CClientDC DC(this);
	m_memDC.CreateCompatibleDC(&DC);
	m_bmp.CreateCompatibleBitmap(&DC, m_wndsize.cx, m_wndsize.cy);
	m_greenPen.CreatePen(PS_SOLID, 1, RGB(0, 200, 0));
	m_memDC.SelectObject(&m_bmp);		// assign bitmap
	m_memDC.SelectObject(&m_greenPen);	// select line color

	m_skinDC.CreateCompatibleDC(&DC);
	m_bgbmp[0].LoadBitmap(IDB_GWBG_LNP);
	m_bgbmp[1].LoadBitmap(IDB_GWBG_LGP);
	m_bgbmp[2].LoadBitmap(IDB_GWBG_DOTS);
	m_bgbmp[3].LoadBitmap(IDB_GWBG_NONE);
	m_skinDC.SelectObject(&m_bgbmp[0]);

	m_peakDC.CreateCompatibleDC(&DC);
	m_pmbmp[0].LoadBitmap(IDB_GWPM_LN);
	m_pmbmp[1].LoadBitmap(IDB_GWPM_LG);
	m_peakDC.SelectObject(&m_pmbmp[0]);

	m_bkbrush.CreateStockObject(BLACK_BRUSH);

	m_wndRect.left	= 0;
	m_wndRect.top	= 0;
	m_wndRect.right	= m_wndsize.cx;
	m_wndRect.bottom= m_wndsize.cy;

	ResetMaxpeakMarks();
	Clear();
	return 0;
}

//-----------------------------------------------------------------------------
void CGraphWnd::OnPaint() 
{
	CMyLock lock(&m_sync_object);

	CPaintDC dc(this); // device context for painting
	dc.BitBlt(0, 0, m_wndsize.cx, m_wndsize.cy, &m_memDC, 0, 0, SRCCOPY);
}

//-----------------------------------------------------------------------------
BOOL CGraphWnd::PreTranslateMessage(MSG* pMsg) 
{
	if (pMsg->message == WM_LBUTTONDBLCLK)
	{
		pMsg->message = WM_LBUTTONDOWN;
	}
	return CWnd::PreTranslateMessage(pMsg);
}

//-----------------------------------------------------------------------------
void CGraphWnd::OnLButtonDown(UINT nFlags, CPoint point) 
{
	m_display_mode = (m_display_mode >= sizeof(VisTypes)-1) ? DRAW_PEAKS :
		m_display_mode + 1;
	SetDisplayMode(m_display_mode);

	CWnd::OnLButtonDown(nFlags, point);
}

//-----------------------------------------------------------------------------
void CGraphWnd::OnRButtonDown(UINT nFlags, CPoint point) 
{
	const int markers[] = {ID_GRAPH_PEAKMETER, ID_GRAPH_PEAKMETERDB,
		ID_GRAPH_OSCILLOSCOPE, ID_GRAPH_NONE};

	CMenu GraphMenu, *pGraphMenu;
	GraphMenu.LoadMenu(IDR_GRAPHMENU);
	pGraphMenu = GraphMenu.GetSubMenu(0);

	pGraphMenu->CheckMenuItem(markers[m_display_mode], MF_CHECKED);
	if (m_bMaxpeaks)
	{
		pGraphMenu->CheckMenuItem(ID_GRAPH_MAXPEAKS, MF_CHECKED);
	}

	ClientToScreen(&point);
	pGraphMenu->TrackPopupMenu(TPM_LEFTALIGN|TPM_LEFTBUTTON,
		point.x, point.y, this);
	
	CWnd::OnRButtonDown(nFlags, point);
}

//-----------------------------------------------------------------------------
void CGraphWnd::OnGraphMenu(UINT nID)
{
	switch (nID)
	{
	case ID_GRAPH_PEAKMETER:
		m_display_mode = DRAW_PEAKS;
		break;
	case ID_GRAPH_PEAKMETERDB:
		m_display_mode = DRAW_PEAKS_DB;
		break;
	case ID_GRAPH_OSCILLOSCOPE:
		m_display_mode = DRAW_LINES;
		break;
	case ID_GRAPH_NONE:
		m_display_mode = DRAW_NONE;
		break;
	default:
		return;
	}
	SetDisplayMode(m_display_mode);
}

//-----------------------------------------------------------------------------
void CGraphWnd::OnGraphMaxpeaks() 
{
	m_bMaxpeaks = !m_bMaxpeaks;

	if (m_bMaxpeaks == false)
	{
		ResetMaxpeakMarks();
	}
}

//-----------------------------------------------------------------------------
void CGraphWnd::OnUpdateGraphMenu(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(TRUE);
}

//-----------------------------------------------------------------------------
void CGraphWnd::Update(char* pSndBuf, int nBufSize)
{
	ClearWindow();

	if (m_stream_handle)
	{
		switch (m_display_mode)
		{
		case DRAW_PEAKS:
			DrawPeaks(LINEAR);
			break;
		case DRAW_PEAKS_DB:
			DrawPeaks(LOGARITHMIC);
			break;
		case DRAW_LINES:
			DrawLines();
			break;
		default:
			return;
		}
	}
	InvalidateRect(NULL);
}

//-----------------------------------------------------------------------------
void CGraphWnd::ClearWindow()
{
	CMyLock lock(&m_sync_object);

	m_memDC.FillRect(&m_wndRect, &m_bkbrush);
	m_memDC.BitBlt(0, 0, m_wndsize.cx, m_wndsize.cy, &m_skinDC, 0, 0, SRCCOPY);
	DrawVASMark();
}

//-----------------------------------------------------------------------------
void CGraphWnd::Clear()
{
	ClearWindow();
	InvalidateRect(NULL);
}

//-----------------------------------------------------------------------------
int CGraphWnd::GetPeakLevel(int nChannel)
{
	DWORD l_ch_level = BASS_ChannelGetLevel(m_stream_handle);

	if (l_ch_level == -1)
	{
		l_ch_level = 0;
	}
	return (LEFT_CHANNEL==nChannel) ? LOWORD(l_ch_level) : HIWORD(l_ch_level);
}

//-----------------------------------------------------------------------------
void CGraphWnd::DrawPeaks(int nMode)
{
	const int START_Y[2] = {5, m_wndsize.cy/2 + 10};
	const int END_Y[2] =  {13, m_wndsize.cy/2 + 18};
	const int MAXPEAK = GetMaxPeakValue();
	const int GRAPH_MAXDB = 60;

	BASS_CHANNELINFO l_ci;
	BOOL l_result = BASS_ChannelGetInfo(m_stream_handle, &l_ci);
	ASSERT(l_result);

	for (UINT i = 0; i < 2; i++)
	{
		double l_cur_peak = GetPeakLevel(i);
		if (nMode == LOGARITHMIC)
		{
			l_cur_peak = (l_cur_peak <= 0) ? 1 : l_cur_peak;
			l_cur_peak = 20 * log10(l_cur_peak/MAXPEAK);			// is < 0
			l_cur_peak = m_wndsize.cx-6 + l_cur_peak/GRAPH_MAXDB * (m_wndsize.cx-6);
		}
		else // linear
		{
			l_cur_peak = int((float)l_cur_peak/MAXPEAK * (m_wndsize.cx-6));
		}

		double& l_max_peak = m_max_peaks[i];
		double& l_old_peak = m_old_peaks[i];

		if (l_cur_peak < l_old_peak)
		{
			l_cur_peak = (l_cur_peak >= 0) ? l_old_peak - 4 : 3;
		}
		l_old_peak = l_cur_peak;
		m_memDC.BitBlt(3, START_Y[i], (int)l_cur_peak, END_Y[i], &m_peakDC, 0, 0, SRCCOPY);

		if (m_bMaxpeaks)
		{
			if (l_cur_peak > l_max_peak)
			{
				l_max_peak = l_cur_peak;
				KillTimer(MAXPEAK_TIMER_ID);
				SetTimer(MAXPEAK_TIMER_ID, 3000, NULL);
			}
			m_memDC.BitBlt(int(3 + l_max_peak-2), START_Y[i], 2, END_Y[i],
				&m_peakDC, int(l_max_peak-2), 0, SRCCOPY);
		}
	} // for

	if (m_bShowVASMark)
	{
		DrawVASMark();
	}
}

//-----------------------------------------------------------------------------
void CGraphWnd::DrawLines()
{
	BASS_CHANNELINFO l_ci;
	BOOL l_result = BASS_ChannelGetInfo(m_stream_handle, &l_ci);
	ASSERT(l_result);

	if (-1 == BASS_ChannelGetData(m_stream_handle, g_play_buffer,
		(l_ci.chans * sizeof(float) * PLAY_BUFFER_SIZE) | BASS_DATA_FLOAT))
	{
		return;
	}

	for (UINT counter = 0; counter < 2; counter++)
	{
		// Handle mono playback.
		int i = (l_ci.chans > 1) ? counter : 0;

		const int CX_START = 3;
		const int CX_END   = 3;
		const int START_POS_Y = (1 + 2*counter) * m_wndsize.cy / 4;
		const int DIVIDER = PLAY_BUFFER_SIZE / (m_wndsize.cx - CX_END);

		int nDrawPosY = START_POS_Y + int(g_play_buffer[i] * m_wndsize.cy / 2);
		m_memDC.MoveTo(CX_START, nDrawPosY);

		for (int j = CX_START + 1; j < m_wndsize.cx - CX_END; j++)
		{
			//nDrawPosY = START_POS_Y + int(g_play_buffer[j*l_ci.chans + i] *
			nDrawPosY = START_POS_Y + int(g_play_buffer[j*DIVIDER + i] *
				m_wndsize.cy / 2);
			m_memDC.LineTo(j++, nDrawPosY);	// j++ !!!
		}
	}
}

//-----------------------------------------------------------------------------
double CGraphWnd::GetMaxPeakdB(char *pSndBuf, int nBufSize, int nChannel)
{
	const int    MAXPEAK = GetMaxPeakValue();
	const double CURPEAK = GetPeakLevel(nChannel);

	return 20 * log10(CURPEAK/MAXPEAK);
}

//-----------------------------------------------------------------------------
void CGraphWnd::ShowVASMark(double fThreshold)
{
	const int MAXPEAK = GetMaxPeakValue();

	m_vas_marks[0] = MAXPEAK * pow(10, (fThreshold/20));
	m_vas_marks[0] = m_vas_marks[0] / MAXPEAK * (m_wndsize.cx-6);
	m_vas_marks[0] = 2 + (m_vas_marks[0] == 0) ? 1 : m_vas_marks[0];

	m_vas_marks[1] = 2 + m_wndsize.cx-6 + fThreshold / 60 * (m_wndsize.cx-6);

	m_bShowVASMark = true;
	Clear();
}

//-----------------------------------------------------------------------------
void CGraphWnd::HideVASMark()
{
	m_bShowVASMark = false;
	Clear();
}

//-----------------------------------------------------------------------------
void CGraphWnd::DrawVASMark()
{
	CMyLock lock(&m_sync_object);

	if ((m_display_mode != DRAW_PEAKS && m_display_mode != DRAW_PEAKS_DB)
		|| !m_bShowVASMark)
	{
		return;
	}
	const int START_X[] = {0, 0};
	const int START_Y[] = {5, 36};
	const int POS_OFFSET= (m_display_mode == DRAW_PEAKS) ?
		(int)m_vas_marks[0] : (int)m_vas_marks[1];
	
	CPen NewPen, *pOldPen;
	NewPen.CreatePen(PS_SOLID, 2, RGB(0, 64, 0));
	
	pOldPen = m_memDC.SelectObject(&NewPen);
	for (int i = 0; i < 2; i++)
	{
		m_memDC.MoveTo(START_X[i] + POS_OFFSET, START_Y[i]);
		m_memDC.LineTo(START_X[i] + POS_OFFSET, START_Y[i] + 7);
	}
	m_memDC.SelectObject(pOldPen);
}

//-----------------------------------------------------------------------------
void CGraphWnd::OnTimer(UINT nIDEvent) 
{
	if (nIDEvent == MAXPEAK_TIMER_ID)
	{
		ResetMaxpeakMarks();
	}
	CWnd::OnTimer(nIDEvent);
}

//-----------------------------------------------------------------------------
void CGraphWnd::ResetMaxpeakMarks()
{
	m_max_peaks[0] = 3;	// peaks window relative
	m_max_peaks[1] = 3;
	KillTimer(MAXPEAK_TIMER_ID);
}

//-----------------------------------------------------------------------------
void CGraphWnd::SetMaxpeaks(bool bEnabled)
{
	m_bMaxpeaks = bEnabled;
}

//-----------------------------------------------------------------------------
bool CGraphWnd::GetMaxpeaks() const
{
	return m_bMaxpeaks;
}

//-----------------------------------------------------------------------------
bool CGraphWnd::StartUpdate(HSTREAM a_stream_handle)
{
	if (!a_stream_handle)
	{
		return false;
	}
	StopUpdate();
	m_stream_handle = a_stream_handle;

	m_timer_id = timeSetEvent(25, 25, (LPTIMECALLBACK)&UpdateVisualization,
		(DWORD_PTR)this, TIME_PERIODIC);
	return (m_timer_id != 0);
}

//-----------------------------------------------------------------------------
void CGraphWnd::StopUpdate()
{
	if (!m_timer_id)
	{
		return;
	}
	m_stream_handle = 0;
	timeKillEvent(m_timer_id);
	m_timer_id = 0;
}
//-----------------------------------------------------------------------------
bool CGraphWnd::SetDisplayMode(int a_new_display_mode)
{
	CMyLock lock(&m_sync_object);

	if (a_new_display_mode > sizeof(VisTypes) - 1)
	{
		return false;
	}
	m_skinDC.SelectObject(&m_bgbmp[a_new_display_mode]);

	// Checking peak bitmap changed.
	if (a_new_display_mode == DRAW_PEAKS || a_new_display_mode == DRAW_PEAKS_DB)
	{
		m_peakDC.SelectObject((a_new_display_mode == DRAW_PEAKS) ?
			&m_pmbmp[0] : &m_pmbmp[1]);
	}
	m_display_mode = a_new_display_mode;
	m_old_peaks[0] = 0;
	m_old_peaks[1] = 0;
	ResetMaxpeakMarks();
	Clear();
	return true;
}
//-----------------------------------------------------------------------------
const int CGraphWnd::GetDisplayMode() const
{
	return m_display_mode;
}
//-----------------------------------------------------------------------------
const int CGraphWnd::GetMaxPeakValue() const
{
	int l_max_possible = 0xFFFF / 2; // 16-bit as default

	if (m_stream_handle)
	{
		BASS_CHANNELINFO l_ci;
		BOOL l_result = BASS_ChannelGetInfo(m_stream_handle, &l_ci);
		if (!l_result)
		{
			int l_err_code = BASS_ErrorGetCode();
			int l_breakpoint = 0;
		}

		if (l_ci.flags & BASS_SAMPLE_8BITS)
		{
			l_max_possible = 0xFF / 2;
		}
		else if (l_ci.flags & BASS_SAMPLE_FLOAT)
		{
			l_max_possible = 1;
		}
	}
	return l_max_possible;
}

//-----------------------------------------------------------------------------
void CALLBACK CGraphWnd::UpdateVisualization(UINT a_timer_id, UINT a_msg,
	DWORD_PTR a_user_ptr, DWORD_PTR a_dw1, DWORD_PTR a_dw2)
{
	CGraphWnd* l_graph_wnd = (CGraphWnd*)a_user_ptr;

	CMyLock lock(&l_graph_wnd->m_sync_object);
	l_graph_wnd->Update(NULL, 0);
}