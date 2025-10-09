#include <wx/colour.h>
#include <wx/pen.h>
#include "ElementDraw.h"

void DrawElement(wxDC& dc, const std::string& type, const std::string& color, int thickness, int x, int y) {
    wxColour wxcolor(color);
    wxPen pen(wxcolor, thickness);
    dc.SetPen(pen);

    if (type == "AND") {
        dc.DrawRectangle(x, y, 30, 40);
        dc.DrawArc(x+30, y+40, x+30, y, x+30, y+20);
        dc.DrawLine(x-10, y+10, x, y+10);
        dc.DrawLine(x-10, y+30, x, y+30);
        dc.DrawLine(x+50, y+20, x+60, y+20);
        }
        else if (type == "OR") {
        // OR门：左弧线+右大弧线
        dc.DrawArc(x+20, y+40, x+20, y, x+30, y+20); // 右大弧
        dc.DrawArc(x+20, y+40, x+20, y, x+5, y+20); // 左小弧
        dc.DrawLine(x+10, y+10, x+26, y+10);
        dc.DrawLine(x+10, y+30, x+26, y+30);
        dc.DrawLine(x+50, y+20, x+60, y+20);
        }
        else if (type == "NOT") {
        // NOT门：三角形+小圆
        wxPoint points[3] = {
            wxPoint(x, y),
            wxPoint(x, y+40),
            wxPoint(x+40, y+20)
        };
        dc.DrawPolygon(3, points);
        dc.DrawCircle(x+45, y+20, 5); 
        dc.DrawLine(x-10, y+20, x, y+20);
        dc.DrawLine(x+50, y+20, x+60, y+20);
    }
}