#pragma once
#include <wx/wx.h>
#include <wx/listctrl.h>
#include "core/SshClient.h"
#include "core/Types.h"

enum class RemoteBrowserMode {
    Browse,    // 浏览 + 双击下载打开
    PickDir    // 选择一个目录返回
};

// 远程文件浏览器（基于 SFTP）。构造时连接，失败则弹错并以取消结束。
class RemoteFileBrowserDialog : public wxDialog {
public:
    RemoteFileBrowserDialog(wxWindow* parent,
                            const SshHost& host,
                            const Credential* cred,
                            const std::string& startPath,
                            RemoteBrowserMode mode = RemoteBrowserMode::Browse);

    // PickDir 模式下返回选定的目录（成功结束时有效）
    std::string GetSelectedDir() const { return m_selectedDir; }

private:
    SshHost m_host;
    Credential m_cred;          // 拷贝一份（cred 可能为 nullptr → 默认 Password 但无秘密）
    bool m_hasCredential;
    RemoteBrowserMode m_mode;
    SshClient m_client;
    bool m_connected = false;

    std::string m_currentPath;
    std::string m_selectedDir;

    wxStaticText* m_pathLabel;
    wxListView* m_listView;

    void CreateControls();
    void Navigate(const std::string& path);
    void OnUp(wxCommandEvent& event);
    void OnListDoubleClick(wxListEvent& event);
    void OnPickDir(wxCommandEvent& event);
    void OnRefresh(wxCommandEvent& event);

    void OpenRemoteFile(const std::string& name);

    wxDECLARE_EVENT_TABLE();
};

enum RemoteBrowserIds {
    ID_RB_LIST = wxID_HIGHEST + 380,
    ID_RB_UP,
    ID_RB_REFRESH,
    ID_RB_PICKDIR
};
