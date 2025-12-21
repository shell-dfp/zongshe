#include <wx/wx.h>
#include <wx/image.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/artprov.h>
#include <wx/treectrl.h>
#include <wx/splitter.h>
#include <wx/aui/aui.h>
#include <wx/dcbuffer.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>
#include "ElementDraw.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <map>
#include <iomanip>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <cstdint>
using json = nlohmann::json;

// ---- 全局 ID ----
enum
{
    ID_CUT = wxID_HIGHEST + 1,
    ID_COPY,
    ID_EDIT_PASTE,
    ID_EDIT_DELETE,
    ID_EDIT_SELECTALL,
    ID_PROJECT_ADD_CIRCUIT,
    ID_SIM_ENABLE,
    ID_SIM_RESET,
    ID_WINDOW_CASCADE,
    ID_HELP_ABOUT,
    ID_FILE_NEW,
    ID_FILE_OPEN,
    ID_FILE_OPENRECENT,
    ID_FILE_CLOSE,
    ID_FILE_SAVE,
};

enum ToolID {
    ID_TOOL_CHGVALUE = wxID_HIGHEST + 100,
    ID_TOOL_EDITSELECT,
    ID_TOOL_EDITTXET,
    ID_TOOL_ADDPIN4,
    ID_TOOL_ADDPIN5,
    ID_TOOL_ADDNOTGATE,
    ID_TOOL_ADDANDGATE,
    ID_TOOL_ADDORGATE,
    ID_TOOL_SHOWPROJECTC,
    ID_TOOL_SHOWSIMULATION,
    ID_TOOL_EDITVIEW
};

// ---- 数据结构 ----
struct ElementInfo {
    std::string type;
    std::string color;
    int thickness = 1;
    int x = 0;
    int y = 0;
    int size = 1;
    int rotationIndex = 0;
    int inputs = 0;
    int outputs = 0;
};

struct ConnectionInfo {
    int aIndex = -1;
    int bIndex = -1;
    int aPin = -1;
    int bPin = -1;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    // 父 connection（若起点来自另一条 connection 的 aux）
    int aConn = -1;
    int aConnAux = -1;

    std::vector<wxPoint> turningPoints;

    struct AuxOutput {
        int segIndex = 0;
        double t = 0.0;
    };
    std::vector<AuxOutput> auxOutputs;
};

// 前向声明（PropertyPanel 需要引用 CanvasPanel）
class CanvasPanel;

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
    void OnCloseWindow(wxCloseEvent& event);
    void OnCut(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnAddCircuit(wxCommandEvent& event);
    void OnSimEnable(wxCommandEvent& event);
    void OnWindowCascade(wxCommandEvent& event);
    void OnHelp(wxCommandEvent& event);
    void OnToolChangeValue(wxCommandEvent& event);
    void OnToolEditSelect(wxCommandEvent& event);
    void OnToolEditText(wxCommandEvent& event);

    // 新增：工具栏仿真开关
    void OnToolShowSimulation(wxCommandEvent& event);

    // 导入/导出网表
    void OnExportNetlist(wxCommandEvent& event);
    void OnImportNetlist(wxCommandEvent& event);

    std::string m_currentPlacementType;
    // 保存对画布的引用以便触发导入/导出 / 检查未保存状态
    CanvasPanel* m_canvas = nullptr;
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
        tree->AppendItem(child3, "Input");
        tree->AppendItem(child3, "Output");
        wxTreeItemId child4 = tree->AppendItem(root, "Gate");
        tree->AppendItem(child4, "AND");
        tree->AppendItem(child4, "OR");
        tree->AppendItem(child4, "NOT");
        tree->AppendItem(child4, "NAND");
        tree->AppendItem(child4, "NOR");
        tree->AppendItem(child4, "XNOR");
        tree->AppendItem(child4, "XOR");
        tree->AppendItem(child4, "Buffer");                  // 新增
        tree->AppendItem(child4, "Odd Parity");              // 新增
        tree->AppendItem(child4, "Controlled Buffer");       // 新增
        tree->AppendItem(child4, "Controlled Inverter");
        wxTreeItemId child5 = tree->AppendItem(root, "Plexers");
        tree->AppendItem(child5, "Multiplexer");
        wxTreeItemId child6 = tree->AppendItem(root, "Arithmetic");
        tree->AppendItem(child6, "Adder");
        wxTreeItemId child7 = tree->AppendItem(root, "Memory");
        tree->AppendItem(child7, "RAM");
        wxTreeItemId child8 = tree->AppendItem(root, "Input/Output");
        tree->AppendItem(child8, "Button");
        wxTreeItemId child9 = tree->AppendItem(root, "Base");
        tree->AppendItem(child9, "Label");
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


// ---- 几何 / 路由 帮助函数 ----
static int orient(const wxPoint& a, const wxPoint& b, const wxPoint& c) {
    long long v = (long long)(b.y - a.y) * (c.x - b.x) - (long long)(b.x - a.x) * (c.y - b.y);
    if (v == 0) return 0;
    return v > 0 ? 1 : 2;
}
static bool onSegment(const wxPoint& a, const wxPoint& b, const wxPoint& p) {
    return p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x)
        && p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y);
}
static bool LineSegmentsIntersect(const wxPoint& p1, const wxPoint& p2, const wxPoint& q1, const wxPoint& q2) {
    int o1 = orient(p1, p2, q1);
    int o2 = orient(p1, p2, q2);
    int o3 = orient(q1, q2, p1);
    int o4 = orient(q1, q2, p2);

    if (o1 != o2 && o3 != o4) return true;
    if (o1 == 0 && onSegment(p1, p2, q1)) return true;
    if (o2 == 0 && onSegment(p1, p2, q2)) return true;
    if (o3 == 0 && onSegment(q1, q2, p1)) return true;
    if (o4 == 0 && onSegment(q1, q2, p2)) return true;
    return false;
}

// pair hash helper
struct PairHash {
    size_t operator()(const std::pair<int, int>& p) const noexcept {
        return ((uint64_t)(uint32_t)p.first << 32) ^ (uint32_t)p.second;
    }
};

// A* 网格与路径简化
static std::vector<wxPoint> SimplifyGridPathToTurningPoints(const std::vector<std::pair<int, int>>& nodes, int gridSize)
{
    std::vector<wxPoint> pts;
    if (nodes.size() <= 2) return pts;
    std::pair<int, int> prev = nodes[0];
    std::pair<int, int> cur = nodes[1];
    int dx0 = cur.first - prev.first;
    int dy0 = cur.second - prev.second;
    for (size_t i = 2; i < nodes.size(); ++i) {
        std::pair<int, int> next = nodes[i];
        int dx1 = next.first - cur.first;
        int dy1 = next.second - cur.second;
        if (dx1 != dx0 || dy1 != dy0) {
            pts.push_back(wxPoint(cur.first * gridSize, cur.second * gridSize));
        }
        prev = cur; cur = next;
        dx0 = dx1; dy0 = dy1;
    }
    return pts;
}

static std::vector<std::pair<int, int>> AStarGridPath(
    const std::pair<int, int>& startNode,
    const std::pair<int, int>& goalNode,
    int gxMin, int gxMax, int gyMin, int gyMax,
    const std::unordered_set<std::pair<int, int>, PairHash>& blocked)
{
    struct Node {
        int f, g;
        int x, y;
        bool operator<(Node const& o) const { return f > o.f; }
    };

    auto heuristic = [](const std::pair<int, int>& a, const std::pair<int, int>& b)->int {
        return std::abs(a.first - b.first) + std::abs(a.second - b.second);
        };

    std::priority_queue<Node> open;
    std::unordered_map<std::pair<int, int>, int, PairHash> gScore;
    std::unordered_map<std::pair<int, int>, std::pair<int, int>, PairHash> cameFrom;
    gScore[startNode] = 0;
    open.push({ heuristic(startNode, goalNode), 0, startNode.first, startNode.second });

    const int dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };

    while (!open.empty()) {
        Node cur = open.top(); open.pop();
        std::pair<int, int> curP{ cur.x, cur.y };
        if (curP == goalNode) {
            std::vector<std::pair<int, int>> path;
            std::pair<int, int> p = goalNode;
            while (!(p == startNode)) {
                path.push_back(p);
                p = cameFrom[p];
            }
            path.push_back(startNode);
            std::reverse(path.begin(), path.end());
            return path;
        }
        if (gScore.find(curP) != gScore.end() && cur.g != gScore[curP]) continue;

        for (auto& d : dirs) {
            int nx = cur.x + d[0];
            int ny = cur.y + d[1];
            if (nx < gxMin || nx > gxMax || ny < gyMin || ny > gyMax) continue;
            std::pair<int, int> np{ nx,ny };
            if (blocked.find(np) != blocked.end()) continue;
            int ng = cur.g + 1;
            auto it = gScore.find(np);
            if (it == gScore.end() || ng < it->second) {
                gScore[np] = ng;
                int f = ng + heuristic(np, goalNode);
                cameFrom[np] = curP;
                open.push({ f, ng, nx, ny });
            }
        }
    }
    return {};
}

static void BuildBlockedGrid(const std::vector<ElementInfo>& elements, int gridSize,
    int gxMin, int gxMax, int gyMin, int gyMax,
    int exceptA, int exceptB,
    std::unordered_set<std::pair<int, int>, PairHash>& outBlocked,
    int extraPadding = 8)
{
    for (size_t i = 0; i < elements.size(); ++i) {
        if ((int)i == exceptA || (int)i == exceptB) continue;
        const ElementInfo& e = elements[i];
        int sz = std::max(1, e.size);
        int left = e.x - extraPadding;
        int top = e.y - extraPadding;
        int right = e.x + BaseElemWidth * sz + extraPadding;
        int bottom = e.y + BaseElemHeight * sz + extraPadding;
        int gx0 = left / gridSize;
        int gy0 = top / gridSize;
        int gx1 = right / gridSize;
        int gy1 = bottom / gridSize;
        for (int gx = gx0; gx <= gx1; ++gx) {
            for (int gy = gy0; gy <= gy1; ++gy) {
                if (gx < gxMin || gx > gxMax || gy < gyMin || gy > gyMax) continue;
                outBlocked.insert({ gx, gy });
            }
        }
    }
}

// 在 ComputeManhattanPath 之前（或在 orient/LineSegmentsIntersect 之后）插入：
static bool SegmentIntersectsElement(const wxPoint& s, const wxPoint& e, const ElementInfo& el) {
    int sz = std::max(1, el.size);
    wxRect r(el.x, el.y, BaseElemWidth * sz, BaseElemHeight * sz);
    // 如果端点在矩形内，则认为相交
    if (r.Contains(s) || r.Contains(e)) return true;
    // 矩形四边
    wxPoint r1(r.GetLeft(), r.GetTop()), r2(r.GetRight(), r.GetTop());
    wxPoint r3(r.GetRight(), r.GetBottom()), r4(r.GetLeft(), r.GetBottom());
    if (LineSegmentsIntersect(s, e, r1, r2)) return true;
    if (LineSegmentsIntersect(s, e, r2, r3)) return true;
    if (LineSegmentsIntersect(s, e, r3, r4)) return true;
    if (LineSegmentsIntersect(s, e, r4, r1)) return true;
    return false;
}


static std::vector<wxPoint> ComputeManhattanPath(const wxPoint& start, const wxPoint& end,
    const std::vector<ElementInfo>& elements, int exceptA = -1, int exceptB = -1)
{
    std::vector<wxPoint> empty;
    if (start.x == end.x || start.y == end.y) {
        return empty;
    }

    auto intersectsAny = [&](const wxPoint& a, const wxPoint& b)->bool {
        for (size_t i = 0; i < elements.size(); ++i) {
            if ((int)i == exceptA || (int)i == exceptB) continue;
            if (SegmentIntersectsElement(a, b, elements[i])) return true;
        }
        return false;
        };

    // 单折尝试
    wxPoint c1(start.x, end.y);
    wxPoint c2(end.x, start.y);
    if (!intersectsAny(start, c1) && !intersectsAny(c1, end)) return { c1 };
    if (!intersectsAny(start, c2) && !intersectsAny(c2, end)) return { c2 };

    // 双折中点尝试（对齐网格）
    auto snapGrid = [](int v)->int { return ((v + 5) / 10) * 10; };
    int midY = snapGrid((start.y + end.y) / 2);
    wxPoint m1(start.x, midY), m2(end.x, midY);
    if (!intersectsAny(start, m1) && !intersectsAny(m1, m2) && !intersectsAny(m2, end)) return { m1, m2 };
    int midX = snapGrid((start.x + end.x) / 2);
    wxPoint n1(midX, start.y), n2(midX, end.y);
    if (!intersectsAny(start, n1) && !intersectsAny(n1, n2) && !intersectsAny(n2, end)) return { n1, n2 };

    // 回退到 A* 网格
    const int gridSize = 10;
    int minX = std::min(start.x, end.x), maxX = std::max(start.x, end.x);
    int minY = std::min(start.y, end.y), maxY = std::max(start.y, end.y);
    for (const auto& el : elements) {
        minX = std::min(minX, el.x - BaseElemWidth);
        minY = std::min(minY, el.y - BaseElemHeight);
        maxX = std::max(maxX, el.x + BaseElemWidth * std::max(1, el.size) + BaseElemWidth);
        maxY = std::max(maxY, el.y + BaseElemHeight * std::max(1, el.size) + BaseElemHeight);
    }
    const int padding = 120;
    minX -= padding; minY -= padding; maxX += padding; maxY += padding;

    int gxMin = (minX) / gridSize; int gyMin = (minY) / gridSize;
    int gxMax = (maxX) / gridSize; int gyMax = (maxY) / gridSize;
    std::pair<int, int> sNode{ start.x / gridSize, start.y / gridSize };
    std::pair<int, int> eNode{ end.x / gridSize, end.y / gridSize };

    std::unordered_set<std::pair<int, int>, PairHash> blocked;
    BuildBlockedGrid(elements, gridSize, gxMin, gxMax, gyMin, gyMax, exceptA, exceptB, blocked, 8);
    blocked.erase(sNode);
    blocked.erase(eNode);

    auto gridPath = AStarGridPath(sNode, eNode, gxMin, gxMax, gyMin, gyMax, blocked);
    if (!gridPath.empty()) {
        return SimplifyGridPathToTurningPoints(gridPath, gridSize);
    }
    return { c1 };
}

