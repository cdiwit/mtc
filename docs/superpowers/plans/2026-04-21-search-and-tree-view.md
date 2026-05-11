# Search and Tree View Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add searchable profile discovery plus switchable list/tree views in the MTC main window, with tree mode grouped by working directory path hierarchy.

**Architecture:** Keep the existing `wxListView` for flat results and add a `wxTreeCtrl` for grouped browsing. Move search, path splitting, and tree construction into a small helper module so `MainFrame` only manages UI state, selection restoration, and view refresh.

**Tech Stack:** C++17, wxWidgets, CMake, CTest, GoogleTest

---

## File Structure

### Modify
- `src/ui/MainFrame.h` — add search control, mode toggle controls, tree control, unified selection state, refresh helpers, tree event handlers.
- `src/ui/MainFrame.cpp` — update layout, wire search + mode events, render list/tree views, restore selection by profile id, refresh after CRUD/import/export.
- `src/core/Types.h` — add lightweight view-mode enum and tree node structs shared by UI helper logic.
- `CMakeLists.txt` — register helper source file and test target, enable CTest.

### Create
- `src/ui/ProfileTreeBuilder.h` — declarations for search normalization, filtering, path splitting, and tree building.
- `src/ui/ProfileTreeBuilder.cpp` — implementation of pure logic used by both list and tree refresh paths.
- `tests/ui/ProfileTreeBuilderTests.cpp` — GoogleTest coverage for search, path splitting, and tree construction.

---

### Task 1: Add test infrastructure for pure UI logic

**Files:**
- Modify: `CMakeLists.txt`
- Create: `tests/ui/ProfileTreeBuilderTests.cpp`

- [ ] **Step 1: Add failing test target declarations**

```cmake
enable_testing()

find_package(GTest CONFIG REQUIRED)

add_executable(mtc_tests
    tests/ui/ProfileTreeBuilderTests.cpp
    src/ui/ProfileTreeBuilder.cpp
)

target_include_directories(mtc_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(mtc_tests PRIVATE GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mtc_tests)
```

- [ ] **Step 2: Write failing tests for search and grouping**

```cpp
#include <gtest/gtest.h>
#include "ui/ProfileTreeBuilder.h"

TEST(ProfileTreeBuilderTests, MatchesNameDescriptionAndDirectoryCaseInsensitively) {
    Profile profile{"1", "Prod API", "Release config", "D:/Work/Proj/A", TerminalType::Cmd, {}, "", ""};
    EXPECT_TRUE(MatchesSearch(profile, "prod"));
    EXPECT_TRUE(MatchesSearch(profile, "release"));
    EXPECT_TRUE(MatchesSearch(profile, "d:/work/proj"));
    EXPECT_FALSE(MatchesSearch(profile, "staging"));
}

TEST(ProfileTreeBuilderTests, SplitsWindowsAndUnixPathsIntoSegments) {
    EXPECT_EQ(SplitWorkingDirectory("D:/Work/Proj/A"), (std::vector<std::string>{"D:", "Work", "Proj", "A"}));
    EXPECT_EQ(SplitWorkingDirectory("/home/user/app"), (std::vector<std::string>{"/", "home", "user", "app"}));
}

TEST(ProfileTreeBuilderTests, GroupsProfilesIntoDirectoryTreeAndUnassignedBucket) {
    Profile a{"1", "Dev", "", "D:/Work/Proj/A", TerminalType::Cmd, {}, "", ""};
    Profile b{"2", "Prod", "", "D:/Work/Proj/A", TerminalType::Cmd, {}, "", ""};
    Profile c{"3", "NoDir", "", "", TerminalType::Cmd, {}, "", ""};
    std::vector<const Profile*> profiles{&a, &b, &c};

    auto tree = BuildProfileTree(profiles);

    ASSERT_EQ(tree.children.size(), 2u);
    EXPECT_EQ(tree.children[0].label, "D:");
    EXPECT_EQ(tree.children[1].label, "未设置工作目录");
}
```

