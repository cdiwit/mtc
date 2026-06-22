#pragma once
#include <wx/wx.h>
#include <wx/notebook.h>
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
    wxTextCtrl* m_txtLinuxWorkingDir;
    wxTextCtrl* m_txtMacWorkingDir;
    wxButton* m_btnBrowse;
    wxButton* m_btnBrowseLinux;
    wxButton* m_btnBrowseMac;
    wxButton* m_btnOpenWorkDir;
    wxChoice* m_choiceTerminal;
    wxTextCtrl* m_txtStartupCommands;
    wxNotebook* m_notebook;
    EnvVarPanel* m_envVarPanel;
    wxChoice* m_choiceSshHost;        // 远程页签：SSH 主机
    wxChoice* m_choiceCredential;     // 远程页签：凭据
    wxTextCtrl* m_txtRemoteDir;       // 远程页签：远程工作目录
    wxButton* m_btnRemoteBrowse;      // 远程页签：远程选择目录

    // 原始配置
    Profile m_originalProfile;
    bool m_isEdit;

    void CreateControls();
    void InitializeFromProfile(const Profile& profile);
    void PopulateRemoteChoices(const std::string& selectedHostId,
                               const std::string& selectedCredId);
    std::string GetSelectedSshHostId() const;
    std::string GetSelectedCredentialId() const;
    void UpdateRemoteBrowseState();

    void OnBrowse(wxCommandEvent& event);
    void OnBrowseLinux(wxCommandEvent& event);
    void OnBrowseMac(wxCommandEvent& event);
    void OnOpenWorkDir(wxCommandEvent& event);
    void OnRemoteSshHostChanged(wxCommandEvent& event);
    void OnRemoteCredentialChanged(wxCommandEvent& event);
    void OnRemoteBrowse(wxCommandEvent& event);
    void OnOK(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};

// 控件 ID
enum ProfileDialogIds {
    ID_PROFILE_NAME = wxID_HIGHEST + 200,
    ID_PROFILE_DESC,
    ID_PROFILE_WORKDIR,
    ID_PROFILE_LINUX_WORKDIR,
    ID_PROFILE_MAC_WORKDIR,
    ID_PROFILE_BROWSE,
    ID_PROFILE_BROWSE_LINUX,
    ID_PROFILE_BROWSE_MAC,
    ID_PROFILE_OPEN_WORKDIR,
    ID_PROFILE_TERMINAL,
    ID_PROFILE_SSH_HOST,
    ID_PROFILE_CREDENTIAL,
    ID_PROFILE_REMOTE_DIR,
    ID_PROFILE_REMOTE_BROWSE
};
