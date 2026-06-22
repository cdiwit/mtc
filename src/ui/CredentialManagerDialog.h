#pragma once
#include <wx/wx.h>
#include <wx/listctrl.h>

// 凭据列表管理对话框（增/改/删）。保存时同步维护系统钥匙串里的秘密。
class CredentialManagerDialog : public wxDialog {
public:
    CredentialManagerDialog(wxWindow* parent);

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

enum CredentialManagerIds {
    ID_CRED_LIST = wxID_HIGHEST + 360,
    ID_CRED_MGR_ADD,
    ID_CRED_MGR_EDIT,
    ID_CRED_MGR_DELETE
};
