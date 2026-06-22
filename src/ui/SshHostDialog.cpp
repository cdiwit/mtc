#include "SshHostDialog.h"
#include <wx/valnum.h>

wxBEGIN_EVENT_TABLE(SshHostDialog, wxDialog)
    EVT_BUTTON(wxID_OK, SshHostDialog::OnOK)
wxEND_EVENT_TABLE()

SshHostDialog::SshHostDialog(wxWindow* parent, const wxString& title)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(420, 260),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_isEdit(false)
{
    CreateControls();
    Centre();
}

SshHostDialog::SshHostDialog(wxWindow* parent, const wxString& title, const SshHost& host)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(420, 260),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_original(host)
    , m_isEdit(true)
{
    CreateControls();
    InitializeFromHost(host);
    Centre();
}

void SshHostDialog::CreateControls() {
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* form = new wxFlexGridSizer(4, 2, 8, 12);
    form->AddGrowableCol(1, 1);

    form->Add(new wxStaticText(panel, wxID_ANY, wxT("名称:")), 0, wxALIGN_CENTER_VERTICAL);
    m_txtName = new wxTextCtrl(panel, wxID_ANY);
    form->Add(m_txtName, 1, wxEXPAND);

    form->Add(new wxStaticText(panel, wxID_ANY, wxT("主机:")), 0, wxALIGN_CENTER_VERTICAL);
    m_txtHost = new wxTextCtrl(panel, wxID_ANY);
    m_txtHost->SetHint(wxT("如 192.168.1.10 或 example.com"));
    form->Add(m_txtHost, 1, wxEXPAND);

    form->Add(new wxStaticText(panel, wxID_ANY, wxT("端口:")), 0, wxALIGN_CENTER_VERTICAL);
    m_txtPort = new wxTextCtrl(panel, wxID_ANY, wxT("22"));
    wxIntegerValidator<int> portValidator;
    portValidator.SetRange(1, 65535);
    m_txtPort->SetValidator(portValidator);
    form->Add(m_txtPort, 1, wxEXPAND);

    form->Add(new wxStaticText(panel, wxID_ANY, wxT("用户名:")), 0, wxALIGN_CENTER_VERTICAL);
    m_txtUsername = new wxTextCtrl(panel, wxID_ANY);
    form->Add(m_txtUsername, 1, wxEXPAND);

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
}

void SshHostDialog::InitializeFromHost(const SshHost& host) {
    m_txtName->SetValue(wxString::FromUTF8(host.name));
    m_txtHost->SetValue(wxString::FromUTF8(host.host));
    m_txtPort->SetValue(wxString::Format(wxT("%d"), host.port));
    m_txtUsername->SetValue(wxString::FromUTF8(host.username));
}

void SshHostDialog::OnOK(wxCommandEvent& event) {
    wxString name = m_txtName->GetValue().Trim().Trim(false);
    wxString host = m_txtHost->GetValue().Trim().Trim(false);
    if (name.IsEmpty()) {
        wxMessageBox(wxT("请输入名称"), wxT("验证失败"), wxOK | wxICON_WARNING, this);
        m_txtName->SetFocus();
        return;
    }
    if (host.IsEmpty()) {
        wxMessageBox(wxT("请输入主机地址"), wxT("验证失败"), wxOK | wxICON_WARNING, this);
        m_txtHost->SetFocus();
        return;
    }
    EndModal(wxID_OK);
}

SshHost SshHostDialog::GetSshHost() const {
    SshHost host = m_original;
    host.name = m_txtName->GetValue().ToStdString();
    host.host = m_txtHost->GetValue().ToStdString();
    long port = 22;
    m_txtPort->GetValue().ToLong(&port);
    host.port = static_cast<int>(port);
    host.username = m_txtUsername->GetValue().ToStdString();
    return host;
}