- [ ] **Step 3: Run test target to verify it fails**

Run: `cmake -S . -B build && cmake --build build --config Release --target mtc_tests`
Expected: FAIL with missing `ProfileTreeBuilder` declarations and unresolved symbols

- [ ] **Step 4: Commit test scaffolding once it is compiling but failing at link/logic level**

```bash
git add CMakeLists.txt tests/ui/ProfileTreeBuilderTests.cpp
git commit -m "test: scaffold profile tree builder tests"
```

### Task 2: Implement search and tree-building helper logic

**Files:**
- Create: `src/ui/ProfileTreeBuilder.h`
- Create: `src/ui/ProfileTreeBuilder.cpp`
- Modify: `src/core/Types.h`
- Test: `tests/ui/ProfileTreeBuilderTests.cpp`

- [ ] **Step 1: Define shared view-model types**

```cpp
enum class ProfileViewMode {
    List,
    Tree
};

struct ProfileTreeNode {
    std::string label;
    std::string fullPath;
    std::vector<ProfileTreeNode> children;
    std::vector<const Profile*> profiles;
};
```

- [ ] **Step 2: Declare pure helper functions**

```cpp
std::string NormalizeSearchText(const std::string& text);
bool MatchesSearch(const Profile& profile, const std::string& normalizedSearch);
std::vector<const Profile*> FilterProfiles(const std::vector<Profile>& profiles, const std::string& searchText);
std::vector<std::string> SplitWorkingDirectory(const std::string& workingDirectory);
ProfileTreeNode BuildProfileTree(const std::vector<const Profile*>& profiles);
```

- [ ] **Step 3: Implement minimal logic to satisfy tests**

```cpp
std::string NormalizeSearchText(const std::string& text) {
    std::string value = text;
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}
```

```cpp
bool MatchesSearch(const Profile& profile, const std::string& normalizedSearch) {
    if (normalizedSearch.empty()) {
        return true;
    }

    auto contains = [&](const std::string& raw) {
        return NormalizeSearchText(raw).find(normalizedSearch) != std::string::npos;
    };

    return contains(profile.name) || contains(profile.description) || contains(profile.workingDirectory);
}
```

```cpp
std::vector<const Profile*> FilterProfiles(const std::vector<Profile>& profiles, const std::string& searchText) {
    std::string normalized = NormalizeSearchText(searchText);
    std::vector<const Profile*> result;
    for (const auto& profile : profiles) {
        if (MatchesSearch(profile, normalized)) {
            result.push_back(&profile);
        }
    }
    return result;
}
```

- [ ] **Step 4: Implement path splitting and tree building**

```cpp
std::vector<std::string> SplitWorkingDirectory(const std::string& workingDirectory);
ProfileTreeNode BuildProfileTree(const std::vector<const Profile*>& profiles);
```

Implementation requirements:
- Windows path `D:/Work/Proj/A` becomes `{"D:", "Work", "Proj", "A"}`
- Unix path `/home/user/app` becomes `{"/", "home", "user", "app"}`
- Empty path returns an empty vector
- Empty-path profiles are attached under child node label `未设置工作目录`
- Reuse existing directory nodes when multiple profiles share a path

- [ ] **Step 5: Run tests and verify they pass**

Run: `cmake --build build --config Release --target mtc_tests && ctest --test-dir build --output-on-failure -C Release`
Expected: PASS for all `ProfileTreeBuilderTests`

- [ ] **Step 6: Commit helper module**

```bash
git add src/core/Types.h src/ui/ProfileTreeBuilder.h src/ui/ProfileTreeBuilder.cpp tests/ui/ProfileTreeBuilderTests.cpp CMakeLists.txt
git commit -m "feat: add profile search and tree builder"
```

### Task 3: Add list/tree mode UI skeleton and search box

**Files:**
- Modify: `src/ui/MainFrame.h`
- Modify: `src/ui/MainFrame.cpp`

