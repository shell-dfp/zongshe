#include<wx/wx.h>
#include<nlohmann/json.hpp>
#include<wx/treectrl.h>
#include"wx/toolbar.h"
#include"wx/splitter.h"
#include"open.xpm"
#include"save.xpm"
#include"new.xpm"
#include <wx/artprov.h> 
#include <wx/aui/aui.h>
#include <wx/panel.h>
#include "ElementDraw.h"
#include<fstream>
using json = nlohmann::json;


class MyApp :public wxApp
{
public:
	virtual bool OnInit();
};

class DrawingPanel :public wxPanel
{
public:
	DrawingPanel(wxWindow* parent);
	
private:
	void OnPaint(wxPaintEvent& event);
	/*void OnSize(wxSizeEvent& event);
	void OnLeftDown(wxMouseEvent& event);
	void OnRightDown(wxMouseEvent& event);
	void OnMouseMove(wxMouseEvent& event);*/
	DECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE(DrawingPanel, wxPanel)
    EVT_PAINT(DrawingPanel::OnPaint)
    /*EVT_SIZE(DrawingPanel::OnSize)
    EVT_LEFT_DOWN(DrawingPanel::OnLeftDown)
    EVT_RIGHT_DOWN(DrawingPanel::OnRightDown)
    EVT_MOTION(DrawingPanel::OnMouseMove)*/
END_EVENT_TABLE()

DrawingPanel::DrawingPanel(wxWindow* parent) :wxPanel(parent, wxID_ANY) {
	SetBackgroundColour(wxColor(255, 255, 255));
	SetBackgroundStyle(wxBG_STYLE_CUSTOM);
}
void DrawingPanel::OnPaint(wxPaintEvent& event) {
	wxPaintDC dc(this);
	dc.SetPen(*wxLIGHT_GREY_PEN);
	int height, width;
	GetClientSize(&width, &height);
	for (int i = 0; i < width; i += 10) {
		for (int j = 0; j < height; j += 10) {
			dc.DrawPoint(i, j);
		}
	}
	std::ifstream file("Elementlib.json");
	if (!file.is_open()) return;
	json j;
	file >> j;

	for (const auto& comp : j["elements"]) {
		std::string type = comp["type"];
		std::string color = comp["color"];
		int thickness = comp["thickness"];
		int x = comp["x"];
		int y = comp["y"];
		DrawElement(dc, type, color, thickness, x, y);

	}
}

/*void DrawingPanel::OnSize(wxSizeEvent& event) {
	Refresh();
	event.Skip();
}

void DrawingPanel::OnLeftDown(wxMouseEvent& event) {

}*/

class MyTreePanel : public wxPanel
{
public:
	MyTreePanel(wxWindow* parent)
		: wxPanel(parent, wxID_ANY)
	{
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
		wxTreeCtrl* tree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);
		wxTreeItemId root = tree->AddRoot("Resource manage");
		wxTreeItemId child1 = tree->AppendItem(root, "Untitled");
		wxTreeItemId child2 = tree->AppendItem(root, "main");
		wxTreeItemId child3 = tree->AppendItem(root, "Wiring");
		tree->AppendItem(child3, "splitter");
		tree->AppendItem(child3, "Pin");
		wxTreeItemId child4 = tree->AppendItem(root, "Gate");
		tree->AppendItem(child4, "AND Gate");
		tree->AppendItem(child4, "OR Gate");
		tree->ExpandAll();
		sizer->Add(tree, 1, wxEXPAND | wxALL, 5);
		SetSizer(sizer);
	}
};

class MyFrame :public wxFrame
{
public:
	MyFrame(const wxString& title);

	void OnQuit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);

private:
	DrawingPanel* m_drawingPanel;
	MyTreePanel* m_mytreePanel;
	DECLARE_EVENT_TABLE()
};

DECLARE_APP(MyApp);
IMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
	MyFrame* frame = new MyFrame(wxT("Logisim"));
	frame->Show(true);
	return true;
}

BEGIN_EVENT_TABLE(MyFrame,wxFrame)
	EVT_MENU(wxID_ABOUT,MyFrame::OnAbout)
	EVT_MENU(wxID_EXIT,MyFrame::OnQuit)
