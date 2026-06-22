#include "RemoteFileBrowserDialog.h"
#include "core/SecretStore.h"
#include <wx/busyinfo.h>
#include <wx/utils.h>
#include <wx/filename.h>
#include <wx/filefn.h>
#include <wx/msgdlg.h>

#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <cstring>

namespace fs = std::filesystem;

wxBEGIN_EVENT_TABLE(RemoteFileBrowserDialog, wxDialog)
    EVT_BUTTON(ID_RB_UP, RemoteFileBrowserDialog::OnUp)
    EVT_BUTTON(ID_RB_REFRESH, RemoteFileBrowserDialog::OnRefresh)
    EVT_BUTTON(ID_RB_PICKDIR, RemoteFileBrowserDialog::OnPickDir)
    EVT_LIST_ITEM_ACTIVATED(ID_RB_LIST, RemoteFileBrowserDialog::OnListDoubleClick)
wxEND_EVENT_TABLE()

RemoteFileBrowserDialog::RemoteFileBrowserDialog(
    wxWindow* parent,
    const SshHost& host,
    const Credential* cred,
    const std::string& startPath,
    RemoteBrowserMode mode)
    : wxDialog(parent, wxID_ANY, wxString::FromUTF8("远程文件浏览 - ") + wxString::FromUTF8(host.name),
               wxDefaultPosition, wxSize(720, 500),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_host(host)
    , m_mode(mode)
{
    if (cred) {
        m_cred = *cred;
        m_hasCredential = true;
    } else {
        m_hasCredential = false;
    }

    CreateControls();

    // 连接（阻塞，期间显示忙等待）
    {
        wxBusyCursor busy;
        bool ok = false;
        if (m_hasCredential) {
            ok = m_client.Connect(m_host, m_cred);
        }
        if (!ok) {
            wxMessageBox(
                wxT("无法连接到远程主机：\n") + wxString::FromUTF8(m_client.LastError()) +
                wxT("\n\n请检查主机、端口、用户名与凭据。"),
                wxT("连接失败"), wxOK | wxICON_ERROR, this);
            EndModal(wxID_CANCEL);
            return;
        }
        m_connected = true;
    }

    // 初始目录：优先 startPath，否则用登录用户家目录 "."
    std::string initial = startPath.empty() ? "." : startPath;
    Navigate(initial);

    Centre();
}

void RemoteFileBrowserDialog::CreateControls() {
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 顶部：路径 + 上级 + 刷新
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
    m_pathLabel = new wxStaticText(panel, wxID_ANY, wxT("/"),
                                   wxDefaultPosition, wxDefaultSize,
                                   wxST_ELLIPSIZE_MIDDLE);
    topSizer->Add(new wxStaticText(panel, wxID_ANY, wxT("当前目录:")),
                  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    topSizer->Add(m_pathLabel, 1, wxALIGN_CENTER_VERTICAL);
    topSizer->Add(new wxButton(panel, ID_RB_UP, wxT("上级"), wxDefaultPosition, wxSize(64, -1)),
                  0, wxLEFT, 4);
    topSizer->Add(new wxButton(panel, ID_RB_REFRESH, wxT("刷新"), wxDefaultPosition, wxSize(64, -1)),
                  0, wxLEFT, 4);
    mainSizer->Add(topSizer, 0, wxEXPAND | wxALL, 10);

    // 文件列表
    m_listView = new wxListView(panel, ID_RB_LIST, wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL);
    m_listView->AppendColumn(wxT("名称"), wxLIST_FORMAT_LEFT, 280);
    m_listView->AppendColumn(wxT("大小"), wxLIST_FORMAT_RIGHT, 100);
    m_listView->AppendColumn(wxT("类型"), wxLIST_FORMAT_LEFT, 100);
    m_listView->AppendColumn(wxT("修改时间"), wxLIST_FORMAT_LEFT, 150);
    m_listView->EnableAlternateRowColours();
    mainSizer->Add(m_listView, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // 底部按钮
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    if (m_mode == RemoteBrowserMode::PickDir) {
        wxButton* btnPick = new wxButton(panel, ID_RB_PICKDIR, wxT("选择此目录"));
        btnSizer->Add(btnPick, 0);
    }
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(panel, wxID_CANCEL, wxT("关闭")), 0);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 10);

    panel->SetSizer(mainSizer);

    wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(panel, 1, wxEXPAND);
    SetSizer(outer);
}

static std::string FormatSize(uint64_t bytes, bool isDir) {
    if (isDir) return "<DIR>";
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    size_t u = 0;
    double s = static_cast<double>(bytes);
    while (s >= 1024 && u < 4) { s /= 1024; ++u; }
    char buf[64];
    if (u == 0) {
        std::snprintf(buf, sizeof(buf), "%llu B",
                      static_cast<unsigned long long>(bytes));
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f %s", s, units[u]);
    }
    return buf;
}

static std::string FormatTime(int64_t t) {
    if (t <= 0) return "";
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
}

void RemoteFileBrowserDialog::Navigate(const std::string& path) {
    if (!m_connected) return;

    std::vector<SshClient::RemoteEntry> entries;
    {
        wxBusyCursor busy;
        entries = m_client.ListDir(path);
        if (entries.empty() && !m_client.LastError().empty()) {
            wxMessageBox(wxString::FromUTF8(m_client.LastError()),
                         wxT("读取目录失败"), wxOK | wxICON_WARNING, this);
            return;
        }
    }

    m_currentPath = path;
    m_pathLabel->SetLabel(wxString::FromUTF8(path));

    m_listView->DeleteAllItems();

    // ".." 返回上级（除非已是根）
    if (path != "/" && path != "//") {
        long index = m_listView->InsertItem(0, wxT(".."));
        m_listView->SetItem(index, 1, wxT(""));
        m_listView->SetItem(index, 2, wxT("上级目录"));
        m_listView->SetItem(index, 3, wxT(""));
        m_listView->SetItemData(index, -1);
    }

    long row = m_listView->GetItemCount();
    for (const auto& e : entries) {
        long idx = m_listView->InsertItem(row, wxString::FromUTF8(e.name));
        m_listView->SetItem(idx, 1, wxString::FromUTF8(FormatSize(e.size, e.isDir)));
        m_listView->SetItem(idx, 2, wxString::FromUTF8(e.isDir ? "目录" : "文件"));
        m_listView->SetItem(idx, 3, wxString::FromUTF8(FormatTime(e.mtime)));
        m_listView->SetItemData(idx, e.isDir ? 1 : 0);
        ++row;
    }
}

void RemoteFileBrowserDialog::OnUp(wxCommandEvent& event) {
    if (m_currentPath.empty() || m_currentPath == "/" ) return;
    std::string parent;
    size_t pos = m_currentPath.find_last_of('/');
    if (pos == 0) {
        parent = "/";
    } else if (pos != std::string::npos) {
        parent = m_currentPath.substr(0, pos);
    }
    if (parent.empty()) parent = "/";
    Navigate(parent);
}

void RemoteFileBrowserDialog::OnRefresh(wxCommandEvent& event) {
    if (!m_currentPath.empty()) Navigate(m_currentPath);
}

void RemoteFileBrowserDialog::OnListDoubleClick(wxListEvent& event) {
    long sel = m_listView->GetFirstSelected();
    if (sel < 0) return;

    wxString name = m_listView->GetItemText(sel, 0);

    if (name == wxT("..")) {
        wxCommandEvent dummy;
        OnUp(dummy);
        return;
    }

    long data = m_listView->GetItemData(sel);
    // 拼接远程完整路径
    std::string base = m_currentPath;
    if (base.empty() || base.back() != '/') base += "/";
    std::string fullPath = base + name.ToStdString();

    if (data == 1) {
        // 目录：进入
        Navigate(fullPath);
    } else {
        // 文件：下载并打开
        OpenRemoteFile(name.ToStdString());
    }
}

void RemoteFileBrowserDialog::OpenRemoteFile(const std::string& name) {
    std::string base = m_currentPath;
    if (base.empty() || base.back() != '/') base += "/";
    std::string remotePath = base + name;

    // 本地临时文件：保留扩展名以便关联打开
    fs::path tempDir = fs::temp_directory_path();
    fs::path localPath = tempDir / ("mtc_remote_" + name);
    // 防止同名残留影响，先移除
    std::error_code ec;
    fs::remove(localPath, ec);

    {
        wxString busyMsg = wxString::FromUTF8("正在下载 ") +
                           wxString::FromUTF8(name) +
                           wxString::FromUTF8(" ...");
        wxBusyInfo info(busyMsg, this);
        wxWindowDisabler disableAll;
        if (!m_client.DownloadFile(remotePath, localPath.string())) {
            wxMessageBox(wxString::FromUTF8(m_client.LastError()),
                         wxT("下载失败"), wxOK | wxICON_ERROR, this);
            return;
        }
    }

    if (!wxLaunchDefaultApplication(wxString::FromUTF8(localPath.string()))) {
        wxMessageBox(wxT("下载完成，但无法用默认程序打开：\n") +
                     wxString::FromUTF8(localPath.string()),
                     wxT("提示"), wxOK | wxICON_INFORMATION, this);
    }
}

void RemoteFileBrowserDialog::OnPickDir(wxCommandEvent& event) {
    m_selectedDir = m_currentPath;
    EndModal(wxID_OK);
}
