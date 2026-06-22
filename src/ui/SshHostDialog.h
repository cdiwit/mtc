#pragma once
#include <wx/wx.h>
#include "core/Types.h"

// 单条 SSH 主机编辑对话框（name / host / port / username）
class SshHostDialog : public wxDialog {
public:
    SshHostDialog(wxWindow* parent, const wxString& title);
    SshHostDialog(wxWindow* parent, const wxString& title, const SshHost& host);

    SshHost GetSshHost() const;

private:
    wxTextCtrl* m_txtName;
    wxTextCtrl* m_txtHost;
    wxTextCtrl* m_txtPort;
    wxTextCtrl* m_txtUsername;

    SshHost m_original;
    bool m_isEdit;

    void CreateControls();
    void InitializeFromHost(const SshHost& host);
    void OnOK(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};