// DrawConnection: 支持转折点，动态端点（若绑定到元素）
static void DrawConnection(wxDC& dc, const ConnectionInfo& c, const std::vector<ElementInfo>& elements, const wxColour& penColor = wxColour(0, 0, 0)) {
    wxPen old = dc.GetPen();
    dc.SetPen(wxPen(penColor, 2));
    wxPoint p1(c.x1, c.y1), p2(c.x2, c.y2);

    // 如果连接到某个元件的输出，使用该元件对应输出 pin 的位置（支持多输出）
    if (c.aIndex >= 0 && c.aIndex < (int)elements.size()) {
        const ElementInfo& ea = elements[c.aIndex];
        int sz = std::max(1, ea.size);
        int w = BaseElemWidth * sz;
        int h = BaseElemHeight * sz;
        int nOut = std::max(1, ea.outputs);
        int pin = (c.aPin < 0) ? 0 : c.aPin;
        float step = (float)h / (nOut + 1);
        int py = ea.y + (int)(step * (pin + 1));
        p1 = wxPoint(ea.x + w, py);
    }
    // 如果连接到某个元件的输入，使用该元件对应输入 pin 的位置（支持多输入）
    if (c.bIndex >= 0 && c.bIndex < (int)elements.size()) {
        const ElementInfo& eb = elements[c.bIndex];
        int sz = std::max(1, eb.size);
        int h = BaseElemHeight * sz;
        int nIn = std::max(1, eb.inputs);
        int pin = (c.bPin < 0) ? 0 : c.bPin;
        float step = (float)h / (nIn + 1);
        int py = eb.y + (int)(step * (pin + 1));
        p2 = wxPoint(eb.x, py);
    }

    std::vector<wxPoint> pts;
    pts.push_back(p1);
    for (const auto& tp : c.turningPoints) pts.push_back(tp);
    pts.push_back(p2);
    for (size_t i = 1; i < pts.size(); ++i) {
        dc.DrawLine(pts[i - 1].x, pts[i - 1].y, pts[i].x, pts[i].y);
    }
    dc.SetPen(old);
}

// ---- 前向声明 ----
class CanvasPanel;

// PropertyPanel
class PropertyPanel : public wxPanel
{
public:
    PropertyPanel(wxWindow* parent, CanvasPanel* canvas = nullptr)
        : wxPanel(parent, wxID_ANY), m_canvas(canvas)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        wxStaticText* label = new wxStaticText(this, wxID_ANY, "Properties");
        sizer->Add(label, 0, wxALIGN_CENTER | wxALL, 5);

        wxFlexGridSizer* grid = new wxFlexGridSizer(2, 7, 5);
        grid->AddGrowableCol(1, 1);

        grid->Add(new wxStaticText(this, wxID_ANY, "X:"), 0, wxALIGN_CENTER_VERTICAL);
        m_spinX = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 0, 2000, 0);
        grid->Add(m_spinX, 0, wxEXPAND);

        grid->Add(new wxStaticText(this, wxID_ANY, "Y:"), 0, wxALIGN_CENTER_VERTICAL);
        m_spinY = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 0, 2000, 0);
        grid->Add(m_spinY, 0, wxEXPAND);

        grid->Add(new wxStaticText(this, wxID_ANY, "Size:"), 0, wxALIGN_CENTER_VERTICAL);
        m_spinSize = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 1, 10, 1);
        grid->Add(m_spinSize, 0, wxEXPAND);

        grid->Add(new wxStaticText(this, wxID_ANY, "Inputs:"), 0, wxALIGN_CENTER_VERTICAL);
        m_spinInputs = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 0, 32, 1);
        grid->Add(m_spinInputs, 0, wxEXPAND);

        grid->Add(new wxStaticText(this, wxID_ANY, "Outputs:"), 0, wxALIGN_CENTER_VERTICAL);
        m_spinOutputs = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 0, 32, 1);
        grid->Add(m_spinOutputs, 0, wxEXPAND);

        sizer->Add(grid, 0, wxALL | wxEXPAND, 5);

        m_btnApply = new wxButton(this, wxID_ANY, "Apply");
        m_btnApply->SetMinSize(wxSize(-1, 28));
        sizer->Add(m_btnApply, 0, wxEXPAND | wxALL, 5);

        SetSizer(sizer);
        SetAutoLayout(true);
        Layout();
        m_btnApply->Show(true);

        m_btnApply->Bind(wxEVT_BUTTON, &PropertyPanel::OnApply, this);
    }

    void UpdateForElement(const ElementInfo& e)
    {
        if (e.type.empty()) {
            m_spinX->SetValue(0);
            m_spinY->SetValue(0);
            m_spinSize->SetValue(1);
            m_spinInputs->SetValue(0);
            m_spinOutputs->SetValue(0);
            if (m_btnApply) m_btnApply->Enable(false);
        }
        else {
            m_spinX->SetValue(e.x);
            m_spinY->SetValue(e.y);
            m_spinSize->SetValue(e.size < 1 ? 1 : e.size);
            m_spinInputs->SetValue(e.inputs < 0 ? 0 : e.inputs);
            m_spinOutputs->SetValue(e.outputs < 0 ? 0 : e.outputs);
            if (m_btnApply) m_btnApply->Enable(true);
        }
    }

    void SetCanvas(CanvasPanel* c) { m_canvas = c; }

private:
    CanvasPanel* m_canvas;
    wxSpinCtrl* m_spinX;
    wxSpinCtrl* m_spinY;
    wxSpinCtrl* m_spinSize;
    wxSpinCtrl* m_spinInputs;
    wxSpinCtrl* m_spinOutputs;
    wxButton* m_btnApply;

    void OnApply(wxCommandEvent& evt);
};



