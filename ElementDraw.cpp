#include "ElementDraw.h"
#include <wx/pen.h>
#include <wx/brush.h>
#include <wx/font.h>
#include <cmath>

// 使用 Header 中的 BaseElemWidth/BaseElemHeight

// 辅助函数：绘制三角形（用于Buffer/Controlled Buffer的输出端）
static void DrawTriangle(wxDC& dc, int x, int y, int width, int height, bool isFilled = true) {
    wxPoint points[3];
    points[0] = wxPoint(x, y);                  // 左上角
    points[1] = wxPoint(x, y + height);          // 左下角
    points[2] = wxPoint(x + width, y + height / 2); // 右侧顶点

    if (isFilled) {
        dc.DrawPolygon(3, points);
    }
    else {
        dc.DrawLines(3, points);
        dc.DrawLine(points[2], points[0]); // 闭合三角形
    }
}

// 辅助函数：绘制异或符号（用于Odd Parity）
static void DrawXorSymbol(wxDC& dc, int x, int y, int size) {
    int crossSize = std::max(5, (int)std::round(8.0 * size));
    // 绘制斜十字（异或符号）
    dc.DrawLine(x - crossSize / 2, y - crossSize / 2, x + crossSize / 2, y + crossSize / 2);
    dc.DrawLine(x - crossSize / 2, y + crossSize / 2, x + crossSize / 2, y - crossSize / 2);
}

// 辅助函数：绘制控制信号符号（用于Controlled系列元件）
static void DrawControlSymbol(wxDC& dc, int x, int y, int size) {
    int circleRadius = std::max(2, (int)std::round(3.0 * size));
    dc.DrawCircle(x, y, circleRadius);
}

