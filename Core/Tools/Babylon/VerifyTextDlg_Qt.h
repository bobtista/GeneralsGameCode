#pragma once

#include <QDialog>
#include "TransDB.h"

class CVerifyTextDlg : public QDialog
{
	Q_OBJECT

public:
	explicit CVerifyTextDlg(const char* expectedText, const char* actualText, QWidget* parent = nullptr);
	~CVerifyTextDlg();
	
	void SetDB(TransDB* db) { m_db = db; }
	void SetLang(LangID langid) { m_langid = langid; }

private slots:
	void onMatch();
	void onNoMatch();

private:
	TransDB* m_db;
	LangID m_langid;
};


