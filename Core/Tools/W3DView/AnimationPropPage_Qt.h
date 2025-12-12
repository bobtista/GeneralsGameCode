#pragma once

#include <QWidget>

class CAnimationPropPage : public QWidget
{
	Q_OBJECT

public:
	explicit CAnimationPropPage(QWidget* parent = nullptr);
	~CAnimationPropPage();

private:
	void initDialog();
};