void DrawElement(wxDC& dc, const std::string& type, const std::string& color, int thickness, int x, int y, int size)
{
    if (size < 1) size = 1;
    wxColour wxcolor(color);
    wxPen pen(wxcolor, thickness);
    dc.SetPen(pen);
    dc.SetBrush(*wxWHITE_BRUSH);

    int w = static_cast<int>(std::round(BaseElemWidth * size));
    int h = static_cast<int>(std::round(BaseElemHeight * size));
    int inset = std::max(2, (int)std::round(4.0 * size)); // 内部图形内缩距离
    int triangleWidth = std::max(5, (int)std::round(8.0 * size)); // 三角形宽度

    // 绘制基础矩形（所有元件共享）
    dc.DrawRectangle(x, y, w, h);

    // 每种元件的专属图形
    if (type == "Buffer") {
        // Buffer：右侧添加三角形（表示信号增强）
        DrawTriangle(dc, x + w - triangleWidth, y + inset, triangleWidth, h - 2 * inset);
    }
    else if (type == "Odd Parity") {
        // Odd Parity：中心绘制异或符号（表示奇偶校验逻辑）
        int centerX = x + w / 2;
        int centerY = y + h / 2;
        DrawXorSymbol(dc, centerX, centerY, size);
    }
    else if (type == "Controlled Buffer") {
        // Controlled Buffer：右侧三角形 + 左上角控制信号标识
        DrawTriangle(dc, x + w - triangleWidth, y + inset, triangleWidth, h - 2 * inset);
        int controlX = x + inset * 2;
        int controlY = y + inset * 2;
        DrawControlSymbol(dc, controlX, controlY, size);
    }
    else if (type == "Controlled Inverter") {
        // Controlled Inverter：右侧三角形 + 左上角控制标识 + 三角形内小圆圈（表示反相）
        DrawTriangle(dc, x + w - triangleWidth, y + inset, triangleWidth, h - 2 * inset);
        int controlX = x + inset * 2;
        int controlY = y + inset * 2;
        DrawControlSymbol(dc, controlX, controlY, size);

        // 三角形内反相圆圈
        int circleX = x + w - triangleWidth / 2;
        int circleY = y + h / 2;
        int circleRadius = std::max(1, (int)std::round(2.0 * size));
        dc.DrawCircle(circleX, circleY, circleRadius);
    }

    // 根据 size 缩放字体（保持居中）
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

    int w = (int)std::round(BaseElemWidth * size);
    int h = (int)std::round(BaseElemHeight * size);
    int inset = std::max(2, (int)std::round(2.0 * size)); // 距离边缘的内缩
    int controlInset = std::max(3, (int)std::round(5.0 * size)); // 控制信号端点内缩

    // Input 元件：只显示输出点（右侧）
    if (type == "Input Pin") {
        int outx = x + w - inset;
        int outy = y + h / 2;
        pins.emplace_back(outx, outy);
        return pins;
    }

    // Output 元件：只显示输入点（左侧）
    if (type == "Output Pin") {
        int inx = x + inset;
        int iny = y + h / 2;
        pins.emplace_back(inx, iny);
        return pins;
    }

    // Buffer：1个输入（左侧居中），1个输出（右侧三角形顶点）
    if (type == "Buffer") {
        // 输入点（左侧居中）
        int inx = x + inset;
        int iny = y + h / 2;
        pins.emplace_back(inx, iny);

        // 输出点（右侧三角形顶点）
        int outx = x + w - inset;
        int outy = y + h / 2;
        pins.emplace_back(outx, outy);
        return pins;
    }

    // Odd Parity：多个输入（左侧均匀分布），1个输出（右侧居中）
    if (type == "Odd Parity") {
        inputs = std::max(2, inputs); // 奇偶校验至少2个输入
        float step = (float)h / (inputs + 1.0f);
        // 输入点（左侧）
        for (int i = 0; i < inputs; ++i) {
            int py = y + (int)std::round(step * (i + 1));
            int px = x + inset;
            pins.emplace_back(px, py);
        }
        // 输出点（右侧居中）
        int outx = x + w - inset;
        int outy = y + h / 2;
        pins.emplace_back(outx, outy);
        return pins;
    }

    // Controlled Buffer/Controlled Inverter：1个数据输入 + 1个控制输入 + 1个输出
    if (type == "Controlled Buffer" || type == "Controlled Inverter") {
        // 数据输入（左侧下方）
        int dataInX = x + inset;
        int dataInY = y + h - controlInset;
        pins.emplace_back(dataInX, dataInY);

        // 控制输入（左上角，靠近控制符号）
        int controlInX = x + controlInset;
        int controlInY = y + controlInset;
        pins.emplace_back(controlInX, controlInY);

        // 输出（右侧居中/三角形顶点）
        int outx = x + w - inset;
        int outy = y + h / 2;
        pins.emplace_back(outx, outy);
        return pins;
    }

    // 默认普通元件：左侧输入均匀分布，右侧单输出
    inputs = std::max(1, inputs);
    float step = (float)h / (inputs + 1.0f);
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
    // 特殊处理：Controlled系列元件强制使用2个输入（数据+控制），Odd Parity至少2个输入
    if (type == "Controlled Buffer" || type == "Controlled Inverter") {
        useInputs = 2;
    }
    else if (type == "Odd Parity") {
        useInputs = std::max(2, useInputs);
    }

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

// 新增：AND门逻辑处理
int Signals(const std::vector<int>& inputs, const std::string& type) {
    // Helper lambdas
    auto any_unknown = [&](const std::vector<int>& v)->bool {
        for (int x : v) if (x == -1) return true;
        return false;
        };
    auto count_ones = [&](const std::vector<int>& v)->int {
        int c = 0;
        for (int x : v) if (x == 1) ++c;
        return c;
        };

    if (inputs.empty()) return -1; // 没有输入，未知
    
    if (type == "AND") {
        for (int v : inputs) {
            if (v == -1) return -1; // 有未知输入，输出未知
            if (v == 0) return 0;   
        }
        return 1; 
    }
    else if (type == "NAND") {
        if (inputs.empty()) return -1;
        for (int v : inputs) {
            if (v == -1) return -1;
            if (v == 0) return 1; 
        }
        return 0; 
    }
    else if (type == "NOR") {
        if (inputs.empty()) return -1;
        for (int v : inputs) {
            if (v == -1) return -1;
            if (v == 1) return 0; 
        }
        return 1;
    }
    if (type == "XNOR") {
        if (any_unknown(inputs)) return -1;
        int ones = count_ones(inputs);
        return (ones % 2 == 1) ? 0 : 1;
    }
    if (type == "XOR") {
        // 奇偶校验（多输入）：若有未知则返回未知，否则计算1的个数的奇偶性
        if (any_unknown(inputs)) return -1;
        int ones = count_ones(inputs);
        return (ones % 2 == 1) ? 1 : 0;
    }
}
