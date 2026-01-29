#include "ProfileDialog.h"
#include "EnvVarPanel.h"
#include "core/TerminalLauncher.h"
#include <wx/dirdlg.h>
#include <wx/statline.h>

wxBEGIN_EVENT_TABLE(ProfileDialog, wxDialog)
    EVT_BUTTON(ID_PROFILE_BROWSE, ProfileDialog::OnBrowse)
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
    wxFlexGridSizer* formSizer = new wxFlexGridSizer(4, 2, 10, 15);
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
    
    // 工作目录
    formSizer->Add(new wxStaticText(panel, wxID_ANY, wxT("工作目录:")), 
                   0, wxALIGN_CENTER_VERTICAL);
    
    wxBoxSizer* workDirSizer = new wxBoxSizer(wxHORIZONTAL);
    m_txtWorkingDir = new wxTextCtrl(panel, ID_PROFILE_WORKDIR);
    m_btnBrowse = new wxButton(panel, ID_PROFILE_BROWSE, wxT("浏览..."), 
                                wxDefaultPosition, wxSize(70, -1));
    workDirSizer->Add(m_txtWorkingDir, 1, wxEXPAND | wxRIGHT, 5);
    workDirSizer->Add(m_btnBrowse, 0);
    formSizer->Add(workDirSizer, 1, wxEXPAND);
    
    // 终端类型
    formSizer->Add(new wxStaticText(panel, wxID_ANY, wxT("终端类型:")), 
                   0, wxALIGN_CENTER_VERTICAL);
    
    m_choiceTerminal = new wxChoice(panel, ID_PROFILE_TERMINAL);
    
    // 添加可用终端选项
    auto terminals = TerminalLauncher::GetAvailableTerminals();
    for (const auto& type : terminals) {
        m_choiceTerminal->Append(
            wxString::FromUTF8(TerminalLauncher::GetTerminalDisplayName(type)),
            new wxStringClientData(wxString::FromUTF8(TerminalTypeToString(type)))
        );
    }
    m_choiceTerminal->SetSelection(0);  // 默认选择"自动检测"
    
    formSizer->Add(m_choiceTerminal, 0, wxEXPAND);
    
    mainSizer->Add(formSizer, 0, wxEXPAND | wxALL, 15);
    
    // 分隔线
    mainSizer->Add(new wxStaticLine(panel), 0, wxEXPAND | wxLEFT | wxRIGHT, 15);
    
    // 环境变量面板
    m_envVarPanel = new EnvVarPanel(panel);
    mainSizer->Add(m_envVarPanel, 1, wxEXPAND | wxALL, 15);
    
    // 分隔线
    mainSizer->Add(new wxStaticLine(panel), 0, wxEXPAND | wxLEFT | wxRIGHT, 15);
    
    // 按钮
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();
    
    wxButton* btnCancel = new wxButton(panel, wxID_CANCEL, wxT("取消"));
    wxButton* btnOK = new wxButton(panel, wxID_OK, wxT("保存"));
    btnOK->SetDefault();
    
    btnSizer->Add(btnCancel, 0, wxRIGHT, 10);
    btnSizer->Add(btnOK, 0);
    
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 15);
    
    panel->SetSizer(mainSizer);
    
    // 对话框整体布局
    wxBoxSizer* dialogSizer = new wxBoxSizer(wxVERTICAL);
    dialogSizer->Add(panel, 1, wxEXPAND);
    SetSizer(dialogSizer);
    
    // 设置最小尺寸
    SetMinSize(wxSize(450, 450));
}

void ProfileDialog::InitializeFromProfile(const Profile& profile) {
    m_txtName->SetValue(wxString::FromUTF8(profile.name));
    m_txtDescription->SetValue(wxString::FromUTF8(profile.description));
    m_txtWorkingDir->SetValue(wxString::FromUTF8(profile.workingDirectory));
    
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
    
    // 获取终端类型
    int selection = m_choiceTerminal->GetSelection();
    if (selection >= 0) {
        wxStringClientData* data = static_cast<wxStringClientData*>(
            m_choiceTerminal->GetClientObject(selection));
        if (data) {
            profile.terminalType = StringToTerminalType(data->GetData().ToStdString());
        }
    }
    
    // 获取环境变量
    profile.environmentVariables = m_envVarPanel->GetEnvVariables();
    
    return profile;
}
