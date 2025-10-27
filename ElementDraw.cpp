#include <wx/colour.h>
#include <wx/pen.h>
#include "ElementDraw.h"
#include <nlohmann/json.hpp>
#include <vector>

void DrawElement(wxDC& dc, const std::string& type, const std::string& color, int thickness, int x, int y) {
    wxColour wxcolor(color);
    wxPen pen(wxcolor, thickness);
    dc.SetPen(pen);

    if (type == "AND") {
        dc.DrawRectangle(x, y, 30, 40);
        dc.DrawArc(x + 30, y + 40, x + 30, y, x + 30, y + 20);
        dc.DrawLine(x - 10, y + 10, x, y + 10);
        dc.DrawLine(x - 10, y + 30, x, y + 30);
        dc.DrawLine(x + 50, y + 20, x + 60, y + 20);
    }
    else if (type == "OR") {
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 30, y + 20); 
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 5, y + 20); 
        dc.DrawLine(x + 10, y + 10, x + 26, y + 10);
        dc.DrawLine(x + 10, y + 30, x + 26, y + 30);
        dc.DrawLine(x + 50, y + 20, x + 60, y + 20);
    }
    else if (type == "NOT") {
        wxPoint points[3] = {
            wxPoint(x, y),
            wxPoint(x, y + 40),
            wxPoint(x + 40, y + 20)
        };
        dc.DrawPolygon(3, points);
        dc.DrawCircle(x + 45, y + 20, 5);
        dc.DrawLine(x - 10, y + 20, x, y + 20);
        dc.DrawLine(x + 50, y + 20, x + 60, y + 20);
    }
    else if (type == "NAND") {
        dc.DrawLine(x, y, x, y + 50);
        dc.DrawArc(x, y + 50, x, y, x, y + 25);
        dc.DrawCircle(x + 25, y + 25, 5);
        dc.DrawLine(x + 30, y + 25, x + 40, y + 25);
        dc.DrawLine(x - 10, y + 15, x, y + 15);
        dc.DrawLine(x - 10, y + 35, x, y + 35);
    }
    else if (type == "XNOR") {
        dc.DrawArc(x + 10, y + 40, x + 10, y, x, y + 20);
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 30, y + 20); 
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 5, y + 20); 
        dc.DrawCircle(x + 55, y + 20, 5);
        dc.DrawLine(x, y + 10, x + 17, y + 10);
        dc.DrawLine(x, y + 30, x + 17, y + 30);
        dc.DrawLine(x + 60, y + 20, x + 70, y + 20);
    }
    else if (type == "XOR") {
        dc.DrawArc(x + 10, y + 40, x + 10, y, x, y + 20);
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 30, y + 20); 
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 5, y + 20); 
        dc.DrawLine(x, y + 10, x + 17, y + 10);
        dc.DrawLine(x, y + 30, x + 17, y + 30);
        dc.DrawLine(x + 50, y + 20, x + 60, y + 20);
    }
    else if (type == "NOR") {
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 30, y + 20);
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 5, y + 20);
        dc.DrawLine(x + 10, y + 30, x + 26, y + 30);
        dc.DrawLine(x + 10, y + 10, x + 26, y + 10);
        dc.DrawCircle(x + 55, y + 20, 5);
        dc.DrawLine(x + 60, y + 20, x + 70, y + 20);
    }
    else if (type == "Input Pin") {
        dc.DrawRectangle(x - 7, y - 7, 14, 14);
        dc.DrawCircle(x, y, 5);
        dc.DrawLine(x + 5, y, x + 15, y);
    }
    else if (type == "Output Pin") {
        dc.DrawCircle(x, y, 5);
        dc.DrawLine(x - 15, y, x - 5, y);
    }
}

std::vector<wxPoint> GetElementPins(const nlohmann::json& el) {
    std::vector<wxPoint> pins;
    if (!el.is_object()) return pins;
    std::string type = el.value("type", std::string());
    int x = el.value("x", 0);
    int y = el.value("y", 0);
    int size = el.value("size", 1);
    // 基于 ElementDraw.cpp 中的坐标近似映射 pin 位置
    if (type == "AND") {
        pins.emplace_back(x, y + 10);      // input0
        pins.emplace_back(x, y + 30);      // input1
        pins.emplace_back(x + 60, y + 20); // output
    }
    else if (type == "OR") {
        pins.emplace_back(x + 10, y + 10);
        pins.emplace_back(x + 10, y + 30);
        pins.emplace_back(x + 60, y + 20);
    }
    else if (type == "NOT") {
        pins.emplace_back(x, y + 20);      // input (left)
        pins.emplace_back(x + 60, y + 20); // output (right)
    }
    else if (type == "XNOR") {
        pins.emplace_back(x, y + 10);
        pins.emplace_back(x, y + 30);
        pins.emplace_back(x + 70, y + 20);
    }
    else if (type == "XOR") {
        pins.emplace_back(x, y + 10);
        pins.emplace_back(x, y + 30);
        pins.emplace_back(x + 60, y + 20);
    }
    else if (type == "NOR") {
        pins.emplace_back(x + 10, y + 10);
        pins.emplace_back(x + 10, y + 30);
        pins.emplace_back(x + 70, y + 20);
    }
    else if (type == "Input Pin") {
        pins.emplace_back(x + 5, y);
    }
    else if (type == "Output Pin") {
        pins.emplace_back(x - 5, y);
    }
    else {
        // 默认：一个中心输出端点
        pins.emplace_back(x + 30, y + 20);
    }
    // 若 size !=1 可以按比例调整（简单处理）
    if (size != 1) {
        for (auto& p : pins) {
            p.x = x + (p.x - x) * size;
            p.y = y + (p.y - y) * size;
        }
    }
    return pins;
}
