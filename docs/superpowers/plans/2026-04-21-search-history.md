# 搜索历史功能实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 保留单一列表视图，强化搜索能力，增加可持久化的搜索历史，支持增删和复用。

**Architecture:** 在 `AppSettings` 中增加 `searchHistory` 字段，通过独立的纯函数模块 (`SearchHistory.h/.cpp`) 管理历史增删去重逻辑，`ConfigManager` 负责持久化读写，`MainFrame` 负责历史按钮 UI 和交互。同时移除树视图相关代码，简化界面。

**Tech Stack:** C++17, wxWidgets, nlohmann/json, GoogleTest

---

## File Structure

| 文件 | 操作 | 职责 |
|------|------|------|
| `src/core/Types.h` | 修改 | AppSettings 增加 searchHistory 字段 |
| `src/core/SearchHistory.h` | 新建 | 搜索历史操作的纯函数声明 |
| `src/core/SearchHistory.cpp` | 新建 | 搜索历史操作的纯函数实现 |
| `src/core/ConfigManager.h` | 修改 | 增加 AddSearchHistory / RemoveSearchHistory / ClearSearchHistory 方法声明 |
| `src/core/ConfigManager.cpp` | 修改 | 读写 searchHistory JSON；实现三个历史操作方法 |
| `src/ui/MainFrame.h` | 修改 | 增加历史按钮成员和事件处理声明；移除树视图相关声明 |
| `src/ui/MainFrame.cpp` | 修改 | 增加历史按钮 UI、Enter 处理、历史菜单；移除树视图代码 |
| `tests/core/SearchHistoryTests.cpp` | 新建 | 搜索历史纯函数的单元测试 |
| `tests/ui/ProfileTreeBuilderTests.cpp` | 保留 | 现有测试不变 |
| `CMakeLists.txt` | 修改 | 主程序和测试目标加入新源文件 |

---

### Task 1: 在 Types.h 中增加 searchHistory 字段

**Files:**
- Modify: `src/core/Types.h:52-57`

- [ ] **Step 1: 在 AppSettings 中增加 searchHistory 字段**

在 `src/core/Types.h` 的 `AppSettings` 结构体末尾添加 `searchHistory` 字段：

```cpp
struct AppSettings {
    TerminalType defaultTerminalType = TerminalType::Auto;
    std::string language = "zh-CN";
    std::string theme = "system";
    bool autoBackup = true;
    std::vector<std::string> searchHistory;
};
```

- [ ] **Step 2: 验证编译**

Run: `cmake --build build --target mtc 2>&1 | tail -5`
Expected: 编译成功（新字段有默认值，不影响现有代码）

---

### Task 2: 创建搜索历史纯函数测试（TDD - RED）

**Files:**
- Create: `tests/core/SearchHistoryTests.cpp`

- [ ] **Step 1: 创建测试文件目录**

Run: `mkdir -p tests/core`

- [ ] **Step 2: 编写搜索历史测试用例**

创建 `tests/core/SearchHistoryTests.cpp`：

