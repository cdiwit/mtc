#pragma once
#include <wx/wx.h>
#include <wx/listctrl.h>
#include "core/ConfigManager.h"

class MainFrame : public wxFrame {
public:
    MainFrame();
    
private:
    // 控件
    wxListView* m_listView;
    wxButton* m_btnNew;
    wxButton* m_btnEdit;
    wxButton* m_btnDelete;
    wxButton* m_btnLaunch;
    wxButton* m_btnDuplicate;
    wxButton* m_btnImport;
    wxButton* m_btnExport;
    wxStatusBar* m_statusBar;
    
    // 初始化
    void CreateControls();
    void RefreshProfileList();
    void UpdateButtonStates();
    void UpdateStatusBar();
    
    // 事件处理
    void OnNewProfile(wxCommandEvent& event);
    void OnEditProfile(wxCommandEvent& event);
    void OnDeleteProfile(wxCommandEvent& event);
    void OnLaunchTerminal(wxCommandEvent& event);
    void OnDuplicateProfile(wxCommandEvent& event);
    void OnImport(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnListDoubleClick(wxListEvent& event);
    void OnListSelectionChanged(wxListEvent& event);
    void OnClose(wxCloseEvent& event);
    
    // 辅助方法
    int GetSelectedIndex();
    const Profile* GetSelectedProfile();
    
    wxDECLARE_EVENT_TABLE();
};

// 控件 ID
enum {
    ID_LIST_PROFILES = wxID_HIGHEST + 1,
    ID_BTN_NEW,
    ID_BTN_EDIT,
    ID_BTN_DELETE,
    ID_BTN_LAUNCH,
    ID_BTN_DUPLICATE,
    ID_BTN_IMPORT,
    ID_BTN_EXPORT
};
