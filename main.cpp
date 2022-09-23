#include <wx/cmdline.h>
#include <wx/wx.h>

#include <vector>

#include "config.h"
#include "saverframe.h"

static const wxCmdLineEntryDesc g_cmdLineDesc[] = {
    {wxCMD_LINE_SWITCH, "h", "help", "Show command line help", wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP},
    {wxCMD_LINE_OPTION, "c", "", "Show configuration dialog", wxCMD_LINE_VAL_NONE},
    {wxCMD_LINE_OPTION, "C", "", "Show configuration dialog", wxCMD_LINE_VAL_NONE},
    {wxCMD_LINE_OPTION, "p", "", "Show preview in child window", wxCMD_LINE_VAL_NUMBER},
    {wxCMD_LINE_OPTION, "P", "", "Show preview in child window", wxCMD_LINE_VAL_NUMBER},
    {wxCMD_LINE_SWITCH, "s", "", "Run the screensaver", wxCMD_LINE_VAL_NONE},
    {wxCMD_LINE_SWITCH, "S", "", "Run the screensaver", wxCMD_LINE_VAL_NONE},

    {wxCMD_LINE_NONE}};

class App : public wxApp
{
  public:
    virtual bool OnInit();
    virtual void OnInitCmdLine(wxCmdLineParser& parser);
    virtual bool OnCmdLineParsed(wxCmdLineParser& parser);

    void OnTimer(const wxTimerEvent& e);
    void OnClose(wxEvent& e);
    void OnFrameClose(wxCloseEvent& e);
    void OnKey(wxKeyEvent& e);

    void StartScreensaver();
    int enumerated_monitor = 1;

  private:
    void switchImage(bool forward);

    Config config;
    bool show_config = false;
    bool show_preview = false;
    long preview_hwnd = 0;
    static BOOL CALLBACK monitorEnumProc(HMONITOR hmonitor, HDC hdc, LPRECT rect, LPARAM lparam);

    std::vector<RECT> monitors;
    std::vector<SaverFrame*> frames;
    int update_frame = 0;
    wxTimer timer;
};

wxIMPLEMENT_APP(App);

bool App::OnInit()
{
    wxApp::OnInit();

    wxImage::AddHandler(new wxJPEGHandler());
    wxImage::AddHandler(new wxPNGHandler());

    if (show_config)
    {
        CONFIG_DIALOG* frame = new CONFIG_DIALOG(config);
        SetTopWindow(frame);
        frame->Show();
    }
    else if (show_preview)
    {
        // TODO
        return false;
    }
    else
    {
        StartScreensaver();
    }

    return true;
}

void App::OnInitCmdLine(wxCmdLineParser& parser)
{
    parser.SetDesc(g_cmdLineDesc);
}

bool App::OnCmdLineParsed(wxCmdLineParser& parser)
{
    show_config = !parser.Found("s") && !parser.Found("S") && !parser.Found("p") && !parser.Found("P");
    show_preview = parser.Found("p", &preview_hwnd) || parser.Found("P", &preview_hwnd);

    return true;
}

void App::StartScreensaver()
{
    LPARAM lparam = reinterpret_cast<LPARAM>(this);

    EnumDisplayMonitors(NULL, NULL, &App::monitorEnumProc, lparam);

    for (const auto& rect : monitors)
    {
        const wxString path =
            (rect.right - rect.left > rect.bottom - rect.top) ? config.landscapeDir : config.portraitDir;

        SaverFrame* frame = new SaverFrame(
            path, config.recursive, config.scale, wxPoint(rect.left + config.margins, rect.top + config.margins),
            wxSize(rect.right - rect.left - 2 * config.margins, rect.bottom - rect.top - 2 * config.margins));

        frame->Show();
        frame->Bind(wxEVT_LEFT_UP, &App::OnClose, this);
        frame->Bind(wxEVT_CLOSE_WINDOW, &App::OnFrameClose, this);
        frame->Bind(wxEVT_KEY_DOWN, &App::OnKey, this);

        frames.push_back(frame);
    }

    for (const auto& frame : frames)
        frame->Draw();

    Bind(wxEVT_TIMER, &App::OnTimer, this);
    timer.Start(config.period * 1000);
}

void App::OnTimer(const wxTimerEvent& e)
{
    if (frames.empty())
        return;

    switchImage(true);
}

void App::switchImage(bool forward)
{
    if (config.stagger)
    {
        if (forward)
            update_frame = (update_frame + 1) % frames.size();

        auto frame = frames[update_frame];

        for (double tick = 0.02; tick < 1.0; tick += .02)
        {
            frame->Transition(forward, tick);
            wxMilliSleep(16);
        }

        frame->Increment(forward);
        frame->Draw();
        frame->LoadNextImage(forward);

        if (!forward)
            update_frame = (update_frame + frames.size() - 1) % frames.size();
    }
    else
    {
        for (double tick = 0.02; tick < 1.0; tick += .02)
        {
            for (const auto& frame : frames)
                frame->Transition(forward, tick);
            wxMilliSleep(16);
        }
        for (const auto& frame : frames)
        {
            frame->Increment(forward);
            frame->Draw();
        }
        for (const auto& frame : frames)
            frame->LoadNextImage(forward);
    }
}

BOOL CALLBACK App::monitorEnumProc(HMONITOR hmonitor, HDC hdc, LPRECT rect, LPARAM lparam)
{
    App* app = reinterpret_cast<App*>(lparam);

    app->monitors.push_back(*rect);

    return true;
}

void App::OnClose(wxEvent& e)
{
    for (const auto& frame : frames)
        frame->Close();
}

void App::OnFrameClose(wxCloseEvent& e)
{
    SaverFrame* frame = static_cast<SaverFrame*>(e.GetEventObject());
    std::vector<SaverFrame*>::iterator where = std::find(frames.begin(), frames.end(), frame);

    if (where != frames.end())
        frames.erase(where);

    frame->Destroy();
}

void App::OnKey(wxKeyEvent& e)
{
    if (e.GetUnicodeKey() == WXK_SPACE)
    {
        if (timer.IsRunning())
            timer.Stop();
        else
            timer.Start(config.period * 1000);
    }
    else if (e.GetKeyCode() == WXK_LEFT || e.GetKeyCode() == WXK_RIGHT)
    {
        switchImage(e.GetKeyCode() == WXK_RIGHT);
    }
    else if (e.GetKeyCode() == WXK_ESCAPE)
    {
        OnClose(wxCommandEvent());
    }
}
