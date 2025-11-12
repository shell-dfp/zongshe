#pragma once
#include <wx/dc.h>
#include <wx/colour.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

// 基础参考尺寸（Canvas 与这里保持一致）
constexpr int BaseElemWidth = 60;
constexpr int BaseElemHeight = 40;

// 绘制元件（增加 size 参数，用于缩放）
void DrawElement(wxDC& dc, const std::string& type, const std::string& color, int thickness, int x, int y, int size = 1);

// 端点相关 API（保持原有重载）
std::vector<wxPoint> GetElementPins(const std::string& type, int x, int y, int size, int inputs);
std::vector<wxPoint> GetElementPins(const nlohmann::json& el);

// 在元件上绘制端点
void DrawElementPins(wxDC& dc, const std::string& type, int x, int y, int size, int inputs, const wxColour& pinColor);
