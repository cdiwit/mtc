#pragma once
#include <wx/wx.h>
#include <wx/listctrl.h>

// SSH 主机列表管理对话框（增/改/删）
class SshHostManagerDialog : public wxDialog {
public:
    SshHostManagerDialog(wxWindow* parent);

private:
    wxListView* m_listView;

    void RefreshList();
    void OnAdd(wxCommandEvent& event);
    void OnEdit(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnListDoubleClick(wxListEvent& event);
    void OnListSelectionChanged(wxListEvent& event);
    void UpdateButtonStates();

    wxDECLARE_EVENT_TABLE();
};

enum SshHostManagerIds {
    ID_SSHHOST_LIST = wxID_HIGHEST + 320,
    ID_SSHHOST_ADD,
    ID_SSHHOST_EDIT,
    ID_SSHHOST_DELETE
};
