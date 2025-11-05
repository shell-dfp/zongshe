#pragma once
#include <wx/dc.h>
#include <wx/colour.h>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

// 绘制元件本体
void DrawElement(wxDC& dc, const std::string& type, const std::string& color, int thickness, int x, int y);

// 从 json 计算端点坐标（兼容原来的调用）
std::vector<wxPoint> GetElementPins(const nlohmann::json& el);

// 重载：直接根据参数计算端点（更灵活，便于外部传入 inputs 与 size）
std::vector<wxPoint> GetElementPins(const std::string& type, int x, int y, int size = 1, int inputs = 1);

// 在元件上绘制端点（蓝色实心点），可传入 inputs（-1 表示使用默认 1）
void DrawElementPins(wxDC& dc, const std::string& type, int x, int y, int size = 1, int inputs = -1, const wxColour& pinColor = wxColour(30, 144, 255));
