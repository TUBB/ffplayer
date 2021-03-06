// playerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "player.h"
#include "playerDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static DWORD WINAPI PlayerOpenThreadProc(LPVOID lpParam)
{
    CplayerDlg *dlg = (CplayerDlg*)lpParam;

    while (1) {
        WaitForSingleObject(dlg->m_hEvent, -1);
        if (dlg->m_bClose) break;

        // stop player first
        if (dlg->m_ffPlayer) {
            player_close(dlg->m_ffPlayer);
            dlg->m_ffPlayer = NULL;
        }

        dlg->m_ffPlayer = player_open(dlg->m_strUrl, dlg->GetSafeHwnd(), &dlg->m_Params);
        if (dlg->m_ffPlayer) {
            int param = 0;
            //++ set dynamic player params
//          param = 150; player_setparam(dlg->m_ffPlayer, PARAM_PLAY_SPEED    , &param);

            // software volume scale -30dB to 12dB
            // range for volume is [-182, 73]
            // -255 - mute, +255 - max volume, 0 - 0dB
            param = -0;  player_setparam(dlg->m_ffPlayer, PARAM_AUDIO_VOLUME  , &param);

//          param = 1;   player_setparam(dlg->m_ffPlayer, PARAM_VFILTER_ENABLE, &param);
            //-- set dynamic player params
        }
    }

    return 0;
}

#define TIMER_ID_FIRST_DIALOG  1
#define TIMER_ID_PROGRESS      2

// CplayerDlg dialog
CplayerDlg::CplayerDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CplayerDlg::IDD, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    m_ffPlayer    = NULL;
    m_bLiveStream = FALSE;
    m_bResetPlayer= FALSE;

    // set player init params
    memset(&m_Params, 0, sizeof(m_Params));
    m_Params.adev_render_type = ADEV_RENDER_TYPE_WAVEOUT;
    m_Params.vdev_render_type = VDEV_RENDER_TYPE_D3D;
}

void CplayerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

void CplayerDlg::PlayerOpenFile(TCHAR *file)
{
    CFileDialog dlg(TRUE);
    TCHAR       str[MAX_PATH];

    // kill player progress timer
    KillTimer(TIMER_ID_PROGRESS);

    // open file dialog
    if (!file) {
        if (dlg.DoModal() == IDOK) {
            wcscpy_s(str, dlg.GetPathName());
        } else {
            OnOK();
            return;
        }
    } else {
        wcscpy_s(str, file);
    }
    WideCharToMultiByte(CP_ACP, 0, str, -1, m_strUrl, MAX_PATH, NULL, NULL);

    // set window title
    SetWindowText(TEXT("testplayer - loading"));

    // player open file
    if (strstr(m_strUrl, "rtmp://") == m_strUrl || strstr(m_strUrl, "http://") == m_strUrl && strstr(m_strUrl, ".m3u8")) {
        m_bLiveStream = TRUE;
    } else {
        m_bLiveStream = FALSE;
    }

    // init player
    m_bResetPlayer = FALSE;
    SetEvent(m_hEvent);
}

BEGIN_MESSAGE_MAP(CplayerDlg, CDialog)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_WM_LBUTTONDOWN()
    ON_WM_CTLCOLOR()
    ON_WM_SIZE()
    ON_COMMAND(ID_OPEN_FILE    , &CplayerDlg::OnOpenFile    )
    ON_COMMAND(ID_VIDEO_MODE   , &CplayerDlg::OnVideoMode   )
    ON_COMMAND(ID_EFFECT_MODE  , &CplayerDlg::OnEffectMode  )
    ON_COMMAND(ID_VRENDER_TYPE , &CplayerDlg::OnVRenderType )
    ON_COMMAND(ID_AUDIO_STREAM , &CplayerDlg::OnAudioStream )
    ON_COMMAND(ID_VIDEO_STREAM , &CplayerDlg::OnVideoStream )
    ON_COMMAND(ID_TAKE_SNAPSHOT, &CplayerDlg::OnTakeSnapshot)
END_MESSAGE_MAP()


// CplayerDlg message handlers