- [ ] **Step 1: Add new controls and state fields in `MainFrame.h`**

```cpp
wxTextCtrl* m_searchCtrl;
wxRadioButton* m_radioListMode;
wxRadioButton* m_radioTreeMode;
wxTreeCtrl* m_treeView;
ProfileViewMode m_currentViewMode = ProfileViewMode::List;
std::string m_searchText;
std::string m_selectedProfileId;
```

- [ ] **Step 2: Add refresh and selection helper declarations**

```cpp
void RefreshView();
void RefreshListView(const std::vector<const Profile*>& filteredProfiles);
void RefreshTreeView(const std::vector<const Profile*>& filteredProfiles);
void UpdateButtonStates();
void RestoreSelection();
const Profile* GetSelectedProfileFromCurrentView();
void OnSearchTextChanged(wxCommandEvent& event);
void OnViewModeChanged(wxCommandEvent& event);
void OnTreeSelectionChanged(wxTreeEvent& event);
void OnTreeItemActivated(wxTreeEvent& event);
```

- [ ] **Step 3: Update layout in `MainFrame.cpp`**

```cpp
m_radioListMode = new wxRadioButton(panel, wxID_ANY, wxT("列表模式"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
m_radioTreeMode = new wxRadioButton(panel, wxID_ANY, wxT("树形模式"));
m_searchCtrl = new wxTextCtrl(panel, wxID_ANY);
m_treeView = new wxTreeCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE);
```

Layout requirements:
- Left side: mode controls, stats text, existing action buttons
- Right side top: search box with placeholder text `搜索名称、描述或工作目录`
- Right side bottom: `m_listView` and `m_treeView` in same sizer; hide `m_treeView` initially

- [ ] **Step 4: Wire search and mode events**

```cpp
m_searchCtrl->Bind(wxEVT_TEXT, &MainFrame::OnSearchTextChanged, this);
m_radioListMode->Bind(wxEVT_RADIOBUTTON, &MainFrame::OnViewModeChanged, this);
m_radioTreeMode->Bind(wxEVT_RADIOBUTTON, &MainFrame::OnViewModeChanged, this);
m_treeView->Bind(wxEVT_TREE_SEL_CHANGED, &MainFrame::OnTreeSelectionChanged, this);
m_treeView->Bind(wxEVT_TREE_ITEM_ACTIVATED, &MainFrame::OnTreeItemActivated, this);
```

- [ ] **Step 5: Build and verify the UI still compiles**

Run: `cmake --build build --config Release --target mtc`
Expected: PASS with new controls visible; tree mode may still be empty

- [ ] **Step 6: Commit UI skeleton**

```bash
git add src/ui/MainFrame.h src/ui/MainFrame.cpp
git commit -m "feat: add search and dual-view shell"
```

### Task 4: Implement list filtering and unified selection restoration

**Files:**
- Modify: `src/ui/MainFrame.h`
- Modify: `src/ui/MainFrame.cpp`
- Test: `tests/ui/ProfileTreeBuilderTests.cpp`

- [ ] **Step 1: Replace direct list refresh with filtered refresh**

```cpp
void MainFrame::RefreshView() {
    const auto& profiles = ConfigManager::GetInstance().GetProfiles();
    auto filteredProfiles = FilterProfiles(profiles, m_searchText);

    if (m_currentViewMode == ProfileViewMode::List) {
        RefreshListView(filteredProfiles);
        m_listView->Show();
        m_treeView->Hide();
    } else {
        RefreshTreeView(filteredProfiles);
        m_treeView->Show();
        m_listView->Hide();
    }

    RestoreSelection();
    UpdateButtonStates();
    Layout();
}
```

- [ ] **Step 2: Update list rendering to track profile ids instead of row indexes**

