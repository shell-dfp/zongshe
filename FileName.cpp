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
    ID_TOOL_ADDORGATE,
    ID_TOOL_ADDINPUTPIN,
    ID_TOOL_ADDOUTPUTPIN
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

class CanvasPanel : public wxPanel
{
public:
    CanvasPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY),
        m_dragging(false),
        m_dragIndex(-1),
        m_connecting(false),
        m_connectStartIndex(-1),
        m_connectStartPin(-1),
        m_hasCache(false)
    {
        // 启用基于 Paint 的背景绘制以支持双缓冲
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(*wxWHITE);
        Bind(wxEVT_PAINT, &CanvasPanel::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &CanvasPanel::OnLeftDown, this);
        Bind(wxEVT_MOTION, &CanvasPanel::OnMouseMove, this);
        Bind(wxEVT_LEFT_UP, &CanvasPanel::OnLeftUp, this);
    }

    void OnPaint(wxPaintEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);

    static void SaveElementsJson(const json& j);

private:
    // drag state
    bool m_dragging;
    int m_dragIndex;           // index in JSON array being dragged
    wxPoint m_dragOffset;      // 鼠标相对元素位置偏移（保证拖拽平滑）

    // connect state
    bool m_connecting;
    int m_connectStartIndex;   // 起点元素索引
    int m_connectStartPin;     // 起点 pin 索引
    wxPoint m_tempLineEnd;     // 临时连线终点（随鼠标移动）

    // 内存缓存，用于在交互期间保存正在修改的 JSON，避免频繁磁盘写
    json m_jCache;
    bool m_hasCache;

    static bool ElementHitTestBox(const json& comp, const wxPoint& pt);
    static json LoadElementsJson();

    // 在元素端点集合中查找是否点中了某个 pin；优先使用缓存（若存在）
    // 返回 pair(elIndex, pinIndex) 或 (-1,-1) 表示未命中
    std::pair<int, int> FindElementPinAt(const wxPoint& pt) const;

    // 绘制连线：优先使用保存在 wire 中的 pin 信息（fromPin/toPin），否则回退到锚点计算
    static void DrawWires(wxDC& dc, const json& j);

    // 保留旧的 Anchor 作为后备
    static wxPoint AnchorPointForElement(const json& el, const wxPoint& toward);
};

