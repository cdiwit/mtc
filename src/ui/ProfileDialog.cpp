#include "ProfileDialog.h"
#include "EnvVarPanel.h"
#include "RemoteFileBrowserDialog.h"
#include "core/ConfigManager.h"
#include "core/TerminalLauncher.h"
#include "utils/PathUtils.h"
#include <wx/dirdlg.h>
#include <wx/statline.h>
#include <wx/tokenzr.h>

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
    EVT_BUTTON(ID_PROFILE_REMOTE_BROWSE, ProfileDialog::OnRemoteBrowse)
    EVT_CHOICE(ID_PROFILE_SSH_HOST, ProfileDialog::OnRemoteSshHostChanged)
    EVT_CHOICE(ID_PROFILE_CREDENTIAL, ProfileDialog::OnRemoteCredentialChanged)
    EVT_BUTTON(wxID_OK, ProfileDialog::OnOK)
wxEND_EVENT_TABLE()

ProfileDialog::ProfileDialog(wxWindow* parent, const wxString& title)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(560, 520),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_isEdit(false)
{
    CreateControls();
    Centre();
}

ProfileDialog::ProfileDialog(wxWindow* parent, const wxString& title, const Profile& profile)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(560, 520),
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

    // Notebook (选项卡)
    m_notebook = new wxNotebook(panel, wxID_ANY);

    // ===== 选项卡 1: 基本信息 =====
    wxPanel* basicTab = new wxPanel(m_notebook);
    wxBoxSizer* basicSizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* formSizer = new wxFlexGridSizer(7, 2, 8, 15);
    formSizer->AddGrowableCol(1, 1);

    // 名称
    formSizer->Add(new wxStaticText(basicTab, wxID_ANY, wxT("配置名称:")),
                   0, wxALIGN_CENTER_VERTICAL);
    m_txtName = new wxTextCtrl(basicTab, ID_PROFILE_NAME);
    formSizer->Add(m_txtName, 1, wxEXPAND);

    // 描述
    formSizer->Add(new wxStaticText(basicTab, wxID_ANY, wxT("描述:")),
                   0, wxALIGN_CENTER_VERTICAL);
    m_txtDescription = new wxTextCtrl(basicTab, ID_PROFILE_DESC);
    formSizer->Add(m_txtDescription, 1, wxEXPAND);

    // Windows 工作目录
    formSizer->Add(new wxStaticText(basicTab, wxID_ANY, wxT("Windows 工作目录:")),
                   0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* workDirSizer = new wxBoxSizer(wxHORIZONTAL);
    m_txtWorkingDir = new wxTextCtrl(basicTab, ID_PROFILE_WORKDIR);
    m_btnBrowse = new wxButton(basicTab, ID_PROFILE_BROWSE, wxT("..."),
                                wxDefaultPosition, wxSize(36, -1));
    workDirSizer->Add(m_txtWorkingDir, 1, wxEXPAND | wxRIGHT, 3);
    workDirSizer->Add(m_btnBrowse, 0);
    formSizer->Add(workDirSizer, 1, wxEXPAND);

    // Linux 工作目录
    formSizer->Add(new wxStaticText(basicTab, wxID_ANY, wxT("Linux 工作目录:")),
                   0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* linuxDirSizer = new wxBoxSizer(wxHORIZONTAL);
    m_txtLinuxWorkingDir = new wxTextCtrl(basicTab, ID_PROFILE_LINUX_WORKDIR);
    m_btnBrowseLinux = new wxButton(basicTab, ID_PROFILE_BROWSE_LINUX, wxT("..."),
                                     wxDefaultPosition, wxSize(36, -1));
    linuxDirSizer->Add(m_txtLinuxWorkingDir, 1, wxEXPAND | wxRIGHT, 3);
    linuxDirSizer->Add(m_btnBrowseLinux, 0);
    formSizer->Add(linuxDirSizer, 1, wxEXPAND);

    // macOS 工作目录
    formSizer->Add(new wxStaticText(basicTab, wxID_ANY, wxT("macOS 工作目录:")),
                   0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* macDirSizer = new wxBoxSizer(wxHORIZONTAL);
    m_txtMacWorkingDir = new wxTextCtrl(basicTab, ID_PROFILE_MAC_WORKDIR);
    m_btnBrowseMac = new wxButton(basicTab, ID_PROFILE_BROWSE_MAC, wxT("..."),
                                   wxDefaultPosition, wxSize(36, -1));
    macDirSizer->Add(m_txtMacWorkingDir, 1, wxEXPAND | wxRIGHT, 3);
    macDirSizer->Add(m_btnBrowseMac, 0);
    formSizer->Add(macDirSizer, 1, wxEXPAND);

    // 终端类型
    formSizer->Add(new wxStaticText(basicTab, wxID_ANY, wxT("终端类型:")),
                   0, wxALIGN_CENTER_VERTICAL);
    m_choiceTerminal = new wxChoice(basicTab, ID_PROFILE_TERMINAL);
    auto terminals = TerminalLauncher::GetAvailableTerminals();
    for (const auto& type : terminals) {
        m_choiceTerminal->Append(
            wxString::FromUTF8(TerminalLauncher::GetTerminalDisplayName(type)),
            new wxStringClientData(wxString::FromUTF8(TerminalTypeToString(type)))
        );
    }
    m_choiceTerminal->SetSelection(0);
    formSizer->Add(m_choiceTerminal, 0, wxEXPAND);

    // 启动命令
    formSizer->Add(new wxStaticText(basicTab, wxID_ANY, wxT("启动命令:")),
                   0, wxALIGN_CENTER_VERTICAL);
    m_txtStartupCommands = new wxTextCtrl(basicTab, wxID_ANY, wxEmptyString,
                                           wxDefaultPosition, wxDefaultSize,
                                           wxTE_MULTILINE);
    m_txtStartupCommands->SetMinSize(wxSize(-1, 80));
    formSizer->Add(m_txtStartupCommands, 1, wxEXPAND);

    basicSizer->Add(formSizer, 1, wxEXPAND | wxALL, 10);
    basicTab->SetSizer(basicSizer);

    // ===== 选项卡 2: 环境变量 =====
    wxPanel* envTab = new wxPanel(m_notebook);
    wxBoxSizer* envSizer = new wxBoxSizer(wxVERTICAL);

    m_envVarPanel = new EnvVarPanel(envTab);
    envSizer->Add(m_envVarPanel, 1, wxEXPAND);

    envTab->SetSizer(envSizer);

    // ===== 选项卡 3: 远程/SSH =====
    wxPanel* remoteTab = new wxPanel(m_notebook);
    wxBoxSizer* remoteSizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* remoteForm = new wxFlexGridSizer(3, 2, 8, 12);
    remoteForm->AddGrowableCol(1, 1);

    remoteForm->Add(new wxStaticText(remoteTab, wxID_ANY, wxT("SSH 主机:")),
                    0, wxALIGN_CENTER_VERTICAL);
    m_choiceSshHost = new wxChoice(remoteTab, ID_PROFILE_SSH_HOST);
    remoteForm->Add(m_choiceSshHost, 1, wxEXPAND);

    remoteForm->Add(new wxStaticText(remoteTab, wxID_ANY, wxT("凭据:")),
                    0, wxALIGN_CENTER_VERTICAL);
    m_choiceCredential = new wxChoice(remoteTab, ID_PROFILE_CREDENTIAL);
    remoteForm->Add(m_choiceCredential, 1, wxEXPAND);

    remoteForm->Add(new wxStaticText(remoteTab, wxID_ANY, wxT("远程工作目录:")),
                    0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* remoteDirSizer = new wxBoxSizer(wxHORIZONTAL);
    m_txtRemoteDir = new wxTextCtrl(remoteTab, ID_PROFILE_REMOTE_DIR);
    m_txtRemoteDir->SetHint(wxT("如 /home/user/project"));
    m_btnRemoteBrowse = new wxButton(remoteTab, ID_PROFILE_REMOTE_BROWSE, wxT("远程选择..."));
    remoteDirSizer->Add(m_txtRemoteDir, 1, wxEXPAND | wxRIGHT, 5);
    remoteDirSizer->Add(m_btnRemoteBrowse, 0);
    remoteForm->Add(remoteDirSizer, 1, wxEXPAND);

    remoteSizer->Add(remoteForm, 0, wxEXPAND | wxALL, 10);

    auto* remoteHint = new wxStaticText(remoteTab, wxID_ANY,
        wxT("提示：SSH 主机与凭据均为空时，启动本地终端。\n")
        wxT("选择 SSH 主机后，点击\"远程选择...\"可浏览远端目录并选定初始路径。\n")
        wxT("密码认证的密码仅用于文件浏览；终端会话用私钥(-i)或交互式输入。"));
    remoteHint->Wrap(460);
    remoteSizer->Add(remoteHint, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    remoteTab->SetSizer(remoteSizer);

    // 添加选项卡
    m_notebook->AddPage(basicTab, wxT("基本信息"));
    m_notebook->AddPage(envTab, wxT("环境变量"));
    m_notebook->AddPage(remoteTab, wxT("远程/SSH"));

    // 填充远程下拉（默认无选择）
    PopulateRemoteChoices("", "");
    UpdateRemoteBrowseState();

    mainSizer->Add(m_notebook, 1, wxEXPAND | wxALL, 10);

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

    mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    panel->SetSizer(mainSizer);

    wxBoxSizer* dialogSizer = new wxBoxSizer(wxVERTICAL);
    dialogSizer->Add(panel, 1, wxEXPAND);
    SetSizer(dialogSizer);

    SetMinSize(wxSize(500, 480));
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

    // 设置启动命令
    wxString cmds;
    for (size_t i = 0; i < profile.startupCommands.size(); i++) {
        if (i > 0) cmds << wxT("\n");
        cmds << wxString::FromUTF8(profile.startupCommands[i]);
    }
    m_txtStartupCommands->SetValue(cmds);

    // 远程/SSH
    PopulateRemoteChoices(profile.sshHostId, profile.credentialId);
    m_txtRemoteDir->SetValue(wxString::FromUTF8(profile.remoteWorkingDirectory));
    UpdateRemoteBrowseState();
}

void ProfileDialog::PopulateRemoteChoices(const std::string& selectedHostId,
                                          const std::string& selectedCredId) {
    // SSH 主机下拉
    m_choiceSshHost->Clear();
    m_choiceSshHost->Append(wxT("无（本地终端）"), new wxStringClientData(wxEmptyString));
    int hostSel = 0;
    int hostIdx = 1;
    for (const auto& h : ConfigManager::GetInstance().GetSshHosts()) {
        wxString label = wxString::FromUTF8(h.name);
        if (!h.username.empty() || !h.host.empty()) {
            label += wxT("  (");
            label += wxString::FromUTF8(h.username.empty() ? h.host : (h.username + "@" + h.host));
            label += wxT(")");
        }
        m_choiceSshHost->Append(label, new wxStringClientData(wxString::FromUTF8(h.id)));
        if (h.id == selectedHostId) hostSel = hostIdx;
        ++hostIdx;
    }
    m_choiceSshHost->SetSelection(hostSel);

    // 凭据下拉
    m_choiceCredential->Clear();
    m_choiceCredential->Append(wxT("无"), new wxStringClientData(wxEmptyString));
    int credSel = 0;
    int credIdx = 1;
    for (const auto& c : ConfigManager::GetInstance().GetCredentials()) {
        wxString label = wxString::FromUTF8(c.name);
        label += wxT("  [") + wxString::FromUTF8(CredentialTypeDisplayName(c.type)) + wxT("]");
        m_choiceCredential->Append(label, new wxStringClientData(wxString::FromUTF8(c.id)));
        if (c.id == selectedCredId) credSel = credIdx;
        ++credIdx;
    }
    m_choiceCredential->SetSelection(credSel);
}

std::string ProfileDialog::GetSelectedSshHostId() const {
    int sel = m_choiceSshHost->GetSelection();
    if (sel <= 0) return "";
    auto* data = static_cast<wxStringClientData*>(m_choiceSshHost->GetClientObject(sel));
    return data ? data->GetData().ToStdString() : "";
}

std::string ProfileDialog::GetSelectedCredentialId() const {
    int sel = m_choiceCredential->GetSelection();
    if (sel <= 0) return "";
    auto* data = static_cast<wxStringClientData*>(m_choiceCredential->GetClientObject(sel));
    return data ? data->GetData().ToStdString() : "";
}

void ProfileDialog::UpdateRemoteBrowseState() {
    // 远程浏览需要能连上：需要同时选定主机与凭据
    bool canBrowse = !GetSelectedSshHostId().empty() && !GetSelectedCredentialId().empty();
    m_btnRemoteBrowse->Enable(canBrowse);
}

void ProfileDialog::OnRemoteSshHostChanged(wxCommandEvent& event) {
    UpdateRemoteBrowseState();
}

void ProfileDialog::OnRemoteCredentialChanged(wxCommandEvent& event) {
    UpdateRemoteBrowseState();
}

void ProfileDialog::OnRemoteBrowse(wxCommandEvent& event) {
    std::string hostId = GetSelectedSshHostId();
    std::string credId = GetSelectedCredentialId();
    if (hostId.empty() || credId.empty()) {
        wxMessageBox(wxT("请先选择 SSH 主机与凭据"), wxT("提示"),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    const SshHost* host = ConfigManager::GetInstance().GetSshHost(hostId);
    const Credential* cred = ConfigManager::GetInstance().GetCredential(credId);
    if (!host || !cred) return;

    std::string startPath = m_txtRemoteDir->GetValue().ToStdString();
    RemoteFileBrowserDialog dlg(this, *host, cred, startPath,
                                RemoteBrowserMode::PickDir);
    if (dlg.ShowModal() == wxID_OK) {
        std::string dir = dlg.GetSelectedDir();
        if (!dir.empty()) {
            m_txtRemoteDir->SetValue(wxString::FromUTF8(dir));
        }
    }
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

    // 远程/SSH
    profile.sshHostId = GetSelectedSshHostId();
    profile.credentialId = GetSelectedCredentialId();
    profile.remoteWorkingDirectory = m_txtRemoteDir->GetValue().ToStdString();

    // 解析启动命令（每行一条）
    profile.startupCommands.clear();
    wxString cmds = m_txtStartupCommands->GetValue();
    // 统一换行符，按 \n 分割
    cmds.Replace(wxT("\r\n"), wxT("\n"));
    cmds.Replace(wxT("\r"), wxT("\n"));
    size_t pos = 0;
    while (pos < cmds.size()) {
        size_t end = cmds.find('\n', pos);
        if (end == wxString::npos) end = cmds.size();
        wxString line = cmds.SubString(pos, end - 1).Trim().Trim(false);
        // 修复 macOS 自动纠正：将 em dash(—) 和 en dash(–) 还原为双连字符(--)
        line.Replace(wxT("—"), wxT("--"));
        line.Replace(wxT("–"), wxT("--"));
        if (!line.IsEmpty()) {
            profile.startupCommands.push_back(line.ToStdString());
        }
        pos = end + 1;
    }

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