// ---- CanvasPanel ----
class CanvasPanel : public wxPanel
{
public:
    CanvasPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxWANTS_CHARS),
        m_backValid(false),
        m_dragging(false),
        m_dragIndex(-1),
        m_connecting(false),
        m_connectStartIsOutput(false),
        m_connectStartPin(-1),
        m_selectedIndex(-1),
        m_propPanel(nullptr),
        m_dirty(false),
        m_connectStartConnIndex(-1),
        m_connectStartConnOutputIndex(-1),
        m_simulating(false),
        m_selectedConnectionIndex(-1)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(*wxWHITE);

        LoadElementsAndConnectionsFromFile();

        Bind(wxEVT_LEFT_DOWN, &CanvasPanel::OnLeftDown, this);
        Bind(wxEVT_PAINT, &CanvasPanel::OnPaint, this);
        Bind(wxEVT_SIZE, &CanvasPanel::OnSize, this);
        Bind(wxEVT_MOTION, &CanvasPanel::OnMouseMove, this);
        Bind(wxEVT_LEFT_UP, &CanvasPanel::OnLeftUp, this);
        Bind(wxEVT_RIGHT_DOWN, &CanvasPanel::OnRightDown, this);
        Bind(wxEVT_RIGHT_UP, &CanvasPanel::OnRightUp, this);

        // 键盘事件：保留 KEY_DOWN，同时绑定 CHAR_HOOK 以更可靠接收 Delete/Backspace
        Bind(wxEVT_KEY_DOWN, &CanvasPanel::OnKeyDown, this);
        Bind(wxEVT_CHAR_HOOK, &CanvasPanel::OnKeyDown, this);

        // 使面板可获得焦点；点击已在 OnLeftDown 中调用 SetFocus()
        SetFocus();
        Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent&) { SetFocus(); });
    }

    bool IsDirty() const { return m_dirty; }
    void SetPropertyPanel(PropertyPanel* p) { m_propPanel = p; if (m_propPanel) m_propPanel->SetCanvas(this); }
    int GetSelectedIndex() const { return m_selectedIndex; }

    // ApplyPropertiesToSelected（包含 inputs/outputs）
    void ApplyPropertiesToSelected(int x, int y, int size, int inputs, int outputs)
    {
        if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_elements.size()) return;
        ElementInfo& e = m_elements[m_selectedIndex];
        e.x = x;
        e.y = y;
        e.size = std::max(1, size);

        // 针对特殊类型约束
        if (IsInputType(e.type)) {
            e.inputs = 0;
            e.outputs = outputs <= 0 ? 1 : outputs;
        }
        else if (IsOutputType(e.type)) {
            e.outputs = 0;
            e.inputs = inputs <= 0 ? 1 : inputs;
        }
        else if (e.type == "NOT" || e.type == "NOT Gate" || e.type == "NOTGate") {
            e.inputs = inputs <= 0 ? 1 : inputs;
            e.outputs = outputs <= 0 ? 1 : outputs;
        }
        else {
            e.inputs = inputs < 1 ? 1 : inputs;
            e.outputs = outputs < 1 ? 1 : outputs;
        }

        SaveElementsAndConnectionsToFile();
        m_backValid = false;
        RebuildBackbuffer();
        Refresh();
        if (m_propPanel) m_propPanel->UpdateForElement(e);
    }

    void OnPaint(wxPaintEvent& event) {
        wxAutoBufferedPaintDC dc(this); dc.Clear();
        wxSize sz = GetClientSize();
        if (!m_backValid || m_backBitmap.GetWidth() != sz.x || m_backBitmap.GetHeight() != sz.y) RebuildBackbuffer();
        if (m_backValid) dc.DrawBitmap(m_backBitmap, 0, 0, false);
        else DrawGrid(dc);
        if (m_dragging && m_dragIndex >= 0 && m_dragIndex < (int)m_elements.size()) {
            const ElementInfo& e = m_elements[m_dragIndex];
            DrawElement(dc, e.type, e.color, e.thickness, m_dragCurrent.x, m_dragCurrent.y, e.size);
        }
        if (m_connecting) {
            ConnectorHit endHit = HitTestConnector(m_tempLineEnd);
            bool valid = (m_connectStartIsOutput && endHit.hit && !endHit.isOutput && endHit.elemIndex != m_connectStartElem);
            wxColour lineColor;
            if (m_connectStartConnIndex >= 0 && m_connectStartConnIndex < (int)m_connections.size()) {
                const auto& srcConn = m_connections[m_connectStartConnIndex];
                bool srcIsOutputToInput = (srcConn.aIndex >= 0 && srcConn.bIndex >= 0);
                lineColor = srcIsOutputToInput ? wxColour(0, 128, 0) : wxColour(0, 0, 0);
            }
            else lineColor = valid ? wxColour(0, 128, 0) : wxColour(0, 0, 0);
            dc.SetPen(wxPen(lineColor, 2));
            wxPoint startPt;
            if (m_connectStartElem >= 0) startPt = GetConnectorPosition(m_connectStartElem, m_connectStartPin, true);
            else startPt = m_connectStartGrid;
            std::vector<wxPoint> tempTurns = ComputeManhattanPath(startPt, m_tempLineEnd, m_elements, m_connectStartElem, -1);
            wxPoint prev = startPt;
            for (const auto& pt : tempTurns) { dc.DrawLine(prev.x, prev.y, pt.x, pt.y); prev = pt; }
            dc.DrawLine(prev.x, prev.y, m_tempLineEnd.x, m_tempLineEnd.y);
        }
    }

    int HitTestConnection(const wxPoint& pt)
    {
        const int LINE_HIT_TOLERANCE = 4;
        for (size_t i = 0; i < m_connections.size(); ++i)
        {
            const auto& conn = m_connections[i];
            std::vector<wxPoint> linePoints;
            linePoints.emplace_back(conn.x1, conn.y1);
            for (const auto& p : conn.turningPoints) linePoints.push_back(p);
            linePoints.emplace_back(conn.x2, conn.y2);

            for (size_t j = 0; j + 1 < linePoints.size(); ++j)
            {
                wxPoint p1 = linePoints[j];
                wxPoint p2 = linePoints[j + 1];
                int dx = p2.x - p1.x;
                int dy = p2.y - p1.y;
                int t = (pt.x - p1.x) * dx + (pt.y - p1.y) * dy;
                if (t <= 0) {
                    int distSq = (pt.x - p1.x) * (pt.x - p1.x) + (pt.y - p1.y) * (pt.y - p1.y);
                    if (distSq <= LINE_HIT_TOLERANCE * LINE_HIT_TOLERANCE) return i;
                }
                else {
                    int lenSq = dx * dx + dy * dy;
                    if (t >= lenSq) {
                        int distSq = (pt.x - p2.x) * (pt.x - p2.x) + (pt.y - p2.y) * (pt.y - p2.y);
                        if (distSq <= LINE_HIT_TOLERANCE * LINE_HIT_TOLERANCE) return i;
                    }
                    else {
                        double projX = p1.x + (t * dx) / (double)lenSq;
                        double projY = p1.y + (t * dy) / (double)lenSq;
                        int distSq = (int)std::round((pt.x - projX) * (pt.x - projX) + (pt.y - projY) * (pt.y - projY));
                        if (distSq <= LINE_HIT_TOLERANCE * LINE_HIT_TOLERANCE) return i;
                    }
                }
            }
        }
        return -1;
    }

    void OnLeftDown(wxMouseEvent& event)
    {
        wxPoint pt = event.GetPosition();
        // 确保画布在点击后获取键盘焦点，以接收 Delete 键等按键事件
        SetFocus();

        // 先尝试检测是否点击在某条连线的段或已有 aux 点上，若命中则在该位置创建 auxOutput（小圆点）
        int hitConn = -1;
        int hitSeg = -1;
        double hitT = 0.0;
        wxPoint nearest;
        if (FindConnectionSegmentHit(pt, hitConn, hitSeg, hitT, nearest, /*maxDist=*/6)) {
            if (hitConn >= 0) {
                ConnectionInfo::AuxOutput ao;
                ao.segIndex = hitSeg;
                ao.t = hitT;
                // 防止与现有 aux 过近（像素距离 <= 2）
                bool dup = false;
                for (const auto& existing : m_connections[hitConn].auxOutputs) {
                    wxPoint expt = AuxOutputToPixel(existing, m_connections[hitConn], m_elements);
                    if (DistanceSquared(expt, nearest) <= 4) { dup = true; break; }
                }
                if (!dup) {
                    m_connections[hitConn].auxOutputs.push_back(ao);
                    m_backValid = false;
                    SaveElementsAndConnectionsToFile();
                    RebuildBackbuffer();
                    Refresh();
                }
            }
            event.Skip();
            return;
        }

        // 优先检测端口（开始/完成连线）
        ConnectorHit connectorHit = HitTestConnector(pt);
        if (m_connecting) {
            // 尝试完成连线
            wxPoint snapped = SnapToGrid(pt);
            ConnectorHit endHit = HitTestConnector(pt);
            ConnectionInfo c;
            if (m_connectStartIsOutput && m_connectStartElem >= 0) {
                wxPoint startPos = GetConnectorPosition(m_connectStartElem, m_connectStartPin, true);
                c.x1 = startPos.x; c.y1 = startPos.y; c.aIndex = m_connectStartElem; c.aPin = m_connectStartPin;
            }
            else if (m_connectStartConnIndex >= 0 && m_connectStartConnOutputIndex >= 0) {
                const auto& parent = m_connections[m_connectStartConnIndex];
                wxPoint startPos = AuxOutputToPixel(parent.auxOutputs[m_connectStartConnOutputIndex], parent, m_elements);
                c.x1 = startPos.x; c.y1 = startPos.y; c.aIndex = -1; c.aPin = -1; c.aConn = m_connectStartConnIndex; c.aConnAux = m_connectStartConnOutputIndex;
            }
            else {
                c.x1 = m_connectStartGrid.x; c.y1 = m_connectStartGrid.y; c.aIndex = -1; c.aPin = -1;
            }

            if (endHit.hit && !endHit.isOutput) {
                wxPoint endPos = GetConnectorPosition(endHit.elemIndex, endHit.pinIndex, false);
                c.x2 = endPos.x; c.y2 = endPos.y; c.bIndex = endHit.elemIndex; c.bPin = endHit.pinIndex;
            }
            else {
                c.x2 = snapped.x; c.y2 = snapped.y; c.bIndex = -1; c.bPin = -1;
            }

            // 计算曼哈顿转折
            c.turningPoints = ComputeManhattanPath(wxPoint(c.x1, c.y1), wxPoint(c.x2, c.y2), m_elements, c.aIndex, c.bIndex);

            if (IsConnectionValid(c)) {
                m_connections.push_back(c);
                SaveElementsAndConnectionsToFile();
                if (m_simulating) {
                    m_connectionSignals.resize(m_connections.size(), -1);
                    PropagateSignals();
                }
            }

            // 重置
            m_connecting = false; m_connectStartElem = -1; m_connectStartPin = -1; m_connectStartIsOutput = false;
            m_connectStartConnIndex = -1; m_connectStartConnOutputIndex = -1;
            m_prevTempLineEnd = wxPoint(-10000, -10000);
            if (HasCapture()) ReleaseMouse();
            m_backValid = false; RebuildBackbuffer(); Refresh();
            m_dirty = true;
            return;
        }
        else if (connectorHit.hit) {
            // 开始连线
            m_connecting = true;
            m_connectStartGrid = SnapToGrid(pt);
            m_tempLineEnd = m_connectStartGrid;
            m_prevTempLineEnd = m_tempLineEnd;
            if (connectorHit.isConnOutput && connectorHit.connIndex >= 0 && connectorHit.connOutputIndex >= 0) {
                m_connectStartElem = -1; m_connectStartPin = -1; m_connectStartIsOutput = true;
                m_connectStartConnIndex = connectorHit.connIndex; m_connectStartConnOutputIndex = connectorHit.connOutputIndex;
            }
            else if (connectorHit.isOutput) {
                m_connectStartElem = connectorHit.elemIndex; m_connectStartPin = connectorHit.pinIndex; m_connectStartIsOutput = true;
                m_connectStartConnIndex = -1; m_connectStartConnOutputIndex = -1;
            }
            else {
                m_connectStartElem = -1; m_connectStartPin = -1; m_connectStartIsOutput = false; m_connectStartConnIndex = -1; m_connectStartConnOutputIndex = -1;
            }
            CaptureMouse();
            m_selectedIndex = -1; m_selectedConnectionIndex = -1;
            RebuildBackbuffer(); Refresh();
            return;
        }

        // 检测是否点中连线（选中）
        int connIdx = HitTestConnection(pt);
        m_selectedIndex = -1; m_selectedConnectionIndex = -1;

        // 仿真状态点击 Input 切换值（优先于拖拽）
        int idx = HitTestElement(pt);
        if (idx >= 0 && m_simulating && IsInputType(m_elements[idx].type)) {
            int cur = -1; if (idx >= 0 && idx < (int)m_elementOutputs.size()) cur = m_elementOutputs[idx];
            if (cur == -1) cur = 0;
            int next = cur ? 0 : 1;
            SetInputValue(idx, next);
            return;
        }

        if (connIdx != -1) {
            m_selectedConnectionIndex = connIdx;
            m_selectedIndex = -1;
            // 确保画布获得焦点以接收 Delete
            SetFocus();
            RebuildBackbuffer(); Refresh();
            return;
        }

        if (idx >= 0) {
            m_selectedIndex = idx;
            // 确保画布获得焦点以接收 Delete
            SetFocus();
            if (m_propPanel) m_propPanel->UpdateForElement(m_elements[idx]);
            m_dragging = true; m_dragIndex = idx;
            m_dragStart = wxPoint(m_elements[idx].x, m_elements[idx].y);
            m_dragOffset = wxPoint(pt.x - m_elements[idx].x, pt.y - m_elements[idx].y);
            m_dragCurrent = m_dragStart; m_prevDragCurrent = m_dragCurrent;
            CaptureMouse();
            return;
        }

        // 未点中元素，尝试把当前选中的放置工具放置为元件
        wxWindow* top = wxGetTopLevelParent(this);
        MyFrame* mf = dynamic_cast<MyFrame*>(top);
        std::string placeType;
        if (mf) placeType = mf->GetPlacementType();
        if (!placeType.empty()) {
            ElementInfo newElem;
            newElem.type = placeType; newElem.color = "black"; newElem.thickness = 1;
            newElem.x = pt.x; newElem.y = pt.y; newElem.size = 1; newElem.rotationIndex = 0;

            if (placeType == "Input") {
                newElem.inputs = 0; newElem.outputs = 1;
            }
            else if (placeType == "Output") {
                newElem.inputs = 1; newElem.outputs = 0;
            }
            else if (placeType == "NOT" || placeType == "NOT Gate" || placeType == "NOTGate") {
                // NOT 门单输入单输出
                newElem.inputs = 1; newElem.outputs = 1;
            }
            else if (placeType == "Buffer") {
                newElem.inputs = 1; newElem.outputs = 1;
            }
            else if (placeType == "Odd Parity") {
                newElem.inputs = 2; newElem.outputs = 1;
            }
            else if (placeType == "Controlled Buffer" || placeType == "Controlled Inverter") {
                newElem.inputs = 2; newElem.outputs = 1;
            }
            else {
                // 默认把其他逻辑门（AND/OR/NAND/NOR/XOR/XNOR 等）设置为 2 输入、1 输出
                newElem.inputs = 2; newElem.outputs = 1;
            }

            m_elements.push_back(newElem);
            SaveElementsAndConnectionsToFile();
            m_backValid = false; m_dirty = true; RebuildBackbuffer();
            if (mf) mf->SetPlacementType(std::string());
            Refresh();
        }
        event.Skip();
    }

    void OnSize(wxSizeEvent& event) { m_backValid = false; Refresh(); event.Skip(); }

    bool AskSaveIfDirty()
    {
        if (!m_dirty) return true;
        wxMessageDialog dlg(this, "检测到未保存的更改，是否保存？", "保存更改", wxYES_NO | wxCANCEL | wxICON_QUESTION);
        int res = dlg.ShowModal();
        if (res == wxID_YES) {
            // 弹出保存对话框，让用户选择保存路径
            wxFileDialog fd(this, "保存文件", "", "Elementlib.json", "JSON files (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
            if (fd.ShowModal() != wxID_OK) {
                // 用户在保存对话框中取消，视为取消关闭
                return false;
            }
            std::string path = fd.GetPath().ToStdString();
            bool ok = SaveElementsAndConnectionsToFile(path);
            if (!ok) {
                wxMessageBox("保存失败，请检查文件路径或权限。", "保存失败", wxOK | wxICON_ERROR);
            }
            return ok;
        }
        else if (res == wxID_NO) {
            return true; // 不保存，直接关闭
        }
        else {
            return false; // 取消关闭
        }
    }

    void OnMouseMove(wxMouseEvent& event)
    {
        wxPoint pt = event.GetPosition();
        if (m_dragging && m_dragIndex >= 0) {
            wxPoint newPos(pt.x - m_dragOffset.x, pt.y - m_dragOffset.y);
            wxRect oldRect = ElementRect(m_prevDragCurrent, m_elements[m_dragIndex].size);
            wxRect newRect = ElementRect(newPos, m_elements[m_dragIndex].size);
            wxRect refreshRect = oldRect.Union(newRect);
            refreshRect.Inflate(10, 10);
            m_dragCurrent = newPos; m_prevDragCurrent = m_dragCurrent;
            RefreshRect(refreshRect);
        }
        else if (m_connecting) {
            wxPoint oldEnd = m_prevTempLineEnd;
            m_tempLineEnd = SnapToGrid(pt);
            wxPoint startPt;
            if (m_connectStartElem >= 0 && m_connectStartIsOutput) startPt = GetConnectorPosition(m_connectStartElem, m_connectStartPin, true);
            else startPt = m_connectStartGrid;
            wxRect oldRect(startPt, oldEnd);
            wxRect newRect(startPt, m_tempLineEnd);
            wxRect refreshRect = oldRect.Union(newRect);
            refreshRect.Inflate(6, 6);
            m_prevTempLineEnd = m_tempLineEnd;
            RefreshRect(refreshRect);
        }
        event.Skip();
    }

    void OnLeftUp(wxMouseEvent& event)
    {
        if (m_dragging && m_dragIndex >= 0) {
            m_elements[m_dragIndex].x = m_dragCurrent.x;
            m_elements[m_dragIndex].y = m_dragCurrent.y;
            // 重新路由与该元件相关的连接，并更新 aux
            for (size_t ci = 0; ci < m_connections.size(); ++ci) {
                auto& conn = m_connections[ci];
                if (conn.aIndex == m_dragIndex || conn.bIndex == m_dragIndex) {
                    wxPoint p1 = (conn.aIndex >= 0 && conn.aIndex < (int)m_elements.size()) ? GetConnectorPosition(conn.aIndex, conn.aPin, true) : wxPoint(conn.x1, conn.y1);
                    wxPoint p2 = (conn.bIndex >= 0 && conn.bIndex < (int)m_elements.size()) ? GetConnectorPosition(conn.bIndex, conn.bPin, false) : wxPoint(conn.x2, conn.y2);
                    conn.turningPoints = ComputeManhattanPath(p1, p2, m_elements, conn.aIndex, conn.bIndex);
                    conn.x1 = p1.x; conn.y1 = p1.y; conn.x2 = p2.x; conn.y2 = p2.y;
                    std::vector<wxPoint> poly; poly.emplace_back(conn.x1, conn.y1);
                    for (const auto& tp : conn.turningPoints) poly.push_back(tp);
                    poly.emplace_back(conn.x2, conn.y2);
                    for (auto& ao : conn.auxOutputs) {
                        wxPoint oldPt = AuxOutputToPixel(ao, conn, m_elements);
                        int newSeg; double newT; wxPoint newPt;
                        std::tie(newSeg, newT, newPt) = ProjectPointToPolylineDetailed(oldPt, poly);
                        if (newSeg < 0) { ao.segIndex = 0; ao.t = 0.0; }
                        else { ao.segIndex = newSeg; ao.t = newT; }
                    }
                    // 更新以该 connection 为父的子连接
                    for (auto& child : m_connections) {
                        if (child.aConn == (int)ci && child.aConnAux >= 0) {
                            const auto& parent = m_connections[child.aConn];
                            if (child.aConnAux < (int)parent.auxOutputs.size()) {
                                wxPoint newStart = AuxOutputToPixel(parent.auxOutputs[child.aConnAux], parent, m_elements);
                                child.x1 = newStart.x; child.y1 = newStart.y;
                                wxPoint endPt = (child.bIndex >= 0 && child.bIndex < (int)m_elements.size()) ? GetConnectorPosition(child.bIndex, child.bPin, false) : wxPoint(child.x2, child.y2);
                                child.turningPoints = ComputeManhattanPath(newStart, endPt, m_elements, child.aIndex, child.bIndex);
                            }
                        }
                    }
                }
            }
            m_dirty = true; m_backValid = false; RebuildBackbuffer(); SaveElementsAndConnectionsToFile();
            if (HasCapture()) ReleaseMouse();
            m_dragging = false; m_dragIndex = -1; m_prevDragCurrent = wxPoint(-10000, -10000);
            if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_elements.size() && m_propPanel) m_propPanel->UpdateForElement(m_elements[m_selectedIndex]);
            Refresh();
        }
        event.Skip();
    }

    void OnRightDown(wxMouseEvent& event)
    {
        wxPoint pt = event.GetPosition();
        ConnectorHit hit = HitTestConnector(pt);
        m_connecting = true;
        m_connectStartGrid = SnapToGrid(pt);
        m_tempLineEnd = m_connectStartGrid;
        m_prevTempLineEnd = m_tempLineEnd;
        m_connectStartConnIndex = -1; m_connectStartConnOutputIndex = -1;
        if (hit.hit && hit.isConnOutput && hit.connIndex >= 0 && hit.connOutputIndex >= 0) {
            const auto& conn = m_connections[hit.connIndex];
            if (hit.connOutputIndex < (int)conn.auxOutputs.size()) {
                wxPoint auxPixel = AuxOutputToPixel(conn.auxOutputs[hit.connOutputIndex], conn, m_elements);
                m_connectStartGrid = auxPixel; m_connectStartElem = -1; m_connectStartPin = -1; m_connectStartIsOutput = true;
                m_connectStartConnIndex = hit.connIndex; m_connectStartConnOutputIndex = hit.connOutputIndex;
            }
            else { m_connectStartElem = -1; m_connectStartPin = -1; m_connectStartIsOutput = false; }
        }
        else if (hit.hit && hit.isOutput) {
            m_connectStartElem = hit.elemIndex; m_connectStartPin = hit.pinIndex; m_connectStartIsOutput = true; m_connectStartConnIndex = -1; m_connectStartConnOutputIndex = -1;
        }
        else { m_connectStartElem = -1; m_connectStartPin = -1; m_connectStartIsOutput = false; m_connectStartConnIndex = -1; m_connectStartConnOutputIndex = -1; }
        CaptureMouse(); event.Skip();
    }

    void OnRightUp(wxMouseEvent& event)
    {
        if (m_connecting) {
            wxPoint pt = event.GetPosition();
            wxPoint snapped = SnapToGrid(pt);
            ConnectorHit endHit = HitTestConnector(pt);

            ConnectionInfo c;
            if (m_connectStartIsOutput && m_connectStartElem >= 0) {
                wxPoint startPos = GetConnectorPosition(m_connectStartElem, m_connectStartPin, true);
                c.x1 = startPos.x; c.y1 = startPos.y; c.aIndex = m_connectStartElem; c.aPin = m_connectStartPin; c.aConn = -1; c.aConnAux = -1;
            }
            else if (m_connectStartConnIndex >= 0 && m_connectStartConnIndex < (int)m_connections.size() && m_connectStartConnOutputIndex >= 0) {
                const auto& parent = m_connections[m_connectStartConnIndex];
                if (m_connectStartConnOutputIndex < (int)parent.auxOutputs.size()) {
                    wxPoint startPos = AuxOutputToPixel(parent.auxOutputs[m_connectStartConnOutputIndex], parent, m_elements);
                    c.x1 = startPos.x; c.y1 = startPos.y; c.aIndex = -1; c.aPin = -1; c.aConn = m_connectStartConnIndex; c.aConnAux = m_connectStartConnOutputIndex;
                }
                else { c.x1 = m_connectStartGrid.x; c.y1 = m_connectStartGrid.y; c.aIndex = -1; c.aPin = -1; c.aConn = -1; c.aConnAux = -1; }
            }
            else { c.x1 = m_connectStartGrid.x; c.y1 = m_connectStartGrid.y; c.aIndex = -1; c.aPin = -1; c.aConn = -1; c.aConnAux = -1; }

            if (endHit.hit && !endHit.isOutput) {
                wxPoint endPos = GetConnectorPosition(endHit.elemIndex, endHit.pinIndex, false);
                c.x2 = endPos.x; c.y2 = endPos.y; c.bIndex = endHit.elemIndex; c.bPin = endHit.pinIndex;
            }
            else { c.x2 = snapped.x; c.y2 = snapped.y; c.bIndex = -1; c.bPin = -1; }

            c.turningPoints = ComputeManhattanPath(wxPoint(c.x1, c.y1), wxPoint(c.x2, c.y2), m_elements, c.aIndex, c.bIndex);
            if (IsConnectionValid(c)) { m_connections.push_back(c); SaveElementsAndConnectionsToFile(); }

            m_backValid = false; RebuildBackbuffer();
            m_prevTempLineEnd = wxPoint(-10000, -10000);
            m_connecting = false; m_connectStartElem = -1; m_connectStartPin = -1; m_connectStartIsOutput = false; m_connectStartConnIndex = -1; m_connectStartConnOutputIndex = -1;
            Refresh(); if (HasCapture()) ReleaseMouse();
            m_dirty = true; m_backValid = false; RebuildBackbuffer(); Refresh();
            if (HasCapture()) ReleaseMouse();
            m_connecting = false; m_connectStartElem = -1; m_connectStartPin = -1; m_connectStartIsOutput = false; m_connectStartConnIndex = -1; m_connectStartConnOutputIndex = -1;
        }
        event.Skip();
    }

    void OnKeyDown(wxKeyEvent& event)
    {
        // Delete 键：优先删除选中连线，其次删除选中元件（原逻辑）
        if (event.GetKeyCode() == WXK_DELETE)
        {
            if (m_selectedConnectionIndex >= 0 && m_selectedConnectionIndex < (int)m_connections.size())
            {
                // 删除选中的连线
                m_connections.erase(m_connections.begin() + m_selectedConnectionIndex);

                // 仿真数据同步
                if (m_simulating) {
                    if ((int)m_connectionSignals.size() > (int)m_connections.size())
                        m_connectionSignals.resize(m_connections.size());
                    PropagateSignals();
                }

                // 重置选中状态
                m_selectedConnectionIndex = -1;
                m_backValid = false;
                RebuildBackbuffer();
                Refresh();
                m_dirty = true;
                SaveElementsAndConnectionsToFile();
                return;
            }

            // 原有：只有选中元素才删除
            if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_elements.size())
            {
                // 1. 先删除与该元件相关的所有连接
                std::vector<ConnectionInfo> remainingConnections;
                for (const auto& conn : m_connections)
                {
                    // 保留不涉及当前选中元件的连接
                    if (conn.aIndex != m_selectedIndex && conn.bIndex != m_selectedIndex)
                    {
                        remainingConnections.push_back(conn);
                    }
                }
                m_connections.swap(remainingConnections);

                // 2. 删除选中的元件
                m_elements.erase(m_elements.begin() + m_selectedIndex);

                // 3. 更新所有连接中涉及的元件索引（因为删除后索引会变化）
                for (auto& conn : m_connections)
                {
                    if (conn.aIndex > m_selectedIndex) conn.aIndex--;
                    if (conn.bIndex > m_selectedIndex) conn.bIndex--;
                }

                // 4. 重置选中状态
                m_selectedIndex = -1;
                if (m_propPanel)
                {
                    // 清空属性面板
                    m_propPanel->UpdateForElement(ElementInfo());
                }

                // 5. 标记为未保存并刷新
                m_dirty = true;
                m_backValid = false;
                RebuildBackbuffer();
                Refresh();
                SaveElementsAndConnectionsToFile();
                return;
            }
        }

        // 其它键或未处理的情况继续传递
        event.Skip();
    }


    // Export netlist (JSON)
    bool ExportNetlist(const std::string& filename)
    {
        try {
            json root; root["netlist"]["components"] = json::array();
            for (size_t i = 0; i < m_elements.size(); ++i) {
                const auto& e = m_elements[i];
                json comp; comp["id"] = (int)i; comp["type"] = e.type; comp["x"] = e.x; comp["y"] = e.y;
                comp["color"] = e.color; comp["thickness"] = e.thickness; comp["size"] = e.size; comp["rotationIndex"] = e.rotationIndex;
                comp["inputs"] = e.inputs; comp["outputs"] = e.outputs;
                root["netlist"]["components"].push_back(comp);
            }
            root["netlist"]["nets"] = json::array();
            for (size_t i = 0; i < m_connections.size(); ++i) {
                const auto& c = m_connections[i];
                json net; net["id"] = (int)i; net["endpoints"] = json::array();
                json epA; epA["compId"] = c.aIndex; epA["pin"] = c.aPin; epA["pos"] = { c.x1, c.y1 }; net["endpoints"].push_back(epA);
                json epB; epB["compId"] = c.bIndex; epB["pin"] = c.bPin; epB["pos"] = { c.x2, c.y2 }; net["endpoints"].push_back(epB);
                net["turningPoints"] = json::array();
                for (const auto& p : c.turningPoints) net["turningPoints"].push_back({ p.x, p.y });
                root["netlist"]["nets"].push_back(net);
            }
            std::ofstream ofs(filename);
            if (!ofs.is_open()) return false;
            ofs << root.dump(4); ofs.close();
            return true;
        }
        catch (...) { return false; }
    }

    // 导出 BookShelf（保持合并实现）
    bool ExportBookShelf(const std::string& nodeFile, const std::string& netFile)
    {
        try {
            std::map<std::pair<int, int>, std::string> extMap;
            int extCounter = 0;
            for (const auto& c : m_connections) {
                if (c.aIndex < 0) {
                    std::pair<int, int> key(c.x1, c.y1);
                    if (extMap.find(key) == extMap.end()) extMap[key] = "ext" + std::to_string(extCounter++);
                }
                if (c.bIndex < 0) {
                    std::pair<int, int> key(c.x2, c.y2);
                    if (extMap.find(key) == extMap.end()) extMap[key] = "ext" + std::to_string(extCounter++);
                }
            }
            // NODE
            {
                std::ofstream nodeofs(nodeFile);
                if (!nodeofs.is_open()) return false;
                nodeofs << "UCLA nodes 1.0\n";
                size_t numNodes = m_elements.size() + extMap.size();
                nodeofs << "NumNodes : " << numNodes << "\n";
                nodeofs << "NumTerminals : " << extMap.size() << "\n";
                for (size_t i = 0; i < m_elements.size(); ++i) {
                    const auto& e = m_elements[i];
                    int w = std::max(1, (int)std::round(60 * e.size));
                    int h = std::max(1, (int)std::round(40 * e.size));
                    nodeofs << "comp" << i << " " << w << " " << h << "\n";
                }
                for (const auto& kv : extMap) nodeofs << kv.second << " 1 1 terminal\n";
                nodeofs.close();
            }
            // NET
            {
                std::ofstream netofs(netFile);
                if (!netofs.is_open()) return false;
                netofs << "UCLA nets 1.0\n";
                netofs << "NumNets : " << m_connections.size() << "\n";
                size_t totalPins = 0;
                for (const auto& c : m_connections) totalPins += 2;
                netofs << "NumPins : " << totalPins << "\n";
                netofs.setf(std::ios::fixed);
                netofs << std::setprecision(6);
                for (size_t i = 0; i < m_connections.size(); ++i) {
                    const auto& c = m_connections[i];
                    int degree = 2;
                    netofs << "NetDegree : " << degree << " n" << i << "\n";
                    {
                        std::string nodeName;
                        double x = (double)c.x1; double y = (double)c.y1;
                        if (c.aIndex >= 0 && c.aIndex < (int)m_elements.size()) nodeName = "comp" + std::to_string(c.aIndex);
                        else {
                            auto it = extMap.find(std::make_pair(c.x1, c.y1));
                            if (it != extMap.end()) nodeName = it->second; else nodeName = "ext_unknown";
                        }
                        netofs << " " << nodeName << " O : " << x << " " << y << "\n";
                    }
                    {
                        std::string nodeName;
                        double x = (double)c.x2; double y = (double)c.y2;
                        if (c.bIndex >= 0 && c.bIndex < (int)m_elements.size()) nodeName = "comp" + std::to_string(c.bIndex);
                        else {
                            auto it = extMap.find(std::make_pair(c.x2, c.y2));
                            if (it != extMap.end()) nodeName = it->second; else nodeName = "ext_unknown";
                        }
                        netofs << " " << nodeName << " I : " << x << " " << y << "\n";
                    }
                }
                netofs.close();
            }
            return true;
        }
        catch (...) { return false; }
    }

    // 导入通用网表（兼容多种结构）
    bool ImportNetlist(const std::string& filename)
    {
        std::ifstream ifs(filename);
        if (!ifs.is_open()) return false;
        json root;
        try { ifs >> root; }
        catch (const std::exception& e) { wxMessageBox(wxString("读取或解析网表失败: ") + e.what(), "Import Error", wxOK | wxICON_ERROR); return false; }
        const json* nl = nullptr;
        if (root.contains("netlist") && root["netlist"].is_object()) nl = &root["netlist"];
        else nl = &root;

        m_elements.clear(); m_connections.clear();
        bool usedIdIndexing = false; std::map<int, ElementInfo> compById; int maxId = -1;
        if (nl->contains("components") && (*nl)["components"].is_array()) {
            for (const auto& comp : (*nl)["components"]) {
                ElementInfo e;
                e.type = comp.value("type", std::string());
                e.color = comp.value("color", std::string("black"));
                e.thickness = comp.value("thickness", 1);
                e.x = comp.value("x", 0);
                e.y = comp.value("y", 0);
                e.size = comp.value("size", 1);
                e.rotationIndex = comp.value("rotationIndex", 0);
                e.inputs = comp.value("inputs", 0);
                e.outputs = comp.value("outputs", 0);
                if (comp.contains("id")) {
                    int id = comp["id"].get<int>(); usedIdIndexing = true; compById[id] = e; if (id > maxId) maxId = id;
                }
                else m_elements.push_back(e);
            }
            if (usedIdIndexing) {
                m_elements.assign(maxId + 1, ElementInfo());
                for (const auto& kv : compById) if (kv.first >= 0 && kv.first < (int)m_elements.size()) m_elements[kv.first] = kv.second;
            }
        }
        else if (nl->contains("elements") && (*nl)["elements"].is_array()) {
            for (const auto& comp : (*nl)["elements"]) {
                ElementInfo e;
                e.type = comp.value("type", std::string());
                e.color = comp.value("color", std::string("black"));
                e.thickness = comp.value("thickness", 1);
                e.x = comp.value("x", 0);
                e.y = comp.value("y", 0);
                e.size = comp.value("size", 1);
                e.rotationIndex = comp.value("rotationIndex", 0);
                e.inputs = comp.value("inputs", 0);
                e.outputs = comp.value("outputs", 0);
                m_elements.push_back(e);
            }
        }

        if (nl->contains("nets") && (*nl)["nets"].is_array()) {
            for (const auto& net : (*nl)["nets"]) {
                if (!net.contains("endpoints") || !net["endpoints"].is_array()) continue;
                std::vector<json> endpoints;
                for (const auto& ep : net["endpoints"]) endpoints.push_back(ep);
                if (endpoints.size() == 2) {
                    ConnectionInfo c;
                    const auto& a = endpoints[0]; const auto& b = endpoints[1];
                    c.aIndex = a.value("compId", -1); c.aPin = a.value("pin", -1);
                    if (a.contains("pos") && a["pos"].is_array() && a["pos"].size() == 2) { c.x1 = a["pos"][0].get<int>(); c.y1 = a["pos"][1].get<int>(); }
                    c.bIndex = b.value("compId", -1); c.bPin = b.value("pin", -1);
                    if (b.contains("pos") && b["pos"].is_array() && b["pos"].size() == 2) { c.x2 = b["pos"][0].get<int>(); c.y2 = b["pos"][1].get<int>(); }
                    if (net.contains("turningPoints") && net["turningPoints"].is_array()) for (const auto& p : net["turningPoints"]) if (p.is_array() && p.size() == 2) c.turningPoints.push_back(wxPoint(p[0].get<int>(), p[1].get<int>()));
                    m_connections.push_back(c);
                }
                else if (endpoints.size() > 2) {
                    const auto& center = endpoints[0];
                    for (size_t k = 1; k < endpoints.size(); ++k) {
                        const auto& other = endpoints[k];
                        ConnectionInfo c;
                        c.aIndex = center.value("compId", -1); c.aPin = center.value("pin", -1);
                        if (center.contains("pos") && center["pos"].is_array() && center["pos"].size() == 2) { c.x1 = center["pos"][0].get<int>(); c.y1 = center["pos"][1].get<int>(); }
                        c.bIndex = other.value("compId", -1); c.bPin = other.value("pin", -1);
                        if (other.contains("pos") && other["pos"].is_array() && other["pos"].size() == 2) { c.x2 = other["pos"][0].get<int>(); c.y2 = other["pos"][1].get<int>(); }
                        m_connections.push_back(c);
                    }
                }
            }
        }
        else if (nl->contains("connections") && (*nl)["connections"].is_array()) {
            for (const auto& cjs : (*nl)["connections"]) {
                ConnectionInfo c;
                c.aIndex = cjs.value("a", -1); c.aPin = cjs.value("aPin", -1);
                c.bIndex = cjs.value("b", -1); c.bPin = cjs.value("bPin", -1);
                c.x1 = cjs.value("x1", 0); c.y1 = cjs.value("y1", 0);
                c.x2 = cjs.value("x2", 0); c.y2 = cjs.value("y2", 0);
                c.aConn = cjs.value("aConn", -1); c.aConnAux = cjs.value("aConnAux", -1);
                if (cjs.contains("turningPoints") && cjs["turningPoints"].is_array()) for (const auto& p : cjs["turningPoints"]) if (p.is_array() && p.size() == 2) c.turningPoints.push_back(wxPoint(p[0].get<int>(), p[1].get<int>()));
                if (cjs.contains("auxOutputs") && cjs["auxOutputs"].is_array()) {
                    for (const auto& av : cjs["auxOutputs"]) {
                        ConnectionInfo::AuxOutput ao;
                        if (av.is_object()) { ao.segIndex = av.value("seg", 0); ao.t = av.value("t", 0.0); }
                        else if (av.is_array() && av.size() == 2) {
                            int px = av[0].get<int>(); int py = av[1].get<int>();
                            std::vector<wxPoint> poly; poly.emplace_back(c.x1, c.y1);
                            for (const auto& tp : c.turningPoints) poly.push_back(tp);
                            poly.emplace_back(c.x2, c.y2);
                            int seg; double t; wxPoint q;
                            std::tie(seg, t, q) = ProjectPointToPolylineDetailed(wxPoint(px, py), poly);
                            ao.segIndex = seg < 0 ? 0 : seg;
                            ao.t = t;
                        }
                        c.auxOutputs.push_back(ao);
                    }
                }
                m_connections.push_back(c);
            }
        }

        bool saved = SaveElementsAndConnectionsToFile();
        if (!saved) wxMessageBox("导入成功，但保存到 Elementlib.json 失败（可能没有写权限）。", "Import", wxOK | wxICON_WARNING);
        m_backValid = false; RebuildBackbuffer(); Refresh();
        return true;
    }

    bool SaveToFile(const std::string& filename)
    {
        // 直接调用已有的 SaveElementsAndConnectionsToFile
        return SaveElementsAndConnectionsToFile(filename);
    }

    // 仿真控制
    bool IsSimulating() const { return m_simulating; }
    void ToggleSimulation()
    {
        m_simulating = !m_simulating;
        if (m_simulating) StartSimulation();
        else StopSimulation();
        m_backValid = false;
        RebuildBackbuffer();
        Refresh();
    }

    void SetInputValue(int elemIndex, int value)
    {
        if (elemIndex < 0 || elemIndex >= (int)m_elements.size()) return;
        if (!IsInputType(m_elements[elemIndex].type)) return;
        if (value != 0 && value != 1) return;
        if ((int)m_elementOutputs.size() != (int)m_elements.size()) m_elementOutputs.assign(m_elements.size(), -1);
        m_elementOutputs[elemIndex] = value;
        PropagateSignals();
        m_backValid = false;
        RebuildBackbuffer();
        Refresh();
    }

