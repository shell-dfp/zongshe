#pragma once
#include <wx/dc.h>
#include <wx/panel.h>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

void DrawElement(wxDC& dc, const std::string& type, const std::string& color, int thickness, int x, int y);
std::vector<wxPoint> GetElementPins(const nlohmann::json& el);
