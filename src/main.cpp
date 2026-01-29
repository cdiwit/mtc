#include <wx/wx.h>
#include "ui/MainFrame.h"
#include "core/ConfigManager.h"
#include "utils/PathUtils.h"

class MTCApp : public wxApp {
public:
    bool OnInit() override {
        // 设置程序语言环境
        wxLocale* locale = new wxLocale();
        locale->Init(wxLANGUAGE_DEFAULT);
        
        // 获取程序所在目录
        auto appDir = PathUtils::GetExecutableDir();
        
        // 初始化配置管理器
        if (!ConfigManager::GetInstance().Initialize(appDir)) {
            wxMessageBox(wxT("无法初始化配置，程序将使用默认设置"), 
                         wxT("警告"), wxOK | wxICON_WARNING);
        }
        
        // 创建主窗口
        MainFrame* frame = new MainFrame();
        frame->Show(true);
        
        return true;
    }
    
    int OnExit() override {
        // 保存配置
        ConfigManager::GetInstance().SaveConfig();
        return wxApp::OnExit();
    }
};

wxIMPLEMENT_APP(MTCApp);
