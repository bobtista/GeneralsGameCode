#pragma once

#include <QDialog>
#include "TransDB.h"

class RetranslateDlg : public QDialog
{
	Q_OBJECT

public:
	explicit RetranslateDlg(QWidget* parent = nullptr);
	~RetranslateDlg();
	
	BabylonText* newtext;
	BabylonText* oldtext;

private slots:
	void onRetranslate();
	void onNoRetranslate();
	void onSkip();
	void onSkipAll();
};