```cpp
void MainFrame::RefreshListView(const std::vector<const Profile*>& filteredProfiles) {
    m_listView->DeleteAllItems();
    for (size_t i = 0; i < filteredProfiles.size(); ++i) {
        const Profile* profile = filteredProfiles[i];
        long row = m_listView->InsertItem(static_cast<long>(i), wxString::FromUTF8(profile->name));
        m_listView->SetItem(row, 1, wxString::FromUTF8(profile->description));
        m_listView->SetItem(row, 2, wxString::FromUTF8(profile->workingDirectory));
        m_listView->SetItem(row, 3, wxString::FromUTF8(TerminalTypeDisplayName(profile->terminalType)));
        m_listView->SetItemData(row, static_cast<long>(i));
    }
}
```

Implementation note: keep a `std::vector<const Profile*> m_visibleProfiles;` field if needed so selection maps back to profile ids safely.

- [ ] **Step 3: Update selection handlers to store `m_selectedProfileId`**

```cpp
void MainFrame::OnListSelectionChanged(wxListEvent& event) {
    const Profile* profile = GetSelectedProfileFromCurrentView();
    m_selectedProfileId = profile ? profile->id : std::string();
    UpdateButtonStates();
}
```

- [ ] **Step 4: Make search input refresh instantly**

```cpp
void MainFrame::OnSearchTextChanged(wxCommandEvent& event) {
    m_searchText = m_searchCtrl->GetValue().ToStdString();
    RefreshView();
}
```

- [ ] **Step 5: Build and manually verify list search**

Run: `cmake --build build --config Release --target mtc`
Expected: PASS
Manual check:
- Typing into search filters list rows immediately
- Clearing search restores all rows
- Buttons still enable only when a profile row is selected

- [ ] **Step 6: Commit list filtering**

```bash
git add src/ui/MainFrame.h src/ui/MainFrame.cpp
git commit -m "feat: filter profiles in list view"
```

### Task 5: Render tree view and distinguish directory/profile nodes

**Files:**
- Modify: `src/ui/MainFrame.h`
- Modify: `src/ui/MainFrame.cpp`

- [ ] **Step 1: Add tree item payload types**

```cpp
struct TreeItemData : public wxTreeItemData {
    bool isProfile;
    std::string profileId;

    TreeItemData(bool profileItem, std::string id = {})
        : isProfile(profileItem), profileId(std::move(id)) {}
};
```

- [ ] **Step 2: Implement `RefreshTreeView` using `BuildProfileTree`**

```cpp
void MainFrame::RefreshTreeView(const std::vector<const Profile*>& filteredProfiles) {
    m_treeView->DeleteAllItems();
    auto tree = BuildProfileTree(filteredProfiles);
    wxTreeItemId root = m_treeView->AddRoot(wxT("配置"), -1, -1, new TreeItemData(false));
    // append directory nodes recursively, then profile leaf nodes
    m_treeView->Expand(root);
}
```

Implementation requirements:
- Directory nodes carry `TreeItemData(false)`
- Profile leaf nodes carry `TreeItemData(true, profile->id)`
- Search mode expands every ancestor path to matched leaves
- Empty result shows root with a single child label `无匹配结果`

- [ ] **Step 3: Implement tree selection and activation behavior**

```cpp
void MainFrame::OnTreeSelectionChanged(wxTreeEvent& event) {
    const Profile* profile = GetSelectedProfileFromCurrentView();
    m_selectedProfileId = profile ? profile->id : std::string();
    UpdateButtonStates();
}
```

```cpp
void MainFrame::OnTreeItemActivated(wxTreeEvent& event) {
    wxTreeItemId item = event.GetItem();
    auto* data = dynamic_cast<TreeItemData*>(m_treeView->GetItemData(item));
    if (data && data->isProfile) {
        wxCommandEvent dummy;
        OnLaunchTerminal(dummy);
    } else if (item.IsOk()) {
        m_treeView->Toggle(item);
    }
}
```

- [ ] **Step 4: Make `GetSelectedProfileFromCurrentView()` work for both views**

