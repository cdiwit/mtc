#include "MainFrame.h"
#include "ProfileDialog.h"
#include "core/TerminalLauncher.h"
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/statline.h>

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON(ID_BTN_NEW, MainFrame::OnNewProfile)
    EVT_BUTTON(ID_BTN_EDIT, MainFrame::OnEditProfile)
    EVT_BUTTON(ID_BTN_DELETE, MainFrame::OnDeleteProfile)
    EVT_BUTTON(ID_BTN_LAUNCH, MainFrame::OnLaunchTerminal)
    EVT_BUTTON(ID_BTN_DUPLICATE, MainFrame::OnDuplicateProfile)
    EVT_BUTTON(ID_BTN_IMPORT, MainFrame::OnImport)
    EVT_BUTTON(ID_BTN_EXPORT, MainFrame::OnExport)
    EVT_LIST_ITEM_ACTIVATED(ID_LIST_PROFILES, MainFrame::OnListDoubleClick)
    EVT_LIST_ITEM_SELECTED(ID_LIST_PROFILES, MainFrame::OnListSelectionChanged)
    EVT_LIST_ITEM_DESELECTED(ID_LIST_PROFILES, MainFrame::OnListSelectionChanged)
    EVT_CLOSE(MainFrame::OnClose)
wxEND_EVENT_TABLE()

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, wxT("MTC - 终端环境管理器"), 
              wxDefaultPosition, wxSize(900, 600))
{
    // 设置最小尺寸
    SetMinSize(wxSize(700, 400));
    
    CreateControls();
    RefreshProfileList();
    UpdateButtonStates();
    UpdateStatusBar();
    
    // Set application icon (Windows only, macOS uses app bundle icon)
#ifdef __WXMSW__
    SetIcon(wxIcon(wxT("APP_ICON"), wxBITMAP_TYPE_ICO_RESOURCE));
#endif
    
    Centre();
}

void MainFrame::CreateControls() {
    wxPanel* panel = new wxPanel(this);
    
    // 设置背景色
    panel->SetBackgroundColour(wxColour(245, 245, 245));
    
    // 主布局
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // 左侧列表区域
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
    
    // 标题
    wxStaticText* titleLabel = new wxStaticText(panel, wxID_ANY, wxT("配置列表"));
    wxFont titleFont = titleLabel->GetFont();
    titleFont.SetPointSize(12);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    titleLabel->SetFont(titleFont);
    leftSizer->Add(titleLabel, 0, wxBOTTOM, 10);
    
    // ListView
    m_listView = new wxListView(panel, ID_LIST_PROFILES, 
                                wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL);
    
    m_listView->AppendColumn(wxT("名称"), wxLIST_FORMAT_LEFT, 150);
    m_listView->AppendColumn(wxT("描述"), wxLIST_FORMAT_LEFT, 180);
    m_listView->AppendColumn(wxT("工作目录"), wxLIST_FORMAT_LEFT, 220);
    m_listView->AppendColumn(wxT("终端"), wxLIST_FORMAT_LEFT, 120);
    
    leftSizer->Add(m_listView, 1, wxEXPAND);
    
    mainSizer->Add(leftSizer, 1, wxEXPAND | wxALL, 15);
    
    // 右侧按钮面板
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
    
    // 按钮样式
    wxSize btnSize(120, 32);
    
    m_btnNew = new wxButton(panel, ID_BTN_NEW, wxT("新建配置"), wxDefaultPosition, btnSize);
    m_btnEdit = new wxButton(panel, ID_BTN_EDIT, wxT("编辑"), wxDefaultPosition, btnSize);
    m_btnDelete = new wxButton(panel, ID_BTN_DELETE, wxT("删除"), wxDefaultPosition, btnSize);
    
    // 启动按钮特殊样式
    m_btnLaunch = new wxButton(panel, ID_BTN_LAUNCH, wxT("▶ 启动终端"), wxDefaultPosition, wxSize(120, 45));
    m_btnLaunch->SetBackgroundColour(wxColour(46, 204, 113));
    m_btnLaunch->SetForegroundColour(*wxWHITE);
    wxFont launchFont = m_btnLaunch->GetFont();
    launchFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_btnLaunch->SetFont(launchFont);
    
    m_btnDuplicate = new wxButton(panel, ID_BTN_DUPLICATE, wxT("复制"), wxDefaultPosition, btnSize);
    m_btnImport = new wxButton(panel, ID_BTN_IMPORT, wxT("导入..."), wxDefaultPosition, btnSize);
    m_btnExport = new wxButton(panel, ID_BTN_EXPORT, wxT("导出..."), wxDefaultPosition, btnSize);
    
    rightSizer->Add(m_btnNew, 0, wxEXPAND | wxBOTTOM, 8);
    rightSizer->Add(m_btnEdit, 0, wxEXPAND | wxBOTTOM, 8);
    rightSizer->Add(m_btnDelete, 0, wxEXPAND | wxBOTTOM, 20);
    
    rightSizer->Add(new wxStaticLine(panel, wxID_ANY, wxDefaultPosition, 
                                      wxDefaultSize, wxLI_HORIZONTAL), 
                    0, wxEXPAND | wxBOTTOM, 20);
    
    rightSizer->Add(m_btnLaunch, 0, wxEXPAND | wxBOTTOM, 20);
    
    rightSizer->Add(new wxStaticLine(panel, wxID_ANY, wxDefaultPosition, 
                                      wxDefaultSize, wxLI_HORIZONTAL), 
                    0, wxEXPAND | wxBOTTOM, 20);
    
    rightSizer->Add(m_btnDuplicate, 0, wxEXPAND | wxBOTTOM, 8);
    rightSizer->Add(m_btnImport, 0, wxEXPAND | wxBOTTOM, 8);
    rightSizer->Add(m_btnExport, 0, wxEXPAND);
    
    rightSizer->AddStretchSpacer();
    
    mainSizer->Add(rightSizer, 0, wxALL, 15);
    
    panel->SetSizer(mainSizer);
    
    // 状态栏
    m_statusBar = CreateStatusBar();
}

