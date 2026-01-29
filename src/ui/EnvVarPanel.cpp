#include "EnvVarPanel.h"

wxBEGIN_EVENT_TABLE(EnvVarPanel, wxPanel)
    EVT_BUTTON(ID_ENV_ADD, EnvVarPanel::OnAdd)
    EVT_BUTTON(ID_ENV_REMOVE, EnvVarPanel::OnRemove)
    EVT_LIST_ITEM_SELECTED(ID_ENV_LIST, EnvVarPanel::OnListSelectionChanged)
    EVT_LIST_ITEM_DESELECTED(ID_ENV_LIST, EnvVarPanel::OnListSelectionChanged)
wxEND_EVENT_TABLE()

EnvVarPanel::EnvVarPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    CreateControls();
    UpdateButtonStates();
}

void EnvVarPanel::CreateControls() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    // 标题
    wxStaticText* label = new wxStaticText(this, wxID_ANY, wxT("环境变量:"));
    mainSizer->Add(label, 0, wxBOTTOM, 5);
    
    // 列表
    m_listView = new wxListView(this, ID_ENV_LIST, 
                                 wxDefaultPosition, wxSize(-1, 150),
                                 wxLC_REPORT | wxLC_SINGLE_SEL);
    
    m_listView->AppendColumn(wxT("变量名"), wxLIST_FORMAT_LEFT, 180);
    m_listView->AppendColumn(wxT("值"), wxLIST_FORMAT_LEFT, 280);
    
    mainSizer->Add(m_listView, 1, wxEXPAND | wxBOTTOM, 10);
    
    // 输入区域
    wxFlexGridSizer* inputSizer = new wxFlexGridSizer(2, 3, 5, 10);
    inputSizer->AddGrowableCol(1, 1);
    
    inputSizer->Add(new wxStaticText(this, wxID_ANY, wxT("变量名:")), 
                    0, wxALIGN_CENTER_VERTICAL);
    m_txtName = new wxTextCtrl(this, ID_ENV_NAME);
    inputSizer->Add(m_txtName, 1, wxEXPAND);
    inputSizer->AddSpacer(0);
    
    inputSizer->Add(new wxStaticText(this, wxID_ANY, wxT("值:")), 
                    0, wxALIGN_CENTER_VERTICAL);
    m_txtValue = new wxTextCtrl(this, ID_ENV_VALUE);
    inputSizer->Add(m_txtValue, 1, wxEXPAND);
    inputSizer->AddSpacer(0);
    
    mainSizer->Add(inputSizer, 0, wxEXPAND | wxBOTTOM, 10);
    
    // 按钮
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    
    m_btnAdd = new wxButton(this, ID_ENV_ADD, wxT("添加"));
    m_btnRemove = new wxButton(this, ID_ENV_REMOVE, wxT("删除选中"));
    
    btnSizer->Add(m_btnAdd, 0, wxRIGHT, 10);
    btnSizer->Add(m_btnRemove, 0);
    
    mainSizer->Add(btnSizer, 0, wxALIGN_LEFT);
    
    SetSizer(mainSizer);
}

void EnvVarPanel::OnAdd(wxCommandEvent& event) {
    wxString name = m_txtName->GetValue().Trim().Trim(false);
    wxString value = m_txtValue->GetValue();
    
    if (name.IsEmpty()) {
        wxMessageBox(wxT("请输入变量名"), wxT("提示"), 
                     wxOK | wxICON_INFORMATION, this);
        m_txtName->SetFocus();
        return;
    }
    
    // 检查是否已存在
    for (int i = 0; i < m_listView->GetItemCount(); i++) {
        if (m_listView->GetItemText(i, 0) == name) {
            // 更新现有值
            m_listView->SetItem(i, 1, value);
            m_txtName->Clear();
            m_txtValue->Clear();
            m_txtName->SetFocus();
            return;
        }
    }
    
    // 添加新项
    long index = m_listView->InsertItem(m_listView->GetItemCount(), name);
    m_listView->SetItem(index, 1, value);
    
    m_txtName->Clear();
    m_txtValue->Clear();
    m_txtName->SetFocus();
    
    UpdateButtonStates();
}

void EnvVarPanel::OnRemove(wxCommandEvent& event) {
    long selected = m_listView->GetFirstSelected();
    if (selected >= 0) {
        m_listView->DeleteItem(selected);
        UpdateButtonStates();
    }
}

void EnvVarPanel::OnListSelectionChanged(wxListEvent& event) {
    UpdateButtonStates();
    
    // 选中时显示内容到输入框
    long selected = m_listView->GetFirstSelected();
    if (selected >= 0) {
        m_txtName->SetValue(m_listView->GetItemText(selected, 0));
        m_txtValue->SetValue(m_listView->GetItemText(selected, 1));
    }
}

void EnvVarPanel::UpdateButtonStates() {
    m_btnRemove->Enable(m_listView->GetFirstSelected() >= 0);
}

std::vector<EnvVariable> EnvVarPanel::GetEnvVariables() const {
    std::vector<EnvVariable> vars;
    
    for (int i = 0; i < m_listView->GetItemCount(); i++) {
        EnvVariable var;
        var.name = m_listView->GetItemText(i, 0).ToStdString();
        var.value = m_listView->GetItemText(i, 1).ToStdString();
        if (!var.name.empty()) {
            vars.push_back(var);
        }
    }
    
    return vars;
}

void EnvVarPanel::SetEnvVariables(const std::vector<EnvVariable>& vars) {
    m_listView->DeleteAllItems();
    
    for (size_t i = 0; i < vars.size(); i++) {
        long index = m_listView->InsertItem(static_cast<long>(i), 
                                             wxString::FromUTF8(vars[i].name));
        m_listView->SetItem(index, 1, wxString::FromUTF8(vars[i].value));
    }
    
    UpdateButtonStates();
}
