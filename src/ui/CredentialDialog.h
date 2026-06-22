#pragma once
#include <wx/wx.h>
#include "core/Types.h"

// 单条凭据编辑对话框（名称 / 类型 / 密码 / 私钥路径+口令）
// 密码与口令不在此保存，仅暂存在内存里，由调用方写入系统钥匙串。
class CredentialDialog : public wxDialog {
public:
    CredentialDialog(wxWindow* parent, const wxString& title);
    CredentialDialog(wxWindow* parent, const wxString& title, const Credential& cred);

    // 元数据（不含秘密）
    Credential GetCredential() const;
    // 编辑模式下，留空表示不修改；返回非空时调用方应写入钥匙串
    std::string GetPassword() const;
    std::string GetPassphrase() const;

private:
    wxTextCtrl* m_txtName;
    wxChoice* m_choiceType;
    wxTextCtrl* m_txtPassword;
    wxTextCtrl* m_txtKeyPath;
    wxButton* m_btnBrowseKey;
    wxTextCtrl* m_txtPassphrase;

    Credential m_original;
    bool m_isEdit;

    void CreateControls();
    void InitializeFromCredential(const Credential& cred);
    void UpdateFieldStates();
    void OnTypeChanged(wxCommandEvent& event);
    void OnBrowseKey(wxCommandEvent& event);
    void OnOK(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};

enum CredentialDialogIds {
    ID_CRED_TYPE = wxID_HIGHEST + 340,
    ID_CRED_KEYPATH,
    ID_CRED_BROWSE_KEY
};
