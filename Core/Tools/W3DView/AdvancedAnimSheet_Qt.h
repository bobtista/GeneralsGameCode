#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QVector>

class HAnimClass;

class CAdvancedAnimSheet : public QDialog
{
	Q_OBJECT

public:
	explicit CAdvancedAnimSheet(QWidget* parent = nullptr, int selectPage = 0);
	~CAdvancedAnimSheet();

	int GetAnimCount();
	HAnimClass** GetAnims();

private:
	void LoadAnims();

	QTabWidget* m_tabWidget;
	QWidget* m_mixingPage;
	QWidget* m_reportPage;
	QVector<int> m_SelectedAnims;
	
	HAnimClass* Anims[128];
	int AnimCount;
	bool AnimsValid;
};



