#include <wx/wx.h>
#include <wx/artprov.h>
#include<wx/treectrl.h>  
#include<wx/splitter.h>
#include <wx/aui/aui.h>
#include <wx/panel.h>
#include "ElementDraw.h"
#include<fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// 定义菜单和工具栏ID
enum
{
    ID_CUT = wxID_HIGHEST + 1,
    ID_COPY,
    ID_ADD_CIRCUIT,
    ID_SIM_ENABLE,
    ID_WINDOW_CASCADE,
    ID_HELP_ABOUT,
    ID_FILE_CLOSE,
    ID_FILE_SAVE,
    ID_FILE_OPENRECENT,

};

enum ToolID {
    ID_TOOL_CHGVALUE,
    ID_TOOL_EDITSELECT,
    ID_TOOL_EDITTXET,
    ID_TOOL_ADDPIN4,
    ID_TOOL_ADDPIN5,
    ID_TOOL_ADDNOTGATE,
    ID_TOOL_ADDANDGATE,
    ID_TOOL_ADDORGATE
};



class MyApp : public wxApp
{
public:
    virtual bool OnInit();
};


class MyFrame : public wxFrame
{
public:
    MyFrame();
	void SetPlacementType(const std::string& type) { m_currentPlacementType = type; }
    std::string GetPlacementType() const { return m_currentPlacementType; }
private:
    void OnOpen(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnCut(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnAddCircuit(wxCommandEvent& event);
    void OnSimEnable(wxCommandEvent& event);
    void OnWindowCascade(wxCommandEvent& event);
    void OnHelp(wxCommandEvent& event);

	wxAuiManager m_mgr;
    std::string m_currentPlacementType;
};



//资源管理器
class MyTreePanel : public wxPanel
{
public:
    MyTreePanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        tree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);
        wxTreeItemId root = tree->AddRoot("Resource manage");
        wxTreeItemId child1 = tree->AppendItem(root, "Untitled");
        wxTreeItemId child2 = tree->AppendItem(root, "main");
        wxTreeItemId child3 = tree->AppendItem(root, "Wiring");
        tree->AppendItem(child3, "splitter");
        tree->AppendItem(child3, "Pin");
        wxTreeItemId child4 = tree->AppendItem(root, "Gate");
        tree->AppendItem(child4, "AND");
        tree->AppendItem(child4, "OR");
		tree->AppendItem(child4, "NOT");
        tree->ExpandAll();
        sizer->Add(tree, 1, wxEXPAND | wxALL, 5);
        SetSizer(sizer);

        tree->Bind(wxEVT_TREE_SEL_CHANGED, &MyTreePanel::OnSelChanged, this);
	}
private:
	wxTreeCtrl* tree;
    void OnSelChanged(wxTreeEvent& event)
    {
        wxTreeItemId item = event.GetItem();
        if (!item.IsOk()) return;
        wxString sel = tree->GetItemText(item);
        // 只在叶子或具体元件项设置类型（你可以添加更多判断）
        // 将选择传给顶层 MyFrame
        wxWindow* top = wxGetTopLevelParent(this);
        if (!top) return;
        MyFrame* mf = dynamic_cast<MyFrame*>(top);
        if (mf) {
            mf->SetPlacementType(sel.ToStdString());
            mf->SetStatusText(wxString("Selected for placement: ") + sel);
        }
	}

};

//属性表
class PropertyPanel : public wxPanel
{
    public:
    PropertyPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        wxStaticText* label = new wxStaticText(this, wxID_ANY, "Properties");
        sizer->Add(label, 0, wxALIGN_CENTER | wxALL, 5);
        // Add more property controls here
        SetSizer(sizer);
	}
};


//画布
class CanvasPanel : public wxPanel
{
public:
    CanvasPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundColour(*wxWHITE);
        Bind(wxEVT_LEFT_DOWN, &CanvasPanel::OnLeftDown, this);
    }
    void OnPaint(wxPaintEvent& event);
	void OnLeftDown(wxMouseEvent& event);

