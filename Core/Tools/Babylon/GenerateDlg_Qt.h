#pragma once

#include <QDialog>
#include "expimp.h"

class CGenerateDlg : public QDialog
{
	Q_OBJECT

public:
	explicit CGenerateDlg(QWidget* parent = nullptr);
	~CGenerateDlg();

	char* FilePrefix(void) { return m_filename; }
	GNOPTIONS* Options(void) { return &m_options; }
	LangID* Languages(void) { return m_langids; }

private slots:
	void onSelectAll();
	void onInvert();
	void onUnicode();
	void onBabylonstr();
	void onIds();
	void onOriginal();
	void accept() override;

private:
	char m_filename[200];
	GNOPTIONS m_options;
	LangID m_langids[200];
	int m_langindices[200];
	int m_num_langs;
};