void MainFrame::RefreshProfileList() {
    m_listView->DeleteAllItems();
    
    const auto& profiles = ConfigManager::GetInstance().GetProfiles();
    
    for (size_t i = 0; i < profiles.size(); i++) {
        const auto& profile = profiles[i];
        
        long index = m_listView->InsertItem(static_cast<long>(i), 
                                             wxString::FromUTF8(profile.name));
        m_listView->SetItem(index, 1, wxString::FromUTF8(profile.description));
        m_listView->SetItem(index, 2, wxString::FromUTF8(profile.workingDirectory));
        m_listView->SetItem(index, 3, 
            wxString::FromUTF8(TerminalTypeDisplayName(profile.terminalType)));
    }
    
    UpdateStatusBar();
    UpdateButtonStates();
}

void MainFrame::UpdateButtonStates() {
    bool hasSelection = GetSelectedIndex() >= 0;
    m_btnEdit->Enable(hasSelection);
    m_btnDelete->Enable(hasSelection);
    m_btnLaunch->Enable(hasSelection);
    m_btnDuplicate->Enable(hasSelection);
    
    bool hasProfiles = !ConfigManager::GetInstance().GetProfiles().empty();
    m_btnExport->Enable(hasProfiles);
}

void MainFrame::UpdateStatusBar() {
    size_t count = ConfigManager::GetInstance().GetProfiles().size();
    m_statusBar->SetStatusText(wxString::Format(wxT("共 %zu 个配置"), count));
}

void MainFrame::OnNewProfile(wxCommandEvent& event) {
    ProfileDialog dlg(this, wxT("新建配置"));
    if (dlg.ShowModal() == wxID_OK) {
        Profile profile = dlg.GetProfile();
        ConfigManager::GetInstance().AddProfile(profile);
        RefreshProfileList();
    }
}

void MainFrame::OnEditProfile(wxCommandEvent& event) {
    const Profile* profile = GetSelectedProfile();
    if (!profile) return;
    
    ProfileDialog dlg(this, wxT("编辑配置"), *profile);
    if (dlg.ShowModal() == wxID_OK) {
        Profile updated = dlg.GetProfile();
        ConfigManager::GetInstance().UpdateProfile(profile->id, updated);
        RefreshProfileList();
    }
}

