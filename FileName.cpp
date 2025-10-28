#include <wx/wx.h>
#include <wx/artprov.h>
#include<wx/treectrl.h>  
#include<wx/splitter.h>
#include <wx/aui/aui.h>
#include<wx/dcbuffer.h>
#include <wx/panel.h>
#include "ElementDraw.h"
#include<fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// 定义菜单和工具栏ID
enum
{
    ID_CUT,
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
    ID_TOOL_ADDORGATE,
	ID_TOOL_ADDOUTPUTPIN,
	ID_TOOL_ADDINPUTPIN,
};

struct ElementInfo {
    std::string type;
    std::string color;
    int thickness = 1;
    int x = 0;
    int y = 0;
    int size = 1;
    int rotationIndex = 0;
    int inputs = 0;
};

struct ConnectionInfo {
    int aIndex = -1;
    int bIndex = -1;
	int aPin = -1;//连线时便于识别输入输出的地方
	int bPin = -1;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
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
    const std::string GetPlacementType() const { return m_currentPlacementType; }
private:
    void OnOpen(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnCut(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnAddCircuit(wxCommandEvent& event);
    void OnSimEnable(wxCommandEvent& event);
    void OnWindowCascade(wxCommandEvent& event);
    void OnHelp(wxCommandEvent& event);

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
		tree->AppendItem(child4, "NAND");
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
        // 只在叶子或具体元件项设置类型
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
        m_backValid(false),
        m_dragging(false),
        m_dragIndex(-1),
        m_connecting(false),
        m_connectStartIsOutput(false),
        m_connectStartPin(-1),
        m_connectEndPin(-1)
    {
        // 启用基于 Paint 的背景绘制以支持双缓冲
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(*wxWHITE);

        // 一次性从文件加载实例到内存（OnPaint 不再读文件）
        LoadElementsAndConnectionsFromFile();

        Bind(wxEVT_LEFT_DOWN, &CanvasPanel::OnLeftDown, this);
        Bind(wxEVT_PAINT, &CanvasPanel::OnPaint, this);
        Bind(wxEVT_SIZE, &CanvasPanel::OnSize, this);
        Bind(wxEVT_MOTION, &CanvasPanel::OnMouseMove, this);
        Bind(wxEVT_LEFT_UP, &CanvasPanel::OnLeftUp, this);
        Bind(wxEVT_RIGHT_DOWN, &CanvasPanel::OnRightDown, this);
        Bind(wxEVT_RIGHT_UP, &CanvasPanel::OnRightUp, this);
    }

    void OnPaint(wxPaintEvent& event);
    void OnLeftDown(wxMouseEvent& event);   

    void OnSize(wxSizeEvent& event)
    {
        // 标记后备位图失效，下一次 OnPaint 会重建
        m_backValid = false;
        Refresh();
        event.Skip();
    }

    void OnMouseMove(wxMouseEvent& event)
    {
        wxPoint pt = event.GetPosition();
        if (m_dragging && m_dragIndex >= 0) {
            // 更新临时位置（不提交），刷新受影响区域
            wxPoint newPos(pt.x - m_dragOffset.x, pt.y - m_dragOffset.y);
            wxRect oldRect = ElementRect(m_dragStart);
            wxRect newRect = ElementRect(newPos);
            m_dragCurrent = newPos;
            // 刷新并集区域，避免全屏重绘
            RefreshRect(oldRect.Union(newRect));
        }
        else if (m_connecting) {
            // 鼠标移动时更新临时终点（吸附到格点）
            m_tempLineEnd = SnapToGrid(pt);
            // 刷新橡皮筋区域
            wxPoint startPt;
            if (m_connectStartElem >= 0 && m_connectStartIsOutput) startPt = GetConnectorPosition(m_connectStartElem, m_connectStartPin, true);
            else startPt = m_connectStartGrid;
            wxRect r(startPt, m_tempLineEnd);
            r.Inflate(6, 6);
            RefreshRect(r);
        }
        event.Skip();
    }

    void OnLeftUp(wxMouseEvent& event)
    {
        if (m_dragging && m_dragIndex >= 0) {
            // 提交位置到数据并保存
            m_elements[m_dragIndex].x = m_dragCurrent.x;
            m_elements[m_dragIndex].y = m_dragCurrent.y;
            SaveElementsAndConnectionsToFile();
            m_backValid = false;
            RebuildBackbuffer();
            if (HasCapture()) ReleaseMouse();
            m_dragging = false;
            m_dragIndex = -1;
            Refresh();
        }
        event.Skip();
    }

    void OnRightDown(wxMouseEvent& event)
    {
        wxPoint pt = event.GetPosition();
        // 现在允许从任意格点开始连线，同时检测起点是否位于某元素的输出
        ConnectorHit hit = HitTestConnector(pt);
        m_connecting = true;
        m_connectStartGrid = SnapToGrid(pt);
        m_tempLineEnd = m_connectStartGrid;
        if (hit.hit && hit.isOutput) {
            m_connectStartElem = hit.elemIndex;
            m_connectStartPin = hit.pinIndex;
            m_connectStartIsOutput = true;
        }
        else {
            m_connectStartElem = -1;
            m_connectStartPin = -1;
            m_connectStartIsOutput = false;
        }
        CaptureMouse();
        event.Skip();
    }

    void OnRightUp(wxMouseEvent& event)
    {
        if (m_connecting) {
            wxPoint pt = event.GetPosition();
            wxPoint snapped = SnapToGrid(pt);
            ConnectorHit endHit = HitTestConnector(pt);

            // 保存连接：无论起点/终点是否在元素上都保存一条连接（用户要求）
            ConnectionInfo c;

            // 如果起点绑定到元素输出，则使用该输出的精确位置，否则使用起始的格点
            if (m_connectStartIsOutput && m_connectStartElem >= 0) {
                wxPoint startPos = GetConnectorPosition(m_connectStartElem, m_connectStartPin, true);
                c.x1 = startPos.x; c.y1 = startPos.y;
                c.aIndex = m_connectStartElem;
                c.aPin = m_connectStartPin;
            }
            else {
                c.x1 = m_connectStartGrid.x; c.y1 = m_connectStartGrid.y;
                c.aIndex = -1; c.aPin = -1;
            }

            // 终点：如果命中某元素的输入，则吸附到输入精确位置并记录；否则使用吸附到的格点
            if (endHit.hit && !endHit.isOutput) {
                wxPoint endPos = GetConnectorPosition(endHit.elemIndex, endHit.pinIndex, false);
                c.x2 = endPos.x; c.y2 = endPos.y;
                c.bIndex = endHit.elemIndex;
                c.bPin = endHit.pinIndex;
            }
            else {
                c.x2 = snapped.x; c.y2 = snapped.y;
                c.bIndex = -1; c.bPin = -1;
            }

            // push and persist
            m_connections.push_back(c);
            SaveElementsAndConnectionsToFile();
            m_backValid = false;
            RebuildBackbuffer();
            Refresh();

            if (HasCapture()) ReleaseMouse();
            m_connecting = false;
            m_connectStartElem = -1;
            m_connectStartPin = -1;
            m_connectStartIsOutput = false;
        }
        event.Skip();
    }

private:
    // 数据
    std::vector<ElementInfo> m_elements;
    std::vector<ConnectionInfo> m_connections;

    // 后备位图
    wxBitmap m_backBitmap;
    bool m_backValid;

    // 拖拽状态
    bool m_dragging;
    int m_dragIndex;
    wxPoint m_dragOffset;
    wxPoint m_dragStart;   // 原始位置
    wxPoint m_dragCurrent; // 临时位置（拖拽时显示）

    // 连线状态
    bool m_connecting;
    wxPoint m_connectStartGrid; // start snapped grid point
    int m_connectStartElem; // optional element index if start attached to element output
    bool m_connectStartIsOutput;
    int m_connectStartPin; // pin index at start (for multiple outputs)
    int m_connectEndPin;
    wxPoint m_tempLineEnd;

    const int ElemWidth = 60;
    const int ElemHeight = 40;
    const int ConnectorRadius = 6; // 命中检测半径

    // 结构：Hit result for connectors
    struct ConnectorHit {
        bool hit = false;
        int elemIndex = -1;
        int pinIndex = -1; // for inputs: which input, for output: usually 0
        bool isOutput = false;
    };

    // 将点吸附到格点（10 像素网格）
    wxPoint SnapToGrid(const wxPoint& p) const {
        int gx = (p.x + 5) / 10 * 10;
        int gy = (p.y + 5) / 10 * 10;
        return wxPoint(gx, gy);
    }

    // 计算元素的输出点（右侧小横线），返回屏幕坐标
    wxPoint GetOutputPoint(const ElementInfo& e) const {
        return wxPoint(e.x + ElemWidth, e.y + ElemHeight / 2);
    }

    // 计算元素某个输入 pin 的点（左侧小横线），pinIndex 从 0 到 inputs-1
    wxPoint GetInputPoint(const ElementInfo& e, int pinIndex) const {
        int n = std::max(1, e.inputs);
        // 均匀分布在高度内
        float step = (float)ElemHeight / (n + 1);
        int py = e.y + (int)(step * (pinIndex + 1));
        return wxPoint(e.x, py);
    }

    // 通用：根据 elem index/pin/isOutput 返回 connector 屏幕坐标
    wxPoint GetConnectorPosition(int elemIndex, int pinIndex, bool isOutput) const {
        if (elemIndex < 0 || elemIndex >= (int)m_elements.size()) return wxPoint(0, 0);
        const ElementInfo& e = m_elements[elemIndex];
        if (isOutput) return GetOutputPoint(e);
        return GetInputPoint(e, pinIndex < 0 ? 0 : pinIndex);
    }

    // 命中检测：检查点是否靠近某元素的输入或输出 connector（小横线）
    ConnectorHit HitTestConnector(const wxPoint& p) const {
        ConnectorHit res;
        for (int i = (int)m_elements.size() - 1; i >= 0; --i) {
            const ElementInfo& e = m_elements[i];
            // 输出
            wxPoint out = GetOutputPoint(e);
            if (DistanceSquared(out, p) <= ConnectorRadius * ConnectorRadius) {
                res.hit = true; res.elemIndex = i; res.pinIndex = 0; res.isOutput = true; return res;
            }
            // 输入 pins
            int n = std::max(1, e.inputs);
            for (int pin = 0; pin < n; ++pin) {
                wxPoint in = GetInputPoint(e, pin);
                if (DistanceSquared(in, p) <= ConnectorRadius * ConnectorRadius) {
                    res.hit = true; res.elemIndex = i; res.pinIndex = pin; res.isOutput = false; return res;
                }
            }
        }
        return res;
    }

    static int DistanceSquared(const wxPoint& a, const wxPoint& b) {
        int dx = a.x - b.x;
        int dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    int HitTestElement(const wxPoint& p) const
    {
        for (int i = (int)m_elements.size() - 1; i >= 0; --i) {
            const ElementInfo& e = m_elements[i];
            wxRect r(e.x, e.y, ElemWidth, ElemHeight);
            if (r.Contains(p)) return i;
        }
        return -1;
    }

    wxPoint ElementCenter(const ElementInfo& e) const
    {
        return wxPoint(e.x + ElemWidth / 2, e.y + ElemHeight / 2);
    }

    wxRect ElementRect(const wxPoint& pos) const
    {
        return wxRect(pos.x, pos.y, ElemWidth, ElemHeight);
    }

    void LoadElementsAndConnectionsFromFile()
    {
        m_elements.clear();
        m_connections.clear();
        std::ifstream file("Elementlib.json");
        if (!file.is_open()) return;
        try {
            json j;
            file >> j;
            if (j.contains("elements") && j["elements"].is_array()) {
                for (const auto& comp : j["elements"]) {
                    ElementInfo e;
                    e.type = comp.value("type", std::string());
                    e.color = comp.value("color", std::string("black"));
                    e.thickness = comp.value("thickness", 1);
                    e.x = comp.value("x", 0);
                    e.y = comp.value("y", 0);
                    e.size = comp.value("size", 1);
                    e.rotationIndex = comp.value("rotationIndex", 0);
                    e.inputs = comp.value("inputs", 0);
                    m_elements.push_back(e);
                }
            }
            if (j.contains("connections") && j["connections"].is_array()) {
                for (const auto& c : j["connections"]) {
                    ConnectionInfo ci;
                    ci.aIndex = c.value("a", -1);
                    ci.aPin = c.value("aPin", -1);
                    ci.bIndex = c.value("b", -1);
                    ci.bPin = c.value("bPin", -1);
                    ci.x1 = c.value("x1", 0);
                    ci.y1 = c.value("y1", 0);
                    ci.x2 = c.value("x2", 0);
                    ci.y2 = c.value("y2", 0);
                    m_connections.push_back(ci);
                }
            }
        }
        catch (...) { /* ignore parse errors */ }
        m_backValid = false;
    }

    void SaveElementsAndConnectionsToFile()
    {
        json j;
        j["elements"] = json::array();
        for (const auto& e : m_elements) {
            json item;
            item["type"] = e.type;
            item["color"] = e.color;
            item["thickness"] = e.thickness;
            item["x"] = e.x;
            item["y"] = e.y;
            item["size"] = e.size;
            item["rotationIndex"] = e.rotationIndex;
            item["inputs"] = e.inputs;
            j["elements"].push_back(item);
        }
        j["connections"] = json::array();
        for (const auto& c : m_connections) {
            json cj;
            cj["a"] = c.aIndex;
            cj["aPin"] = c.aPin;
            cj["b"] = c.bIndex;
            cj["bPin"] = c.bPin;
            cj["x1"] = c.x1; cj["y1"] = c.y1;
            cj["x2"] = c.x2; cj["y2"] = c.y2;
            j["connections"].push_back(cj);
        }
        std::ofstream ofs("Elementlib.json");
        if (ofs.is_open()) {
            ofs << j.dump(4);
            ofs.close();
        }
    }

    void RebuildBackbuffer()
    {
        wxSize sz = GetClientSize();
        if (sz.x <= 0 || sz.y <= 0) { m_backValid = false; return; }

        m_backBitmap = wxBitmap(sz.x, sz.y);
        wxMemoryDC mdc(m_backBitmap);
        mdc.SetBackground(wxBrush(GetBackgroundColour()));
        mdc.Clear();

        DrawGrid(mdc);

        // 绘制连接线（在元素下方） — 所有连接都画出来，若连接保存时为 output->input 则用红色
        for (const auto& c : m_connections) {
            bool isOutputToInput = (c.aIndex >= 0 && c.bIndex >= 0);
            if (isOutputToInput) {
                wxPen p(wxColour(255, 0, 0), 2); // 红色
                mdc.SetPen(p);
            }
            else {
                mdc.SetPen(*wxBLACK_PEN);
            }
            mdc.DrawLine(c.x1, c.y1, c.x2, c.y2);
        }

        // 绘制元素（ElementDraw.cpp 内部会绘制小横线）
        for (const auto& comp : m_elements) {
            DrawElement(mdc, comp.type, comp.color, comp.thickness, comp.x, comp.y);
        }

        mdc.SelectObject(wxNullBitmap);
        m_backValid = true;
    }

    void DrawGrid(wxDC& dc)
    {
        dc.SetPen(*wxLIGHT_GREY_PEN);
        int width = 0, height = 0;
        GetClientSize(&width, &height);
        for (int x = 0; x < width; x += 10)
            for (int y = 0; y < height; y += 10)
                dc.DrawPoint(x, y);
    }



};




bool MyApp::OnInit()
{
    try {
        json j;
        j["elements"] = json::array();
        std::ofstream ofs("Elementlib.json");
        if (ofs.is_open()) {
            ofs << j.dump(4);
            ofs.close();
        }
    }
    catch (...) {
    }

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
	toolBar->AddTool(ID_TOOL_EDITTXET, "Edit Text", wxArtProvider::GetBitmap(wxART_COPY, wxART_TOOLBAR));
    toolBar->AddTool(ID_TOOL_ADDINPUTPIN, "Input Pin", wxArtProvider::GetBitmap(wxART_GO_BACK, wxART_TOOLBAR));
    toolBar->AddTool(ID_TOOL_ADDOUTPUTPIN, "Output Pin", wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_TOOLBAR));
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
    Bind(wxEVT_TOOL, [this](wxCommandEvent& event) {
        this->SetPlacementType("Input Pin");
        this->SetStatusText("Selected for placement: Input Pin");
        }, ID_TOOL_ADDINPUTPIN);

    Bind(wxEVT_TOOL, [this](wxCommandEvent& event) {
        this->SetPlacementType("Output Pin");
        this->SetStatusText("Selected for placement: Output Pin");
        }, ID_TOOL_ADDOUTPUTPIN);
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
    // 自动双缓冲，避免闪烁
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    // 如果后备位图不存在或尺寸变化则重建
    wxSize sz = GetClientSize();
    if (!m_backValid || m_backBitmap.GetWidth() != sz.x || m_backBitmap.GetHeight() != sz.y) {
        RebuildBackbuffer();
    }

    // 将后备位图一次性绘制到屏幕
    if (m_backValid) {
        dc.DrawBitmap(m_backBitmap, 0, 0, false);
    }
    else {
        
        DrawGrid(dc);
    }

    // 如果正在拖拽，在后备图上叠加绘制临时位置（拖拽过程中元素只在临时位置显示）
    if (m_dragging && m_dragIndex >= 0 && m_dragIndex < (int)m_elements.size()) {
        const ElementInfo& e = m_elements[m_dragIndex];
        DrawElement(dc, e.type, e.color, e.thickness, m_dragCurrent.x, m_dragCurrent.y);
    }

    // 如果正在连线，绘制橡皮筋（从起始元素中心到当前鼠标）
    if (m_connecting) {
        // 判断当前鼠标是否在有效目标输入上
        ConnectorHit endHit = HitTestConnector(m_tempLineEnd);
        bool valid = (m_connectStartIsOutput && endHit.hit && !endHit.isOutput && endHit.elemIndex != m_connectStartElem);
        if (valid) {
            wxPen p(wxColour(0, 128, 0), 2); // 深绿色
            dc.SetPen(p);
        }
        else {
            dc.SetPen(*wxBLACK_PEN);
        }
        // 线从起点 connector 到当前鼠标（或到捕获到的吸附点）
        wxPoint startPt = GetConnectorPosition(m_connectStartElem, m_connectStartPin, m_connectStartIsOutput);
        dc.DrawLine(startPt.x, startPt.y, m_tempLineEnd.x, m_tempLineEnd.y);
    }
}



void CanvasPanel::OnLeftDown(wxMouseEvent& event) {
    wxPoint pt = event.GetPosition();

    int idx = HitTestElement(pt);
    if (idx >= 0) {
        // 开始拖动（不立即更改元素数组位置）
        m_dragging = true;
        m_dragIndex = idx;
        m_dragStart = wxPoint(m_elements[idx].x, m_elements[idx].y);
        m_dragOffset = wxPoint(pt.x - m_elements[idx].x, pt.y - m_elements[idx].y);
        m_dragCurrent = m_dragStart;
        CaptureMouse();
    } else {

        // 从顶层 MyFrame 读取当前树中选择的放置类型
        wxWindow* top = wxGetTopLevelParent(this);
        MyFrame* mf = dynamic_cast<MyFrame*>(top);
        std::string placeType;
        if (mf) placeType = mf->GetPlacementType();

        // 如果树上有选择类型，则在单击处放置该类型元件
        if (!placeType.empty()) {

            ElementInfo newElem;
            newElem.type = placeType;
            newElem.color = "black";
            newElem.thickness = 1;
            newElem.x = pt.x;
            newElem.y = pt.y;
            newElem.size = 1;
            newElem.rotationIndex = 0;
            newElem.inputs = 2;

            // 更新内存缓存并写入文件（保存操作）
            m_elements.push_back(newElem);
            SaveElementsAndConnectionsToFile();

            m_backValid = false;
            RebuildBackbuffer();

            // 清除当前放置类型（让用户重新选择）
            if (mf) mf->SetPlacementType(std::string());

            // 刷新画布
            Refresh();
        }
    }

    event.Skip();
}



wxIMPLEMENT_APP(MyApp);
