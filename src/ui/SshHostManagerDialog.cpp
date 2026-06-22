#include "SshHostManagerDialog.h"
#include "SshHostDialog.h"
#include "core/ConfigManager.h"
#include <wx/msgdlg.h>

wxBEGIN_EVENT_TABLE(SshHostManagerDialog, wxDialog)
    EVT_BUTTON(ID_SSHHOST_ADD, SshHostManagerDialog::OnAdd)
    EVT_BUTTON(ID_SSHHOST_EDIT, SshHostManagerDialog::OnEdit)
    EVT_BUTTON(ID_SSHHOST_DELETE, SshHostManagerDialog::OnDelete)
    EVT_LIST_ITEM_ACTIVATED(ID_SSHHOST_LIST, SshHostManagerDialog::OnListDoubleClick)
    EVT_LIST_ITEM_SELECTED(ID_SSHHOST_LIST, SshHostManagerDialog::OnListSelectionChanged)
    EVT_LIST_ITEM_DESELECTED(ID_SSHHOST_LIST, SshHostManagerDialog::OnListSelectionChanged)
wxEND_EVENT_TABLE()

SshHostManagerDialog::SshHostManagerDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, wxT("SSH 主机管理"),
               wxDefaultPosition, wxSize(560, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    m_listView = new wxListView(panel, ID_SSHHOST_LIST, wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL);
    m_listView->AppendColumn(wxT("名称"), wxLIST_FORMAT_LEFT, 140);
    m_listView->AppendColumn(wxT("主机"), wxLIST_FORMAT_LEFT, 180);
    m_listView->AppendColumn(wxT("端口"), wxLIST_FORMAT_LEFT, 70);
    m_listView->AppendColumn(wxT("用户名"), wxLIST_FORMAT_LEFT, 120);
    m_listView->EnableAlternateRowColours();
    mainSizer->Add(m_listView, 1, wxEXPAND | wxALL, 10);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(new wxButton(panel, ID_SSHHOST_ADD, wxT("新建")), 0, wxRIGHT, 8);
    btnSizer->Add(new wxButton(panel, ID_SSHHOST_EDIT, wxT("编辑")), 0, wxRIGHT, 8);
    btnSizer->Add(new wxButton(panel, ID_SSHHOST_DELETE, wxT("删除")), 0);
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

void SshHostManagerDialog::RefreshList() {
    m_listView->DeleteAllItems();
    const auto& hosts = ConfigManager::GetInstance().GetSshHosts();
    for (size_t i = 0; i < hosts.size(); ++i) {
        const auto& h = hosts[i];
        long index = m_listView->InsertItem(static_cast<long>(i), wxString::FromUTF8(h.name));
        m_listView->SetItem(index, 1, wxString::FromUTF8(h.host));
        m_listView->SetItem(index, 2, wxString::Format(wxT("%d"), h.port));
        m_listView->SetItem(index, 3, wxString::FromUTF8(h.username));
        m_listView->SetItemData(index, static_cast<long>(i));
    }
    UpdateButtonStates();
}

void SshHostManagerDialog::OnAdd(wxCommandEvent& event) {
    SshHostDialog dlg(this, wxT("新建 SSH 主机"));
    if (dlg.ShowModal() == wxID_OK) {
        ConfigManager::GetInstance().AddSshHost(dlg.GetSshHost());
        RefreshList();
    }
}

void SshHostManagerDialog::OnEdit(wxCommandEvent& event) {
    long sel = m_listView->GetFirstSelected();
    if (sel < 0) return;
    size_t idx = static_cast<size_t>(m_listView->GetItemData(sel));
    const auto& hosts = ConfigManager::GetInstance().GetSshHosts();
    if (idx >= hosts.size()) return;

    SshHostDialog dlg(this, wxT("编辑 SSH 主机"), hosts[idx]);
    if (dlg.ShowModal() == wxID_OK) {
        ConfigManager::GetInstance().UpdateSshHost(hosts[idx].id, dlg.GetSshHost());
        RefreshList();
    }
}

void SshHostManagerDialog::OnDelete(wxCommandEvent& event) {
    long sel = m_listView->GetFirstSelected();
    if (sel < 0) return;
    size_t idx = static_cast<size_t>(m_listView->GetItemData(sel));
    const auto& hosts = ConfigManager::GetInstance().GetSshHosts();
    if (idx >= hosts.size()) return;

    int result = wxMessageBox(
        wxString::Format(wxT("确定删除 SSH 主机 \"%s\" 吗？\n引用该主机的配置将退化为本地终端。"),
                         wxString::FromUTF8(hosts[idx].name)),
        wxT("确认删除"), wxYES_NO | wxICON_QUESTION, this);
    if (result == wxYES) {
        ConfigManager::GetInstance().DeleteSshHost(hosts[idx].id);
        RefreshList();
    }
}

void SshHostManagerDialog::OnListDoubleClick(wxListEvent& event) {
    wxCommandEvent dummy;
    OnEdit(dummy);
}

void SshHostManagerDialog::OnListSelectionChanged(wxListEvent& event) {
    UpdateButtonStates();
}

void SshHostManagerDialog::UpdateButtonStates() {
    bool hasSelection = m_listView->GetFirstSelected() >= 0;
    FindWindow(ID_SSHHOST_EDIT)->Enable(hasSelection);
    FindWindow(ID_SSHHOST_DELETE)->Enable(hasSelection);
}