```cpp
#include <gtest/gtest.h>
#include "core/SearchHistory.h"
#include <vector>
#include <string>

TEST(SearchHistoryTests, AddToEmptyHistory) {
    std::vector<std::string> history;
    AddToSearchHistory(history, "keyword");
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0], "keyword");
}

TEST(SearchHistoryTests, AddMovesExistingToFront) {
    std::vector<std::string> history = {"alpha", "beta", "gamma"};
    AddToSearchHistory(history, "beta");
    ASSERT_EQ(history.size(), 3u);
    EXPECT_EQ(history[0], "beta");
    EXPECT_EQ(history[1], "alpha");
    EXPECT_EQ(history[2], "gamma");
}

TEST(SearchHistoryTests, AddTrimsWhitespace) {
    std::vector<std::string> history;
    AddToSearchHistory(history, "  hello  ");
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0], "hello");
}

TEST(SearchHistoryTests, AddEmptyStringDoesNothing) {
    std::vector<std::string> history;
    AddToSearchHistory(history, "");
    AddToSearchHistory(history, "   ");
    EXPECT_TRUE(history.empty());
}

TEST(SearchHistoryTests, AddLimitsToTen) {
    std::vector<std::string> history;
    for (int i = 0; i < 15; ++i) {
        AddToSearchHistory(history, "item" + std::to_string(i));
    }
    ASSERT_EQ(history.size(), 10u);
    EXPECT_EQ(history[0], "item14");
    EXPECT_EQ(history[9], "item5");
}

TEST(SearchHistoryTests, RemoveExistingItem) {
    std::vector<std::string> history = {"alpha", "beta", "gamma"};
    RemoveFromSearchHistory(history, "beta");
    ASSERT_EQ(history.size(), 2u);
    EXPECT_EQ(history[0], "alpha");
    EXPECT_EQ(history[1], "gamma");
}

TEST(SearchHistoryTests, RemoveNonexistentDoesNothing) {
    std::vector<std::string> history = {"alpha", "beta"};
    RemoveFromSearchHistory(history, "missing");
    EXPECT_EQ(history.size(), 2u);
}

TEST(SearchHistoryTests, ClearRemovesAll) {
    std::vector<std::string> history = {"a", "b", "c"};
    ClearSearchHistory(history);
    EXPECT_TRUE(history.empty());
}

TEST(SearchHistoryTests, ClearOnEmptyIsSafe) {
    std::vector<std::string> history;
    ClearSearchHistory(history);
    EXPECT_TRUE(history.empty());
}
```

---

### Task 3: 创建 SearchHistory 纯函数实现（TDD - GREEN）

**Files:**
- Create: `src/core/SearchHistory.h`
- Create: `src/core/SearchHistory.cpp`

- [ ] **Step 1: 创建 SearchHistory.h**

```cpp
#pragma once
#include <string>
#include <vector>

void AddToSearchHistory(std::vector<std::string>& history, const std::string& keyword);
void RemoveFromSearchHistory(std::vector<std::string>& history, const std::string& keyword);
void ClearSearchHistory(std::vector<std::string>& history);
```

- [ ] **Step 2: 创建 SearchHistory.cpp**

```cpp
#include "SearchHistory.h"
#include <algorithm>

namespace {
std::string TrimKeyword(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}
}  // namespace

void AddToSearchHistory(std::vector<std::string>& history, const std::string& keyword) {
    std::string trimmed = TrimKeyword(keyword);
    if (trimmed.empty()) {
        return;
    }

    auto it = std::find(history.begin(), history.end(), trimmed);
    if (it != history.end()) {
        history.erase(it);
    }
    history.insert(history.begin(), trimmed);

    if (history.size() > 10) {
        history.resize(10);
    }
}

void RemoveFromSearchHistory(std::vector<std::string>& history, const std::string& keyword) {
    auto it = std::find(history.begin(), history.end(), keyword);
    if (it != history.end()) {
        history.erase(it);
    }
}

void ClearSearchHistory(std::vector<std::string>& history) {
    history.clear();
}
```

- [ ] **Step 3: 更新 CMakeLists.txt 测试目标**

在 `CMakeLists.txt` 中，将测试目标加入新源文件：

将：
```cmake
add_executable(mtc_tests
    tests/ui/ProfileTreeBuilderTests.cpp
    src/ui/ProfileTreeBuilder.cpp
)
```

改为：
```cmake
add_executable(mtc_tests
    tests/ui/ProfileTreeBuilderTests.cpp
    tests/core/SearchHistoryTests.cpp
    src/ui/ProfileTreeBuilder.cpp
    src/core/SearchHistory.cpp
)
```

同时在主程序 SOURCES 列表中加入新文件：

将：
```cmake
set(SOURCES
    src/main.cpp
    src/core/ConfigManager.cpp
    src/core/TerminalLauncher.cpp
    src/ui/MainFrame.cpp
    src/ui/ProfileDialog.cpp
    src/ui/EnvVarPanel.cpp
    src/ui/ProfileTreeBuilder.cpp
    src/utils/PathUtils.cpp
)
```

改为：
```cmake
set(SOURCES
    src/main.cpp
    src/core/ConfigManager.cpp
    src/core/SearchHistory.cpp
    src/core/TerminalLauncher.cpp
    src/ui/MainFrame.cpp
    src/ui/ProfileDialog.cpp
    src/ui/EnvVarPanel.cpp
    src/ui/ProfileTreeBuilder.cpp
    src/utils/PathUtils.cpp
)
```

