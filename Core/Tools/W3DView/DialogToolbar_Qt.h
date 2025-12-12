#pragma once

#include <QToolBar>

class DialogToolbarClass : public QToolBar
{
	Q_OBJECT

public:
	explicit DialogToolbarClass(QWidget* parent = nullptr);
	~DialogToolbarClass();

	void Enable_Button(int id, bool benable = true);
};



