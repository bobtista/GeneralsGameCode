#pragma once

#include <QDialog>
#include <QTabWidget>

class CEmitterPropertySheet : public QDialog
{
	Q_OBJECT

public:
	explicit CEmitterPropertySheet(QWidget* parent = nullptr);
	~CEmitterPropertySheet();

private:
	QTabWidget* m_tabWidget;
};



