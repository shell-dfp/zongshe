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
using json = nlohmann::json;

// 定义菜单和工具栏ID
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
    ID_TOOL_CHGVALUE,
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
    // 添加转折点信息
    std::vector<wxPoint>转折点; // 存储折线的转折点
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

// 属性面板：控制选中元件的位置和大小
class PropertyPanel : public wxPanel
{
public:
    PropertyPanel(wxWindow* parent, CanvasPanel* canvas = nullptr)
        : wxPanel(parent, wxID_ANY), m_canvas(canvas)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        wxStaticText* label = new wxStaticText(this, wxID_ANY, "Properties");
        sizer->Add(label, 0, wxALIGN_CENTER | wxALL, 5);

        wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
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

        sizer->Add(grid, 0, wxALL | wxEXPAND, 5);

        m_btnApply = new wxButton(this, wxID_ANY, "Apply");
        // 改为 wxEXPAND 让按钮水平占满并更容易在狭小布局下可见
        sizer->Add(m_btnApply, 0, wxEXPAND | wxALL, 5);

        SetSizer(sizer);

        m_btnApply->Bind(wxEVT_BUTTON, &PropertyPanel::OnApply, this);
    }

    // 当 Canvas 选中/更新元件时由 Canvas 调用，填充控件
    void UpdateForElement(const ElementInfo& e)
    {
        m_spinX->SetValue(e.x);
        m_spinY->SetValue(e.y);
        m_spinSize->SetValue(e.size < 1 ? 1 : e.size);
    }

    void SetCanvas(CanvasPanel* c) { m_canvas = c; }