private:
    // 数据
    std::vector<ElementInfo> m_elements;
    std::vector<ConnectionInfo> m_connections;

    // 仿真相关
    std::vector<int> m_connectionSignals; // -1 unknown, 0,1
    std::vector<int> m_elementOutputs;
    bool m_simulating;

    // 后备位图
    wxBitmap m_backBitmap;
    bool m_backValid;

    // 拖拽
    bool m_dragging;
    int m_dragIndex;
    wxPoint m_dragOffset;
    wxPoint m_dragStart;
    wxPoint m_dragCurrent;
    wxPoint m_prevDragCurrent;

    // 连线
    bool m_connecting;
    wxPoint m_connectStartGrid;
    int m_connectStartElem;
    bool m_connectStartIsOutput;
    int m_connectStartPin;
    int m_connectStartConnIndex;
    int m_connectStartConnOutputIndex;
    int m_connectEndPin;
    wxPoint m_tempLineEnd;
    wxPoint m_prevTempLineEnd;

    // 选中
    int m_selectedIndex;
    PropertyPanel* m_propPanel;
    int m_selectedConnectionIndex;

    // 未保存
    bool m_dirty;

    const int ElemWidth = 60;
    const int ElemHeight = 40;
    const int ConnectorRadius = 6;

    struct ConnectorHit {
        bool hit = false;
        int elemIndex = -1;
        int pinIndex = -1;
        bool isOutput = false;
        bool isConnOutput = false;
        int connIndex = -1;
        int connOutputIndex = -1;
    };

    // ---- 辅助方法 ----
    static bool IsInputType(const std::string& type) {
        if (type.empty()) return false;
        if (type == "Input" || type == "Input Pin" || type == "InputPin") return true;
        if (type.size() >= 5 && type.substr(0, 5) == "Input") return true;
        return false;
    }
    static bool IsOutputType(const std::string& type) {
        if (type.empty()) return false;
        if (type == "Output" || type == "Output Pin" || type == "OutputPin") return true;
        if (type.size() >= 6 && type.substr(0, 6) == "Output") return true;
        return false;
    }

    wxPoint SnapToGrid(const wxPoint& p) const {
        int gx = (p.x + 5) / 10 * 10;
        int gy = (p.y + 5) / 10 * 10;
        return wxPoint(gx, gy);
    }

    // 支持多个输出/输入分布
    wxPoint GetOutputPoint(const ElementInfo& e, int pinIndex = 0) const {
        int sz = std::max(1, e.size);
        int w = BaseElemWidth * sz;
        int h = BaseElemHeight * sz;
        int n = std::max(1, e.outputs);
        float step = (float)h / (n + 1);
        int py = e.y + (int)(step * (pinIndex + 1));
        return wxPoint(e.x + w, py);
    }
    wxPoint GetInputPoint(const ElementInfo& e, int pinIndex) const {
        int sz = std::max(1, e.size);
        int w = BaseElemWidth * sz;
        int h = BaseElemHeight * sz;
        int n = std::max(1, e.inputs);
        float step = (float)h / (n + 1);
        int py = e.y + (int)(step * (pinIndex + 1));
        return wxPoint(e.x, py);
    }
    wxPoint GetConnectorPosition(int elemIndex, int pinIndex, bool isOutput) const {
        if (elemIndex < 0 || elemIndex >= (int)m_elements.size()) return wxPoint(0, 0);
        const ElementInfo& e = m_elements[elemIndex];
        if (isOutput) return GetOutputPoint(e, pinIndex < 0 ? 0 : pinIndex);
        return GetInputPoint(e, pinIndex < 0 ? 0 : pinIndex);
    }

