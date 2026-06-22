#include "CredentialDialog.h"
#include <wx/filedlg.h>

wxBEGIN_EVENT_TABLE(CredentialDialog, wxDialog)
    EVT_CHOICE(ID_CRED_TYPE, CredentialDialog::OnTypeChanged)
    EVT_BUTTON(ID_CRED_BROWSE_KEY, CredentialDialog::OnBrowseKey)
    EVT_BUTTON(wxID_OK, CredentialDialog::OnOK)
wxEND_EVENT_TABLE()

CredentialDialog::CredentialDialog(wxWindow* parent, const wxString& title)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(480, 340),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_isEdit(false)
{
    CreateControls();
    Centre();
}

CredentialDialog::CredentialDialog(wxWindow* parent, const wxString& title, const Credential& cred)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(480, 340),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_original(cred)
    , m_isEdit(true)
{
    CreateControls();
    InitializeFromCredential(cred);
    Centre();
}

void CredentialDialog::CreateControls() {
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* form = new wxFlexGridSizer(5, 2, 8, 12);
    form->AddGrowableCol(1, 1);

    form->Add(new wxStaticText(panel, wxID_ANY, wxT("名称:")), 0, wxALIGN_CENTER_VERTICAL);
    m_txtName = new wxTextCtrl(panel, wxID_ANY);
    form->Add(m_txtName, 1, wxEXPAND);

    form->Add(new wxStaticText(panel, wxID_ANY, wxT("类型:")), 0, wxALIGN_CENTER_VERTICAL);
    m_choiceType = new wxChoice(panel, ID_CRED_TYPE);
    m_choiceType->Append(wxT("密码"), new wxStringClientData(wxT("password")));
    m_choiceType->Append(wxT("私钥"), new wxStringClientData(wxT("privateKey")));
    m_choiceType->Append(wxT("SSH Agent"), new wxStringClientData(wxT("sshAgent")));
    m_choiceType->SetSelection(0);
    form->Add(m_choiceType, 1, wxEXPAND);

    form->Add(new wxStaticText(panel, wxID_ANY, wxT("密码:")), 0, wxALIGN_CENTER_VERTICAL);
    m_txtPassword = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                   wxDefaultSize, wxTE_PASSWORD);
    if (m_isEdit) m_txtPassword->SetHint(wxT("留空表示不修改"));
    form->Add(m_txtPassword, 1, wxEXPAND);

    form->Add(new wxStaticText(panel, wxID_ANY, wxT("私钥路径:")), 0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* keySizer = new wxBoxSizer(wxHORIZONTAL);
    m_txtKeyPath = new wxTextCtrl(panel, ID_CRED_KEYPATH);
    m_txtKeyPath->SetHint(wxT("~/.ssh/id_rsa"));
    m_btnBrowseKey = new wxButton(panel, ID_CRED_BROWSE_KEY, wxT("..."),
                                  wxDefaultPosition, wxSize(36, -1));
    keySizer->Add(m_txtKeyPath, 1, wxEXPAND | wxRIGHT, 3);
    keySizer->Add(m_btnBrowseKey, 0);
    form->Add(keySizer, 1, wxEXPAND);

    form->Add(new wxStaticText(panel, wxID_ANY, wxT("口令:")), 0, wxALIGN_CENTER_VERTICAL);
    m_txtPassphrase = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                     wxDefaultSize, wxTE_PASSWORD);
    m_txtPassphrase->SetHint(wxT("可选"));
    form->Add(m_txtPassphrase, 1, wxEXPAND);

    mainSizer->Add(form, 1, wxEXPAND | wxALL, 12);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(panel, wxID_CANCEL, wxT("取消")), 0, wxRIGHT, 8);
    wxButton* btnOK = new wxButton(panel, wxID_OK, wxT("保存"));
    btnOK->SetDefault();
    btnSizer->Add(btnOK, 0);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    panel->SetSizer(mainSizer);

    wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(panel, 1, wxEXPAND);
    SetSizer(outer);
    Fit();

    UpdateFieldStates();
}

void CredentialDialog::InitializeFromCredential(const Credential& cred) {
    m_txtName->SetValue(wxString::FromUTF8(cred.name));
    m_txtKeyPath->SetValue(wxString::FromUTF8(cred.keyPath));

    std::string typeStr = CredentialTypeToString(cred.type);
    for (unsigned int i = 0; i < m_choiceType->GetCount(); ++i) {
        auto* data = static_cast<wxStringClientData*>(m_choiceType->GetClientObject(i));
        if (data && data->GetData() == wxString::FromUTF8(typeStr)) {
            m_choiceType->SetSelection(i);
            break;
        }
    }
    UpdateFieldStates();
}

void CredentialDialog::UpdateFieldStates() {
    int sel = m_choiceType->GetSelection();
    auto* data = static_cast<wxStringClientData*>(m_choiceType->GetClientObject(sel));
    std::string typeStr = data ? data->GetData().ToStdString() : "password";

    bool isPassword = (typeStr == "password");
    bool isKey = (typeStr == "privateKey");

    m_txtPassword->Enable(isPassword);
    m_txtKeyPath->Enable(isKey);
    m_btnBrowseKey->Enable(isKey);
    m_txtPassphrase->Enable(isKey);
}

void CredentialDialog::OnTypeChanged(wxCommandEvent& event) {
    UpdateFieldStates();
}

void CredentialDialog::OnBrowseKey(wxCommandEvent& event) {
    wxFileDialog dlg(this, wxT("选择私钥文件"), wxEmptyString, wxEmptyString,
                     wxT("所有文件 (*.*)|*.*"),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        m_txtKeyPath->SetValue(dlg.GetPath());
    }
}

void CredentialDialog::OnOK(wxCommandEvent& event) {
    wxString name = m_txtName->GetValue().Trim().Trim(false);
    if (name.IsEmpty()) {
        wxMessageBox(wxT("请输入名称"), wxT("验证失败"), wxOK | wxICON_WARNING, this);
        m_txtName->SetFocus();
        return;
    }

    int sel = m_choiceType->GetSelection();
    auto* data = static_cast<wxStringClientData*>(m_choiceType->GetClientObject(sel));
    std::string typeStr = data ? data->GetData().ToStdString() : "password";

    if (typeStr == "privateKey" && !m_isEdit) {
        wxString key = m_txtKeyPath->GetValue().Trim().Trim(false);
        if (key.IsEmpty()) {
            wxMessageBox(wxT("私钥类型需要指定私钥文件路径"), wxT("验证失败"),
                         wxOK | wxICON_WARNING, this);
            m_txtKeyPath->SetFocus();
            return;
        }
    }
    EndModal(wxID_OK);
}

Credential CredentialDialog::GetCredential() const {
    Credential cred = m_original;
    cred.name = m_txtName->GetValue().ToStdString();

    int sel = m_choiceType->GetSelection();
    auto* data = static_cast<wxStringClientData*>(m_choiceType->GetClientObject(sel));
    cred.type = StringToCredentialType(data ? data->GetData().ToStdString() : "password");

    cred.keyPath = m_txtKeyPath->GetValue().ToStdString();
    return cred;
}

std::string CredentialDialog::GetPassword() const {
    return m_txtPassword->GetValue().ToStdString();
}

std::string CredentialDialog::GetPassphrase() const {
    return m_txtPassphrase->GetValue().ToStdString();
}
