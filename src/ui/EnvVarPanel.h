#pragma once
#include <wx/wx.h>
#include <wx/listctrl.h>
#include "core/Types.h"

// 环境变量编辑面板
class EnvVarPanel : public wxPanel {
public:
    EnvVarPanel(wxWindow* parent);
    
    // 获取/设置环境变量列表
    std::vector<EnvVariable> GetEnvVariables() const;
    void SetEnvVariables(const std::vector<EnvVariable>& vars);
    
private:
    wxListView* m_listView;
    wxTextCtrl* m_txtName;
    wxTextCtrl* m_txtValue;
    wxButton* m_btnAdd;
    wxButton* m_btnRemove;
    
    void CreateControls();
    void OnAdd(wxCommandEvent& event);
    void OnRemove(wxCommandEvent& event);
    void OnListSelectionChanged(wxListEvent& event);
    void UpdateButtonStates();
    
    wxDECLARE_EVENT_TABLE();
};

// 控件 ID
enum EnvVarPanelIds {
    ID_ENV_LIST = wxID_HIGHEST + 100,
    ID_ENV_NAME,
    ID_ENV_VALUE,
    ID_ENV_ADD,
    ID_ENV_REMOVE
};
