#include<wx/wx.h>
#include"wx/toolbar.h"
#include"wx/splitter.h"
#include"open.xpm"
#include"save.xpm"
#include"new.xpm"

class MyApp :public wxApp
{
public:
	virtual bool OnInit();
};

class MyFrame :public wxFrame
{
public:
	MyFrame(const wxString& title);

	void OnQuit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);

private:
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
	wxPanel* leftWindow = new wxPanel(splitter, wxID_ANY);
	wxPanel* rightWindow = new wxPanel(splitter, wxID_ANY);
	leftWindow->SetBackgroundColour(*wxWHITE);
	splitter->SplitVertically(leftWindow, rightWindow, 200);
}