private:
    CanvasPanel* m_canvas;
    wxSpinCtrl* m_spinX;
    wxSpinCtrl* m_spinY;
    wxSpinCtrl* m_spinSize;
    wxButton* m_btnApply;

    void OnApply(wxCommandEvent& evt);
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
        m_connectEndPin(-1),
        m_selectedIndex(-1),
        m_propPanel(nullptr),
        m_dirty(false)
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
    }
    // 判断是否有未保存内容
    bool IsDirty() const { return m_dirty; }

    // 让属性面板和画布互相绑定
    void SetPropertyPanel(PropertyPanel* p) { m_propPanel = p; if (m_propPanel) m_propPanel->SetCanvas(this); }

    int GetSelectedIndex() const { return m_selectedIndex; }

    // 属性面板调用：将属性应用到当前选中元件
    void ApplyPropertiesToSelected(int x, int y, int size)
    {
        if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_elements.size()) return;
        ElementInfo& e = m_elements[m_selectedIndex];
        e.x = x;
        e.y = y;
        e.size = std::max(1, size);
        // 当尺寸改变时，画布元素宽高在 ElementDraw 中使用 BaseElemWidth/Height，
        // CanvasPanel 中用于命中检测的 ElemWidth/ElemHeight 固定，不随 size 自动缩放。
        // 如果需要随 size 缩放，请在这里调整 ElemWidth/ElemHeight 或 HitTest 与绘制策略。
        SaveElementsAndConnectionsToFile();
        m_backValid = false;
        RebuildBackbuffer();
        Refresh();
        if (m_propPanel) m_propPanel->UpdateForElement(e);
    }

    // 退出/关闭前询问保存，返回 true 表示可以继续关闭，false 表示取消关闭
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

        // 拖拽：绘制临时位置（拖拽过程中元素只在临时位置显示）
        if (m_dragging && m_dragIndex >= 0 && m_dragIndex < (int)m_elements.size()) {
            const ElementInfo& e = m_elements[m_dragIndex];
            DrawElement(dc, e.type, e.color, e.thickness, m_dragCurrent.x, m_dragCurrent.y, e.size);
        }

        // 连线：橡皮筋，颜色根据是否为有效（从元件输出到另一个元件输入）动态改变
        if (m_connecting) {
            ConnectorHit endHit = HitTestConnector(m_tempLineEnd);
            bool valid = (m_connectStartIsOutput && endHit.hit && !endHit.isOutput && endHit.elemIndex != m_connectStartElem);
            if (valid) {
                wxPen p(wxColour(0, 128, 0), 2); // 深绿色
                dc.SetPen(p);
            }
            else {
                dc.SetPen(*wxBLACK_PEN);
            }
            wxPoint startPt;
            if (m_connectStartElem >= 0)
                startPt = GetConnectorPosition(m_connectStartElem, m_connectStartPin, true);
            else
                startPt = m_connectStartGrid;
            dc.DrawLine(startPt.x, startPt.y, m_tempLineEnd.x, m_tempLineEnd.y);
        }
    }

    void OnLeftDown(wxMouseEvent& event)
    {
        wxPoint pt = event.GetPosition();
        int idx = HitTestElement(pt);
        if (idx >= 0) {
            // 选中（在开始拖动前），通知属性面板
            m_selectedIndex = idx;
            if (m_propPanel) m_propPanel->UpdateForElement(m_elements[idx]);

            // 开始拖动
            m_dragging = true;
            m_dragIndex = idx;
            m_dragStart = wxPoint(m_elements[idx].x, m_elements[idx].y);
            m_dragOffset = wxPoint(pt.x - m_elements[idx].x, pt.y - m_elements[idx].y);
            m_dragCurrent = m_dragStart;
            m_prevDragCurrent = m_dragCurrent; // 记录初始 prev
            CaptureMouse();
        }
        else {
            wxWindow* top = wxGetTopLevelParent(this);
            MyFrame* mf = dynamic_cast<MyFrame*>(top);
            std::string placeType;
            if (mf) placeType = mf->GetPlacementType();
            if (!placeType.empty()) {
                ElementInfo newElem;
                newElem.type = placeType;
                newElem.color = "black";
                newElem.thickness = 1;
                newElem.x = pt.x;
                newElem.y = pt.y;
                newElem.size = 1;
                newElem.rotationIndex = 0;
                if (placeType == "Buffer") {
                    newElem.inputs = 1; // Buffer 1个输入
                }
                else if (placeType == "Odd Parity") {
                    newElem.inputs = 2; // 奇偶校验至少2个输入
                }
                else if (placeType == "Controlled Buffer" || placeType == "Controlled Inverter") {
                    newElem.inputs = 2; // 受控元件有2个输入（数据+控制）
                }
                else {
                    newElem.inputs = 1; // 默认值
                }
                m_elements.push_back(newElem);
                SaveElementsAndConnectionsToFile();
                m_backValid = false;
                m_dirty = true;
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

            // 使用 prev 与 new 的并集来刷新，确保旧痕迹被清除
            wxRect oldRect = ElementRect(m_prevDragCurrent, m_elements[m_dragIndex].size);
            wxRect newRect = ElementRect(newPos, m_elements[m_dragIndex].size);
            wxRect refreshRect = oldRect.Union(newRect);
            refreshRect.Inflate(4, 4); // 包含笔宽/边界
            m_dragCurrent = newPos;

            // 更新 prev
            m_prevDragCurrent = m_dragCurrent;

            RefreshRect(refreshRect);
        }
        else if (m_connecting) {
            wxPoint oldEnd = m_prevTempLineEnd;
            m_tempLineEnd = SnapToGrid(pt);
            // 起点位置（可能是元素 connector 或 grid）
            wxPoint startPt;
            if (m_connectStartElem >= 0 && m_connectStartIsOutput)
                startPt = GetConnectorPosition(m_connectStartElem, m_connectStartPin, true);
            else
                startPt = m_connectStartGrid;

            wxRect oldRect(startPt, oldEnd);
            wxRect newRect(startPt, m_tempLineEnd);
            wxRect refreshRect = oldRect.Union(newRect);
            refreshRect.Inflate(6, 6); // 包括线宽
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
            SaveElementsAndConnectionsToFile();
            m_backValid = false;
            RebuildBackbuffer();
            if (HasCapture()) ReleaseMouse();
            m_dragging = false;
            m_dragIndex = -1;
            // reset prev
            m_prevDragCurrent = wxPoint(-10000, -10000);

            // 拖动后更新属性面板显示（如果选中仍为该元素）
            if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_elements.size() && m_propPanel) {
                m_propPanel->UpdateForElement(m_elements[m_selectedIndex]);
            }

            // 不立即写盘，标记为未保存
            m_dirty = true;
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
        m_prevTempLineEnd = m_tempLineEnd; // 初始化 prev
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

            // 只在连线两端都粘连到有效元件端口（输出->输入）时保存连线，其他视为无效并丢弃
            if (IsConnectionValid(c)) {
                m_connections.push_back(c);
                SaveElementsAndConnectionsToFile();
            }
            // 否则不保存（无效连线直接消失）

            m_backValid = false;
            RebuildBackbuffer();

            // 重置并刷新整个受影响区域（安全）
            m_prevTempLineEnd = wxPoint(-10000, -10000);
            m_connecting = false;
            m_connectStartElem = -1;
            m_connectStartPin = -1;
            m_connectStartIsOutput = false;
            Refresh();
            if (HasCapture()) ReleaseMouse();

            // 不立即写盘，标记为未保存
            m_dirty = true;
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

    // 导出为网表（JSON 格式）
    bool ExportNetlist(const std::string& filename)
    {
        try {
            json root;
            root["netlist"]["components"] = json::array();
            for (size_t i = 0; i < m_elements.size(); ++i) {
                const auto& e = m_elements[i];
                json comp;
                comp["id"] = (int)i;
                comp["type"] = e.type;
                comp["x"] = e.x;
                comp["y"] = e.y;
                comp["color"] = e.color;
                comp["thickness"] = e.thickness;
                comp["size"] = e.size;
                comp["rotationIndex"] = e.rotationIndex;
                comp["inputs"] = e.inputs;
                root["netlist"]["components"].push_back(comp);
            }

            // nets: 每个 connection 作为一个 net，包含端点列表和转折点
            root["netlist"]["nets"] = json::array();
            for (size_t i = 0; i < m_connections.size(); ++i) {
                const auto& c = m_connections[i];
                json net;
                net["id"] = (int)i;
                net["endpoints"] = json::array();

                json epA;
                epA["compId"] = c.aIndex;
                epA["pin"] = c.aPin;
                epA["pos"] = { c.x1, c.y1 };
                net["endpoints"].push_back(epA);

                json epB;
                epB["compId"] = c.bIndex;
                epB["pin"] = c.bPin;
                epB["pos"] = { c.x2, c.y2 };
                net["endpoints"].push_back(epB);

                // 转折点
                net["turningPoints"] = json::array();
                for (const auto& p : c.转折点) {
                    net["turningPoints"].push_back({ p.x, p.y });
                }

                root["netlist"]["nets"].push_back(net);
            }

            std::ofstream ofs(filename);
            if (!ofs.is_open()) return false;
            ofs << root.dump(4);
            ofs.close();
            return true;
        }
        catch (...) {
            return false;
        }
    }

    // 从网表文件导入（JSON 格式） - 改进版本，容错并支持多种字段布局
    bool ImportNetlist(const std::string& filename)
    {
        std::ifstream ifs(filename);
        if (!ifs.is_open()) return false;

        json root;
        try {
            ifs >> root;
        }
        catch (const std::exception& e) {
            // 解析失败
            wxMessageBox(wxString("读取或解析网表文件失败: ") + e.what(), "Import Error", wxOK | wxICON_ERROR);
            return false;
        }

        // 支持两种布局：
        // 1) { "netlist": { "components": [...], "nets": [...] } }
        // 2) 直接 { "components": [...], "nets": [...] } 或旧的 elements/connections 结构
        const json* nl = nullptr;
        if (root.contains("netlist") && root["netlist"].is_object()) {
            nl = &root["netlist"];
        }
        else {
            nl = &root;
        }

        // 清空当前数据
        m_elements.clear();
        m_connections.clear();

        // 读取 components（兼容 "components" 和旧的 "elements"）
        bool usedIdIndexing = false;
        std::map<int, ElementInfo> compById;
        int maxId = -1;

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

                if (comp.contains("id")) {
                    int id = comp["id"].get<int>();
                    usedIdIndexing = true;
                    compById[id] = e;
                    if (id > maxId) maxId = id;
                }
                else {
                    // 无 id 的情况下先 push_back 到临时 vector
                    m_elements.push_back(e);
                }
            }

            // 如果使用了 id，则把 map 转为 vector，保留按照 id 的索引位置
            if (usedIdIndexing) {
                m_elements.assign(maxId + 1, ElementInfo());
                for (const auto& kv : compById) {
                    if (kv.first >= 0 && kv.first < (int)m_elements.size())
                        m_elements[kv.first] = kv.second;
                }
            }
        }
        else if (nl->contains("elements") && (*nl)["elements"].is_array()) {
            // 兼容旧版 Elementlib.json 的 "elements"
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
                m_elements.push_back(e);
            }
        }

        // 读取 nets -> 转换为 connections
        // 支持 "nets"（每个 net 含 endpoints）或旧的 "connections"
        if (nl->contains("nets") && (*nl)["nets"].is_array()) {
            for (const auto& net : (*nl)["nets"]) {
                if (!net.contains("endpoints") || !net["endpoints"].is_array()) continue;

                // 收集所有端点
                std::vector<json> endpoints;
                for (const auto& ep : net["endpoints"]) endpoints.push_back(ep);

                // 两端点 -> 直接一条连接
                if (endpoints.size() == 2) {
                    ConnectionInfo c;
                    const auto& a = endpoints[0];
                    const auto& b = endpoints[1];

                    c.aIndex = a.value("compId", -1);
                    c.aPin = a.value("pin", -1);
                    if (a.contains("pos") && a["pos"].is_array() && a["pos"].size() == 2) {
                        c.x1 = a["pos"][0].get<int>();
                        c.y1 = a["pos"][1].get<int>();
                    }

                    c.bIndex = b.value("compId", -1);
                    c.bPin = b.value("pin", -1);
                    if (b.contains("pos") && b["pos"].is_array() && b["pos"].size() == 2) {
                        c.x2 = b["pos"][0].get<int>();
                        c.y2 = b["pos"][1].get<int>();
                    }

                    if (net.contains("turningPoints") && net["turningPoints"].is_array()) {
                        for (const auto& p : net["turningPoints"]) {
                            if (p.is_array() && p.size() == 2)
                                c.转折点.push_back(wxPoint(p[0].get<int>(), p[1].get<int>()));
                        }
                    }

                    m_connections.push_back(c);
                }
                // 多端点 -> star 拆分成多条连接（第一个为中心）
                else if (endpoints.size() > 2) {
                    const auto& center = endpoints[0];
                    for (size_t k = 1; k < endpoints.size(); ++k) {
                        const auto& other = endpoints[k];
                        ConnectionInfo c;
                        c.aIndex = center.value("compId", -1);
                        c.aPin = center.value("pin", -1);
                        if (center.contains("pos") && center["pos"].is_array() && center["pos"].size() == 2) {
                            c.x1 = center["pos"][0].get<int>();
                            c.y1 = center["pos"][1].get<int>();
                        }

                        c.bIndex = other.value("compId", -1);
                        c.bPin = other.value("pin", -1);
                        if (other.contains("pos") && other["pos"].is_array() && other["pos"].size() == 2) {
                            c.x2 = other["pos"][0].get<int>();
                            c.y2 = other["pos"][1].get<int>();
                        }
                        m_connections.push_back(c);
                    }
                }
            }
        }
        else if (nl->contains("connections") && (*nl)["connections"].is_array()) {
            // 兼容旧的 connections 格式
            for (const auto& cjs : (*nl)["connections"]) {
                ConnectionInfo c;
                c.aIndex = cjs.value("a", -1);
                c.aPin = cjs.value("aPin", -1);
                c.bIndex = cjs.value("b", -1);
                c.bPin = cjs.value("bPin", -1);
                c.x1 = cjs.value("x1", 0);
                c.y1 = cjs.value("y1", 0);
                c.x2 = cjs.value("x2", 0);
                c.y2 = cjs.value("y2", 0);

                if (cjs.contains("turningPoints") && cjs["turningPoints"].is_array()) {
                    for (const auto& p : cjs["turningPoints"]) {
                        if (p.is_array() && p.size() == 2)
                            c.转折点.push_back(wxPoint(p[0].get<int>(), p[1].get<int>()));
                    }
                }
                m_connections.push_back(c);
            }
        }

        // 导入后刷新画布并持久化到默认 Elementlib.json（失败不阻止导入结果显示）
        bool saved = SaveElementsAndConnectionsToFile(); // 使用默认路径
        if (!saved) {
            wxMessageBox("导入成功，但保存到 Elementlib.json 失败（可能没有写权限）。", "Import", wxOK | wxICON_WARNING);
        }

        m_backValid = false;
        RebuildBackbuffer();
        Refresh();
        return true;
    }

    // 导出 BookShelf 格式：.node 和 .net 文件
    bool ExportBookShelf(const std::string& nodeFile, const std::string& netFile)
    {
        // 统计并为外部终端（连接到画布但未与元件相连的端点）分配名称 ext0, ext1, ...
        try {
            std::map<std::pair<int, int>, std::string> extMap;
            int extCounter = 0;
            for (const auto& c : m_connections) {
                if (c.aIndex < 0) {
                    std::pair<int, int> key(c.x1, c.y1);
                    if (extMap.find(key) == extMap.end()) {
                        extMap[key] = "ext" + std::to_string(extCounter++);
                    }
                }
                if (c.bIndex < 0) {
                    std::pair<int, int> key(c.x2, c.y2);
                    if (extMap.find(key) == extMap.end()) {
                        extMap[key] = "ext" + std::to_string(extCounter++);
                    }
                }
            }

            // NODE 文件
            {
                std::ofstream nodeofs(nodeFile);
                if (!nodeofs.is_open()) return false;

                nodeofs << "UCLA nodes 1.0\n";
                // NumNodes 包含常规模块 + 外部终端
                size_t numNodes = m_elements.size() + extMap.size();
                nodeofs << "NumNodes : " << numNodes << "\n";
                nodeofs << "NumTerminals : " << extMap.size() << "\n";

                // 列出常规模块 comp<i> width height
                for (size_t i = 0; i < m_elements.size(); ++i) {
                    const auto& e = m_elements[i];
                    int w = std::max(1, (int)std::round(60 * e.size));
                    int h = std::max(1, (int)std::round(40 * e.size));
                    nodeofs << "comp" << i << " " << w << " " << h << "\n";
                }

                // 列出外部终端：使用默认宽高 1，并标注 terminal
                for (const auto& kv : extMap) {
                    const std::string& name = kv.second;
                    nodeofs << name << " " << 1 << " " << 1 << " terminal\n";
                }

                nodeofs.close();
            }

            // NET 文件
            {
                std::ofstream netofs(netFile);
                if (!netofs.is_open()) return false;

                netofs << "UCLA nets 1.0\n";
                netofs << "NumNets : " << m_connections.size() << "\n";

                // 计算总引脚数（每条 connection 做为一个 net，当前实现为 2 引脚）
                size_t totalPins = 0;
                for (const auto& c : m_connections) {
                    // 如果后来支持多端点 net，可在此统计实际度数
                    totalPins += 2;
                }
                netofs << "NumPins : " << totalPins << "\n";

                // 每个 net 的格式参考图片：NetDegree : <deg> n<i> 然后每行输出 " <node> <I|O> : <x> <y>"
                netofs.setf(std::ios::fixed);
                netofs << std::setprecision(6);

                for (size_t i = 0; i < m_connections.size(); ++i) {
                    const auto& c = m_connections[i];
                    // 当前均为 degree 2（A 与 B）
                    int degree = 2;
                    netofs << "NetDegree : " << degree << " n" << i << "\n";

                    // 端点 A
                    {
                        std::string nodeName;
                        double x = (double)c.x1;
                        double y = (double)c.y1;
                        if (c.aIndex >= 0 && c.aIndex < (int)m_elements.size()) {
                            nodeName = "comp" + std::to_string(c.aIndex);
                        }
                        else {
                            auto it = extMap.find(std::make_pair(c.x1, c.y1));
                            if (it != extMap.end()) nodeName = it->second;
                            else nodeName = "ext_unknown";
                        }
                        // A 端我们按约定用 O（输出），但实际场景可根据元件类型进一步区分
                        netofs << " " << nodeName << " O : " << x << " " << y << "\n";
                    }

                    // 端点 B
                    {
                        std::string nodeName;
                        double x = (double)c.x2;
                        double y = (double)c.y2;
                        if (c.bIndex >= 0 && c.bIndex < (int)m_elements.size()) {
                            nodeName = "comp" + std::to_string(c.bIndex);
                        }
                        else {
                            auto it = extMap.find(std::make_pair(c.x2, c.y2));
                            if (it != extMap.end()) nodeName = it->second;
                            else nodeName = "ext_unknown";
                        }
                        // B 端按约定用 I（输入）
                        netofs << " " << nodeName << " I : " << x << " " << y << "\n";
                    }
                }

                netofs.close();
            }

            return true;
        }
        catch (...) {
            return false;
        }
    }

    // 新增：保存当前元素和连接到指定文件（并自动导出.node / .net）
        bool SaveToFile(const std::string & filename)
    {
        // 直接调用已有的 SaveElementsAndConnectionsToFile
        return SaveElementsAndConnectionsToFile(filename);
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
    wxPoint m_prevDragCurrent; // 上一次临时位置（用于刷新并集）

    // 连线状态
    bool m_connecting;
    wxPoint m_connectStartGrid; // start snapped grid point
    int m_connectStartElem; // optional element index if start attached to element output
    bool m_connectStartIsOutput;
    int m_connectStartPin; // pin index at start (for multiple outputs)
    int m_connectEndPin;
    wxPoint m_tempLineEnd;
    wxPoint m_prevTempLineEnd; // 上一次橡皮筋终点

    // 选中
    int m_selectedIndex;
    PropertyPanel* m_propPanel;

    // 未保存标志
    bool m_dirty;

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

    // ---------- 新增/修改的辅助方法 ----------
    // 判断连接是否合法（两端均粘连到有效元件端口且不是连接到同一元件的同端）
    bool IsConnectionValid(const ConnectionInfo& c) const {
        // 要求两端都绑定到元素索引
        if (c.aIndex < 0 || c.bIndex < 0) return false;
        if (c.aIndex >= (int)m_elements.size() || c.bIndex >= (int)m_elements.size()) return false;
        // 不允许连接到同一元件（可按需放宽）
        if (c.aIndex == c.bIndex) return false;
        // 检查 aPin / bPin 是否在合法范围（a 为输出，b 为输入）
        const ElementInfo& aElem = m_elements[c.aIndex];
        const ElementInfo& bElem = m_elements[c.bIndex];
        int bInputs = std::max(1, bElem.inputs);
        if (c.bPin < 0 || c.bPin >= bInputs) return false;
        // aPin 通常为 0（单输出），允许 -1 或 0
        if (c.aPin < -1 || c.aPin > 0) return false;
        return true;
    }

    // 清理当前连接数组，删除不合法的连接（在加载或保存后调用）
    void CleanConnections() {
        std::vector<ConnectionInfo> keep;
        keep.reserve(m_connections.size());
        for (const auto& c : m_connections) {
            if (IsConnectionValid(c)) keep.push_back(c);
        }
        if (keep.size() != m_connections.size()) {
            m_connections.swap(keep);
        }
    }

    // 以下 helper 与原来相同（省略细节保持一致）——保留你现有实现
    wxPoint SnapToGrid(const wxPoint& p) const {
        int gx = (p.x + 5) / 10 * 10;
        int gy = (p.y + 5) / 10 * 10;
        return wxPoint(gx, gy);
    }
    wxPoint GetOutputPoint(const ElementInfo& e) const {
        int sz = std::max(1, e.size);
        int w = BaseElemWidth * sz;
        int h = BaseElemHeight * sz;
        return wxPoint(e.x + w, e.y + h / 2);
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
        if (isOutput) return GetOutputPoint(e);
        return GetInputPoint(e, pinIndex < 0 ? 0 : pinIndex);
    }
    ConnectorHit HitTestConnector(const wxPoint& p) const {
        ConnectorHit res;
        for (int i = (int)m_elements.size() - 1; i >= 0; --i) {
            const ElementInfo& e = m_elements[i];
            wxPoint out = GetOutputPoint(e);
            if (DistanceSquared(out, p) <= ConnectorRadius * ConnectorRadius) { res.hit = true; res.elemIndex = i; res.pinIndex = 0; res.isOutput = true; return res; }
            int n = std::max(1, e.inputs);
            for (int pin = 0; pin < n; ++pin) {
                wxPoint in = GetInputPoint(e, pin);
                if (DistanceSquared(in, p) <= ConnectorRadius * ConnectorRadius) { res.hit = true; res.elemIndex = i; res.pinIndex = pin; res.isOutput = false; return res; }
            }
        }
        return res;
    }
    static int DistanceSquared(const wxPoint& a, const wxPoint& b) { int dx = a.x - b.x; int dy = a.y - b.y; return dx * dx + dy * dy; }
    int HitTestElement(const wxPoint& p) const {
        // 使用元素的 size 来计算矩形，避免命中检测与显示/缩放不一致
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
    // 新增：按 size 返回元素矩形（用于拖拽刷新等）
    wxRect ElementRect(const wxPoint& pos, int size) const { int sz = std::max(1, size); return wxRect(pos.x, pos.y, BaseElemWidth * sz, BaseElemHeight * sz); }
    // 保留兼容单参版本（使用默认基础大小）
    wxRect ElementRect(const wxPoint& pos) const { return wxRect(pos.x, pos.y, BaseElemWidth, BaseElemHeight); }

    void LoadElementsAndConnectionsFromFile()
    {
        m_elements.clear();
        m_connections.clear();
        std::ifstream file("Elementlib.json");
        if (!file.is_open()) { m_dirty = false; return; }
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
        catch (...) { /* ignore parse errors */ }

        // 载入后清理无效连线（例如未绑定到两端端口的自由连线）
        CleanConnections();

        // 从文件加载后视为已保存状态
        m_dirty = false;
        m_backValid = false;
    }

    bool SaveElementsAndConnectionsToFile(const std::string& filename = "Elementlib.json")
    {
        // 在保存前，若连线绑定到了某个元件索引，更新该连线的坐标以便文件中也保存最新位置
        for (auto& c : m_connections) {
            if (c.aIndex >= 0 && c.aIndex < (int)m_elements.size()) {
                wxPoint p = GetConnectorPosition(c.aIndex, c.aPin, true);
                c.x1 = p.x; c.y1 = p.y;
            }
            if (c.bIndex >= 0 && c.bIndex < (int)m_elements.size()) {
                wxPoint p = GetConnectorPosition(c.bIndex, c.bPin, false);
                c.x2 = p.x; c.y2 = p.y;
            }
        }

        // 保存前也清理一次，避免把无效连线写入文件
        CleanConnections();
        try{
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

                // 保存转折点
                cj["turningPoints"] = json::array();
                for (const auto& p : c.转折点) {
                    cj["turningPoints"].push_back({ p.x, p.y });
                }
                j["connections"].push_back(cj);
            }
            std::ofstream ofs(filename);
            if (!ofs.is_open()) return false;
            ofs << j.dump(4);
            ofs.close();

            // 自动保存 .node 和 .net 文件
            std::string base = filename;
            size_t pos = base.find_last_of('.');
            if (pos != std::string::npos) base = base.substr(0, pos);
            std::string nodeFile = base + ".node";
            std::string netFile = base + ".net";
            bool bsOk = ExportBookShelf(nodeFile, netFile);

            // 成功写入后清除未保存标志
            m_dirty = false;
            return bsOk;
        }
        catch (...) {
            // 保持 m_dirty 原样（写入失败仍为未保存）
            return false;
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

        // 绘制连接线（在元素下方）
        for (const auto& c : m_connections) {
            // 计算当前应该绘制的端点：如果连线绑定到了某个元件索引，则动态计算该端点的位置
            wxPoint p1(c.x1, c.y1);
            wxPoint p2(c.x2, c.y2);
            if (c.aIndex >= 0 && c.aIndex < (int)m_elements.size()) {
                p1 = GetConnectorPosition(c.aIndex, c.aPin, true);
            }
            if (c.bIndex >= 0 && c.bIndex < (int)m_elements.size()) {
                p2 = GetConnectorPosition(c.bIndex, c.bPin, false);
            }

            bool isOutputToInput = (c.aIndex >= 0 && c.bIndex >= 0);
            if (isOutputToInput) {
                wxPen p(wxColour(255, 0, 0), 2); // 红色
                mdc.SetPen(p);
            }
            else {
                mdc.SetPen(*wxBLACK_PEN);
            }
            mdc.DrawLine(p1.x, p1.y, p2.x, p2.y);
        }

        // 绘制元素（ElementDraw.cpp 内部会绘制小横线）
        for (const auto& comp : m_elements) {
            // 传入 comp.size 以匹配新的 DrawElement 签名
            DrawElement(mdc, comp.type, comp.color, comp.thickness, comp.x, comp.y, comp.size);
        }

        // 在元素上显现端点（放在元素之后以保证可见）
        const int pinRadius = 4;
        for (int i = 0; i < (int)m_elements.size(); ++i) {
            const ElementInfo& e = m_elements[i];

            // 输出端点（单个）
            wxPoint outPt = GetOutputPoint(e);
            bool outConnected = false;
            for (const auto& c : m_connections) {
                if (c.aIndex == i) { outConnected = true; break; }
            }
            wxColour outColor = outConnected ? wxColour(0, 128, 0) : wxColour(30, 144, 255); // 连接 -> 绿, 否则蓝
            mdc.SetBrush(wxBrush(outColor));
            mdc.SetPen(wxPen(outColor, 1));
            mdc.DrawCircle(outPt.x, outPt.y, pinRadius);

            // 输入端点（根据 e.inputs 数量绘制）
            int nInputs = std::max(1, e.inputs);
            for (int pin = 0; pin < nInputs; ++pin) {
                wxPoint inPt = GetInputPoint(e, pin);
                bool inConnected = false;
                for (const auto& c : m_connections) {
                    if (c.bIndex == i && c.bPin == pin) { inConnected = true; break; }
                }
                wxColour inColor = inConnected ? wxColour(0, 128, 0) : wxColour(30, 144, 255);
                mdc.SetBrush(wxBrush(inColor));
                mdc.SetPen(wxPen(inColor, 1));
                mdc.DrawCircle(inPt.x, inPt.y, pinRadius);
            }
        }

        mdc.SelectObject(wxNullBitmap);
        m_backValid = true;
    }

    void DrawGrid(wxDC& dc)
    {
        dc.SetPen(*wxLIGHT_GREY_PEN);
        int height, width;
        GetClientSize(&width, &height);
        for (int i = 0; i < width; i += 10) {
            for (int j = 0; j < height; j += 10) {
                dc.DrawPoint(i, j);
            }
        }
    }
};


// PropertyPanel::OnApply 的实现（放在 CanvasPanel 定义之后以便访问 CanvasPanel）
void PropertyPanel::OnApply(wxCommandEvent& evt)
{
    if (!m_canvas) return;
    int sel = m_canvas->GetSelectedIndex();
    if (sel < 0) {
        wxMessageBox("没有选中的元件。", "Info", wxOK | wxICON_INFORMATION);
        return;
    }
    int x = m_spinX->GetValue();
    int y = m_spinY->GetValue();
    int size = m_spinSize->GetValue();
    m_canvas->ApplyPropertiesToSelected(x, y, size);
}


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
    wxInitAllImageHandlers();
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
    menuFile->Append(wxID_NEW, "New         Crtl+N");
    menuFile->Append(wxID_EXIT, "Exit");
    menuFile->Append(ID_FILE_OPENRECENT, "Import Netlist...");
    menuFile->Append(ID_FILE_SAVE, "Export Netlist...");


    // Edit 
    wxMenu* menuEdit = new wxMenu;
    menuEdit->Append(ID_CUT, "Cut");
    menuEdit->Append(ID_COPY, "Copy");

    // Project 
    wxMenu* menuProject = new wxMenu;
    menuProject->Append(ID_PROJECT_ADD_CIRCUIT, "Add Circuit");

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


    wxString exePath = wxStandardPaths::Get().GetExecutablePath();
    wxFileName exeFn(exePath);
    wxString resDir = exeFn.GetPath();
    wxString imgDir = wxFileName(resDir, "image").GetFullPath();

    // 辅助加载函数（按目标尺寸缩放并返回 bitmap；只用作图标，不显示文字）
    wxSize toolSize(20, 20); // 小图标尺寸
    auto LoadBitmapSafe = [&](const wxString& relName, const wxSize& size) -> wxBitmap {
        wxString full = wxFileName(imgDir, relName).GetFullPath();
        if (!wxFileExists(full)) {
            wxLogWarning("Toolbar image not found: %s", full);
            return wxArtProvider::GetBitmap(wxART_MISSING_IMAGE, wxART_TOOLBAR);
        }
        wxImage img;
        if (!img.LoadFile(full)) {
            wxLogWarning("Failed to load image file: %s", full);
            return wxArtProvider::GetBitmap(wxART_MISSING_IMAGE, wxART_TOOLBAR);
        }
        if (size.x > 0 && size.y > 0) {
            img = img.Rescale(size.x, size.y, wxIMAGE_QUALITY_HIGH);
        }
        wxBitmap bmp(img);
        if (!bmp.IsOk()) {
            wxLogWarning("Bitmap invalid after load: %s", full);
            return wxArtProvider::GetBitmap(wxART_MISSING_IMAGE, wxART_TOOLBAR);
        }
        return bmp;
        };


    wxPanel* topPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    wxBoxSizer* topPanelSizer = new wxBoxSizer(wxVERTICAL);
    topPanelSizer->SetMinSize(wxSize(-1, 0));
    topPanel->SetBackgroundColour(GetBackgroundColour());

    wxToolBar* topBar1 = new wxToolBar(topPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER);
    topBar1->SetToolBitmapSize(toolSize);
    topBar1->SetMargins(0, 0);
    topBar1->SetToolPacking(4);
    topBar1->SetToolSeparation(6);
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


    wxToolBar* topBar2 = new wxToolBar(topPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER);
    topBar2->SetToolBitmapSize(toolSize);
    topBar2->SetMargins(0, 0);
    topBar2->SetToolPacking(4);
    topBar2->SetToolSeparation(6);
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
    prop->SetMinSize(wxSize(-1, 140)); // 可根据需要调整最小高度

    // 将树放上方、属性放下方，树占用剩余空间，属性固定显示但可随高度变化
    leftSizer->Add(treePanel, 1, wxEXPAND | wxALL, 2);
    leftSizer->Add(prop, 0, wxEXPAND | wxALL, 2);

    leftPanel->SetSizer(leftSizer);
    // 保证左侧面板不会被缩得比属性面板更小，确保 Apply 可见
    leftPanel->SetMinSize(wxSize(200, 160));

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
    // --- end 工具栏替换 ---

    CreateStatusBar();

    // 绑定关闭窗口事件以在有未保存更改时提示保存
    Bind(wxEVT_CLOSE_WINDOW, &MyFrame::OnCloseWindow, this);
    Bind(wxEVT_MENU, &MyFrame::OnOpen, this, wxID_NEW);
    Bind(wxEVT_MENU, &MyFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MyFrame::OnCut, this, ID_CUT);
    Bind(wxEVT_MENU, &MyFrame::OnCopy, this, ID_COPY);
    Bind(wxEVT_MENU, &MyFrame::OnAddCircuit, this, ID_PROJECT_ADD_CIRCUIT);
    Bind(wxEVT_MENU, &MyFrame::OnSimEnable, this, ID_SIM_ENABLE);
    Bind(wxEVT_MENU, &MyFrame::OnWindowCascade, this, ID_WINDOW_CASCADE);
    Bind(wxEVT_MENU, &MyFrame::OnHelp, this, ID_HELP_ABOUT);

    // 绑定导入/导出网表
    Bind(wxEVT_MENU, &MyFrame::OnImportNetlist, this, ID_FILE_OPENRECENT);
    Bind(wxEVT_MENU, &MyFrame::OnExportNetlist, this, ID_FILE_SAVE);

    // 绑定事件处理（在 MyFrame 类中添加对应的成员函数）
    Bind(wxEVT_TOOL, &MyFrame::OnToolChangeValue, this, ID_TOOL_CHGVALUE);
    Bind(wxEVT_TOOL, &MyFrame::OnToolEditSelect, this, ID_TOOL_EDITSELECT);
    Bind(wxEVT_TOOL, &MyFrame::OnToolEditText, this, ID_TOOL_EDITTXET);
    // ... 继续绑定其他工具
}