- [ ] **Step 4: 重新配置 CMake 并运行测试**

Run: `cd D:/WorkSpace/cdiwit/mtc && cmake -B build -G "Visual Studio 17 2022" 2>&1 | tail -5`
Run: `cmake --build build --target mtc_tests 2>&1 | tail -5`
Run: `ctest --test-dir build --output-on-failure 2>&1`
Expected: 所有测试通过（原有 3 个 + 新增 9 个 = 12 个测试）

- [ ] **Step 5: Commit**

```bash
git add src/core/SearchHistory.h src/core/SearchHistory.cpp tests/core/SearchHistoryTests.cpp CMakeLists.txt
git commit -m "feat: add search history utility functions with tests"
```

---

### Task 4: 扩展 ConfigManager 持久化 searchHistory

**Files:**
- Modify: `src/core/ConfigManager.h:39-40`
- Modify: `src/core/ConfigManager.cpp:90-98` (LoadConfig settings section)
- Modify: `src/core/ConfigManager.cpp:144-150` (SaveConfig settings section)

- [ ] **Step 1: 在 ConfigManager.h 增加三个方法声明**

在 `ConfigManager.h` 的 `// 设置` 区块下增加：

```cpp
    // 设置
    const AppSettings& GetSettings() const { return m_config.settings; }
    void UpdateSettings(const AppSettings& settings);
    void AddSearchHistory(const std::string& keyword);
    void RemoveSearchHistory(const std::string& keyword);
    void ClearSearchHistory();
```

同时在文件头部加入 include：

```cpp
#include "SearchHistory.h"
```

- [ ] **Step 2: 在 ConfigManager.cpp 的 LoadConfig 中读取 searchHistory**

在 `LoadConfig()` 的 `// 解析设置` 块末尾，增加 searchHistory 读取。

当前代码（约 line 91-98）：
```cpp
        if (j.contains("settings")) {
            const auto& js = j["settings"];
            std::string defaultTerm = js.value("defaultTerminalType", "auto");
            m_config.settings.defaultTerminalType = StringToTerminalType(defaultTerm);
            m_config.settings.language = js.value("language", "zh-CN");
            m_config.settings.theme = js.value("theme", "system");
            m_config.settings.autoBackup = js.value("autoBackup", true);
        }
```

改为：
```cpp
        if (j.contains("settings")) {
            const auto& js = j["settings"];
            std::string defaultTerm = js.value("defaultTerminalType", "auto");
            m_config.settings.defaultTerminalType = StringToTerminalType(defaultTerm);
            m_config.settings.language = js.value("language", "zh-CN");
            m_config.settings.theme = js.value("theme", "system");
            m_config.settings.autoBackup = js.value("autoBackup", true);

            m_config.settings.searchHistory.clear();
            if (js.contains("searchHistory") && js["searchHistory"].is_array()) {
                for (const auto& item : js["searchHistory"]) {
                    if (item.is_string()) {
                        m_config.settings.searchHistory.push_back(item.get<std::string>());
                    }
                }
            }
        }
```

- [ ] **Step 3: 在 ConfigManager.cpp 的 SaveConfig 中写入 searchHistory**

在 `SaveConfig()` 的设置序列化块末尾，增加 searchHistory 写入。

当前代码（约 line 144-150）：
```cpp
        j["settings"] = {
            {"defaultTerminalType", TerminalTypeToString(m_config.settings.defaultTerminalType)},
            {"language", m_config.settings.language},
            {"theme", m_config.settings.theme},
            {"autoBackup", m_config.settings.autoBackup}
        };
```

改为：
```cpp
        j["settings"] = {
            {"defaultTerminalType", TerminalTypeToString(m_config.settings.defaultTerminalType)},
            {"language", m_config.settings.language},
            {"theme", m_config.settings.theme},
            {"autoBackup", m_config.settings.autoBackup}
        };

        json searchHistoryJson = json::array();
        for (const auto& item : m_config.settings.searchHistory) {
            searchHistoryJson.push_back(item);
        }
        j["settings"]["searchHistory"] = searchHistoryJson;
```

