
#include <wx/artprov.h>
#include<wx/treectrl.h>  
#include<wx/splitter.h>
#include <wx/aui/aui.h>
#include<wx/dcbuffer.h>
#include <wx/panel.h>
#include "ElementDraw.h"
#include<fstream>
#include <nlohmann/json.hpp>
#include "ElementManager.h"
using json = nlohmann::json;

// 菜单和按钮ID
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
    ID_TOOL_ADDORGATE
};

struct ConnectionInfo {
    int aIndex = -1;
    int bIndex = -1;
    int aPin = -1;//连线时用来标识连接的点
    int bPin = -1;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    // 添加转折点信息
    std::vector<wxPoint>转折点; // 存储折线的转折点
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
        tree->AppendItem(child4, "Buffer");
        tree->AppendItem(child4, "NOR");
        tree->AppendItem(child4, "XOR");
        tree->AppendItem(child4, "XNOR");
        tree->AppendItem(child4, "Odd Parity");
        tree->AppendItem(child4, "Even Parity");
        tree->AppendItem(child4, "Controlled Buffer");
        tree->AppendItem(child4, "Controlled Inverter");
        tree->ExpandAll();
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
        wxWindow* top = wxGetTopLevelParent(this);
        if (!top) return;
        MyFrame* mf = dynamic_cast<MyFrame*>(top);
        if (mf) {
            mf->SetPlacementType(sel.ToStdString());
            mf->SetStatusText(wxString("Selected for placement: ") + sel);
        }
    }
};