protected:
    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(CanvasPanel, wxPanel)
   EVT_PAINT(CanvasPanel::OnPaint)
wxEND_EVENT_TABLE()



bool MyApp::OnInit()
{

    MyFrame* frame = new MyFrame();
    frame->Show(true);
    return true;
}

MyFrame::MyFrame()
    : wxFrame(NULL, -1, "logisim")
{
    SetSize(800,600);
    // File 
    wxMenu* menuFile = new wxMenu;
    menuFile->Append(wxID_NEW, "Open New File");
    menuFile->Append(wxID_EXIT, "Exit");
    menuFile->Append(ID_FILE_OPENRECENT, "OpenRecent");
	menuFile->Append(ID_FILE_SAVE, "Save");

       
    // Edit 
    wxMenu* menuEdit = new wxMenu;
    menuEdit->Append(ID_CUT, "Cut");
    menuEdit->Append(ID_COPY, "Copy");

    // Project 
    wxMenu* menuProject = new wxMenu;
    menuProject->Append(ID_ADD_CIRCUIT, "Add Circuit");

    // Simulate 
    wxMenu* menuSim = new wxMenu;
    menuSim->Append(ID_SIM_ENABLE, "Enable");

    // Window 
    wxMenu* menuWindow = new wxMenu;
    menuWindow->Append(ID_WINDOW_CASCADE, "Cascade Windows");

    // Help 
    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(ID_HELP_ABOUT, "About");

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, _T("File"));
    menuBar->Append(menuEdit, _T("Edit"));
    menuBar->Append(menuProject, _T("Project"));
    menuBar->Append(menuSim, _T("Simulate"));
    menuBar->Append(menuWindow, _T("Window"));
    menuBar->Append(menuHelp, _T("Help"));

    SetMenuBar(menuBar);


    //工具栏
	wxToolBar* toolBar = CreateToolBar();
    toolBar->AddTool(ID_TOOL_CHGVALUE, "Change Value", wxArtProvider::GetBitmap(wxART_NEW, wxART_TOOLBAR));
    toolBar->AddTool(ID_TOOL_EDITSELECT, "Edit selection", wxArtProvider::GetBitmap(wxART_CUT, wxART_TOOLBAR));
    toolBar->AddSeparator();
    toolBar->Realize();
    /*
    wxBitmap myIcon1(wxT("image/logisim2.png"), wxBITMAP_TYPE_PNG);
    toolBar->AddTool(ID_TOOL_CHGVALUE, "Change Value", myIcon1);
	wxBitmap myIcon2(wxT("image/logisim3.png"), wxBITMAP_TYPE_PNG);
    toolBar->AddTool(ID_TOOL_EDITSELECT, "Edit selection",myIcon2);
	wxBitmap myIcon3(wxT("image/logisim4.png"), wxBITMAP_TYPE_PNG);
	toolBar->AddTool(ID_TOOL_EDITTXET, "Edit Text", myIcon3);
	wxBitmap myIcon4(wxT("image/logisim5.png"), wxBITMAP_TYPE_PNG);
	toolBar->AddTool(ID_TOOL_ADDPIN4, "Add Pin 4", myIcon4);
	wxBitmap myIcon5(wxT("image/logisim6.png"), wxBITMAP_TYPE_PNG);
	toolBar->AddTool(ID_TOOL_ADDPIN5, "Add Pin 5", myIcon5);
	wxBitmap myIcon6(wxT("image/logisim7.png"), wxBITMAP_TYPE_PNG);
	toolBar->AddTool(ID_TOOL_ADDNOTGATE, "Add NOT Gate", myIcon6);
    toolBar->Realize();
    */

	//划分窗口，左侧资源管理器，右侧画布
	wxSplitterWindow* splitter = new wxSplitterWindow(this,wxID_ANY);
	MyTreePanel* leftPanel = new MyTreePanel(splitter);
    CanvasPanel* rightPanel = new CanvasPanel(splitter);
	splitter->SplitVertically(leftPanel, rightPanel, 200);


    CreateStatusBar();

    Bind(wxEVT_MENU, &MyFrame::OnOpen, this, wxID_NEW);
    Bind(wxEVT_MENU, &MyFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MyFrame::OnCut, this, ID_CUT);
    Bind(wxEVT_MENU, &MyFrame::OnCopy, this, ID_COPY);
    Bind(wxEVT_MENU, &MyFrame::OnAddCircuit, this, ID_ADD_CIRCUIT);
    Bind(wxEVT_MENU, &MyFrame::OnSimEnable, this, ID_SIM_ENABLE);
    Bind(wxEVT_MENU, &MyFrame::OnWindowCascade, this, ID_WINDOW_CASCADE);
    Bind(wxEVT_MENU, &MyFrame::OnHelp, this, ID_HELP_ABOUT);
}