```cpp
const Profile* MainFrame::GetSelectedProfileFromCurrentView();
```

Implementation requirements:
- List mode resolves through the visible filtered profile collection
- Tree mode resolves through `TreeItemData::profileId`
- Directory nodes return `nullptr`

- [ ] **Step 5: Build and manually verify tree mode**

Run: `cmake --build build --config Release --target mtc`
Expected: PASS
Manual check:
- Switching to tree mode shows working-directory hierarchy
- Profiles with empty directories appear under `未设置工作目录`
- Selecting a directory disables edit/delete/launch/duplicate
- Double-clicking a profile leaf launches terminal

- [ ] **Step 6: Commit tree view rendering**

```bash
git add src/ui/MainFrame.h src/ui/MainFrame.cpp
git commit -m "feat: add profile tree view"
```

### Task 6: Restore context across mode switches and CRUD/import/export operations

**Files:**
- Modify: `src/ui/MainFrame.cpp`
- Modify: `src/ui/MainFrame.h`

- [ ] **Step 1: Implement mode switching with context preservation**

```cpp
void MainFrame::OnViewModeChanged(wxCommandEvent& event) {
    m_currentViewMode = m_radioTreeMode->GetValue() ? ProfileViewMode::Tree : ProfileViewMode::List;
    RefreshView();
}
```

- [ ] **Step 2: Implement `RestoreSelection()`**

```cpp
void MainFrame::RestoreSelection() {
    if (m_selectedProfileId.empty()) {
        return;
    }
    // list mode: find matching visible profile and select row
    // tree mode: traverse nodes, select matching profile item, expand ancestors
}
```

- [ ] **Step 3: Update CRUD and import/export handlers to call `RefreshView()` and preserve selection rules**

```cpp
ConfigManager::GetInstance().AddProfile(profile);
m_selectedProfileId = ConfigManager::GetInstance().GetProfiles().back().id;
RefreshView();
```

```cpp
ConfigManager::GetInstance().DuplicateProfile(profile->id);
m_selectedProfileId = ConfigManager::GetInstance().GetProfiles().back().id;
RefreshView();
```

Implementation requirements:
- Edit keeps same id selected
- Delete selects next visible item, otherwise previous, otherwise clears selection
- Import keeps current search text and current view mode

- [ ] **Step 4: Build and run all tests**

Run: `cmake --build build --config Release && ctest --test-dir build --output-on-failure -C Release`
Expected: PASS

- [ ] **Step 5: Perform manual regression checks**

Checklist:
- List search filters instantly
- Tree search keeps only matched paths and auto-expands them
- Switching between list/tree preserves search text
- Switching to tree expands selected profile path when possible
- Directory nodes never enable profile-only actions
- New/Edit/Duplicate/Delete/Import/Export all refresh through the active mode without stale selection

- [ ] **Step 6: Commit final integration**

```bash
git add src/ui/MainFrame.h src/ui/MainFrame.cpp src/ui/ProfileTreeBuilder.h src/ui/ProfileTreeBuilder.cpp src/core/Types.h CMakeLists.txt tests/ui/ProfileTreeBuilderTests.cpp
git commit -m "feat: add searchable list and tree profile views"
```

---

## Self-Review

### Spec coverage
- Search by name/description/directory: covered in Tasks 1, 2, 4
- Dual list/tree modes: covered in Tasks 3, 5, 6
- Tree grouped by working-directory hierarchy: covered in Tasks 1, 2, 5
- Preserve tree structure and auto-expand matches: covered in Tasks 2, 5, 6
- Left navigation + right content layout: covered in Task 3
- CRUD/import/export refresh behavior: covered in Task 6

### Placeholder scan
- No TBD/TODO placeholders remain.
- Each task includes concrete files, commands, and expected outcomes.

### Type consistency
- Shared types use `ProfileViewMode` and `ProfileTreeNode` consistently.
- Selection restoration uses `m_selectedProfileId` consistently across list and tree flows.