- [ ] **Step 4: 在 ConfigManager.cpp 末尾增加三个历史操作方法**

在 `UpdateSettings` 方法后增加：

```cpp
void ConfigManager::AddSearchHistory(const std::string& keyword) {
    ::AddToSearchHistory(m_config.settings.searchHistory, keyword);
    SaveConfig();
}

void ConfigManager::RemoveSearchHistory(const std::string& keyword) {
    ::RemoveFromSearchHistory(m_config.settings.searchHistory, keyword);
    SaveConfig();
}

void ConfigManager::ClearSearchHistory() {
    ::ClearSearchHistory(m_config.settings.searchHistory);
    SaveConfig();
}
```

- [ ] **Step 5: 验证编译**

Run: `cmake --build build --target mtc 2>&1 | tail -5`
Expected: 编译成功

- [ ] **Step 6: Commit**

```bash
git add src/core/ConfigManager.h src/core/ConfigManager.cpp
git commit -m "feat: persist search history in config.json via ConfigManager"
```

---

### Task 5: 在 MainFrame 中增加历史按钮和 Enter 处理

**Files:**
- Modify: `src/ui/MainFrame.h`
- Modify: `src/ui/MainFrame.cpp`

- [ ] **Step 1: 修改 MainFrame.h — 增加新成员和事件处理**

将整个 `MainFrame.h` 替换为：

```cpp
#pragma once
#include <wx/wx.h>
#include <wx/listctrl.h>
#include "core/ConfigManager.h"

class MainFrame : public wxFrame {
public:
    MainFrame();

private:
    // 控件
    wxTextCtrl* m_searchCtrl;
    wxButton* m_btnSearchHistory;
    wxListView* m_listView;
    wxButton* m_btnNew;
    wxButton* m_btnEdit;
    wxButton* m_btnDelete;
    wxButton* m_btnLaunch;
    wxButton* m_btnDuplicate;
    wxButton* m_btnImport;
    wxButton* m_btnExport;
    wxStatusBar* m_statusBar;

    std::string m_searchText;
    std::string m_selectedProfileId;
    std::vector<const Profile*> m_visibleProfiles;

    // 初始化
    void CreateControls();
    void RefreshProfileList();
    void RefreshView();
    void RefreshListView();
    void RestoreSelection();
    void ClearCurrentViewSelection();
    std::string DetermineSelectionAfterDelete(const std::string& deletingProfileId) const;
    void LaunchProfile(const Profile* profile);
    void UpdateButtonStates();
    void UpdateStatusBar();

    // 搜索历史
    void ShowSearchHistoryMenu();

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
    void OnSearchTextChanged(wxCommandEvent& event);
    void OnSearchEnter(wxCommandEvent& event);
    void OnSearchHistoryClicked(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    // 辅助方法
    int GetSelectedIndex();
    const Profile* GetSelectedProfile();
    const Profile* GetSelectedProfileFromListView();

    wxDECLARE_EVENT_TABLE();
};

// 控件 ID
enum {
    ID_LIST_PROFILES = wxID_HIGHEST + 1,
    ID_SEARCH_CTRL,
    ID_BTN_SEARCH_HISTORY,
    ID_BTN_NEW,
    ID_BTN_EDIT,
    ID_BTN_DELETE,
    ID_BTN_LAUNCH,
    ID_BTN_DUPLICATE,
    ID_BTN_IMPORT,
    ID_BTN_EXPORT
};
```

关键变更：
- 移除 `m_radioListMode`, `m_radioTreeMode`, `m_treeView`, `m_currentViewMode`
- 增加 `m_btnSearchHistory`
- 移除 `RefreshTreeView`, `OnViewModeChanged`, `OnTreeSelectionChanged`, `OnTreeItemActivated`
- 增加 `OnSearchEnter`, `OnSearchHistoryClicked`, `ShowSearchHistoryMenu`
- `GetSelectedProfileFromCurrentView` 重命名为 `GetSelectedProfileFromListView`
- 移除 `ID_TREE_PROFILES`, `ID_RADIO_LIST_MODE`, `ID_RADIO_TREE_MODE`
- 增加 `ID_BTN_SEARCH_HISTORY`
- 移除 `#include <wx/treectrl.h>`

