#pragma once

#include <QDialog>
#include "TransDB.h"

class VerifyDlg : public QDialog
{
	Q_OBJECT

public:
	explicit VerifyDlg(QWidget* parent = nullptr);
	~VerifyDlg();
	
	void SetDB(TransDB* db) { m_db = db; }
	void SetLang(LangID langid) { m_langid = langid; }

private slots:
	void onMatch();
	void onNoMatch();
	void onStop();
	void onPlay();
	void onPause();

private:
	TransDB* m_db;
	LangID m_langid;
	BabylonText* m_babylon_text;
};



