#pragma once

#include <QApplication>

class CMainFrame;

class CW3DViewApp : public QApplication
{
	Q_OBJECT

public:
	CW3DViewApp(int& argc, char** argv);
	~CW3DViewApp();

	bool InitInstance();
	int ExitInstance();

private slots:
	void onAppAbout();

private:
	bool m_bInitialized;
	CMainFrame* m_mainFrame;
};

extern CW3DViewApp* theApp;



