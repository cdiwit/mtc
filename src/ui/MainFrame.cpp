#include "MainFrame.h"
#include "ProfileDialog.h"
#include "core/TerminalLauncher.h"
#include "ui/ProfileTreeBuilder.h"
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/statline.h>
#include <wx/menu.h>
#include <wx/choicdlg.h>

#include <algorithm>
#include <utility>

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON(ID_BTN_NEW, MainFrame::OnNewProfile)
    EVT_BUTTON(ID_BTN_EDIT, MainFrame::OnEditProfile)
    EVT_BUTTON(ID_BTN_DELETE, MainFrame::OnDeleteProfile)
    EVT_BUTTON(ID_BTN_LAUNCH, MainFrame::OnLaunchTerminal)
    EVT_BUTTON(ID_BTN_DUPLICATE, MainFrame::OnDuplicateProfile)
    EVT_BUTTON(ID_BTN_IMPORT, MainFrame::OnImport)
    EVT_BUTTON(ID_BTN_EXPORT, MainFrame::OnExport)
    EVT_BUTTON(ID_BTN_OPEN_DIR, MainFrame::OnOpenWorkDir)
    EVT_TEXT(ID_SEARCH_CTRL, MainFrame::OnSearchTextChanged)
    EVT_TEXT_ENTER(ID_SEARCH_CTRL, MainFrame::OnSearchEnter)
    EVT_BUTTON(ID_BTN_CLEAR_SEARCH, MainFrame::OnClearSearch)
    EVT_BUTTON(ID_BTN_SEARCH_HISTORY, MainFrame::OnSearchHistoryClicked)
    EVT_LIST_ITEM_ACTIVATED(ID_LIST_PROFILES, MainFrame::OnListDoubleClick)
    EVT_LIST_ITEM_SELECTED(ID_LIST_PROFILES, MainFrame::OnListSelectionChanged)
    EVT_LIST_ITEM_DESELECTED(ID_LIST_PROFILES, MainFrame::OnListSelectionChanged)
    EVT_CLOSE(MainFrame::OnClose)
    EVT_SYS_COLOUR_CHANGED(MainFrame::OnSysColourChanged)
wxEND_EVENT_TABLE()

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, wxT("MTC - 终端环境管理器"),
              wxDefaultPosition, wxSize(1000, 620)) {
    SetMinSize(wxSize(760, 460));

    CreateControls();
    RefreshView();
    UpdateButtonStates();
    UpdateStatusBar();

#ifdef __WXMSW__
    SetIcon(wxIcon(wxT("APP_ICON"), wxBITMAP_TYPE_ICO_RESOURCE));
#endif

    Centre();
}

void MainFrame::CreateControls() {
    wxPanel* panel = new wxPanel(this);

    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);

    // 左侧面板
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* sideTitle = new wxStaticText(panel, wxID_ANY, wxT("视图与操作"));
    wxFont titleFont = sideTitle->GetFont();
    titleFont.SetPointSize(11);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    sideTitle->SetFont(titleFont);
    leftSizer->Add(sideTitle, 0, wxBOTTOM, 10);

    wxSize btnSize(140, 32);
    m_btnNew = new wxButton(panel, ID_BTN_NEW, wxT("新建配置"), wxDefaultPosition, btnSize);
    m_btnEdit = new wxButton(panel, ID_BTN_EDIT, wxT("编辑"), wxDefaultPosition, btnSize);
    m_btnDelete = new wxButton(panel, ID_BTN_DELETE, wxT("删除"), wxDefaultPosition, btnSize);

    m_btnLaunch = new wxButton(panel, ID_BTN_LAUNCH, wxT("▶ 启动终端"),
                               wxDefaultPosition, wxSize(140, 45));
#ifdef _WIN32
    m_btnLaunch->SetBackgroundColour(wxColour(46, 204, 113));
    m_btnLaunch->SetForegroundColour(*wxWHITE);