void MyFrame::OnOpen(wxCommandEvent& event)
{
    wxMessageBox("打开新文件", "File", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void MyFrame::OnCut(wxCommandEvent& event)
{
    wxMessageBox("剪切", "Edit", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnCopy(wxCommandEvent& event)
{
    wxMessageBox("复制", "Edit", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnAddCircuit(wxCommandEvent& event)
{
    wxMessageBox("添加", "Project", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnSimEnable(wxCommandEvent& event)
{
    wxMessageBox("仿真启用", "Simulate", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnWindowCascade(wxCommandEvent& event)
{
    wxMessageBox("窗口", "Window", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnHelp(wxCommandEvent& event)
{
    wxMessageBox("Logisim 帮助", "Help", wxOK | wxICON_INFORMATION);
}

void CanvasPanel::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    dc.SetPen(*wxLIGHT_GREY_PEN);
    int height, width;
    GetClientSize(&width, &height);
    for (int i = 0; i < width; i += 10) {
        for (int j = 0; j < height; j += 10) {
            dc.DrawPoint(i, j);
        }
    }
    std::ifstream file("Elementlib.json");
    if (!file.is_open()) return;
    json j;
    file >> j;

    for (const auto& comp : j["elements"]) {
        std::string type = comp["type"];
        std::string color = comp["color"];
        int thickness = comp["thickness"];
        int x = comp["x"];
        int y = comp["y"];
        DrawElement(dc, type, color, thickness, x, y);

    }

   
}

void CanvasPanel::OnLeftDown(wxMouseEvent& event) {
    wxPoint pt = event.GetPosition();

    // 从顶层 MyFrame 读取当前树中选择的放置类型
    wxWindow* top = wxGetTopLevelParent(this);
    MyFrame* mf = dynamic_cast<MyFrame*>(top);
    std::string placeType;
    if (mf) placeType = mf->GetPlacementType();

    // 如果树上有选择类型，则在单击处放置该类型元件
    if (!placeType.empty()) {
        // 读取现有 JSON，追加新元素并写回
        json j;
        std::ifstream ifs("Elementlib.json");
        if (ifs.is_open()) {
            try { ifs >> j; }
            catch (...) { j = json::object(); }
            ifs.close();
        }
        if (!j.contains("elements") || !j["elements"].is_array()) j["elements"] = json::array();

        json newElem;
        newElem["type"] = placeType;
        newElem["color"] = "black";
        newElem["thickness"] = 1;
        newElem["x"] = pt.x;
        newElem["y"] = pt.y;
        // 可选字段初始化
        newElem["size"] = 1;
        newElem["rotationIndex"] = 0;
        newElem["inputs"] = 2;

        j["elements"].push_back(newElem);

        std::ofstream ofs("Elementlib.json");
        if (ofs.is_open()) {
            ofs << j.dump(4);
            ofs.close();
        }

        // 可选：清除当前放置类型（让用户重新选择）
        if (mf) mf->SetPlacementType(std::string());

        // 刷新画布
        Refresh();
    }

    event.Skip();
}



wxIMPLEMENT_APP(MyApp);