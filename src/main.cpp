#include <wx/wx.h>
#include <wx/snglinst.h>
#include "ui/MainFrame.h"
#include "core/ConfigManager.h"
#include "utils/PathUtils.h"

#ifdef __WXMSW__
#include <windows.h>
#endif

class MTCApp : public wxApp {
public:
    bool OnInit() override {
        wxLocale* locale = new wxLocale();
        locale->Init(wxLANGUAGE_DEFAULT);

        auto appDir = PathUtils::GetExecutableDir();

        // 基于程序目录的单实例检测，不同目录的程序各自独立
        wxString dirId = wxString(appDir).Lower();
        dirId.Replace("\\", "_");
        dirId.Replace("/", "_");
        dirId.Replace(":", "_");
        dirId.Replace(" ", "_");

        m_instanceChecker = new wxSingleInstanceChecker("MTC-" + dirId);

        if (m_instanceChecker->IsAnotherRunning()) {
            ActivateExistingWindow();
            delete m_instanceChecker;
            return false;
        }

        if (!ConfigManager::GetInstance().Initialize(appDir)) {
            wxMessageBox(wxT("无法初始化配置，程序将使用默认设置"),
                         wxT("警告"), wxOK | wxICON_WARNING);
        }

        MainFrame* frame = new MainFrame();
        frame->Show(true);

        return true;
    }

    int OnExit() override {
        delete m_instanceChecker;
        ConfigManager::GetInstance().SaveConfig();
        return wxApp::OnExit();
    }

private:
    wxSingleInstanceChecker* m_instanceChecker = nullptr;

    void ActivateExistingWindow() {
#ifdef __WXMSW__
        HWND hwnd = ::FindWindow(NULL, wxString(wxT("MTC - 终端环境管理器")).wc_str());
        if (hwnd) {
            if (::IsIconic(hwnd)) {
                ::ShowWindow(hwnd, SW_RESTORE);
            }
            ::SetForegroundWindow(hwnd);
            ::BringWindowToTop(hwnd);
        }
#endif
    }
};

wxIMPLEMENT_APP(MTCApp);