#endif
    wxFont launchFont = m_btnLaunch->GetFont();
    launchFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_btnLaunch->SetFont(launchFont);

    m_btnDuplicate = new wxButton(panel, ID_BTN_DUPLICATE, wxT("复制"), wxDefaultPosition, btnSize);
    m_btnImport = new wxButton(panel, ID_BTN_IMPORT, wxT("导入..."), wxDefaultPosition, btnSize);
    m_btnExport = new wxButton(panel, ID_BTN_EXPORT, wxT("导出..."), wxDefaultPosition, btnSize);
    m_btnOpenDir = new wxButton(panel, ID_BTN_OPEN_DIR, wxT("打开目录"), wxDefaultPosition, btnSize);

    leftSizer->Add(m_btnNew, 0, wxEXPAND | wxBOTTOM, 8);
    leftSizer->Add(m_btnEdit, 0, wxEXPAND | wxBOTTOM, 8);
    leftSizer->Add(m_btnDelete, 0, wxEXPAND | wxBOTTOM, 14);
    leftSizer->Add(m_btnLaunch, 0, wxEXPAND | wxBOTTOM, 14);
    leftSizer->Add(new wxStaticLine(panel, wxID_ANY), 0, wxEXPAND | wxBOTTOM, 14);
    leftSizer->Add(m_btnDuplicate, 0, wxEXPAND | wxBOTTOM, 8);
    leftSizer->Add(m_btnImport, 0, wxEXPAND | wxBOTTOM, 8);
    leftSizer->Add(m_btnExport, 0, wxEXPAND | wxBOTTOM, 14);
    leftSizer->Add(new wxStaticLine(panel, wxID_ANY), 0, wxEXPAND | wxBOTTOM, 14);
    leftSizer->Add(m_btnOpenDir, 0, wxEXPAND);
    leftSizer->AddStretchSpacer();

    mainSizer->Add(leftSizer, 0, wxEXPAND | wxALL, 15);

    // 右侧面板
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);

    // 搜索栏：搜索框 + 清除按钮 + 历史按钮
    wxBoxSizer* searchSizer = new wxBoxSizer(wxHORIZONTAL);
    m_searchCtrl = new wxTextCtrl(panel, ID_SEARCH_CTRL, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_searchCtrl->SetHint(wxT("搜索名称、描述或工作目录"));
    m_btnClearSearch = new wxButton(panel, ID_BTN_CLEAR_SEARCH, wxT("清除"),
                                    wxDefaultPosition, wxSize(52, -1));
    m_btnClearSearch->Enable(false);
    m_btnSearchHistory = new wxButton(panel, ID_BTN_SEARCH_HISTORY, wxT("历史"),
                                      wxDefaultPosition, wxSize(60, -1));
    searchSizer->Add(m_searchCtrl, 1, wxEXPAND | wxRIGHT, 5);
    searchSizer->Add(m_btnClearSearch, 0, wxRIGHT, 5);
    searchSizer->Add(m_btnSearchHistory, 0);
    rightSizer->Add(searchSizer, 0, wxEXPAND | wxBOTTOM, 10);

    // 列表视图
    long listStyle = wxLC_REPORT | wxLC_SINGLE_SEL;
#ifdef __WXOSX__
    listStyle |= wxLC_HRULES | wxLC_VRULES;
#endif
    m_listView = new wxListView(panel, ID_LIST_PROFILES,
                                wxDefaultPosition, wxDefaultSize,
                                listStyle);
    m_listView->AppendColumn(wxT("名称"), wxLIST_FORMAT_LEFT, 180);
    m_listView->AppendColumn(wxT("描述"), wxLIST_FORMAT_LEFT, 220);
    m_listView->AppendColumn(wxT("工作目录"), wxLIST_FORMAT_LEFT, 280);
    m_listView->AppendColumn(wxT("终端"), wxLIST_FORMAT_LEFT, 140);

#ifdef __WXOSX__
    wxFont listFont = m_listView->GetFont();
    listFont.SetFaceName("PingFang SC");
    m_listView->SetFont(listFont);
#endif
#ifndef __WXMSW__
    auto* imgList = new wxImageList(1, 20);
    imgList->Add(wxBitmap(1, 24));
    m_listView->AssignImageList(imgList, wxIMAGE_LIST_SMALL);
#endif
    m_listView->EnableAlternateRowColours();
    m_listView->SetAlternateRowColour(
        wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW).ChangeLightness(93));

    rightSizer->Add(m_listView, 1, wxEXPAND);

    mainSizer->Add(rightSizer, 1, wxEXPAND | wxTOP | wxRIGHT | wxBOTTOM, 15);

    panel->SetSizer(mainSizer);
    m_statusBar = CreateStatusBar();
}