// 当没有精确点击到端口时，若点击落在元件区域内，会自动找到最近的输入端并返回（实现“自动对齐到输入点”）
    ConnectorHit HitTestConnector(const wxPoint& p) const {
        ConnectorHit res;
        for (int i = (int)m_elements.size() - 1; i >= 0; --i) {
            const ElementInfo& e = m_elements[i];
            int nOut = std::max(1, e.outputs);
            for (int op = 0; op < nOut; ++op) {
                wxPoint out = GetOutputPoint(e, op);
                if (DistanceSquared(out, p) <= ConnectorRadius * ConnectorRadius) {
                    res.hit = true; res.elemIndex = i; res.pinIndex = op; res.isOutput = true; return res;
                }
            }
            int n = std::max(1, e.inputs);
            for (int pin = 0; pin < n; ++pin) {
                wxPoint in = GetInputPoint(e, pin);
                if (DistanceSquared(in, p) <= ConnectorRadius * ConnectorRadius) { res.hit = true; res.elemIndex = i; res.pinIndex = pin; res.isOutput = false; return res; }
            }
        }
        // auxOutputs 命中检测（多个 connection）
        for (int ci = (int)m_connections.size() - 1; ci >= 0; --ci) {
            const auto& c = m_connections[ci];
            for (int ai = (int)c.auxOutputs.size() - 1; ai >= 0; --ai) {
                wxPoint ap = AuxOutputToPixel(c.auxOutputs[ai], c, m_elements);
                if (DistanceSquared(ap, p) <= ConnectorRadius * ConnectorRadius) {
                    res.hit = true;
                    res.isOutput = true;
                    res.isConnOutput = true;
                    res.connIndex = ci;
                    res.connOutputIndex = ai;
                    res.elemIndex = -1;
                    res.pinIndex = -1;
                    return res;
                }
            }
        }
        return res;
    }

    static int DistanceSquared(const wxPoint& a, const wxPoint& b) { int dx = a.x - b.x; int dy = a.y - b.y; return dx * dx + dy * dy; }

    int HitTestElement(const wxPoint& p) const {
        for (int i = (int)m_elements.size() - 1; i >= 0; --i) {
            const ElementInfo& e = m_elements[i];
            int sz = std::max(1, e.size);
            int w = BaseElemWidth * sz;
            int h = BaseElemHeight * sz;
            wxRect r(e.x, e.y, w, h);
            if (r.Contains(p)) return i;
        }
        return -1;
    }


    // Build polyline for a connection (包含端点与 turningPoints)
    static std::vector<wxPoint> BuildConnectionPolyline(const ConnectionInfo& c, const std::vector<ElementInfo>& elements) {
        std::vector<wxPoint> poly;
        wxPoint p1(c.x1, c.y1), p2(c.x2, c.y2);
        if (c.aIndex >= 0 && c.aIndex < (int)elements.size()) {
            const ElementInfo& ea = elements[c.aIndex];
            int sz = std::max(1, ea.size);
            int w = BaseElemWidth * sz;
            int h = BaseElemHeight * sz;
            int nOut = std::max(1, ea.outputs);
            int pin = (c.aPin < 0) ? 0 : c.aPin;
            float step = (float)h / (nOut + 1);
            int py = ea.y + (int)(step * (pin + 1));
            p1 = wxPoint(ea.x + w, py);
        }
        if (c.bIndex >= 0 && c.bIndex < (int)elements.size()) {
            const ElementInfo& eb = elements[c.bIndex];
            int sz = std::max(1, eb.size);
            int h = BaseElemHeight * sz;
            int nIn = std::max(1, eb.inputs);
            int pin = (c.bPin < 0) ? 0 : c.bPin;
            float step = (float)h / (nIn + 1);
            int py = eb.y + (int)(step * (pin + 1));
            p2 = wxPoint(eb.x, py);
        }
        poly.push_back(p1);
        for (const auto& tp : c.turningPoints) poly.push_back(tp);
        poly.push_back(p2);
        return poly;
    }

    // 投影点到段，返回 t 与最近点
    static std::pair<double, wxPoint> ProjectPointToSegmentT(const wxPoint& a, const wxPoint& b, const wxPoint& p) {
        int dx = b.x - a.x;
        int dy = b.y - a.y;
        if (dx == 0 && dy == 0) return { 0.0, a };
        double denom = double(dx) * dx + double(dy) * dy;
        double t = ((p.x - a.x) * (double)dx + (p.y - a.y) * (double)dy) / denom;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        wxPoint q((int)std::round(a.x + t * dx), (int)std::round(a.y + t * dy));
        return { t, q };
    }

    static std::tuple<int, double, wxPoint> ProjectPointToPolylineDetailed(const wxPoint& p, const std::vector<wxPoint>& pts) {
        if (pts.size() < 2) return { -1, 0.0, p };
        int bestSeg = -1;
        double bestT = 0.0;
        int bestSq = INT_MAX;
        wxPoint bestPt = pts.front();
        for (size_t i = 1; i < pts.size(); ++i) {
            auto pr = ProjectPointToSegmentT(pts[i - 1], pts[i], p);
            double t = pr.first;
            wxPoint q = pr.second;
            int dx = p.x - q.x;
            int dy = p.y - q.y;
            int d2 = dx * dx + dy * dy;
            if (d2 < bestSq) {
                bestSq = d2;
                bestSeg = (int)i - 1;
                bestT = t;
                bestPt = q;
            }
        }
        return { bestSeg, bestT, bestPt };
    }

    static wxPoint AuxOutputToPixel(const ConnectionInfo::AuxOutput& ao, const ConnectionInfo& c, const std::vector<ElementInfo>& elements) {
        auto poly = BuildConnectionPolyline(c, elements);
        if (poly.size() < 2) return wxPoint(c.x1, c.y1);
        int seg = ao.segIndex;
        if (seg < 0) seg = 0;
        if (seg >= (int)poly.size() - 1) seg = (int)poly.size() - 2;
        const wxPoint& a = poly[seg];
        const wxPoint& b = poly[seg + 1];
        int dx = b.x - a.x;
        int dy = b.y - a.y;
        double t = ao.t;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        return wxPoint((int)std::round(a.x + t * dx), (int)std::round(a.y + t * dy));
    }

    // FindConnectionSegmentHit：返回最近 segment 的 conn/seg/t/nearest
    bool FindConnectionSegmentHit(const wxPoint& p, int& outConnIndex, int& outSegIndex, double& outT, wxPoint& outNearest, int maxDist = 6) const {
        int bestSq = maxDist * maxDist;
        bool found = false;
        wxPoint bestPt(0, 0);
        int bestConn = -1;
        int bestSeg = -1;
        double bestT = 0.0;

        for (int ci = 0; ci < (int)m_connections.size(); ci++) {
            const auto& c = m_connections[ci];
            std::vector<wxPoint> pts = BuildConnectionPolyline(c, m_elements);
            for (size_t i = 1; i < pts.size(); ++i) {
                auto pr = ProjectPointToSegmentT(pts[i - 1], pts[i], p);
                double t = pr.first;
                wxPoint q = pr.second;
                int dx = p.x - q.x;
                int dy = p.y - q.y;
                int d2 = dx * dx + dy * dy;
                if (d2 <= bestSq) {
                    bestSq = d2;
                    bestPt = q;
                    bestConn = ci;
                    bestSeg = (int)i - 1;
                    bestT = t;
                    found = true;
                }
            }
            for (int ai = 0; ai < (int)c.auxOutputs.size(); ++ai) {
                wxPoint ap = AuxOutputToPixel(c.auxOutputs[ai], c, m_elements);
                int dx = p.x - ap.x;
                int dy = p.y - ap.y;
                int d2 = dx * dx + dy * dy;
                if (d2 <= bestSq) {
                    bestSq = d2;
                    bestPt = ap;
                    bestConn = ci;
                    bestSeg = c.auxOutputs[ai].segIndex;
                    bestT = c.auxOutputs[ai].t;
                    found = true;
                }
            }
        }

        if (found) {
            outConnIndex = bestConn;
            outSegIndex = bestSeg;
            outT = bestT;
            outNearest = bestPt;
            return true;
        }
        return false;
    }

    // 判断连接是否合法（合并两边实现）
    bool IsConnectionValid(const ConnectionInfo& c) const {
        if (c.bIndex < 0) return false;
        if (c.bIndex >= (int)m_elements.size()) return false;
        const ElementInfo& bElem = m_elements[c.bIndex];
        int bInputs = std::max(1, bElem.inputs);
        if (c.bPin < 0 || c.bPin >= bInputs) return false;

        if (c.aIndex >= 0) {
            if (c.aIndex >= (int)m_elements.size()) return false;
            if (c.aPin < -1 || c.aPin > 0) return false;
        }
        else {
            if (c.aConn >= 0) {
                if (c.aConn < 0 || c.aConn >= (int)m_connections.size()) return false;
                const auto& parent = m_connections[c.aConn];
                if (c.aConnAux < 0 || c.aConnAux >= (int)parent.auxOutputs.size()) return false;
            }
        }
        if (c.aIndex >= 0 && c.bIndex >= 0 && c.aIndex == c.bIndex) return false;
        return true;
    }

    void CleanConnections() {
        std::vector<ConnectionInfo> keep;
        keep.reserve(m_connections.size());
        for (const auto& c : m_connections) if (IsConnectionValid(c)) keep.push_back(c);
        if (keep.size() != m_connections.size()) m_connections.swap(keep);
    }

    // Load / Save
    void LoadElementsAndConnectionsFromFile()
    {
        m_elements.clear();
        m_connections.clear();
        std::ifstream file("Elementlib.json");
        if (!file.is_open()) { m_dirty = false; return; }
        try {
            json j; file >> j;
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
                    e.outputs = comp.value("outputs", 0);
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
                    ci.aConn = c.value("aConn", -1);
                    ci.aConnAux = c.value("aConnAux", -1);
                    if (c.contains("turningPoints") && c["turningPoints"].is_array()) {
                        for (const auto& p : c["turningPoints"]) ci.turningPoints.push_back(wxPoint(p[0], p[1]));
                    }
                    if (c.contains("auxOutputs") && c["auxOutputs"].is_array()) {
                        for (const auto& av : c["auxOutputs"]) {
                            ConnectionInfo::AuxOutput ao;
                            if (av.is_object()) {
                                ao.segIndex = av.value("seg", 0);
                                ao.t = av.value("t", 0.0);
                            }
                            else if (av.is_array() && av.size() == 2) {
                                int px = av[0].get<int>();
                                int py = av[1].get<int>();
                                std::vector<wxPoint> poly;
                                poly.emplace_back(ci.x1, ci.y1);
                                for (const auto& tp : ci.turningPoints) poly.push_back(tp);
                                poly.emplace_back(ci.x2, ci.y2);
                                int seg; double t; wxPoint q;
                                std::tie(seg, t, q) = ProjectPointToPolylineDetailed(wxPoint(px, py), poly);
                                ao.segIndex = seg < 0 ? 0 : seg;
                                ao.t = t;
                            }
                            ci.auxOutputs.push_back(ao);
                        }
                    }
                    m_connections.push_back(ci);
                }
            }
        }
        catch (...) {}
        CleanConnections();
        m_dirty = false;
        m_backValid = false;
        m_connectionSignals.clear();
        m_elementOutputs.clear();
    }

    bool SaveElementsAndConnectionsToFile(const std::string& filename = "Elementlib.json")
    {
        for (auto& c : m_connections) {
            if (c.aIndex >= 0 && c.aIndex < (int)m_elements.size()) {
                wxPoint p = GetConnectorPosition(c.aIndex, c.aPin, true);
                c.x1 = p.x; c.y1 = p.y;
            }
            if (c.bIndex >= 0 && c.bIndex < (int)m_elements.size()) {
                wxPoint p = GetConnectorPosition(c.bIndex, c.bPin, false);
                c.x2 = p.x; c.y2 = p.y;
            }
            if (c.aConn >= 0 && c.aConn < (int)m_connections.size() && c.aConnAux >= 0) {
                const auto& parent = m_connections[c.aConn];
                if (c.aConnAux < (int)parent.auxOutputs.size()) {
                    wxPoint p = AuxOutputToPixel(parent.auxOutputs[c.aConnAux], parent, m_elements);
                    c.x1 = p.x; c.y1 = p.y;
                }
            }
        }
        CleanConnections();
        try {
            json j; j["elements"] = json::array();
            for (const auto& e : m_elements) {
                json item;
                item["type"] = e.type;
                item["color"] = e.color;
                item["thickness"] = e.thickness;
                item["x"] = e.x; item["y"] = e.y;
                item["size"] = e.size;
                item["rotationIndex"] = e.rotationIndex;
                item["inputs"] = e.inputs;
                item["outputs"] = e.outputs;
                j["elements"].push_back(item);
            }
            j["connections"] = json::array();
            for (const auto& c : m_connections) {
                json cj;
                cj["a"] = c.aIndex; cj["aPin"] = c.aPin;
                cj["b"] = c.bIndex; cj["bPin"] = c.bPin;
                cj["x1"] = c.x1; cj["y1"] = c.y1;
                cj["x2"] = c.x2; cj["y2"] = c.y2;
                cj["aConn"] = c.aConn; cj["aConnAux"] = c.aConnAux;
                cj["turningPoints"] = json::array();
                for (const auto& p : c.turningPoints) cj["turningPoints"].push_back({ p.x, p.y });
                cj["auxOutputs"] = json::array();
                for (const auto& ao : c.auxOutputs) {
                    wxPoint pt = AuxOutputToPixel(ao, c, m_elements);
                    json ajo; ajo["seg"] = ao.segIndex; ajo["t"] = ao.t; ajo["pos"] = { pt.x, pt.y };
                    cj["auxOutputs"].push_back(ajo);
                }
                j["connections"].push_back(cj);
            }
            std::ofstream ofs(filename);
            if (!ofs.is_open()) return false;
            ofs << j.dump(4);
            ofs.close();

            std::string base = filename;
            size_t pos = base.find_last_of('.');
            if (pos != std::string::npos) base = base.substr(0, pos);
            std::string nodeFile = base + ".node";
            std::string netFile = base + ".net";
            bool bsOk = ExportBookShelf(nodeFile, netFile);
            m_dirty = false;
            return bsOk;
        }
        catch (...) {
            return false;
        }
    }


    // 仿真：Start/Stop/Propagate，以及简单的元件行为（以首个已知输入为准并取反）
    void StartSimulation()
    {
        m_connectionSignals.assign(m_connections.size(), -1);
        m_elementOutputs.assign(m_elements.size(), -1);
        for (size_t i = 0; i < m_elements.size(); ++i) {
            if (IsInputType(m_elements[i].type)) m_elementOutputs[i] = 0;
        }
        PropagateSignals();
    }

    void StopSimulation()
    {
        m_simulating = false;
        m_connectionSignals.clear();
        m_elementOutputs.clear();
        m_backValid = false; RebuildBackbuffer(); Refresh();
    }

    static int InvertSignalStatic(int s) {
        if (s == 0) return 1;
        if (s == 1) return 0;
        return -1;
    }

    // PropagateSignals：两步迭代直到稳定（简化规则：元素输出 = invert(first known input)；Input 元件输出由 m_elementOutputs 固定）
    void PropagateSignals()
    {
        if (m_connections.empty()) return;
        if ((int)m_connectionSignals.size() != (int)m_connections.size()) m_connectionSignals.assign(m_connections.size(), -1);
        if ((int)m_elementOutputs.size() != (int)m_elements.size()) m_elementOutputs.assign(m_elements.size(), -1);

        bool changed = true;
        int iter = 0;
        const int maxIter = 200;
        while (changed && iter++ < maxIter) {
            changed = false;
            // 1) 元件输出 -> 线
            for (size_t ci = 0; ci < m_connections.size(); ++ci) {
                const auto& c = m_connections[ci];
                if (c.aIndex >= 0 && c.aIndex < (int)m_elementOutputs.size()) {
                    int srcVal = m_elementOutputs[c.aIndex];
                    if (srcVal != -1 && m_connectionSignals[ci] != srcVal) {
                        m_connectionSignals[ci] = srcVal; changed = true;
                    }
                }
                // 起点来自父 connection aux 的情况
                else if (c.aConn >= 0 && c.aConn < (int)m_connections.size()) {
                    // 如果父连接有信号，传播
                    int parentIndex = c.aConn;
                    if (parentIndex >= 0 && parentIndex < (int)m_connectionSignals.size()) {
                        int pv = m_connectionSignals[parentIndex];
                        if (pv != -1 && m_connectionSignals[ci] != pv) { m_connectionSignals[ci] = pv; changed = true; }
                    }
                }
            }
            // 2) 线 -> 元件输出（按规则）
            for (size_t ei = 0; ei < m_elements.size(); ++ei) {
                if (IsInputType(m_elements[ei].type)) continue; // Input 的输出由自己控制

                // 收集所有连到该元件输入端的连线信号（保持 pin 顺序以防以后扩展）
                std::vector<int> inputSignals;
                // 确定目标元件应该有多少输入（保证顺序一致）
                int expectedInputs = std::max(1, m_elements[ei].inputs);
                // 初始化为 unknown，随后对存在连线的 pin 填充
                inputSignals.assign(expectedInputs, -1);

                for (size_t ci = 0; ci < m_connections.size(); ++ci) {
                    const auto& c = m_connections[ci];
                    if (c.bIndex == (int)ei) {
                        int pin = c.bPin;
                        int val = -1;
                        if (ci < m_connectionSignals.size()) val = m_connectionSignals[ci];
                        if (pin >= 0 && pin < (int)inputSignals.size()) inputSignals[pin] = val;
                        else inputSignals.push_back(val); // 非规范 pin 时追加
                    }
                }

                int newOut = -1;
                // 优先处理 Output 类型：直接反映第一个已知输入值（不通过 Signals）
                if (!inputSignals.empty()) {
                    if (IsOutputType(m_elements[ei].type)) {
                        for (int v : inputSignals) {
                            if (v != -1) { newOut = v; break; }
                        }
                        // 若没有已知输入，则保持 unknown (-1)
                    }
                    else {
                        // 其他门/元件仍由 Signals 计算
                        newOut = Signals(inputSignals, m_elements[ei].type);
                    }
                }
                else {
                    newOut = -1;
                }

                if (newOut != m_elementOutputs[ei]) {
                    m_elementOutputs[ei] = newOut;
                    changed = true;
                }
            }
        }

    }

    // RebuildBackbuffer & 绘制
    void RebuildBackbuffer()
    {
        wxSize sz = GetClientSize();
        if (sz.x <= 0 || sz.y <= 0) { m_backValid = false; return; }
        m_backBitmap = wxBitmap(sz.x, sz.y);
        wxMemoryDC mdc(m_backBitmap);
        mdc.SetBackground(wxBrush(GetBackgroundColour()));
        mdc.Clear();
        DrawGrid(mdc);

        const int auxRadius = 3;
        for (size_t ci = 0; ci < m_connections.size(); ++ci) {
            const auto& c = m_connections[ci];
            ConnectionInfo tc = c;
            if (tc.aConn >= 0 && tc.aConn < (int)m_connections.size() && tc.aConnAux >= 0) {
                const auto& parent = m_connections[tc.aConn];
                if (tc.aConnAux < (int)parent.auxOutputs.size()) {
                    wxPoint sp = AuxOutputToPixel(parent.auxOutputs[tc.aConnAux], parent, m_elements);
                    tc.x1 = sp.x; tc.y1 = sp.y;
                }
            }
            bool isOutputToInput = ((tc.aIndex >= 0) || (tc.aConn >= 0)) && (tc.bIndex >= 0);
            wxColour lineColor = isOutputToInput ? wxColour(0, 128, 0) : wxColour(0, 0, 0);
            // 仿真态时根据信号显示颜色
            if (m_simulating && (int)ci < (int)m_connectionSignals.size() && m_connectionSignals[ci] != -1) {
                int sig = m_connectionSignals[ci];
                lineColor = (sig == 0) ? wxColour(30, 144, 255) : wxColour(0, 160, 0);
            }
            // 选中高亮
            if ((int)ci == m_selectedConnectionIndex) {
                mdc.SetPen(wxPen(wxColour(30, 144, 255), 4));
            }
            else {
                mdc.SetPen(wxPen(lineColor, 2));
            }
            DrawConnection(mdc, tc, m_elements, lineColor);

            mdc.SetBrush(wxBrush(lineColor));
            mdc.SetPen(wxPen(lineColor, 1));
            for (const auto& ao : c.auxOutputs) {
                wxPoint pt = AuxOutputToPixel(ao, c, m_elements);
                mdc.DrawCircle(pt.x, pt.y, auxRadius);
            }
        }

        // 元件绘制
        for (const auto& comp : m_elements) {
            DrawElement(mdc, comp.type, comp.color, comp.thickness, comp.x, comp.y, comp.size);
        }

        // 绘制端点与仿真值显示
        const int pinRadius = 4;
        for (int i = 0; i < (int)m_elements.size(); ++i) {
            const ElementInfo& e = m_elements[i];
            // 输出端点
            if (e.type != "Output") {
                int nOutputs = std::max(0, e.outputs);
                if (nOutputs == 0) nOutputs = 1;
                for (int op = 0; op < nOutputs; ++op) {
                    wxPoint outPt = GetOutputPoint(e, op);
                    bool outConnected = false;
                    for (const auto& c : m_connections) if (c.aIndex == i && c.aPin == op) { outConnected = true; break; }
                    wxColour outColor = outConnected ? wxColour(0, 128, 0) : wxColour(30, 144, 255);
                    mdc.SetBrush(wxBrush(outColor)); mdc.SetPen(wxPen(outColor, 1));
                    mdc.DrawCircle(outPt.x, outPt.y, pinRadius);
                }
            }
            // 输入端点
            if (e.type != "Input") {
                int nInputs = std::max(0, e.inputs);
                if (nInputs == 0) nInputs = 1;
                for (int pin = 0; pin < nInputs; ++pin) {
                    wxPoint inPt = GetInputPoint(e, pin);
                    bool inConnected = false;
                    for (const auto& c : m_connections) if (c.bIndex == i && c.bPin == pin) { inConnected = true; break; }
                    wxColour inColor = inConnected ? wxColour(0, 128, 0) : wxColour(30, 144, 255);
                    mdc.SetBrush(wxBrush(inColor)); mdc.SetPen(wxPen(inColor, 1));
                    mdc.DrawCircle(inPt.x, inPt.y, pinRadius);
                }
            }

            // 仿真显示
            if (m_simulating) {
                if (IsInputType(e.type)) {
                    int val = -1;
                    if (i >= 0 && i < (int)m_elementOutputs.size()) val = m_elementOutputs[i];
                    if (val != -1) {
                        wxString vs = wxString::Format("%d", val);
                        int fontSize = std::max(8, 12 * e.size);
                        wxFont font(fontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
                        mdc.SetFont(font);
                        mdc.SetTextForeground(wxColour(0, 0, 0));
                        int w = BaseElemWidth * std::max(1, e.size);
                        int x = e.x + w / 2 - fontSize / 2;
                        int y = e.y - fontSize - 4;
                        mdc.DrawText(vs, x, y);
                    }
                }
                else {
                    int outv = -1; if (i >= 0 && i < (int)m_elementOutputs.size()) outv = m_elementOutputs[i];
                    if (outv != -1) {
                        wxString vs = wxString::Format("%d", outv);
                        int fontSize = std::max(8, 12 * e.size);
                        wxFont font(fontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
                        mdc.SetFont(font);
                        mdc.SetTextForeground(wxColour(0, 0, 0));
                        mdc.DrawText(vs, e.x + std::max(10, BaseElemWidth * e.size) + 6, e.y + BaseElemHeight * e.size / 2 - 8);
                    }
                }
            }
        }

        mdc.SelectObject(wxNullBitmap);
        m_backValid = true;
    }

    void DrawGrid(wxDC& dc)
    {
        dc.SetPen(*wxLIGHT_GREY_PEN);
        int height, width; GetClientSize(&width, &height);
        for (int i = 0; i < width; i += 10) for (int j = 0; j < height; j += 10) dc.DrawPoint(i, j);
    }

    // FindConnectionSegmentHit 的公开包装
    bool FindConnectionSegmentHitPublic(const wxPoint& p, int& outConnIndex, int& outSegIndex, double& outT, wxPoint& outNearest, int maxDist = 6) const {
        return const_cast<CanvasPanel*>(this)->FindConnectionSegmentHit(p, outConnIndex, outSegIndex, outT, outNearest, maxDist);
    }

    // ---- 事件处理 ----

    // Draw helpers
    wxRect ElementRect(const wxPoint& pos, int size) const { int sz = std::max(1, size); return wxRect(pos.x, pos.y, BaseElemWidth * sz, BaseElemHeight * sz); }
    wxRect ElementRect(const wxPoint& pos) const { return wxRect(pos.x, pos.y, BaseElemWidth, BaseElemHeight); }


};

// PropertyPanel::OnApply
void PropertyPanel::OnApply(wxCommandEvent& evt)
{
    if (!m_canvas) return;
    int sel = m_canvas->GetSelectedIndex();
    if (sel < 0) { wxMessageBox("没有选中的元件。", "Info", wxOK | wxICON_INFORMATION); return; }
    int x = m_spinX->GetValue();
    int y = m_spinY->GetValue();
    int size = m_spinSize->GetValue();
    int inputs = m_spinInputs->GetValue();
    int outputs = m_spinOutputs->GetValue();
    m_canvas->ApplyPropertiesToSelected(x, y, size, inputs, outputs);
}


bool MyApp::OnInit()
{
    try {
        json j; j["elements"] = json::array(); j["connections"] = json::array();
        std::ofstream ofs("Elementlib.json");
        if (ofs.is_open()) { ofs << j.dump(4); ofs.close(); }
    }
    catch (...) {}
    wxInitAllImageHandlers();
    MyFrame* frame = new MyFrame();
    frame->Show(true);
    return true;
}

MyFrame::MyFrame()
    : wxFrame(NULL, -1, "logisim")
{
    SetSize(800, 600);
    // menus
    wxMenu* menuFile = new wxMenu;
    menuFile->Append(wxID_NEW, "New         Crtl+N");
    menuFile->Append(wxID_EXIT, "Exit");
    menuFile->Append(ID_FILE_OPENRECENT, "Import Netlist...");
    menuFile->Append(ID_FILE_SAVE, "Export Netlist...");

    wxMenu* menuEdit = new wxMenu;
    menuEdit->Append(ID_CUT, "Cut");
    menuEdit->Append(ID_COPY, "Copy");

    wxMenu* menuProject = new wxMenu;
    menuProject->Append(ID_PROJECT_ADD_CIRCUIT, "Add Circuit");

    wxMenu* menuSim = new wxMenu;
    menuSim->Append(ID_SIM_ENABLE, "Enable");

    wxMenu* menuWindow = new wxMenu;
    menuWindow->Append(ID_WINDOW_CASCADE, "Cascade Windows");

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

    wxString exePath = wxStandardPaths::Get().GetExecutablePath();
    wxFileName exeFn(exePath);
    wxString resDir = exeFn.GetPath();
    wxString imgDir = wxFileName(resDir, "image").GetFullPath();

    auto LoadBitmapSafe = [&](const wxString& relName, const wxSize& size) -> wxBitmap {
        wxString full = wxFileName(imgDir, relName).GetFullPath();
        if (!wxFileExists(full)) { wxLogWarning("Toolbar image not found: %s", full); return wxArtProvider::GetBitmap(wxART_MISSING_IMAGE, wxART_TOOLBAR); }
        wxImage img; if (!img.LoadFile(full)) { wxLogWarning("Failed to load image file: %s", full); return wxArtProvider::GetBitmap(wxART_MISSING_IMAGE, wxART_TOOLBAR); }
        if (size.x > 0 && size.y > 0) img = img.Rescale(size.x, size.y, wxIMAGE_QUALITY_HIGH);
        wxBitmap bmp(img); if (!bmp.IsOk()) return wxArtProvider::GetBitmap(wxART_MISSING_IMAGE, wxART_TOOLBAR);
        return bmp;
        };

    wxPanel* topPanel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* topPanelSizer = new wxBoxSizer(wxVERTICAL);
    wxSize toolSize(20, 20);
    wxToolBar* topBar1 = new wxToolBar(topPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER);
    topBar1->SetToolBitmapSize(toolSize);
    topBar1->AddTool(ID_TOOL_CHGVALUE, wxEmptyString, LoadBitmapSafe("logisim2.png", toolSize), "Change value");
    topBar1->AddTool(ID_TOOL_EDITSELECT, wxEmptyString, LoadBitmapSafe("logisim3.png", toolSize), "Edit selection");
    topBar1->AddTool(ID_TOOL_EDITTXET, wxEmptyString, LoadBitmapSafe("logisim4.png", toolSize), "Edit text");
    topBar1->AddSeparator();
    topBar1->AddTool(ID_TOOL_ADDPIN4, wxEmptyString, LoadBitmapSafe("logisim5.png", toolSize), "Add Pin 4");
    topBar1->AddTool(ID_TOOL_ADDPIN5, wxEmptyString, LoadBitmapSafe("logisim6.png", toolSize), "Add Pin 5");
    topBar1->AddTool(ID_TOOL_ADDNOTGATE, wxEmptyString, LoadBitmapSafe("logisim7.png", toolSize), "Add NOT Gate");
    topBar1->AddTool(ID_TOOL_ADDANDGATE, wxEmptyString, LoadBitmapSafe("logisim8.png", toolSize), "Add AND Gate");
    topBar1->AddTool(ID_TOOL_ADDORGATE, wxEmptyString, LoadBitmapSafe("logisim9.png", toolSize), "Add OR Gate");
    topBar1->Realize();

    wxToolBar* topBar2 = new wxToolBar(topPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER);
    topBar2->SetToolBitmapSize(toolSize);
    topBar2->AddTool(ID_TOOL_SHOWPROJECTC, wxEmptyString, LoadBitmapSafe("logisim11.png", toolSize), "Show projects circuit");
    topBar2->AddTool(ID_TOOL_SHOWSIMULATION, wxEmptyString, LoadBitmapSafe("logisim12.png", toolSize), "Show simulation results");
    topBar2->AddTool(ID_TOOL_EDITVIEW, wxEmptyString, LoadBitmapSafe("logisim13.png", toolSize), "Edit view");
    topBar2->Realize();

    topPanelSizer->Add(topBar1, 0, wxEXPAND | wxALL, 0);
    topPanelSizer->Add(topBar2, 0, wxEXPAND | wxALL, 0);
    topPanel->SetSizer(topPanelSizer);

    // 划分窗口：左侧为（树 + 属性），右侧为画布
    wxSplitterWindow* splitter = new wxSplitterWindow(this, wxID_ANY);

    // 先创建画布（右侧），因为属性面板需要引用它
    CanvasPanel* canvas = new CanvasPanel(splitter);

    // 关键：保存对画布的引用，使导入/导出与未保存检查生效
    m_canvas = canvas;

    // 左侧使用一个普通 panel 并用 sizer 管理树与属性，使属性面板随容器大小自适应
    wxPanel* leftPanel = new wxPanel(splitter, wxID_ANY);
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);

    MyTreePanel* treePanel = new MyTreePanel(leftPanel);
    PropertyPanel* prop = new PropertyPanel(leftPanel, canvas);

    // 设置属性面板的最小高度，保证在多数窗口下完整显示；同时使用 sizer 控制伸缩
    // 修改：将属性面板最小高度显著增大，元件库保持较小固定高度
    prop->SetMinSize(wxSize(-1, 360)); // 属性面板更大高度
    treePanel->SetMinSize(wxSize(-1, 120)); // 元件库较小高度

    // 将树放上方、属性放下方
    // treePanel 使用固定（比例0）布局以保持较小高度，prop 使用比例1占据剩余并可伸展
    leftSizer->Add(treePanel, 0, wxEXPAND | wxALL, 2); // 固定较小高度
    leftSizer->Add(prop, 1, wxEXPAND | wxALL, 2);      // 占用剩余空间并放大

    leftPanel->SetSizer(leftSizer);

    // 确保 sizer 布局生效，使 Apply 按钮可见
    leftPanel->Layout();
    leftSizer->Layout();
    prop->Layout();

    // 保证左侧面板在默认打开时不会被缩得比属性面板更小，确保 Apply 可见
    // 这里把 leftPanel 的最小高度设置为属性面板高度 + margin（40），并设置宽度为 240
    wxSize propMin = prop->GetMinSize();
    int leftMinW = 240;
    int leftMinH = std::max(200, propMin.y + 40);
    leftPanel->SetMinSize(wxSize(leftMinW, leftMinH));

    // 让画布知道属性面板
    canvas->SetPropertyPanel(prop);

    // 主拆分：左为 leftPanel，右为画布
    splitter->SplitVertically(leftPanel, canvas, 200);

    // 设置拆分器最小面板大小，避免左侧被压得过小导致属性不可见
    splitter->SetMinimumPaneSize(160);

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(topPanel, 0, wxEXPAND);
    mainSizer->Add(splitter, 1, wxEXPAND);
    SetSizer(mainSizer);

    // 强制设置最小窗口高度，确保属性面板始终能完整显示（当用户尝试把窗口缩得过小时仍能得到合理约束）
    int minFrameH = std::max(480, leftMinH + 220);
    SetMinSize(wxSize(700, minFrameH));
    CreateStatusBar();

    Bind(wxEVT_CLOSE_WINDOW, &MyFrame::OnCloseWindow, this);
    Bind(wxEVT_MENU, &MyFrame::OnOpen, this, wxID_NEW);
    Bind(wxEVT_MENU, &MyFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MyFrame::OnCut, this, ID_CUT);
    Bind(wxEVT_MENU, &MyFrame::OnCopy, this, ID_COPY);
    Bind(wxEVT_MENU, &MyFrame::OnAddCircuit, this, ID_PROJECT_ADD_CIRCUIT);
    Bind(wxEVT_MENU, &MyFrame::OnSimEnable, this, ID_SIM_ENABLE);
    Bind(wxEVT_MENU, &MyFrame::OnWindowCascade, this, ID_WINDOW_CASCADE);
    Bind(wxEVT_MENU, &MyFrame::OnHelp, this, ID_HELP_ABOUT);

    Bind(wxEVT_MENU, &MyFrame::OnImportNetlist, this, ID_FILE_OPENRECENT);
    Bind(wxEVT_MENU, &MyFrame::OnExportNetlist, this, ID_FILE_SAVE);

    Bind(wxEVT_TOOL, &MyFrame::OnToolChangeValue, this, ID_TOOL_CHGVALUE);
    Bind(wxEVT_TOOL, &MyFrame::OnToolEditSelect, this, ID_TOOL_EDITSELECT);
    Bind(wxEVT_TOOL, &MyFrame::OnToolEditText, this, ID_TOOL_EDITTXET);
    Bind(wxEVT_TOOL, &MyFrame::OnToolShowSimulation, this, ID_TOOL_SHOWSIMULATION);
}