BOOL CplayerDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE );  // Set big icon
    SetIcon(m_hIcon, FALSE);  // Set small icon

    // load accelerators
    m_hAcc = LoadAccelerators(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_ACCELERATOR1)); 

    // TODO: Add extra initialization here
    MoveWindow(0, 0, 800, 480);

    // get dc
    m_pDrawDC = GetDC();

    // setup init timer
    SetTimer(TIMER_ID_FIRST_DIALOG, 100, NULL);

    m_bClose  = FALSE;
    m_hEvent  = CreateEvent (NULL, FALSE, FALSE, NULL);
    m_hThread = CreateThread(NULL, 0, PlayerOpenThreadProc, this, 0, NULL);

    return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CplayerDlg::OnPaint()
{
    if (IsIconic()) {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width () - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    } else {
        LONGLONG total = 1, pos = 0;
        player_getparam(m_ffPlayer, PARAM_MEDIA_DURATION, &total);
        player_getparam(m_ffPlayer, PARAM_MEDIA_POSITION, &pos  );
        if (!m_bLiveStream) {
            if (pos > 0) {
                CPaintDC dc(this);
                RECT fill  = m_rtClient;
                fill.right = (LONG)(fill.right * pos / total);
                fill.top   = fill.bottom - 2;
                dc.FillSolidRect(&fill, RGB(250, 150, 0));
                fill.left  = fill.right;
                fill.right = m_rtClient.right;
                dc.FillSolidRect(&fill, RGB(0, 0, 0));
            }
        } else {
            SetWindowText(pos == -1 ? TEXT("testplayer - buffering") : TEXT("testplayer"));
        }
        CDialog::OnPaint();
    }
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CplayerDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CplayerDlg::OnDestroy()
{
    CDialog::OnDestroy();
    ReleaseDC(m_pDrawDC);

    m_bClose = TRUE;
    SetEvent(m_hEvent);
    WaitForSingleObject(m_hThread, -1);
    CloseHandle(m_hEvent );
    CloseHandle(m_hThread);

    // close player
    if (m_ffPlayer) {
        player_close(m_ffPlayer);
        m_ffPlayer = NULL;
    }
}

void CplayerDlg::OnTimer(UINT_PTR nIDEvent)
{
    switch (nIDEvent)
    {
    case TIMER_ID_FIRST_DIALOG:
        // kill timer first
        KillTimer(TIMER_ID_FIRST_DIALOG);
        PlayerOpenFile(__argc > 1 ? __wargv[1] : NULL);
        break;

    case TIMER_ID_PROGRESS:
        RECT rect;
        rect.top    = m_rtClient.bottom - 2;
        rect.left   = m_rtClient.left;
        rect.bottom = m_rtClient.bottom;
        rect.right  = m_rtClient.right;
        InvalidateRect(&rect, FALSE);
        break;

    default:
        CDialog::OnTimer(nIDEvent);
        break;
    }
}

void CplayerDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (!m_bLiveStream) {
        if (point.y > m_rtClient.bottom - 8) {
            LONGLONG total = 1;
            player_getparam(m_ffPlayer, PARAM_MEDIA_DURATION, &total);
            KillTimer(TIMER_ID_PROGRESS);
            player_seek(m_ffPlayer, total * point.x / m_rtClient.right, SEEK_PRECISELY);
            SetTimer (TIMER_ID_PROGRESS, 100, NULL);
        } else {
            if (!m_bPlayPause) player_pause(m_ffPlayer);
            else player_play(m_ffPlayer);
            m_bPlayPause = !m_bPlayPause;
        }
    }

    CDialog::OnLButtonDown(nFlags, point);
}

HBRUSH CplayerDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

    // TODO: Change any attributes of the DC here
    // TODO: Return a different brush if the default is not desired
    if (pWnd == this) return (HBRUSH)GetStockObject(BLACK_BRUSH);
    else return hbr;
}

void CplayerDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    if (nType != SIZE_MINIMIZED) {
        GetClientRect(&m_rtClient);
        player_setrect(m_ffPlayer, 0, 0, 0, cx, cy - 2);
        player_setrect(m_ffPlayer, 1, 0, 0, cx, cy - 2);
    }
}

BOOL CplayerDlg::PreTranslateMessage(MSG *pMsg) 
{
    if (TranslateAccelerator(GetSafeHwnd(), m_hAcc, pMsg)) return TRUE;

    if (pMsg->message == MSG_FFPLAYER) {
        switch (pMsg->wParam)
        {
        case MSG_INIT_DONE:
            SetWindowText(TEXT("testplayer"));
            player_setrect(m_ffPlayer, 0, 0, 0, m_rtClient.right, m_rtClient.bottom - 2);
            player_setrect(m_ffPlayer, 1, 0, 0, m_rtClient.right, m_rtClient.bottom - 2);
            if (m_bResetPlayer) {
                player_seek(m_ffPlayer, m_llLastPos, SEEK_PRECISELY);
                if (!m_bPlayPause) player_play(m_ffPlayer);
                m_bResetPlayer = FALSE;
            } else {
                player_play(m_ffPlayer);
                m_bPlayPause = FALSE;
            }
            SetTimer(TIMER_ID_PROGRESS, 100, NULL);
            break;
        case MSG_PLAY_COMPLETED:
            if (!m_bLiveStream) {
                PlayerOpenFile(NULL);
            }
            break;
        }
        return TRUE;
    } else return CDialog::PreTranslateMessage(pMsg);
}

void CplayerDlg::OnOpenFile()
{
    PlayerOpenFile(NULL);
}

void CplayerDlg::OnAudioStream()
{
    player_getparam(m_ffPlayer, PARAM_MEDIA_POSITION, &m_llLastPos);
    m_Params.audio_stream_cur++; m_Params.audio_stream_cur %= m_Params.audio_stream_total;
    m_bResetPlayer = TRUE;
    SetEvent(m_hEvent);
}

void CplayerDlg::OnVideoStream()
{
    player_getparam(m_ffPlayer, PARAM_MEDIA_POSITION, &m_llLastPos);
    m_Params.video_stream_cur++; m_Params.video_stream_cur %= m_Params.video_stream_total;
    m_bResetPlayer = TRUE;
    SetEvent(m_hEvent);
}

void CplayerDlg::OnVideoMode()
{
    int mode = 0;
    player_getparam(m_ffPlayer, PARAM_VIDEO_MODE, &mode);
    mode++; mode %= VIDEO_MODE_MAX_NUM;
    player_setparam(m_ffPlayer, PARAM_VIDEO_MODE, &mode);
}

void CplayerDlg::OnEffectMode()
{
    int mode = 0;
    player_getparam(m_ffPlayer, PARAM_VISUAL_EFFECT, &mode);
    mode++; mode %= VISUAL_EFFECT_MAX_NUM;
    player_setparam(m_ffPlayer, PARAM_VISUAL_EFFECT, &mode);
}

void CplayerDlg::OnVRenderType()
{
    player_getparam(m_ffPlayer, PARAM_MEDIA_POSITION, &m_llLastPos);
    m_Params.vdev_render_type++; m_Params.vdev_render_type %= VDEV_RENDER_TYPE_MAX_NUM;
    m_bResetPlayer = TRUE;
    SetEvent(m_hEvent);
}

void CplayerDlg::OnTakeSnapshot()
{
    player_snapshot(m_ffPlayer, "snapshot.jpg", 0, 0, 1000);
}