void MainFrame::RefreshProfileList() {
    RefreshView();
}

void MainFrame::ClearCurrentViewSelection() {
    long selected = m_listView->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    while (selected != -1) {
        m_listView->Select(selected, false);
        selected = m_listView->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    }
}

std::string MainFrame::DetermineSelectionAfterDelete(const std::string& deletingProfileId) const {
    if (deletingProfileId.empty() || m_visibleProfiles.empty()) {
        return "";
    }

    auto it = std::find_if(
        m_visibleProfiles.begin(),
        m_visibleProfiles.end(),
        [&](const Profile* profile) {
            return profile != nullptr && profile->id == deletingProfileId;
        });

    if (it == m_visibleProfiles.end()) {
        return "";
    }

    const size_t deletingIndex = static_cast<size_t>(std::distance(m_visibleProfiles.begin(), it));

    for (size_t i = deletingIndex + 1; i < m_visibleProfiles.size(); ++i) {
        const auto* candidate = m_visibleProfiles[i];
        if (candidate != nullptr && candidate->id != deletingProfileId) {
            return candidate->id;
        }
    }

    for (size_t i = deletingIndex; i > 0; --i) {
        const auto* candidate = m_visibleProfiles[i - 1];
        if (candidate != nullptr && candidate->id != deletingProfileId) {
            return candidate->id;
        }
    }

    return "";
}

void MainFrame::RefreshView() {
    const auto& profiles = ConfigManager::GetInstance().GetProfiles();
    m_visibleProfiles = FilterProfiles(profiles, m_searchText);

    RefreshListView();
    RestoreSelection();
    UpdateStatusBar();
    UpdateButtonStates();
}

void MainFrame::RefreshListView() {
    m_listView->DeleteAllItems();

    for (size_t i = 0; i < m_visibleProfiles.size(); ++i) {
        const auto* profile = m_visibleProfiles[i];
        if (profile == nullptr) {
            continue;
        }

        long index = m_listView->InsertItem(static_cast<long>(i), wxString::FromUTF8(profile->name));
        m_listView->SetItem(index, 1, wxString::FromUTF8(profile->description));
        m_listView->SetItem(index, 2, wxString::FromUTF8(profile->GetWorkingDirectory()));
        m_listView->SetItem(index, 3, wxString::FromUTF8(TerminalTypeDisplayName(profile->terminalType)));
    }
}

void MainFrame::RestoreSelection() {
    if (m_selectedProfileId.empty()) {
        ClearCurrentViewSelection();
        return;
    }

    for (size_t i = 0; i < m_visibleProfiles.size(); ++i) {
        const auto* profile = m_visibleProfiles[i];
        if (profile != nullptr && profile->id == m_selectedProfileId) {
            long index = static_cast<long>(i);
            m_listView->Select(index);
            m_listView->Focus(index);
            m_listView->EnsureVisible(index);
            return;
        }
    }

    m_selectedProfileId.clear();
    ClearCurrentViewSelection();
}

void MainFrame::UpdateButtonStates() {
    bool hasSelection = GetSelectedProfileFromListView() != nullptr;
    m_btnEdit->Enable(hasSelection);
    m_btnDelete->Enable(hasSelection);
    m_btnLaunch->Enable(hasSelection);
    m_btnDuplicate->Enable(hasSelection);

    bool hasProfiles = !ConfigManager::GetInstance().GetProfiles().empty();
    m_btnExport->Enable(hasProfiles);
}

void MainFrame::UpdateStatusBar() {
    size_t total = ConfigManager::GetInstance().GetProfiles().size();
    if (m_searchText.empty()) {
        m_statusBar->SetStatusText(wxString::Format(wxT("共 %zu 个配置"), total));
    } else {
        m_statusBar->SetStatusText(
            wxString::Format(wxT("显示 %zu / 共 %zu 个配置"), m_visibleProfiles.size(), total));
    }
}

void MainFrame::OnNewProfile(wxCommandEvent& event) {
    ProfileDialog dlg(this, wxT("新建配置"));
    if (dlg.ShowModal() == wxID_OK) {
        Profile profile = dlg.GetProfile();
        ConfigManager& configManager = ConfigManager::GetInstance();
        const size_t previousCount = configManager.GetProfiles().size();

        configManager.AddProfile(profile);

        const auto& profiles = configManager.GetProfiles();
        if (profiles.size() > previousCount) {
            m_selectedProfileId = profiles[previousCount].id;
        } else {
            m_selectedProfileId.clear();
        }

        RefreshView();
    }
}