void MyFrame::OnOpen(wxCommandEvent& event) { wxMessageBox("打开新文件", "File", wxOK | wxICON_INFORMATION); }
void MyFrame::OnExit(wxCommandEvent& event) { Close(true); }
void MyFrame::OnCloseWindow(wxCloseEvent& event) {
    if (m_canvas && m_canvas->IsDirty()) {
        if (!m_canvas->AskSaveIfDirty()) { event.Veto(); return; }
    }
    event.Skip();
}
void MyFrame::OnCut(wxCommandEvent& event) { wxMessageBox("剪切", "Edit", wxOK | wxICON_INFORMATION); }
void MyFrame::OnCopy(wxCommandEvent& event) { wxMessageBox("复制", "Edit", wxOK | wxICON_INFORMATION); }
void MyFrame::OnAddCircuit(wxCommandEvent& event) { wxMessageBox("添加", "Project", wxOK | wxICON_INFORMATION); }
void MyFrame::OnSimEnable(wxCommandEvent& event) { wxMessageBox("仿真启用", "Simulate", wxOK | wxICON_INFORMATION); }
void MyFrame::OnWindowCascade(wxCommandEvent& event) { wxMessageBox("窗口", "Window", wxOK | wxICON_INFORMATION); }
void MyFrame::OnHelp(wxCommandEvent& event) { wxMessageBox("Logisim 帮助", "Help", wxOK | wxICON_INFORMATION); }

