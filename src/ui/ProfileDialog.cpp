#include "ProfileDialog.h"
#include "EnvVarPanel.h"
#include "core/TerminalLauncher.h"
#include "utils/PathUtils.h"
#include <wx/dirdlg.h>
#include <wx/statline.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <cstdlib>
#elif defined(__linux__)
#include <cstdlib>
#endif

wxBEGIN_EVENT_TABLE(ProfileDialog, wxDialog)
    EVT_BUTTON(ID_PROFILE_BROWSE, ProfileDialog::OnBrowse)
    EVT_BUTTON(ID_PROFILE_BROWSE_LINUX, ProfileDialog::OnBrowseLinux)
    EVT_BUTTON(ID_PROFILE_BROWSE_MAC, ProfileDialog::OnBrowseMac)
    EVT_BUTTON(ID_PROFILE_OPEN_WORKDIR, ProfileDialog::OnOpenWorkDir)
    EVT_BUTTON(wxID_OK, ProfileDialog::OnOK)
wxEND_EVENT_TABLE()

ProfileDialog::ProfileDialog(wxWindow* parent, const wxString& title)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(550, 550),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_isEdit(false)
{
    CreateControls();
    Centre();
}

ProfileDialog::ProfileDialog(wxWindow* parent, const wxString& title, const Profile& profile)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(550, 550),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_originalProfile(profile)
    , m_isEdit(true)
{
    CreateControls();
    InitializeFromProfile(profile);
    Centre();
}