void MainFrame::OnEditProfile(wxCommandEvent& event) {
    const Profile* profile = GetSelectedProfile();
    if (!profile) {
        return;
    }

    ProfileDialog dlg(this, wxT("编辑配置"), *profile);
    if (dlg.ShowModal() == wxID_OK) {
        Profile updated = dlg.GetProfile();
        ConfigManager::GetInstance().UpdateProfile(profile->id, updated);
        m_selectedProfileId = profile->id;
        RefreshView();
    }
}

void MainFrame::OnDeleteProfile(wxCommandEvent& event) {
    const Profile* profile = GetSelectedProfile();
    if (!profile) {
        return;
    }

    const std::string deletingProfileId = profile->id;
    const std::string nextSelectionId = DetermineSelectionAfterDelete(deletingProfileId);

    int result = wxMessageBox(
        wxString::Format(wxT("确定要删除配置 \"%s\" 吗？"), wxString::FromUTF8(profile->name)),
        wxT("确认删除"),
        wxYES_NO | wxICON_QUESTION,
        this);

    if (result == wxYES) {
        ConfigManager::GetInstance().DeleteProfile(deletingProfileId);
        m_selectedProfileId = nextSelectionId;
        RefreshView();
    }
}

void MainFrame::LaunchProfile(const Profile* profile) {
    if (profile == nullptr) {
        return;
    }

    std::string errorMsg;
    if (TerminalLauncher::Launch(*profile, &errorMsg)) {
        m_statusBar->SetStatusText(wxString::Format(wxT("终端已启动: %s"), wxString::FromUTF8(profile->name)));
        return;
    }

    wxString msg = wxT("启动终端失败，请检查配置");
    if (!errorMsg.empty()) {
        msg += wxT("\n详细错误: ") + wxString::FromUTF8(errorMsg);
    }

    wxMessageBox(msg, wxT("错误"), wxOK | wxICON_ERROR, this);
}

void MainFrame::OnLaunchTerminal(wxCommandEvent& event) {
    const Profile* profile = GetSelectedProfile();
    if (!profile) {
        wxMessageBox(wxT("请先选择一个配置"), wxT("提示"), wxOK | wxICON_INFORMATION, this);
        return;
    }

    LaunchProfile(profile);
}

void MainFrame::OnDuplicateProfile(wxCommandEvent& event) {
    const Profile* profile = GetSelectedProfile();
    if (!profile) {
        return;
    }

    Profile duplicated = ConfigManager::GetInstance().DuplicateProfile(profile->id);
    m_selectedProfileId = duplicated.id;
    RefreshView();
}

void MainFrame::OnImport(wxCommandEvent& event) {
    wxFileDialog dlg(this, wxT("导入配置"), wxEmptyString, wxEmptyString,
                     wxT("JSON 文件 (*.json)|*.json"),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dlg.ShowModal() == wxID_OK) {
        std::string path = dlg.GetPath().ToStdString();
        if (ConfigManager::GetInstance().ImportConfig(path)) {
            RefreshView();
            wxMessageBox(wxT("配置导入成功"), wxT("提示"), wxOK | wxICON_INFORMATION, this);
        } else {
            wxMessageBox(wxT("配置导入失败，请检查文件格式"), wxT("错误"), wxOK | wxICON_ERROR, this);
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
            wxMessageBox(wxT("配置导出成功"), wxT("提示"), wxOK | wxICON_INFORMATION, this);
        } else {
            wxMessageBox(wxT("配置导出失败"), wxT("错误"), wxOK | wxICON_ERROR, this);
        }
    }
}

