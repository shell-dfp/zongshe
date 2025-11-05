#include <wx/colour.h>
#include <wx/pen.h>
#include <wx/brush.h>
#include "ElementDraw.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <cmath>

// 与画布保持一致的参考元件基础尺寸（若 CanvasPanel 不同请同步）
static const int BaseElemWidth = 60;
static const int BaseElemHeight = 40;

void DrawElement(wxDC& dc, const std::string& type, const std::string& color, int thickness, int x, int y) {
    wxColour wxcolor(color);
    wxPen pen(wxcolor, thickness);
    dc.SetPen(pen);

    if (type == "AND") {
        dc.DrawRectangle(x, y, 30, 40);
        dc.DrawArc(x + 30, y + 40, x + 30, y, x + 30, y + 20);
    }
    else if (type == "OR") {
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 30, y + 20);
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 5, y + 20);
    }
    else if (type == "NOT") {
        wxPoint points[3] = {
            wxPoint(x, y),
            wxPoint(x, y + 40),
            wxPoint(x + 40, y + 20)
        };
        dc.DrawPolygon(3, points);
        dc.DrawCircle(x + 45, y + 20, 5);
    }
    else if (type == "NAND") {
        dc.DrawLine(x, y, x, y + 50);
        dc.DrawArc(x, y + 50, x, y, x, y + 25);
        dc.DrawCircle(x + 25, y + 25, 5);
    }
    else if (type == "XNOR") {
        dc.DrawArc(x + 10, y + 40, x + 10, y, x, y + 20);
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 30, y + 20);
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 5, y + 20);
        dc.DrawCircle(x + 55, y + 20, 5);
    }
    else if (type == "XOR") {
        dc.DrawArc(x + 10, y + 40, x + 10, y, x, y + 20);
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 30, y + 20);
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 5, y + 20);
    }
    else if (type == "NOR") {
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 30, y + 20);
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 5, y + 20);
    }
    else if (type == "Input Pin") {
        dc.DrawRectangle(x - 7, y - 7, 14, 14);
        dc.DrawCircle(x, y, 5);
    }
    else if (type == "Output Pin") {
        dc.DrawCircle(x, y, 5);
    }
}

// 重载：根据 type/x/y/size/inputs 返回端点坐标，端点位置位于元件边界并随 size 缩放
std::vector<wxPoint> GetElementPins(const std::string& type, int x, int y, int size, int inputs) {
    std::vector<wxPoint> pins;
    if (size < 1) size = 1;
    if (inputs < 1) inputs = 1;

    int w = (int)std::round(BaseElemWidth * size);
    int h = (int)std::round(BaseElemHeight * size);

    // Input 元件：只显示输出点（右侧）
    if (type == "Input Pin") {
        int outx = x + w - std::max(2, (int)std::round(2.0 * size));
        int outy = y + h / 2;
        pins.emplace_back(outx, outy);
        return pins;
    }

    // Output 元件：只显示输入点（左侧）
    if (type == "Output Pin") {
        int inx = x + std::max(2, (int)std::round(2.0 * size));
        int iny = y + h / 2;
        pins.emplace_back(inx, iny);
        return pins;
    }

    // 普通元件：左侧输入均匀分布，右侧单输出
    float step = (float)h / (inputs + 1.0f);
    int inset = std::max(2, (int)std::round(2.0 * size)); // 距离边缘的内缩，保证点在元件上
    for (int i = 0; i < inputs; ++i) {
        int py = y + (int)std::round(step * (i + 1));
        int px = x + inset; // 左侧
        pins.emplace_back(px, py);
    }
    int outx = x + w - inset;
    int outy = y + h / 2;
    pins.emplace_back(outx, outy);

    return pins;
}

// 原来 json 版本，保持兼容：从 json 中读取 size 和 inputs 并调用重载
std::vector<wxPoint> GetElementPins(const nlohmann::json& el) {
    if (!el.is_object()) return {};
    std::string type = el.value("type", std::string());
    int x = el.value("x", 0);
    int y = el.value("y", 0);
    int size = el.value("size", 1);
    int inputs = el.value("inputs", 1);
    return GetElementPins(type, x, y, size, inputs);
}

// 绘制端点：蓝色实心小圆，半径随 size 缩放（并保证最小可见）
// inputs 参数可传 -1 表示使用默认（从 json/外部应优先传入真实 inputs）
void DrawElementPins(wxDC& dc, const std::string& type, int x, int y, int size, int inputs, const wxColour& pinColor)
{
    // 如果调用者未传入 inputs（-1），尝试使用 1（调用方应优先提供真实 inputs）
    int useInputs = (inputs < 1) ? 1 : inputs;
    auto pins = GetElementPins(type, x, y, size, useInputs);

    // 半径根据 size 缩放（Base radius = 3）
    int r = std::max(2, (int)std::round(3.0 * size));

    // 画实心蓝点并加白色描边以保证可见
    wxBrush brush(pinColor);
    wxPen borderPen(*wxWHITE, 1);
    for (const auto& p : pins) {
        dc.SetPen(wxNullPen);
        dc.SetBrush(brush);
        dc.DrawCircle(p.x, p.y, r);

        dc.SetPen(borderPen);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawCircle(p.x, p.y, r);
    }
}