- [ ] **Step 2: 修改 MainFrame.cpp — 移除树视图代码，增加搜索历史**

将整个 `MainFrame.cpp` 替换为：

```cpp
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
#include <functional>
#include <iterator>
#include <utility>

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON(ID_BTN_NEW, MainFrame::OnNewProfile)
    EVT_BUTTON(ID_BTN_EDIT, MainFrame::OnEditProfile)
    EVT_BUTTON(ID_BTN_DELETE, MainFrame::OnDeleteProfile)
    EVT_BUTTON(ID_BTN_LAUNCH, MainFrame::OnLaunchTerminal)
    EVT_BUTTON(ID_BTN_DUPLICATE, MainFrame::OnDuplicateProfile)
    EVT_BUTTON(ID_BTN_IMPORT, MainFrame::OnImport)
    EVT_BUTTON(ID_BTN_EXPORT, MainFrame::OnExport)
    EVT_TEXT(ID_SEARCH_CTRL, MainFrame::OnSearchTextChanged)
    EVT_TEXT_ENTER(ID_SEARCH_CTRL, MainFrame::OnSearchEnter)
    EVT_BUTTON(ID_BTN_SEARCH_HISTORY, MainFrame::OnSearchHistoryClicked)
    EVT_LIST_ITEM_ACTIVATED(ID_LIST_PROFILES, MainFrame::OnListDoubleClick)
    EVT_LIST_ITEM_SELECTED(ID_LIST_PROFILES, MainFrame::OnListSelectionChanged)
    EVT_LIST_ITEM_DESELECTED(ID_LIST_PROFILES, MainFrame::OnListSelectionChanged)
    EVT_CLOSE(MainFrame::OnClose)
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
    panel->SetBackgroundColour(wxColour(245, 245, 245));

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
    m_btnLaunch->SetBackgroundColour(wxColour(46, 204, 113));
    m_btnLaunch->SetForegroundColour(*wxWHITE);
    wxFont launchFont = m_btnLaunch->GetFont();
    launchFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_btnLaunch->SetFont(launchFont);

    m_btnDuplicate = new wxButton(panel, ID_BTN_DUPLICATE, wxT("复制"), wxDefaultPosition, btnSize);
    m_btnImport = new wxButton(panel, ID_BTN_IMPORT, wxT("导入..."), wxDefaultPosition, btnSize);
    m_btnExport = new wxButton(panel, ID_BTN_EXPORT, wxT("导出..."), wxDefaultPosition, btnSize);

    leftSizer->Add(m_btnNew, 0, wxEXPAND | wxBOTTOM, 8);
    leftSizer->Add(m_btnEdit, 0, wxEXPAND | wxBOTTOM, 8);
    leftSizer->Add(m_btnDelete, 0, wxEXPAND | wxBOTTOM, 14);
    leftSizer->Add(m_btnLaunch, 0, wxEXPAND | wxBOTTOM, 14);
    leftSizer->Add(new wxStaticLine(panel, wxID_ANY), 0, wxEXPAND | wxBOTTOM, 14);
    leftSizer->Add(m_btnDuplicate, 0, wxEXPAND | wxBOTTOM, 8);
    leftSizer->Add(m_btnImport, 0, wxEXPAND | wxBOTTOM, 8);
    leftSizer->Add(m_btnExport, 0, wxEXPAND);
    leftSizer->AddStretchSpacer();

    mainSizer->Add(leftSizer, 0, wxEXPAND | wxALL, 15);

    // 右侧面板
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);

    // 搜索栏：搜索框 + 历史按钮
    wxBoxSizer* searchSizer = new wxBoxSizer(wxHORIZONTAL);
    m_searchCtrl = new wxTextCtrl(panel, ID_SEARCH_CTRL, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_searchCtrl->SetHint(wxT("搜索名称、描述或工作目录"));
    m_btnSearchHistory = new wxButton(panel, ID_BTN_SEARCH_HISTORY, wxT("历史"),
                                      wxDefaultPosition, wxSize(60, -1));
    searchSizer->Add(m_searchCtrl, 1, wxEXPAND | wxRIGHT, 5);
    searchSizer->Add(m_btnSearchHistory, 0);
    rightSizer->Add(searchSizer, 0, wxEXPAND | wxBOTTOM, 10);

    // 列表视图
    m_listView = new wxListView(panel, ID_LIST_PROFILES,
                                wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL);
    m_listView->AppendColumn(wxT("名称"), wxLIST_FORMAT_LEFT, 180);
    m_listView->AppendColumn(wxT("描述"), wxLIST_FORMAT_LEFT, 220);
    m_listView->AppendColumn(wxT("工作目录"), wxLIST_FORMAT_LEFT, 280);
    m_listView->AppendColumn(wxT("终端"), wxLIST_FORMAT_LEFT, 140);

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
        m_listView->SetItem(index, 2, wxString::FromUTF8(profile->workingDirectory));
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
    size_t count = ConfigManager::GetInstance().GetProfiles().size();
    m_statusBar->SetStatusText(wxString::Format(wxT("共 %zu 个配置"), count));
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
    RefreshView();
}

void MainFrame::OnSearchEnter(wxCommandEvent& event) {
    std::string keyword = event.GetString().utf8_string();
    ConfigManager::GetInstance().AddSearchHistory(keyword);
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
```

