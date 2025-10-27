#include <wx/colour.h>
#include <wx/pen.h>
#include "ElementDraw.h"

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
        // OR藷ㄩ酘說盄+衵湮說盄
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 30, y + 20); // 衵湮說
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 5, y + 20); // 酘苤說
        dc.DrawLine(x + 10, y + 10, x + 26, y + 10);
        dc.DrawLine(x + 10, y + 30, x + 26, y + 30);
        dc.DrawLine(x + 50, y + 20, x + 60, y + 20);
    }
    else if (type == "NOT") {
        // NOT藷ㄩ褒倛+苤埴
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
        //XOR藷ㄩ酘說盄+褒倛
        dc.DrawArc(x + 10, y + 40, x + 10, y, x , y + 20);
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 30, y + 20); // 衵湮說
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 5, y + 20); // 酘苤說
        dc.DrawCircle(x + 55, y + 20, 5);
        dc.DrawLine(x , y + 10, x + 17, y + 10);
        dc.DrawLine(x , y + 30, x + 17, y + 30);
        dc.DrawLine(x + 60, y + 20, x + 70, y + 20);
    }
    else if (type == "XOR") {
        dc.DrawArc(x + 10, y + 40, x + 10, y, x , y + 20);
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 30, y + 20); // 衵湮說
        dc.DrawArc(x + 20, y + 40, x + 20, y, x + 5, y + 20); // 酘苤說
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