void MyFrame::OnExportNetlist(wxCommandEvent& event)
{
    if (!m_canvas) return;
    wxFileDialog dlg(this, "Export netlist", "", "netlist.json", "JSON files (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        std::string path = dlg.GetPath().ToStdString();
        bool ok = m_canvas->ExportNetlist(path); // use ExportNetlist for JSON
        // and also save full project (.node/.net) via SaveToFile if implemented
        if (ok) wxMessageBox("导出成功", "Export", wxOK | wxICON_INFORMATION);
        else wxMessageBox("导出失败", "Export", wxOK | wxICON_ERROR);
    }
}

void MyFrame::OnImportNetlist(wxCommandEvent& event)
{
    if (!m_canvas) return;
    wxFileDialog dlg(this, "Import netlist", "", "", "JSON files (*.json)|*.json", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        std::string path = dlg.GetPath().ToStdString();
        if (m_canvas->ImportNetlist(path)) wxMessageBox("导入成功", "Import", wxOK | wxICON_INFORMATION);
        else wxMessageBox("导入失败", "Import", wxOK | wxICON_ERROR);
    }
}

void MyFrame::OnToolChangeValue(wxCommandEvent& event) {}
void MyFrame::OnToolEditSelect(wxCommandEvent& event) { SetPlacementType("EditSelect"); SetStatusText("Selected tool: Edit selection"); }
void MyFrame::OnToolEditText(wxCommandEvent& event) { SetPlacementType("EditText"); SetStatusText("Selected tool: Edit text"); }
void MyFrame::OnToolShowSimulation(wxCommandEvent& event) { if (!m_canvas) return; m_canvas->ToggleSimulation(); if (m_canvas->IsSimulating()) SetStatusText("Simulation: ON"); else SetStatusText("Simulation: OFF"); }

wxIMPLEMENT_APP(MyApp);