void MyFrame::OnOpen(wxCommandEvent& event)
{
    wxMessageBox("打开新文件", "File", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void MyFrame::OnCloseWindow(wxCloseEvent& event)
{
    if (m_canvas && m_canvas->IsDirty()) {
        if (!m_canvas->AskSaveIfDirty()) {
            // 用户选择取消，阻止窗口关闭
            event.Veto();
            return;
        }
    }
    // 允许关闭（默认处理）
    event.Skip();
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

void MyFrame::OnExportNetlist(wxCommandEvent& event)
{
    if (!m_canvas) return;
    wxFileDialog dlg(this, "Export netlist", "", "netlist.json", "JSON files (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        std::string path = dlg.GetPath().ToStdString();

        // 使用 CanvasPanel 的 SaveToFile 做完整保存（包含 .node/.net 的自动导出）
        bool ok = m_canvas->SaveToFile(path);

        // BookShelf 文件名自动生成（与 JSON 同目录）
        std::string base = path;
        size_t pos = base.find_last_of('.');
        if (pos != std::string::npos) base = base.substr(0, pos);
        std::string nodeFile = base + ".node";
        std::string netFile = base + ".net";

        // 检查文件是否真实存在（诊断）
        bool nodeExists = wxFileExists(wxString(nodeFile));
        bool netExists = wxFileExists(wxString(netFile));

        if (ok && nodeExists && netExists) {
            wxString msg = wxString("导出成功（含 BookShelf 格式）\nJSON: ") + wxString(path) +
                wxString("\nNODE: ") + wxString(nodeFile) +
                wxString("\nNET: ") + wxString(netFile);
            wxMessageBox(msg, "Export", wxOK | wxICON_INFORMATION);
        }
        else {
            wxString err = wxString("导出时发生问题：\n");
            if (!ok) err += wxString("- 保存失败: ") + wxString(path) + "\n";
            if (!nodeExists) err += wxString("- 未找到 .node 文件: ") + wxString(nodeFile) + "\n";
            if (!netExists) err += wxString("- 未找到 .net 文件: ") + wxString(netFile) + "\n";
            err += wxString("\n请检查路径、权限或路径中是否包含非 ASCII 字符（在部分环境可能导致写入失败）。");
            wxMessageBox(err, "Export Error", wxOK | wxICON_ERROR);
        }
    }
}

void MyFrame::OnImportNetlist(wxCommandEvent& event)
{
    if (!m_canvas) return;
    wxFileDialog dlg(this, "Import netlist", "", "", "JSON files (*.json)|*.json", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        std::string path = dlg.GetPath().ToStdString();
        if (m_canvas->ImportNetlist(path)) {
            wxMessageBox("导入成功", "Import", wxOK | wxICON_INFORMATION);
        }
        else {
            wxMessageBox("导入失败", "Import", wxOK | wxICON_ERROR);
        }
    }
}

void MyFrame::OnToolChangeValue(wxCommandEvent& event)
{
}

void MyFrame::OnToolEditSelect(wxCommandEvent& event)
{
    SetPlacementType("EditSelect");
    SetStatusText("Selected tool: Edit selection");
}

void MyFrame::OnToolEditText(wxCommandEvent& event)
{
    SetPlacementType("EditText");
    SetStatusText("Selected tool: Edit text");
}

wxIMPLEMENT_APP(MyApp);
