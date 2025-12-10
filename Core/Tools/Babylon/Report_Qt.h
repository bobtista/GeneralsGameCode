#pragma once

#include <QDialog>
#include "expimp.h"

class CReport : public QDialog
{
	Q_OBJECT

public:
	explicit CReport(QWidget* parent = nullptr);
	~CReport();

	char* Filename(void) { return m_filename; }
	RPOPTIONS* Options(void) { return &m_options; }
	LangID* Languages(void) { return m_langids; }

private slots:
	void onSelectAll();
	void onInvert();
	void onShowDetails();
	void accept() override;

private:
	char m_filename[300];
	RPOPTIONS m_options;
	LangID m_langids[200];
	int m_langindices[200];
	int m_num_langs;
};