END_EVENT_TABLE()

void MyFrame::OnAbout(wxCommandEvent& event)
{
	wxString msg;
	msg.Printf(wxT("Hello and welcome to %s"), wxVERSION_STRING);
	wxMessageBox(msg, wxT("About Logisim"), wxOK | wxICON_INFORMATION, this);
}

void MyFrame::OnQuit(wxCommandEvent& event)
{
	Close();
}

MyFrame::MyFrame(const wxString& title) :wxFrame(NULL, wxID_ANY, title,wxDefaultPosition,wxSize(1000,600))
	,m_drawingPanel(nullptr),m_mytreePanel(nullptr)
{	
	//菜单栏
	wxMenu* fileMenu = new wxMenu;
	fileMenu->Append(wxID_NEW, wxT("&New\tCtrl+N"), wxT("Create"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_OPEN, wxT("&Open...\tCtrl+O"), wxT("Open"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_CLOSE, wxT("&Close\tCtrl+Shift+W"), wxT("Close"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_SAVE, wxT("&Save\tCtrl+S"), wxT("Save"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_SAVEAS, wxT("&Save As\tCtrl+Shift+S"), wxT("Save as"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_EXIT, wxT("&Exit\tCtrl+Q"), wxT("Exit"));

	wxMenu* editMenu = new wxMenu;
	editMenu->Append(wxID_CUT, wxT("&Cut\tCtrl+X"), wxT("Cut"));
	editMenu->AppendSeparator();
	editMenu->Append(wxID_COPY, wxT("&Copy\tCtrl+C"), wxT("Copy"));
	editMenu->AppendSeparator();
	editMenu->Append(wxID_PASTE, wxT("&Paste\tCtrl+V"), wxT("Paste"));

	wxMenu* projectMenu = new wxMenu;
	projectMenu->Append(wxID_NEW, wxT("&Add Circuit"), wxT("Add Circuit..."));

	wxMenu* simulateMenu = new wxMenu;
	wxMenu* windowMenu = new wxMenu;

	wxMenu* helpMenu = new wxMenu;
	helpMenu->Append(wxID_ABOUT, wxT("&About..."), wxT("Show about dialog"));

	wxMenuBar* menuBar = new wxMenuBar();
	menuBar->Append(fileMenu, wxT("&File"));
	menuBar->Append(editMenu, wxT("&Edit"));
	menuBar->Append(projectMenu, wxT("&Project"));
	menuBar->Append(simulateMenu, wxT("&Simulate"));
	menuBar->Append(windowMenu, wxT("&Window"));
	menuBar->Append(helpMenu, wxT("&Help"));

	SetMenuBar(menuBar);

	//工具栏
	wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxNO_BORDER);
	wxBitmap bmpOpen(open_xpm);
	wxBitmap bmpSave(save_xpm);
	wxBitmap bmpNew(new_xpm);
	toolBar->AddTool(wxID_OPEN, wxT("Open"), bmpOpen, wxT("Open"));
	toolBar->AddTool(wxID_SAVE, wxT("Save"), bmpSave, wxT("Save"));
	toolBar->AddTool(wxID_NEW, wxT("New"), bmpNew, wxT("New"));
	toolBar->Realize();
	SetToolBar(toolBar);

	CreateStatusBar();
	SetStatusText(wxT("Welcome to Logisim!"));

	//分割窗口
	wxSplitterWindow* splitter = new wxSplitterWindow(this, wxID_ANY,wxPoint(0,0),wxSize(1000,600),wxSP_3D);
	wxPanel* leftWindow = new MyTreePanel(splitter);
	wxPanel* rightWindow = new DrawingPanel(splitter);
	splitter->SplitVertically(leftWindow, rightWindow, 300);
	wxSplitterWindow* splitterLeft = new wxSplitterWindow(leftWindow, wxID_ANY, wxPoint(0, 0), wxSize(300, 600), wxSP_3D);
	wxPanel* upperWindow = new wxPanel(splitterLeft, wxID_ANY);
	wxPanel* belowWindow = new wxPanel(splitterLeft, wxID_ANY);
	upperWindow->SetBackgroundColour(*wxWHITE);
	splitterLeft->SplitHorizontally(upperWindow, belowWindow, 300);
}