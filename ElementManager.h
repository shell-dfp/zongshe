#pragma once
#ifndef ELEMENTMANAGER_H
#define ELEMENTMANAGER_H

#include <wx/wx.h>
#include <nlohmann/json.hpp>
#include <vector>

class ElementManager {
private:
    std::vector<nlohmann::json> m_elements; // 存储所有元件
    int m_selectedIndex; // 选中的元件索引（-1表示无选中）

public:
    ElementManager() : m_selectedIndex(-1) {}

    // 添加元件
    void AddElement(const nlohmann::json& element) {
        m_elements.push_back(element);
    }

    // 删除选中的元件
    void DeleteSelectedElement() {
        if (m_selectedIndex != -1 && m_selectedIndex < (int)m_elements.size()) {
            m_elements.erase(m_elements.begin() + m_selectedIndex);
            m_selectedIndex = -1; // 清除选中状态
        }
    }

    // 检查点是否在元件范围内（用于选择元件）
    void CheckElementSelection(int x, int y) {
        m_selectedIndex = -1; // 先重置选中状态
        // 从后往前检查（后添加的元件在上方）
        for (int i = m_elements.size() - 1; i >= 0; --i) {
            const auto& el = m_elements[i];
            if (IsPointInElement(el, x, y)) {
                m_selectedIndex = i;
                break;
            }
        }
    }

    // 获取所有元件
    const std::vector<nlohmann::json>& GetElements() const {
        return m_elements;
    }

    // 获取选中的元件索引
    int GetSelectedIndex() const {
        return m_selectedIndex;
    }

private:
    // 判断点是否在元件范围内（根据元件类型判断边界）
    bool IsPointInElement(const nlohmann::json& el, int x, int y) {
        std::string type = el.value("type", "");
        int elX = el.value("x", 0);
        int elY = el.value("y", 0);
        int size = el.value("size", 1);

        // 根据不同元件类型计算边界（参考ElementDraw.cpp的绘制范围）
        if (type == "AND") {
            return x >= elX - 10 && x <= elX + 60 &&
                y >= elY && y <= elY + 40;
        }
        else if (type == "OR" || type == "NOR") {
            return x >= elX && x <= elX + 60 &&
                y >= elY && y <= elY + 40;
        }
        else if (type == "NOT") {
            return x >= elX - 10 && x <= elX + 60 &&
                y >= elY && y <= elY + 40;
        }
        else if (type == "NAND") {
            return x >= elX - 10 && x <= elX + 40 &&
                y >= elY && y <= elY + 50;
        }
        else if (type == "XOR" || type == "XNOR") {
            return x >= elX - 10 && x <= elX + 70 &&
                y >= elY && y <= elY + 40;
        }
        else if (type == "Buffer") {
            return x >= elX - 10 && x <= elX + 40 &&
                y >= elY && y <= elY + 20;
        }
        else if (type == "Odd Parity" || type == "Even Parity") {
            return x >= elX - 10 && x <= elX + 50 &&
                y >= elY && y <= elY + 40;
        }
        else if (type == "Controlled Buffer" || type == "Controlled Inverter") {
            return x >= elX - 10 && x <= elX + 40 &&
                y >= elY && y <= elY + 40;
        }
        else if (type == "Input Pin" || type == "Output Pin") {
            return x >= elX - 10 && x <= elX + 10 &&
                y >= elY - 10 && y <= elY + 10;
        }
        // 其他未明确的元件类型使用默认边界
        return x >= elX - 10 && x <= elX + 60 &&
            y >= elY - 10 && y <= elY + 60;
    }
       
};

// 绘图面板类（集成事件处理）
class ElementCanvas : public wxPanel {
private:
    ElementManager m_manager;

public:
    ElementCanvas(wxWindow* parent) : wxPanel(parent) {
        // 注册事件
        Bind(wxEVT_PAINT, &ElementCanvas::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &ElementCanvas::OnMouseLeftDown, this);
        Bind(wxEVT_KEY_DOWN, &ElementCanvas::OnKeyDown, this);
        SetFocus(); // 确保能接收键盘事件
    }

    // 添加元件（供外部调用）
    void AddElement(const nlohmann::json& element) {
        m_manager.AddElement(element);
        Refresh();
    }

private:
    // 绘制所有元件
    void OnPaint(wxPaintEvent& event) {
        wxPaintDC dc(this);
        const auto& elements = m_manager.GetElements();
        for (size_t i = 0; i < elements.size(); ++i) {
            const auto& el = elements[i];
            // 绘制元件
            DrawElement(dc,
                el.value("type", ""),
                el.value("color", "#000000"),
                el.value("thickness", 1),
                el.value("x", 0),
                el.value("y", 0));
            // 绘制选中状态（边框）
            if (i == (size_t)m_manager.GetSelectedIndex()) {
                dc.SetPen(wxPen(wxColour(255, 0, 0), 2, wxPENSTYLE_DOT)); // 红色虚线边框
                dc.DrawRectangle(GetElementBounds(el)); // 绘制选中边框
            }
        }
    }

    // 鼠标点击选择元件
    void OnMouseLeftDown(wxMouseEvent& event) {
        SetFocus(); // 获取焦点以接收键盘事件
        wxPoint pos = event.GetPosition();
        m_manager.CheckElementSelection(pos.x, pos.y);
        Refresh(); // 重绘以显示选中状态
    }

    // 键盘事件（Delete键删除）
    void OnKeyDown(wxKeyEvent& event) {
        if (event.GetKeyCode() == WXK_DELETE) {
            m_manager.DeleteSelectedElement();
            Refresh(); // 重绘以移除删除的元件
        }
        else {
            event.Skip(); // 其他键不处理，传递事件
        }
    }

    // 获取元件边界（用于绘制选中边框）
    wxRect GetElementBounds(const nlohmann::json& el) {
        std::string type = el.value("type", "");
        int x = el.value("x", 0);
        int y = el.value("y", 0);

        if (type == "AND") return wxRect(x - 10, y, 70, 40);
        if (type == "OR" || type == "NOR") return wxRect(x, y, 60, 40);
        if (type == "NOT") return wxRect(x - 10, y, 70, 40);
        if (type == "NAND") return wxRect(x - 10, y, 50, 50);
        if (type == "XOR" || type == "XNOR") return wxRect(x - 10, y, 80, 40);
        if (type == "Buffer") return wxRect(x - 10, y, 50, 20);
        if (type == "Odd Parity" || type == "Even Parity") return wxRect(x - 10, y, 60, 40);
        if (type == "Controlled Buffer" || type == "Controlled Inverter") return wxRect(x - 10, y, 50, 40);
        if (type == "Input Pin" || type == "Output Pin") return wxRect(x - 10, y - 10, 20, 20);
        return wxRect(x - 10, y - 10, 80, 80); // 默认边界
    }
};

#endif // ELEMENTMANAGER_H