void MainFrame::OnOpenWorkDir(wxCommandEvent& event) {
    const Profile* profile = GetSelectedProfile();
    if (!profile) {
        wxMessageBox(wxT("请先选择一个配置"), wxT("提示"), wxOK | wxICON_INFORMATION, this);
        return;
    }

    std::string dir = profile->GetWorkingDirectory();
    if (dir.empty()) {
        wxMessageBox(wxT("该配置未设置当前平台的工作目录"), wxT("提示"), wxOK | wxICON_INFORMATION, this);
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

void MainFrame::OnListDoubleClick(wxListEvent& event) {
    wxCommandEvent dummy;
    OnLaunchTerminal(dummy);
}

void MainFrame::OnListSelectionChanged(wxListEvent& event) {
    const Profile* selected = GetSelectedProfileFromListView();
    if (selected) {
        m_selectedProfileId = selected->id;
    } else {
        m_selectedProfileId.clear();
    }
    UpdateButtonStates();
}

void MainFrame::OnSearchTextChanged(wxCommandEvent& event) {
    m_searchText = event.GetString().utf8_string();
    m_btnClearSearch->Enable(!m_searchText.empty());
    RefreshView();
}

void MainFrame::OnSearchEnter(wxCommandEvent& event) {
    std::string keyword = event.GetString().utf8_string();
    ConfigManager::GetInstance().AddSearchHistory(keyword);
}

void MainFrame::OnClearSearch(wxCommandEvent& event) {
    m_searchCtrl->Clear();
    m_searchText.clear();
    m_btnClearSearch->Enable(false);
    RefreshView();
}

void MainFrame::OnSearchHistoryClicked(wxCommandEvent& event) {
    ShowSearchHistoryMenu();
}

void MainFrame::ShowSearchHistoryMenu() {
    const auto& history = ConfigManager::GetInstance().GetSettings().searchHistory;

    wxMenu menu;

    if (history.empty()) {
        menu.Append(wxID_ANY, wxT("暂无搜索历史"))->Enable(false);
    } else {
        for (size_t i = 0; i < history.size(); ++i) {
            std::string keyword = history[i];
            int itemId = wxID_HIGHEST + 1000 + static_cast<int>(i);
            menu.Append(itemId, wxString::FromUTF8(keyword));
            menu.Bind(wxEVT_MENU, [this, keyword](wxCommandEvent&) {
                m_searchCtrl->SetValue(wxString::FromUTF8(keyword));
                m_searchText = keyword;
                ConfigManager::GetInstance().AddSearchHistory(keyword);
                RefreshView();
            }, itemId);
        }
        menu.AppendSeparator();
        menu.Append(wxID_HIGHEST + 2000, wxT("删除某条历史..."));
        menu.Append(wxID_HIGHEST + 2001, wxT("清空历史"));

        menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            const auto& hist = ConfigManager::GetInstance().GetSettings().searchHistory;
            if (hist.empty()) {
                return;
            }

            wxArrayString choices;
            for (const auto& item : hist) {
                choices.Add(wxString::FromUTF8(item));
            }

            wxSingleChoiceDialog dlg(this, wxT("选择要删除的搜索历史："), wxT("删除搜索历史"), choices);
            if (dlg.ShowModal() == wxID_OK) {
                int selection = dlg.GetSelection();
                if (selection >= 0 && selection < static_cast<int>(hist.size())) {
                    ConfigManager::GetInstance().RemoveSearchHistory(hist[selection]);
                }
            }
        }, wxID_HIGHEST + 2000);

        menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            int result = wxMessageBox(wxT("确定要清空所有搜索历史吗？"), wxT("确认清空"),
                                      wxYES_NO | wxICON_QUESTION, this);
            if (result == wxYES) {
                ConfigManager::GetInstance().ClearSearchHistory();
            }
        }, wxID_HIGHEST + 2001);
    }

    PopupMenu(&menu);
}

void MainFrame::OnClose(wxCloseEvent& event) {
    ConfigManager::GetInstance().SaveConfig();
    event.Skip();
}

void MainFrame::OnSysColourChanged(wxSysColourChangedEvent& event) {
    m_listView->SetAlternateRowColour(
        wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW).ChangeLightness(93));
    RefreshView();
    event.Skip();
}

int MainFrame::GetSelectedIndex() {
    return m_listView->GetFirstSelected();
}

const Profile* MainFrame::GetSelectedProfile() {
    return GetSelectedProfileFromListView();
}

const Profile* MainFrame::GetSelectedProfileFromListView() {
    int index = GetSelectedIndex();
    if (index < 0) {
        return nullptr;
    }

    const size_t visibleIndex = static_cast<size_t>(index);
    if (visibleIndex >= m_visibleProfiles.size()) {
        return nullptr;
    }

    return m_visibleProfiles[visibleIndex];
}
