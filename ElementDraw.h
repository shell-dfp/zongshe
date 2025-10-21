#pragma once
#include<wx/dc.h>
#include<string>

void DrawElement(wxDC& dc, const std::string& type, const std::string& color, int thickness, int x, int y);