- [ ] **Step 3: 清理 Types.h 中不再需要的 ProfileViewMode 枚举**

从 `src/core/Types.h` 中移除：

```cpp
enum class ProfileViewMode {
    List,
    Tree
};
```

- [ ] **Step 4: 验证编译**

Run: `cmake --build build --target mtc 2>&1 | tail -10`
Expected: 编译成功

- [ ] **Step 5: 运行全部测试**

Run: `ctest --test-dir build --output-on-failure 2>&1`
Expected: 所有 12 个测试通过

- [ ] **Step 6: Commit**

```bash
git add src/ui/MainFrame.h src/ui/MainFrame.cpp src/core/Types.h
git commit -m "feat: add search history UI with history button, remove tree view"
```

---

### Task 6: 手工验证

**无代码变更，纯验证步骤。**

- [ ] **Step 1: 运行程序**

Run: `./bin/mtc`

- [ ] **Step 2: 验证以下场景**

1. 输入关键词并按 Enter → 列表过滤生效
2. 按 Enter 后，点击"历史"按钮 → 历史菜单显示刚输入的词
3. 点击历史项 → 搜索框回填并立即执行搜索
4. 关闭并重启程序 → 历史仍然存在
5. 在历史菜单中点击"删除某条历史..." → 选择后删除成功
6. 清空历史后 → 显示"暂无搜索历史"
7. 确认左侧面板不再有"列表视图"/"树视图"单选按钮
8. 确认不再有树视图控件

---

## Self-Review Checklist

- [x] **Spec coverage:**
  - 保留单一列表视图 → Task 5 移除树视图
  - 保留并强化列表搜索 → 保留现有 FilterProfiles 逻辑
  - 增加可持久化的搜索历史 → Task 1-4
  - 支持从历史中快速复用搜索词 → Task 5 ShowSearchHistoryMenu
  - 支持删除单条历史与清空全部历史 → Task 5 菜单中的删除/清空
  - 只有按 Enter 才写入历史 → Task 5 OnSearchEnter
  - 历史最多 10 条 → Task 3 AddToSearchHistory
  - 重复词移到最前面 → Task 3 AddToSearchHistory
  - 空字符串不写入 → Task 3 AddToSearchHistory
  - 历史菜单无数据时显示"暂无搜索历史" → Task 5 ShowSearchHistoryMenu

- [x] **Placeholder scan:** 无 TBD、TODO、占位符

- [x] **Type consistency:**
  - `std::vector<std::string> searchHistory` 在 Types.h 定义，ConfigManager 和 SearchHistory 函数均使用相同类型
  - `AddToSearchHistory` / `RemoveFromSearchHistory` / `ClearSearchHistory` 在 SearchHistory.h 声明，SearchHistory.cpp 实现，ConfigManager.cpp 调用——签名一致
  - `GetSelectedProfileFromListView` 在 .h 声明和 .cpp 实现一致
