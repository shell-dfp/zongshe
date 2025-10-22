#include <wx/wx.h>
#include <wx/artprov.h>
#include<wx/treectrl.h>  
#include<wx/splitter.h>
#include <wx/aui/aui.h>
#include <wx/panel.h>
#include "ElementDraw.h"
#include<fstream>
#include <nlohmann/json.hpp>
#include <wx/stdpaths.h>

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
        tree->AppendItem(child3, "Input Pin");
        tree->AppendItem(child3, "Output Pin");
        tree->AppendItem(child3, "Clock");
        wxTreeItemId child4 = tree->AppendItem(root, "Gate");
        tree->AppendItem(child4, "AND");
        tree->AppendItem(child4, "OR");
        tree->AppendItem(child4, "NOT");
        tree->AppendItem(child4, "NAND");
        tree->AppendItem(child4, "NOR");
        tree->AppendItem(child4, "XOR");
        tree->AppendItem(child4, "XNOR");
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
        : wxPanel(parent, wxID_ANY),
        m_dragging(false),
        m_dragIndex(-1)
    {
        SetBackgroundColour(*wxWHITE);
    }
    void OnPaint(wxPaintEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);

    static void SaveElementsJson(const json& j);

protected:
    wxDECLARE_EVENT_TABLE();

private:
    bool m_dragging;
    int m_dragIndex;           // index in JSON array
    wxPoint m_dragOffset;      // 鼠标相对元件位置偏移（保证拖拽平滑）

    // 简单的命中测试：假设每个元件以 (x,y) 为中心，用 size 字段决定占用像素
    static bool ElementHitTest(const json& comp, const wxPoint& pt);
    // 读取 JSON 并返回元素数量，空时返回 0；失败返回 0
    static json LoadElementsJson();
    // 返回命中的元素索引，未命中返回 -1
    static int FindElementIndexAt(const wxPoint& pt);
};


bool MyApp::OnInit()
{
    json j;
    j["elements"] = json::array();
    CanvasPanel::SaveElementsJson(j);
    MyFrame* frame = new MyFrame();
    frame->Show(true);
    return true;
}

MyFrame::MyFrame()
    : wxFrame(NULL, -1, "logisim")
{
    SetSize(800, 600);
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

    //划分窗口，左侧资源管理器，右侧画布
    wxSplitterWindow* splitter = new wxSplitterWindow(this, wxID_ANY);
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

wxBEGIN_EVENT_TABLE(CanvasPanel, wxPanel)
    EVT_PAINT(CanvasPanel::OnPaint)
    EVT_LEFT_DOWN(CanvasPanel::OnLeftDown)
    EVT_MOTION(CanvasPanel::OnMouseMove)
    EVT_LEFT_UP(CanvasPanel::OnLeftUp)
wxEND_EVENT_TABLE()

bool CanvasPanel::ElementHitTest(const json& comp, const wxPoint& pt) {
    if (!comp.is_object())
        return false;
    int x = comp.value("x", 0);
    int y = comp.value("y", 0);
    int size = comp.value("size", 1);
    int half = 20 * std::max(1, size);
    return (pt.x >= x - half && pt.x <= x + half && pt.y >= y - half && pt.y <= y + half);
}

json CanvasPanel::LoadElementsJson() {
    json j;
    try {
        std::ifstream ifs("Elementlib.json");
        if (ifs.is_open()) {
            ifs >> j;
            ifs.close();
        }
    }
    catch (...) {
        // 出现写文件错误时忽略（可添加日志）
    }
    if (!j.contains("elements") || !j["elements"].is_array())
        j["elements"] = json::array();
    return j;
}

void CanvasPanel::SaveElementsJson(const json& j) {
    try {
        std::ofstream ofs("Elementlib.json");
        if (ofs.is_open()) {
            ofs << j.dump(4);
            ofs.close();
        }
    }
    catch (...) {}
}

int CanvasPanel::FindElementIndexAt(const wxPoint& pt) {
    json j = LoadElementsJson();
    for (size_t i = 0; i < j["elements"].size(); i++) {
        if (ElementHitTest(j["elements"][i], pt))
            return static_cast<int>(i);
    }
    return -1;
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
    json j = CanvasPanel::LoadElementsJson();
    if (!j.contains("elements") || !j["elements"].is_array()) return;

    for (const auto& comp : j["elements"]) {
        std::string type = comp.value("type", std::string());
        std::string color = comp.value("color", std::string("black"));
        int thickness = comp.value("thickness", 1);
        int x = comp.value("x", 0);
        int y = comp.value("y", 0);
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

    // 放置新元件（当树上有选择类型时）
    if (!placeType.empty()) {
        json j = LoadElementsJson();
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
        SaveElementsJson(j);

        // 可选：清除当前放置类型（让用户重新选择）
        if (mf) mf->SetPlacementType(std::string());

        // 刷新画布
        Refresh();
        event.Skip();
        return;
    }

    // 若未处于放置模式，尝试选择已有元件并进入拖拽模式
    int idx = FindElementIndexAt(pt);
    if (idx >= 0) {
        // 记住要拖拽的元素和偏移
        json j = LoadElementsJson();
        if (idx < (int)j["elements"].size()) {
            int ex = j["elements"][idx].value("x", 0);
            int ey = j["elements"][idx].value("y", 0);
            m_dragging = true;
            m_dragIndex = idx;
            m_dragOffset = wxPoint(pt.x - ex, pt.y - ey);
            SetCursor(wxCursor(wxCURSOR_HAND));
        }
    }
    event.Skip();
}

void CanvasPanel::OnMouseMove(wxMouseEvent& event) {
    if (!m_dragging || m_dragIndex < 0) {
        event.Skip();
        return;
    }

    if (event.LeftIsDown()) {
        wxPoint pos = event.GetPosition();
        int newx = pos.x - m_dragOffset.x;
        int newy = pos.y - m_dragOffset.y;

        // 更新 JSON 中对应元素的位置并保存（为了简单直接写文件）
        json j = LoadElementsJson();
        if (m_dragIndex >= 0 && m_dragIndex < (int)j["elements"].size()) {
            j["elements"][m_dragIndex]["x"] = newx;
            j["elements"][m_dragIndex]["y"] = newy;
            SaveElementsJson(j);
            Refresh(); // 触发重绘，OnPaint 会读取最新 JSON 并绘制
        }
    }
    event.Skip();
}

void CanvasPanel::OnLeftUp(wxMouseEvent& event) {
    if (m_dragging) {
        // 结束拖拽
        m_dragging = false;
        m_dragIndex = -1;
        SetCursor(wxCursor(wxCURSOR_ARROW));
    }
    event.Skip();
}

wxIMPLEMENT_APP(MyApp);