class PropertyPanel : public wxPanel
{
public:
    PropertyPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        wxStaticText* label = new wxStaticText(this, wxID_ANY, "Properties");
        sizer->Add(label, 0, wxALIGN_CENTER | wxALL, 5);
        SetSizer(sizer);
    }
};

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
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(*wxWHITE);

        LoadElementsAndConnectionsFromFile();

        // 绑定事件（新增键盘事件）
        Bind(wxEVT_LEFT_DOWN, &CanvasPanel::OnLeftDown, this);
        Bind(wxEVT_PAINT, &CanvasPanel::OnPaint, this);
        Bind(wxEVT_SIZE, &CanvasPanel::OnSize, this);
        Bind(wxEVT_MOTION, &CanvasPanel::OnMouseMove, this);
        Bind(wxEVT_LEFT_UP, &CanvasPanel::OnLeftUp, this);
        Bind(wxEVT_RIGHT_DOWN, &CanvasPanel::OnRightDown, this);
        Bind(wxEVT_RIGHT_UP, &CanvasPanel::OnRightUp, this);
        Bind(wxEVT_KEY_DOWN, &CanvasPanel::OnKeyDown, this); // 绑定Delete键事件
    }

    void OnPaint(wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.Clear();

        wxSize sz = GetClientSize();
        if (!m_backValid || m_backBitmap.GetWidth() != sz.x || m_backBitmap.GetHeight() != sz.y) {
            RebuildBackbuffer();
        }

        if (m_backValid) {
            dc.DrawBitmap(m_backBitmap, 0, 0, false);
        }
        else {
            DrawGrid(dc);
        }

        // 绘制拖拽中的元件
        if (m_dragging && m_dragIndex >= 0 && m_dragIndex < (int)m_manager.GetElements().size()) {
            const auto& elements = m_manager.GetElements();
            const auto& e = elements[m_dragIndex];
            DrawElement(dc,
                e.value("type", ""),
                e.value("color", "black"),
                e.value("thickness", 1),
                m_dragCurrent.x,
                m_dragCurrent.y);
        }

        // 绘制连接线
        if (m_connecting) {
            ConnectorHit endHit = HitTestConnector(m_tempLineEnd);
            bool valid = (m_connectStartIsOutput && endHit.hit && !endHit.isOutput && endHit.elemIndex != m_connectStartElem);
            if (valid) {
                wxPen p(wxColour(0, 128, 0), 2);
                dc.SetPen(p);
            }
            else {
                dc.SetPen(*wxBLACK_PEN);
            }

            // 绘制临时折线
            wxPoint startPt = GetConnectorPosition(m_connectStartElem, m_connectStartPin, m_connectStartIsOutput);
            DrawOrthogonalLine(dc, startPt, m_tempLineEnd);
        }

        // 绘制选中边框（在顶层绘制，确保可见）
        int selectedIndex = m_manager.GetSelectedIndex();
        if (selectedIndex != -1) {
            const auto& elements = m_manager.GetElements();
            if (selectedIndex < (int)elements.size()) {
                const auto& el = elements[selectedIndex];
                dc.SetPen(wxPen(wxColour(255, 0, 0), 2, wxPENSTYLE_DOT));
                dc.DrawRectangle(GetElementBounds(el));
            }
        }
    }

    // 处理键盘事件（Delete键删除选中元件）
    void OnKeyDown(wxKeyEvent& event) {
        if (event.GetKeyCode() == WXK_DELETE) {
            m_manager.DeleteSelectedElement();
            SaveElementsAndConnectionsToFile(); // 保存删除操作
            m_backValid = false;
            RebuildBackbuffer();
            Refresh();
        }
        else {
            event.Skip();
        }
    }

    void OnLeftDown(wxMouseEvent& event)
    {
        SetFocus(); // 确保画布获取焦点，能接收键盘事件
        wxPoint pt = event.GetPosition();
        int idx = HitTestElement(pt);

        if (idx >= 0) {
            // 选中元件并准备拖拽
            m_manager.CheckElementSelection(pt.x, pt.y); // 更新选中状态
            m_dragging = true;
            m_dragIndex = idx;
            const auto& elements = m_manager.GetElements();
            m_dragStart = wxPoint(elements[idx]["x"], elements[idx]["y"]);
            m_dragOffset = wxPoint(pt.x - elements[idx]["x"], pt.y - elements[idx]["y"]);
            m_dragCurrent = m_dragStart;
            CaptureMouse();
            Refresh(); // 刷新以显示选中边框
        }
        else {
            // 添加新元件
            wxWindow* top = wxGetTopLevelParent(this);
            MyFrame* mf = dynamic_cast<MyFrame*>(top);
            std::string placeType;
            if (mf) placeType = mf->GetPlacementType();
            if (!placeType.empty()) {
                nlohmann::json newElem;
                newElem["type"] = placeType;
                newElem["color"] = "black";
                newElem["thickness"] = 1;
                newElem["x"] = pt.x;
                newElem["y"] = pt.y;
                newElem["size"] = 1;
                newElem["rotationIndex"] = 0;
                newElem["inputs"] = 0;

                m_manager.AddElement(newElem);
                SaveElementsAndConnectionsToFile();
                m_backValid = false;
                RebuildBackbuffer();
                if (mf) mf->SetPlacementType(std::string());
                Refresh();
            }
        }
        event.Skip();
    }

    void OnSize(wxSizeEvent& event)
    {
        m_backValid = false;
        Refresh();
        event.Skip();
    }

    void OnMouseMove(wxMouseEvent& event)
    {
        wxPoint pt = event.GetPosition();
        if (m_dragging && m_dragIndex >= 0) {
            wxPoint newPos(pt.x - m_dragOffset.x, pt.y - m_dragOffset.y);
            wxRect oldRect = ElementRect(m_dragStart);
            wxRect newRect = ElementRect(newPos);
            m_dragCurrent = newPos;
            RefreshRect(oldRect.Union(newRect));
        }
        else if (m_connecting) {
            m_tempLineEnd = SnapToGrid(pt);
            wxPoint startPt;
            if (m_connectStartElem >= 0 && m_connectStartIsOutput)
                startPt = GetConnectorPosition(m_connectStartElem, m_connectStartPin, true);
            else
                startPt = m_connectStartGrid;

            // 计算需要刷新的区域
            wxRect r = CalculateLineBoundingBox(startPt, m_tempLineEnd);
            r.Inflate(6, 6);
            RefreshRect(r);
        }
        event.Skip();
    }

    void OnLeftUp(wxMouseEvent& event)
    {
        if (m_dragging && m_dragIndex >= 0) {
            // 更新拖拽后的元件位置
            auto& elements = const_cast<std::vector<nlohmann::json>&>(m_manager.GetElements());
            elements[m_dragIndex]["x"] = m_dragCurrent.x;
            elements[m_dragIndex]["y"] = m_dragCurrent.y;
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

            ConnectionInfo c;

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

            // 计算折线的转折点
            c.转折点 = CalculateOrthogonalTurningPoints(wxPoint(c.x1, c.y1), wxPoint(c.x2, c.y2));

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
    ElementManager m_manager; // 使用ElementManager管理元件（替代原m_elements）
    std::vector<ConnectionInfo> m_connections;

    wxBitmap m_backBitmap;
    bool m_backValid;

    bool m_dragging;
    int m_dragIndex;
    wxPoint m_dragOffset;
    wxPoint m_dragStart;
    wxPoint m_dragCurrent;

    bool m_connecting;
    wxPoint m_connectStartGrid;
    int m_connectStartElem;
    bool m_connectStartIsOutput;
    int m_connectStartPin;
    int m_connectEndPin;
    wxPoint m_tempLineEnd;

    const int ElemWidth = 60;
    const int ElemHeight = 40;
    const int ConnectorRadius = 6;

    struct ConnectorHit {
        bool hit = false;
        int elemIndex = -1;
        int pinIndex = -1;
        bool isOutput = false;
    };

    // 获取元件边界（用于选中边框绘制）
    wxRect GetElementBounds(const nlohmann::json& el)const {
        std::string type = el.value("type", "");
        int x = el.value("x", 0);
        int y = el.value("y", 0);
        if (type == "AND") return wxRect(x - 10, y, 70, 40);
        if (type == "OR" || type == "NOR") return wxRect(x, y, 60, 40);
        if (type == "NOT") return wxRect(x - 10, y, 70, 40);
        if (type == "Input Pin" || type == "Output Pin") return wxRect(x - 10, y - 10, 20, 20);
        return wxRect(x, y, 50, 50); // 默认边界
    }

    wxPoint SnapToGrid(const wxPoint& p) const {
        int gx = (p.x + 5) / 10 * 10;
        int gy = (p.y + 5) / 10 * 10;
        return wxPoint(gx, gy);
    }

    wxPoint GetOutputPoint(const nlohmann::json& e) const {
        return wxPoint(e["x"].get<int>() + ElemWidth, e["y"].get<int>() + ElemHeight / 2);
    }

    wxPoint GetInputPoint(const nlohmann::json& e, int pinIndex) const {
        int n = std::max(1, e.value("inputs", 0));
        float step = (float)ElemHeight / (n + 1);
        int py = e["y"].get<int>() + (int)(step * (pinIndex + 1));
        return wxPoint(e["x"].get<int>(), py);
    }

    wxPoint GetConnectorPosition(int elemIndex, int pinIndex, bool isOutput) const {
        const auto& elements = m_manager.GetElements();
        if (elemIndex < 0 || elemIndex >= (int)elements.size()) return wxPoint(0, 0);
        const auto& e = elements[elemIndex];
        if (isOutput) return GetOutputPoint(e);
        return GetInputPoint(e, pinIndex < 0 ? 0 : pinIndex);
    }

    ConnectorHit HitTestConnector(const wxPoint& p) const {
        ConnectorHit res;
        const auto& elements = m_manager.GetElements();
        for (int i = (int)elements.size() - 1; i >= 0; --i) {
            const auto& e = elements[i];
            wxPoint out = GetOutputPoint(e);
            if (DistanceSquared(out, p) <= ConnectorRadius * ConnectorRadius) {
                res.hit = true; res.elemIndex = i; res.pinIndex = 0; res.isOutput = true; return res;
            }
            int n = std::max(1, e.value("inputs", 0));
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
        const auto& elements = m_manager.GetElements();
        for (int i = (int)elements.size() - 1; i >= 0; --i) {
            const auto& e = elements[i];
            wxRect r = GetElementBounds(e);
            if (r.Contains(p)) return i;
        }
        return -1;
    }

    wxPoint ElementCenter(const nlohmann::json& e) const
    {
        return wxPoint(e["x"].get<int>() + ElemWidth / 2, e["y"].get<int>() + ElemHeight / 2);
    }

    wxRect ElementRect(const wxPoint& pos) const
    {
        return wxRect(pos.x, pos.y, ElemWidth, ElemHeight);
    }

    void LoadElementsAndConnectionsFromFile()
    {
        // 清空现有数据
        // 注意：ElementManager没有提供清空方法，这里通过添加空元素+删除的方式间接清空
        while (m_manager.GetSelectedIndex() != -1) {
            m_manager.DeleteSelectedElement();
        }
        m_connections.clear();

        std::ifstream file("Elementlib.json");
        if (!file.is_open()) return;
        try {
            json j;
            file >> j;
            if (j.contains("elements") && j["elements"].is_array()) {
                for (const auto& comp : j["elements"]) {
                    m_manager.AddElement(comp); // 使用ElementManager添加元件
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

                    // 加载转折点
                    if (c.contains("turningPoints") && c["turningPoints"].is_array()) {
                        for (const auto& p : c["turningPoints"]) {
                            ci.转折点.push_back(wxPoint(p[0], p[1]));
                        }
                    }
                    m_connections.push_back(ci);
                }
            }
        }
        catch (...) { /* 忽略解析错误 */ }
        m_backValid = false;
    }

    void SaveElementsAndConnectionsToFile()
    {
        json j;
        j["elements"] = json::array();
        const auto& elements = m_manager.GetElements();
        for (const auto& e : elements) {
            j["elements"].push_back(e); // 直接存储json对象
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

            // 保存转折点
            cj["turningPoints"] = json::array();
            for (const auto& p : c.转折点) {
                cj["turningPoints"].push_back({ p.x, p.y });
            }

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

        // 绘制连接线（正交折线）
        for (const auto& c : m_connections) {
            bool isOutputToInput = (c.aIndex >= 0 && c.bIndex >= 0);
            if (isOutputToInput) {
                wxPen p(wxColour(255, 0, 0), 2);
                mdc.SetPen(p);
            }
            else {
                mdc.SetPen(*wxBLACK_PEN);
            }

            DrawOrthogonalLine(mdc, wxPoint(c.x1, c.y1), wxPoint(c.x2, c.y2), c.转折点);
        }

        // 绘制元件
        const auto& elements = m_manager.GetElements();
        for (const auto& comp : elements) {
            DrawElement(mdc,
                comp.value("type", ""),
                comp.value("color", "black"),
                comp.value("thickness", 1),
                comp.value("x", 0),
                comp.value("y", 0));
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

    // 计算正交折线的转折点（水平-垂直或垂直-水平）
    std::vector<wxPoint> CalculateOrthogonalTurningPoints(const wxPoint& start, const wxPoint& end)
    {
        std::vector<wxPoint> turningPoints;

        // 如果是水平线或垂直线，不需要转折点
        if (start.x == end.x || start.y == end.y) {
            return turningPoints;
        }

        // 计算转折点（先水平后垂直）
        turningPoints.push_back(wxPoint(end.x, start.y));

        return turningPoints;
    }

    // 绘制正交折线
    void DrawOrthogonalLine(wxDC& dc, const wxPoint& start, const wxPoint& end,
        const std::vector<wxPoint>& turningPoints = std::vector<wxPoint>())
    {
        wxPoint current = start;

        // 如果没有提供转折点，则计算它们
        std::vector<wxPoint> points = turningPoints;
        if (points.empty() && !(start.x == end.x || start.y == end.y)) {
            points = CalculateOrthogonalTurningPoints(start, end);
        }

        // 绘制到每个转折点的线段
        for (const auto& pt : points) {
            dc.DrawLine(current.x, current.y, pt.x, pt.y);
            current = pt;
        }

        // 绘制最后一段到终点
        dc.DrawLine(current.x, current.y, end.x, end.y);
    }

    // 计算线条的边界框用于刷新
    wxRect CalculateLineBoundingBox(const wxPoint& start, const wxPoint& end)
    {
        std::vector<wxPoint> points = CalculateOrthogonalTurningPoints(start, end);
        int minX = start.x, maxX = start.x;
        int minY = start.y, maxY = start.y;

        auto updateBounds = [&](const wxPoint& p) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
            };

        updateBounds(end);
        for (const auto& p : points) {
            updateBounds(p);
        }

        return wxRect(minX, minY, maxX - minX, maxY - minY);
    }
};

bool MyApp::OnInit()
{
    try {
        json j;
        j["elements"] = json::array();
        j["connections"] = json::array();
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
    SetSize(800, 600);
    wxMenu* menuFile = new wxMenu;
    menuFile->Append(wxID_NEW, "Open New File");
    menuFile->Append(wxID_EXIT, "Exit");
    menuFile->Append(ID_FILE_OPENRECENT, "OpenRecent");
    menuFile->Append(ID_FILE_SAVE, "Save");

    wxMenu* menuEdit = new wxMenu;
    menuEdit->Append(ID_CUT, "Cut");
    menuEdit->Append(ID_COPY, "Copy");

    wxMenu* menuProject = new wxMenu;
    menuProject->Append(ID_ADD_CIRCUIT, "Add Circuit");

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

    wxToolBar* toolBar = CreateToolBar();
    toolBar->AddTool(ID_TOOL_CHGVALUE, "Change Value", wxArtProvider::GetBitmap(wxART_NEW, wxART_TOOLBAR));
    toolBar->AddTool(ID_TOOL_EDITSELECT, "Edit selection", wxArtProvider::GetBitmap(wxART_CUT, wxART_TOOLBAR));
    toolBar->AddSeparator();
    toolBar->Realize();

    wxSplitterWindow* splitter = new wxSplitterWindow(this, wxID_ANY);
    MyTreePanel* leftPanel = new MyTreePanel(splitter);
    CanvasPanel* rightPanel = new CanvasPanel(splitter); // 使用修改后的CanvasPanel
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
    wxMessageBox("新建文件", "File", wxOK | wxICON_INFORMATION);
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
    wxMessageBox("启用模拟", "Simulate", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnWindowCascade(wxCommandEvent& event)
{
    wxMessageBox("级联", "Window", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnHelp(wxCommandEvent& event)
{
    wxMessageBox("Logisim 帮助", "Help", wxOK | wxICON_INFORMATION);
}

wxIMPLEMENT_APP(MyApp);