void ProfileDialog::CreateControls() {
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 基本信息区域
    wxFlexGridSizer* formSizer = new wxFlexGridSizer(7, 2, 8, 15);
    formSizer->AddGrowableCol(1, 1);

    // 名称
    formSizer->Add(new wxStaticText(panel, wxID_ANY, wxT("配置名称:")),
                   0, wxALIGN_CENTER_VERTICAL);
    m_txtName = new wxTextCtrl(panel, ID_PROFILE_NAME);
    formSizer->Add(m_txtName, 1, wxEXPAND);

    // 描述
    formSizer->Add(new wxStaticText(panel, wxID_ANY, wxT("描述:")),
                   0, wxALIGN_CENTER_VERTICAL);
    m_txtDescription = new wxTextCtrl(panel, ID_PROFILE_DESC);
    formSizer->Add(m_txtDescription, 1, wxEXPAND);

    // Windows 工作目录
    formSizer->Add(new wxStaticText(panel, wxID_ANY, wxT("Windows 工作目录:")),
                   0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* workDirSizer = new wxBoxSizer(wxHORIZONTAL);
    m_txtWorkingDir = new wxTextCtrl(panel, ID_PROFILE_WORKDIR);
    m_btnBrowse = new wxButton(panel, ID_PROFILE_BROWSE, wxT("..."),
                                wxDefaultPosition, wxSize(36, -1));
    workDirSizer->Add(m_txtWorkingDir, 1, wxEXPAND | wxRIGHT, 3);
    workDirSizer->Add(m_btnBrowse, 0);
    formSizer->Add(workDirSizer, 1, wxEXPAND);

    // Linux 工作目录
    formSizer->Add(new wxStaticText(panel, wxID_ANY, wxT("Linux 工作目录:")),
                   0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* linuxDirSizer = new wxBoxSizer(wxHORIZONTAL);
    m_txtLinuxWorkingDir = new wxTextCtrl(panel, ID_PROFILE_LINUX_WORKDIR);
    m_btnBrowseLinux = new wxButton(panel, ID_PROFILE_BROWSE_LINUX, wxT("..."),
                                     wxDefaultPosition, wxSize(36, -1));
    linuxDirSizer->Add(m_txtLinuxWorkingDir, 1, wxEXPAND | wxRIGHT, 3);
    linuxDirSizer->Add(m_btnBrowseLinux, 0);
    formSizer->Add(linuxDirSizer, 1, wxEXPAND);

    // macOS 工作目录
    formSizer->Add(new wxStaticText(panel, wxID_ANY, wxT("macOS 工作目录:")),
                   0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* macDirSizer = new wxBoxSizer(wxHORIZONTAL);
    m_txtMacWorkingDir = new wxTextCtrl(panel, ID_PROFILE_MAC_WORKDIR);
    m_btnBrowseMac = new wxButton(panel, ID_PROFILE_BROWSE_MAC, wxT("..."),
                                   wxDefaultPosition, wxSize(36, -1));
    macDirSizer->Add(m_txtMacWorkingDir, 1, wxEXPAND | wxRIGHT, 3);
    macDirSizer->Add(m_btnBrowseMac, 0);
    formSizer->Add(macDirSizer, 1, wxEXPAND);

    // 终端类型
    formSizer->Add(new wxStaticText(panel, wxID_ANY, wxT("终端类型:")),
                   0, wxALIGN_CENTER_VERTICAL);
    m_choiceTerminal = new wxChoice(panel, ID_PROFILE_TERMINAL);
    auto terminals = TerminalLauncher::GetAvailableTerminals();
    for (const auto& type : terminals) {
        m_choiceTerminal->Append(
            wxString::FromUTF8(TerminalLauncher::GetTerminalDisplayName(type)),
            new wxStringClientData(wxString::FromUTF8(TerminalTypeToString(type)))
        );
    }
    m_choiceTerminal->SetSelection(0);
    formSizer->Add(m_choiceTerminal, 0, wxEXPAND);

    mainSizer->Add(formSizer, 0, wxEXPAND | wxALL, 15);

    // 分隔线
    mainSizer->Add(new wxStaticLine(panel), 0, wxEXPAND | wxLEFT | wxRIGHT, 15);

    // 环境变量面板
    m_envVarPanel = new EnvVarPanel(panel);
    mainSizer->Add(m_envVarPanel, 1, wxEXPAND | wxALL, 15);

    // 分隔线
    mainSizer->Add(new wxStaticLine(panel), 0, wxEXPAND | wxLEFT | wxRIGHT, 15);

    // 按钮区域
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_btnOpenWorkDir = new wxButton(panel, ID_PROFILE_OPEN_WORKDIR, wxT("打开工作目录"));
    btnSizer->Add(m_btnOpenWorkDir, 0);
    btnSizer->AddStretchSpacer();

    wxButton* btnCancel = new wxButton(panel, wxID_CANCEL, wxT("取消"));
    wxButton* btnOK = new wxButton(panel, wxID_OK, wxT("保存"));
    btnOK->SetDefault();

    btnSizer->Add(btnCancel, 0, wxRIGHT, 10);
    btnSizer->Add(btnOK, 0);

    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 15);

    panel->SetSizer(mainSizer);

    wxBoxSizer* dialogSizer = new wxBoxSizer(wxVERTICAL);
    dialogSizer->Add(panel, 1, wxEXPAND);
    SetSizer(dialogSizer);

    SetMinSize(wxSize(500, 520));
}

void ProfileDialog::InitializeFromProfile(const Profile& profile) {
    m_txtName->SetValue(wxString::FromUTF8(profile.name));
    m_txtDescription->SetValue(wxString::FromUTF8(profile.description));
    m_txtWorkingDir->SetValue(wxString::FromUTF8(profile.workingDirectory));
    m_txtLinuxWorkingDir->SetValue(wxString::FromUTF8(profile.linuxWorkingDirectory));
    m_txtMacWorkingDir->SetValue(wxString::FromUTF8(profile.macWorkingDirectory));
    
    // 设置终端类型
    std::string terminalStr = TerminalTypeToString(profile.terminalType);
    for (unsigned int i = 0; i < m_choiceTerminal->GetCount(); i++) {
        wxStringClientData* data = static_cast<wxStringClientData*>(
            m_choiceTerminal->GetClientObject(i));
        if (data && data->GetData() == wxString::FromUTF8(terminalStr)) {
            m_choiceTerminal->SetSelection(i);
            break;
        }
    }
    
    // 设置环境变量
    m_envVarPanel->SetEnvVariables(profile.environmentVariables);
}

void ProfileDialog::OnBrowse(wxCommandEvent& event) {
    wxDirDialog dlg(this, wxT("选择工作目录"), m_txtWorkingDir->GetValue(),
                    wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    
    if (dlg.ShowModal() == wxID_OK) {
        m_txtWorkingDir->SetValue(dlg.GetPath());
    }
}

void ProfileDialog::OnOK(wxCommandEvent& event) {
    // 验证
    wxString name = m_txtName->GetValue().Trim().Trim(false);
    if (name.IsEmpty()) {
        wxMessageBox(wxT("请输入配置名称"), wxT("验证失败"), 
                     wxOK | wxICON_WARNING, this);
        m_txtName->SetFocus();
        return;
    }
    
    EndModal(wxID_OK);
}

Profile ProfileDialog::GetProfile() const {
    Profile profile = m_originalProfile;

    profile.name = m_txtName->GetValue().ToStdString();
    profile.description = m_txtDescription->GetValue().ToStdString();
    profile.workingDirectory = m_txtWorkingDir->GetValue().ToStdString();
    profile.linuxWorkingDirectory = m_txtLinuxWorkingDir->GetValue().ToStdString();
    profile.macWorkingDirectory = m_txtMacWorkingDir->GetValue().ToStdString();

    int selection = m_choiceTerminal->GetSelection();
    if (selection >= 0) {
        wxStringClientData* data = static_cast<wxStringClientData*>(
            m_choiceTerminal->GetClientObject(selection));
        if (data) {
            profile.terminalType = StringToTerminalType(data->GetData().ToStdString());
        }
    }

    profile.environmentVariables = m_envVarPanel->GetEnvVariables();

    return profile;
}

void ProfileDialog::OnBrowseLinux(wxCommandEvent& event) {
    wxDirDialog dlg(this, wxT("选择 Linux 工作目录"), m_txtLinuxWorkingDir->GetValue(),
                    wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        m_txtLinuxWorkingDir->SetValue(dlg.GetPath());
    }
}

void ProfileDialog::OnBrowseMac(wxCommandEvent& event) {
    wxDirDialog dlg(this, wxT("选择 macOS 工作目录"), m_txtMacWorkingDir->GetValue(),
                    wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        m_txtMacWorkingDir->SetValue(dlg.GetPath());
    }
}

void ProfileDialog::OnOpenWorkDir(wxCommandEvent& event) {
    std::string dir;
#ifdef _WIN32
    dir = m_txtWorkingDir->GetValue().ToStdString();
#elif defined(__linux__)
    dir = m_txtLinuxWorkingDir->GetValue().ToStdString();
#elif defined(__APPLE__)
    dir = m_txtMacWorkingDir->GetValue().ToStdString();
#endif

    if (dir.empty()) {
        wxMessageBox(wxT("当前平台的工作目录为空"), wxT("提示"), wxOK | wxICON_INFORMATION, this);
        return;
    }

    if (!fs::exists(dir)) {
        wxMessageBox(wxString::Format(wxT("目录不存在:\n%s"), wxString::FromUTF8(dir)),
                     wxT("错误"), wxOK | wxICON_WARNING, this);
        return;
    }

#ifdef _WIN32
    ShellExecuteW(nullptr, L"explore", wxString::FromUTF8(dir).wc_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    system(("open \"" + dir + "\"").c_str());
#elif defined(__linux__)
    system(("xdg-open \"" + dir + "\"").c_str());
#endif
}