bool CanvasPanel::ElementHitTestBox(const json& comp, const wxPoint& pt) {
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
    catch (...) {}
    if (!j.contains("elements") || !j["elements"].is_array())
        j["elements"] = json::array();
    if (!j.contains("wires") || !j["wires"].is_array())
        j["wires"] = json::array();
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

std::pair <int, int> CanvasPanel::FindElementPinAt(const wxPoint& pt) const {
    const json j = m_hasCache ? m_jCache : LoadElementsJson();
    const int hitRadius = 8;
    const int rr = hitRadius * hitRadius;
    for (size_t i = 0; i < j["elements"].size(); ++i) {
        auto pins = GetElementPins(j["elements"][i]);
        for (size_t p = 0; p < pins.size(); ++p) {
            int dx = pt.x - pins[p].x;
            int dy = pt.y - pins[p].y;
            if (dx * dx + dy * dy <= rr) return { static_cast<int>(i), static_cast<int>(p) };
        }
    }
    return { -1,-1 };
}

void CanvasPanel::DrawWires(wxDC& dc, const json& j) {
    if (!j.contains("wires") || !j["wires"].is_array()) return;
    wxPen pen(wxColour(30, 144, 255), 3, wxPENSTYLE_SOLID); // 更醒目的颜色与宽度
    dc.SetPen(pen);
    for (const auto& w : j["wires"]) {
        int fromEl = -1, toEl = -1, fromPin = -1, toPin = -1;
        if (w.is_object()) {
            if (w.contains("fromEl")) 
                fromEl = w.value("fromEl", -1);
            if (w.contains("toEl")) 
                toEl = w.value("toEl", -1);
            if (w.contains("from")) { // old: element index only
                if (fromEl == -1) fromEl = w.value("from", -1);
            }
            if (w.contains("to")) {
                if (toEl == -1) toEl = w.value("to", -1);
            }
            if (w.contains("fromPin")) fromPin = w.value("fromPin", -1);
            if (w.contains("toPin")) toPin = w.value("toPin", -1);
        }
        if (fromEl < 0 || toEl < 0) continue;
        wxPoint p1, p2;
        // 计算 p1
        if (fromPin >= 0 && fromEl < (int)j["elements"].size()) {
            auto pins = GetElementPins(j["elements"][fromEl]);
            if (fromPin < (int)pins.size()) p1 = pins[fromPin];
            else p1 = pins.empty() ? wxPoint(j["elements"][fromEl].value("x", 0), j["elements"][fromEl].value("y", 0)) : pins[0];
        }
        else {
            // 退回到基于锚点的计算（以前逻辑）
            p1 = AnchorPointForElement(j["elements"][fromEl], wxPoint(j["elements"][toEl].value("x", 0), j["elements"][toEl].value("y", 0)));
        }
        // 计算 p2
        if (toPin >= 0 && toEl < (int)j["elements"].size()) {
            auto pins = GetElementPins(j["elements"][toEl]);
            if (toPin < (int)pins.size()) p2 = pins[toPin];
            else p2 = pins.empty() ? wxPoint(j["elements"][toEl].value("x", 0), j["elements"][toEl].value("y", 0)) : pins[0];
        }
        else {
            p2 = AnchorPointForElement(j["elements"][toEl], wxPoint(j["elements"][fromEl].value("x", 0), j["elements"][fromEl].value("y", 0)));
        }
        dc.DrawLine(p1.x, p1.y, p2.x, p2.y);
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.DrawCircle(p1.x, p1.y, 3);
        dc.DrawCircle(p2.x, p2.y, 3);
    }
}

wxPoint CanvasPanel::AnchorPointForElement(const json& el, const wxPoint& toward) {
    int x = el.value("x", 0);
    int y = el.value("y", 0);
    int size = el.value("size", 1);
    int half = 20 * std::max(1, size);
    double dx = toward.x - x;
    double dy = toward.y - y;
    if (dx == 0 && dy == 0) return wxPoint(x, y);
    double tpx = (dx != 0) ? ((dx > 0) ? (half / dx) : (-half / dx)) : 1e9;
    double tpy = (dy != 0) ? ((dy > 0) ? (half / dy) : (-half / dy)) : 1e9;
    double tmin = std::min(std::abs(tpx), std::abs(tpy));
    double ax = x + dx * tmin;
    double ay = y + dy * tmin;
    if (ax < x - half) ax = x - half;
    if (ax > x + half) ax = x + half;
    if (ay < y - half) ay = y - half;
    if (ay > y + half) ay = y + half;
    return wxPoint(static_cast<int>(std::round(ax)), static_cast<int>(std::round(ay)));
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

    json j = m_hasCache ? m_jCache : CanvasPanel::LoadElementsJson();
    if (!j.contains("elements") || !j["elements"].is_array()) return;

    // 绘制元素
    for (const auto& comp : j["elements"]) {
        std::string type = comp.value("type", std::string());
        std::string color = comp.value("color", std::string("black"));
        int thickness = comp.value("thickness", 1);
        int x = comp.value("x", 0);
        int y = comp.value("y", 0);
        DrawElement(dc, type, color, thickness, x, y);

        /*auto pins = GetElementPins(comp);
        wxBrush b(*wxBLUE_BRUSH);
        dc.SetBrush(b);
        for (const auto &p : pins) {
            dc.DrawCircle(p.x, p.y, 3);
        }*/
    }

    // 绘制连线
    CanvasPanel::DrawWires(dc, j);

    // 若正在交互连线，绘制临时虚线（从起点 pin 到鼠标位置）
    if (m_connecting && m_connectStartIndex >= 0) {
        json jj = m_hasCache ? m_jCache : CanvasPanel::LoadElementsJson();
        if (jj.contains("elements") && m_connectStartIndex < (int)jj["elements"].size()) {
            auto pins = GetElementPins(jj["elements"][m_connectStartIndex]);
            wxPoint start = (m_connectStartPin >= 0 && m_connectStartPin < (int)pins.size()) ? pins[m_connectStartPin]
                : pins.empty() ? wxPoint(jj["elements"][m_connectStartIndex].value("x", 0), jj["elements"][m_connectStartIndex].value("y", 0))
                : pins[0];
            wxPen tmpPen(wxColour(0, 0, 255), 2, wxPENSTYLE_SHORT_DASH);
            dc.SetPen(tmpPen);
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawLine(start.x, start.y, m_tempLineEnd.x, m_tempLineEnd.y);
        }
    }
}

void CanvasPanel::OnLeftDown(wxMouseEvent& event) {
    wxPoint pt = event.GetPosition();
    wxWindow* top = wxGetTopLevelParent(this);
    MyFrame* mf = dynamic_cast<MyFrame*>(top);
    std::string placeType;
    if (mf) placeType = mf->GetPlacementType();

    if (!placeType.empty()) {
        json j = LoadElementsJson();
        if (!j.contains("elements") || !j["elements"].is_array()) j["elements"] = json::array();

        json newElem;
        newElem["type"] = placeType;
        newElem["color"] = "black";
        newElem["thickness"] = 1;
        newElem["x"] = pt.x;
        newElem["y"] = pt.y;
        newElem["size"] = 1;
        newElem["rotationIndex"] = 0;
        newElem["inputs"] = 2;

        j["elements"].push_back(newElem);
        SaveElementsJson(j);

        if (mf) mf->SetPlacementType(std::string());
        Refresh();
        return;
    }

    // Shift+点击：开始连接（若点击到某元件的 pin）
    if (event.ShiftDown()) {
        auto hit = FindElementPinAt(pt);
        if (hit.first >= 0) {
            m_connecting = true;
            m_connectStartIndex = hit.first;
            m_connectStartPin = hit.second;
            m_tempLineEnd = pt;
            SetCursor(wxCursor(wxCURSOR_CROSS));
            m_jCache = LoadElementsJson();
            m_hasCache = true;
            if (mf) {
                std::string t = m_jCache["elements"][hit.first].value("type", std::string());
                mf->SetStatusText(wxString::FromUTF8(("Connecting from: " + t).c_str()));
            }
            Refresh();
            return;
        }
    }
    // 普通点击：尝试开始拖拽
    json j = LoadElementsJson();
    int idx = -1;
    for (size_t i = 0; i < j["elements"].size(); ++i) {
        if (ElementHitTestBox(j["elements"][i], pt)) { idx = (int)i; break; }
    }
    if (idx >= 0) {
        int ex = j["elements"][idx].value("x", 0);
        int ey = j["elements"][idx].value("y", 0);
        m_dragging = true;
        m_dragIndex = idx;
        m_dragOffset = wxPoint(pt.x - ex, pt.y - ey);
        SetCursor(wxCursor(wxCURSOR_HAND));
        m_jCache = j;
        m_hasCache = true;
    }
}

void CanvasPanel::OnMouseMove(wxMouseEvent& event) {
    wxPoint pos = event.GetPosition();

    // 连接交互：更新临时终点并重绘
    if (m_connecting && m_connectStartIndex >= 0) {
        m_tempLineEnd = pos;
        Refresh();
        return;
    }

    // 拖拽交互：更新缓存中的元素位置并重绘（不写盘）
    if (m_dragging && m_dragIndex >= 0 && m_hasCache) {
        if (event.LeftIsDown()) {
            int newx = pos.x - m_dragOffset.x;
            int newy = pos.y - m_dragOffset.y;
            if (m_dragIndex >= 0 && m_dragIndex < (int)m_jCache["elements"].size()) {
                m_jCache["elements"][m_dragIndex]["x"] = newx;
                m_jCache["elements"][m_dragIndex]["y"] = newy;
                Refresh();
            }
        }
        return;
    }
}

void CanvasPanel::OnLeftUp(wxMouseEvent& event) {
    wxPoint pt = event.GetPosition();

    // 如果处于连接模式：尝试以 pin 为端点建立连接并在缓存/文件中保存
    if (m_connecting && m_connectStartIndex >= 0) {
        auto dstHit = FindElementPinAt(pt);
        int dst = dstHit.first;
        int dstPin = dstHit.second;
        if (dst >= 0 && dst != m_connectStartIndex) {
            if (!m_hasCache) m_jCache = LoadElementsJson();
            if (!m_jCache.contains("wires") || !m_jCache["wires"].is_array()) m_jCache["wires"] = json::array();
            // 防止重复连线（无向检查）
            bool exists = false;
            for (const auto& w : m_jCache["wires"]) {
                int a = w.value("fromEl", w.value("from", -1));
                int b = w.value("toEl", w.value("to", -1));
                int ap = w.value("fromPin", -1);
                int bp = w.value("toPin", -1);
                if ((a == m_connectStartIndex && b == dst && ap == m_connectStartPin && bp == dstPin) ||
                    (a == dst && b == m_connectStartIndex && ap == dstPin && bp == m_connectStartPin)) {
                    exists = true; break;
                }
            }
            if (!exists) {
                json w;
                w["fromEl"] = m_connectStartIndex;
                w["fromPin"] = m_connectStartPin;
                w["toEl"] = dst;
                w["toPin"] = dstPin;
                w["color"] = "black";
                m_jCache["wires"].push_back(w);
            }
            SaveElementsJson(m_jCache);
        }
        // 结束连接模式
        m_connecting = false;
        m_connectStartIndex = -1;
        m_connectStartPin = -1;
        m_hasCache = false;
        m_jCache = json();
        SetCursor(wxCursor(wxCURSOR_ARROW));
        wxWindow* top = wxGetTopLevelParent(this);
        MyFrame* mf = dynamic_cast<MyFrame*>(top);
        if (mf) mf->SetStatusText(wxEmptyString);
        Refresh();
        return;
    }

    // 拖拽结束：将缓存写回文件并清理
    if (m_dragging && m_hasCache) {
        SaveElementsJson(m_jCache);
    }
    m_dragging = false;
    m_dragIndex = -1;
    m_hasCache = false;
    m_jCache = json();
    SetCursor(wxCursor(wxCURSOR_ARROW));
    Refresh();
}

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
    toolBar->AddTool(ID_TOOL_ADDINPUTPIN, "Input Pin", wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_TOOLBAR));
    toolBar->AddTool(ID_TOOL_ADDOUTPUTPIN, "Output Pin", wxArtProvider::GetBitmap(wxART_GO_BACK, wxART_TOOLBAR));
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

wxIMPLEMENT_APP(MyApp);
