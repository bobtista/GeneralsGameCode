#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include "expimp.h"

namespace Ui {
    class ExportDlg;
}

class CExportDlg : public QDialog
{
	Q_OBJECT

public:
	explicit CExportDlg(QWidget* parent = nullptr);
	~CExportDlg();

	LangID Language(void) { return m_langid; }
	const char* Filename(void) { return m_filename; }
	TROPTIONS* Options(void) { return &m_options; }

private slots:
	void onSelchangeCombolang();
	void onSelendokCombolang();
	void accept() override;

private:
	Ui::ExportDlg* ui;
	LangID m_langid;
	char m_filename[200];
	TROPTIONS m_options;
	int m_got_lang;
	int m_max_index;
};



