#include "CredentialManagerDialog.h"
#include "CredentialDialog.h"
#include "core/ConfigManager.h"
#include "core/SecretStore.h"
#include <wx/msgdlg.h>

namespace {
// 钥匙串 key 约定（与 SshClient 保持一致）
std::string PasswordKey(const std::string& id) {
    return "mtc:cred:" + id + ":password";
}
std::string PassphraseKey(const std::string& id) {
    return "mtc:cred:" + id + ":passphrase";
}
}  // namespace

wxBEGIN_EVENT_TABLE(CredentialManagerDialog, wxDialog)
    EVT_BUTTON(ID_CRED_MGR_ADD, CredentialManagerDialog::OnAdd)
    EVT_BUTTON(ID_CRED_MGR_EDIT, CredentialManagerDialog::OnEdit)
    EVT_BUTTON(ID_CRED_MGR_DELETE, CredentialManagerDialog::OnDelete)
    EVT_LIST_ITEM_ACTIVATED(ID_CRED_LIST, CredentialManagerDialog::OnListDoubleClick)
    EVT_LIST_ITEM_SELECTED(ID_CRED_LIST, CredentialManagerDialog::OnListSelectionChanged)
    EVT_LIST_ITEM_DESELECTED(ID_CRED_LIST, CredentialManagerDialog::OnListSelectionChanged)
wxEND_EVENT_TABLE()

CredentialManagerDialog::CredentialManagerDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, wxT("凭据管理"),
               wxDefaultPosition, wxSize(560, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    m_listView = new wxListView(panel, ID_CRED_LIST, wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL);
    m_listView->AppendColumn(wxT("名称"), wxLIST_FORMAT_LEFT, 180);
    m_listView->AppendColumn(wxT("类型"), wxLIST_FORMAT_LEFT, 120);
    m_listView->AppendColumn(wxT("私钥路径"), wxLIST_FORMAT_LEFT, 220);
    m_listView->EnableAlternateRowColours();
    mainSizer->Add(m_listView, 1, wxEXPAND | wxALL, 10);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(new wxButton(panel, ID_CRED_MGR_ADD, wxT("新建")), 0, wxRIGHT, 8);
    btnSizer->Add(new wxButton(panel, ID_CRED_MGR_EDIT, wxT("编辑")), 0, wxRIGHT, 8);
    btnSizer->Add(new wxButton(panel, ID_CRED_MGR_DELETE, wxT("删除")), 0);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(panel, wxID_CANCEL, wxT("关闭")), 0);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    panel->SetSizer(mainSizer);

    wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(panel, 1, wxEXPAND);
    SetSizer(outer);

    RefreshList();
    UpdateButtonStates();
    Centre();
}

void CredentialManagerDialog::RefreshList() {
    m_listView->DeleteAllItems();
    const auto& creds = ConfigManager::GetInstance().GetCredentials();
    for (size_t i = 0; i < creds.size(); ++i) {
        const auto& c = creds[i];
        long index = m_listView->InsertItem(static_cast<long>(i), wxString::FromUTF8(c.name));
        m_listView->SetItem(index, 1, wxString::FromUTF8(CredentialTypeDisplayName(c.type)));
        m_listView->SetItem(index, 2, wxString::FromUTF8(c.keyPath));
        m_listView->SetItemData(index, static_cast<long>(i));
    }
    UpdateButtonStates();
}

void CredentialManagerDialog::OnAdd(wxCommandEvent& event) {
    CredentialDialog dlg(this, wxT("新建凭据"));
    if (dlg.ShowModal() != wxID_OK) return;

    Credential cred = dlg.GetCredential();
    std::string password = dlg.GetPassword();
    std::string passphrase = dlg.GetPassphrase();

    // 先存元数据拿到新 id，再写钥匙串
    Credential stored = ConfigManager::GetInstance().AddCredential(cred);

    if (stored.type == CredentialType::Password && !password.empty()) {
        SecretStore::Set(PasswordKey(stored.id), password);
    } else if (stored.type == CredentialType::PrivateKey) {
        if (!passphrase.empty()) {
            SecretStore::Set(PassphraseKey(stored.id), passphrase);
        }
    }
    RefreshList();
}

void CredentialManagerDialog::OnEdit(wxCommandEvent& event) {
    long sel = m_listView->GetFirstSelected();
    if (sel < 0) return;
    size_t idx = static_cast<size_t>(m_listView->GetItemData(sel));
    const auto& creds = ConfigManager::GetInstance().GetCredentials();
    if (idx >= creds.size()) return;

    const Credential& existing = creds[idx];
    CredentialDialog dlg(this, wxT("编辑凭据"), existing);
    if (dlg.ShowModal() != wxID_OK) return;

    Credential updated = dlg.GetCredential();
    std::string password = dlg.GetPassword();
    std::string passphrase = dlg.GetPassphrase();
    const std::string& id = existing.id;

    ConfigManager::GetInstance().UpdateCredential(id, updated);

    // 根据新类型维护钥匙串：写入新秘密，清理无关的旧秘密
    if (updated.type == CredentialType::Password) {
        if (!password.empty()) {
            SecretStore::Set(PasswordKey(id), password);
        }
        SecretStore::Delete(PassphraseKey(id));
    } else if (updated.type == CredentialType::PrivateKey) {
        if (!passphrase.empty()) {
            SecretStore::Set(PassphraseKey(id), passphrase);
        }
        SecretStore::Delete(PasswordKey(id));
    } else {  // SshAgent
        SecretStore::Delete(PasswordKey(id));
        SecretStore::Delete(PassphraseKey(id));
    }
    RefreshList();
}

void CredentialManagerDialog::OnDelete(wxCommandEvent& event) {
    long sel = m_listView->GetFirstSelected();
    if (sel < 0) return;
    size_t idx = static_cast<size_t>(m_listView->GetItemData(sel));
    const auto& creds = ConfigManager::GetInstance().GetCredentials();
    if (idx >= creds.size()) return;

    const Credential& existing = creds[idx];
    int result = wxMessageBox(
        wxString::Format(wxT("确定删除凭据 \"%s\" 吗？\n将同时清除钥匙串中保存的秘密。\n引用该凭据的配置将变为不带凭据。"),
                         wxString::FromUTF8(existing.name)),
        wxT("确认删除"), wxYES_NO | wxICON_QUESTION, this);
    if (result == wxYES) {
        SecretStore::Delete(PasswordKey(existing.id));
        SecretStore::Delete(PassphraseKey(existing.id));
        ConfigManager::GetInstance().DeleteCredential(existing.id);
        RefreshList();
    }
}

void CredentialManagerDialog::OnListDoubleClick(wxListEvent& event) {
    wxCommandEvent dummy;
    OnEdit(dummy);
}

void CredentialManagerDialog::OnListSelectionChanged(wxListEvent& event) {
    UpdateButtonStates();
}

void CredentialManagerDialog::UpdateButtonStates() {
    bool hasSelection = m_listView->GetFirstSelected() >= 0;
    FindWindow(ID_CRED_MGR_EDIT)->Enable(hasSelection);
    FindWindow(ID_CRED_MGR_DELETE)->Enable(hasSelection);
}
