#include "ElementDraw.h"
#include <wx/pen.h>
#include <wx/brush.h>
#include <wx/font.h>
#include <cmath>

// 使用 Header 中的 BaseElemWidth/BaseElemHeight

void DrawElement(wxDC& dc, const std::string& type, const std::string& color, int thickness, int x, int y, int size)
{
    if (size < 1) size = 1;
    wxColour wxcolor(color);
    wxPen pen(wxcolor, thickness);
    dc.SetPen(pen);
    dc.SetBrush(*wxWHITE_BRUSH);

    int w = static_cast<int>(std::round(BaseElemWidth * size));
    int h = static_cast<int>(std::round(BaseElemHeight * size));

    // 统一绘制矩形（根据 size 缩放）
    dc.DrawRectangle(x, y, w, h);

    // 根据 size 缩放字体
    int fontSize = std::max(8, 12 * size);
    wxFont font(fontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    dc.SetFont(font);
    wxString label(type);
    wxSize textSize = dc.GetTextExtent(label);
    int textX = x + (w - textSize.GetWidth()) / 2;
    int textY = y + (h - textSize.GetHeight()) / 2;
    dc.DrawText(label, textX, textY);
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
    int useInputs = (inputs < 1) ? 1 : inputs;
    auto pins = GetElementPins(type, x, y, size, useInputs);

    int r = std::max(2, (int)std::round(3.0 * size));

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
