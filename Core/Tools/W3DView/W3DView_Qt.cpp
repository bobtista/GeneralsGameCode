#include "W3DView_Qt.h"
#include "MainFrm_Qt.h"
#include <QMessageBox>
#include <QDir>

CW3DViewApp* theApp = nullptr;

// TheSuperHackers @refactor bobtista 01/01/2025 Stub Windows types for cross-platform compatibility
#ifndef _WIN32
typedef void* HINSTANCE;
typedef void* HWND;
#endif

HINSTANCE ApplicationHInstance = NULL;
HWND ApplicationHWnd = NULL;

const char* gAppPrefix = "w3_";
const char* g_strFile = "data\\Generals.str";
const char* g_csfFile = "data\\%s\\Generals.csf";

CW3DViewApp::CW3DViewApp(int& argc, char** argv) :
	QApplication(argc, argv),
	m_bInitialized(false),
	m_mainFrame(nullptr)
{
	theApp = this;
}

CW3DViewApp::~CW3DViewApp()
{
	if (m_bInitialized) {
		ExitInstance();
	}
}

bool CW3DViewApp::InitInstance()
{
	if (m_bInitialized) {
		return true;
	}
	
	m_mainFrame = new CMainFrame();
	m_mainFrame->show();
	
	m_bInitialized = true;
	return true;
}

int CW3DViewApp::ExitInstance()
{
	if (m_mainFrame) {
		delete m_mainFrame;
		m_mainFrame = nullptr;
	}
	
	m_bInitialized = false;
	return 0;
}

void CW3DViewApp::onAppAbout()
{
	QMessageBox::about(nullptr, "About W3DView", "W3DView - 3D Model Viewer");
}

int main(int argc, char* argv[])
{
	CW3DViewApp app(argc, argv);
	
	if (!app.InitInstance()) {
		return 1;
	}
	
	return app.exec();
}


