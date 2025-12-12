#pragma once

#include <QDialog>
#include <QComboBox>
#include <QPushButton>

class ResolutionDialogClass : public QDialog
{
	Q_OBJECT

public:
	explicit ResolutionDialogClass(QWidget* parent = nullptr);
	~ResolutionDialogClass();
	
	int GetWidth() const { return m_width; }
	int GetHeight() const { return m_height; }

private slots:
	void accept() override;

private:
	QComboBox* m_widthCombo;
	QComboBox* m_heightCombo;
	int m_width;
	int m_height;
};