void MainFrame::OnDeleteProfile(wxCommandEvent& event) {
    const Profile* profile = GetSelectedProfile();
    if (!profile) return;
    
    int result = wxMessageBox(
        wxString::Format(wxT("确定要删除配置 \"%s\" 吗？"), 
                         wxString::FromUTF8(profile->name)),
        wxT("确认删除"),
        wxYES_NO | wxICON_QUESTION,
        this
    );
    
    if (result == wxYES) {
        ConfigManager::GetInstance().DeleteProfile(profile->id);
        RefreshProfileList();
    }
}

void MainFrame::OnLaunchTerminal(wxCommandEvent& event) {
    const Profile* profile = GetSelectedProfile();
    if (!profile) {
        wxMessageBox(wxT("请先选择一个配置"), wxT("提示"), 
                     wxOK | wxICON_INFORMATION, this);
        return;
    }
    
    std::string errorMsg;
    if (TerminalLauncher::Launch(*profile, &errorMsg)) {
        m_statusBar->SetStatusText(
            wxString::Format(wxT("终端已启动: %s"), 
                             wxString::FromUTF8(profile->name)));
    } else {
        wxString msg = wxT("启动终端失败，请检查配置");
        if (!errorMsg.empty()) {
            msg += wxT("\n详细错误: ") + wxString::FromUTF8(errorMsg);
        }
        
        wxMessageBox(msg, wxT("错误"), 
                     wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::OnDuplicateProfile(wxCommandEvent& event) {
    const Profile* profile = GetSelectedProfile();
    if (!profile) return;
    
    ConfigManager::GetInstance().DuplicateProfile(profile->id);
    RefreshProfileList();
    
    // 选中新复制的配置（最后一项）
    int lastIndex = m_listView->GetItemCount() - 1;
    if (lastIndex >= 0) {
        m_listView->Select(lastIndex);
        m_listView->EnsureVisible(lastIndex);
    }
}

void MainFrame::OnImport(wxCommandEvent& event) {
    wxFileDialog dlg(this, wxT("导入配置"), wxEmptyString, wxEmptyString,
                     wxT("JSON 文件 (*.json)|*.json"),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    
    if (dlg.ShowModal() == wxID_OK) {
        std::string path = dlg.GetPath().ToStdString();
        if (ConfigManager::GetInstance().ImportConfig(path)) {
            RefreshProfileList();
            wxMessageBox(wxT("配置导入成功"), wxT("提示"), 
                         wxOK | wxICON_INFORMATION, this);
        } else {
            wxMessageBox(wxT("配置导入失败，请检查文件格式"), wxT("错误"), 
                         wxOK | wxICON_ERROR, this);
        }
    }
}

void MainFrame::OnExport(wxCommandEvent& event) {
    wxFileDialog dlg(this, wxT("导出配置"), wxEmptyString, wxT("config.json"),
                     wxT("JSON 文件 (*.json)|*.json"),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    
    if (dlg.ShowModal() == wxID_OK) {
        std::string path = dlg.GetPath().ToStdString();
        if (ConfigManager::GetInstance().ExportConfig(path)) {
            wxMessageBox(wxT("配置导出成功"), wxT("提示"), 
                         wxOK | wxICON_INFORMATION, this);
        } else {
            wxMessageBox(wxT("配置导出失败"), wxT("错误"), 
                         wxOK | wxICON_ERROR, this);
        }
    }
}

void MainFrame::OnListDoubleClick(wxListEvent& event) {
    // 双击启动终端
    wxCommandEvent dummy;
    OnLaunchTerminal(dummy);
}

void MainFrame::OnListSelectionChanged(wxListEvent& event) {
    UpdateButtonStates();
}

void MainFrame::OnClose(wxCloseEvent& event) {
    // 保存配置
    ConfigManager::GetInstance().SaveConfig();
    event.Skip();
}

int MainFrame::GetSelectedIndex() {
    return m_listView->GetFirstSelected();
}

const Profile* MainFrame::GetSelectedProfile() {
    int index = GetSelectedIndex();
    if (index < 0) return nullptr;
    
    const auto& profiles = ConfigManager::GetInstance().GetProfiles();
    if (static_cast<size_t>(index) >= profiles.size()) return nullptr;
    
    return &profiles[static_cast<size_t>(index)];
}
