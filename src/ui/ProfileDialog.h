#pragma once
#include <wx/wx.h>
#include "core/Types.h"

class EnvVarPanel;

// 配置编辑对话框
class ProfileDialog : public wxDialog {
public:
    ProfileDialog(wxWindow* parent, const wxString& title);
    ProfileDialog(wxWindow* parent, const wxString& title, const Profile& profile);
    
    // 获取编辑后的配置
    Profile GetProfile() const;
    
private:
    // 控件
    wxTextCtrl* m_txtName;
    wxTextCtrl* m_txtDescription;
    wxTextCtrl* m_txtWorkingDir;
    wxButton* m_btnBrowse;
    wxChoice* m_choiceTerminal;
    EnvVarPanel* m_envVarPanel;
    
    // 原始配置
    Profile m_originalProfile;
    bool m_isEdit;
    
    void CreateControls();
    void InitializeFromProfile(const Profile& profile);
    
    void OnBrowse(wxCommandEvent& event);
    void OnOK(wxCommandEvent& event);
    
    wxDECLARE_EVENT_TABLE();
};

// 控件 ID
enum ProfileDialogIds {
    ID_PROFILE_NAME = wxID_HIGHEST + 200,
    ID_PROFILE_DESC,
    ID_PROFILE_WORKDIR,
    ID_PROFILE_BROWSE,
    ID_PROFILE_TERMINAL
};
