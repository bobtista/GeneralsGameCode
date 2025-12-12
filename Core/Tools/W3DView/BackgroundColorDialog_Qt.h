#pragma once

#include <QDialog>
#include <QDoubleSpinBox>

class CBackgroundColorDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CBackgroundColorDialog(QWidget* parent = nullptr);
	~CBackgroundColorDialog();
	
	float GetRed() const { return m_red; }
	float GetGreen() const { return m_green; }
	float GetBlue() const { return m_blue; }

private slots:
	void accept() override;

private:
	QDoubleSpinBox* m_redSpinBox;
	QDoubleSpinBox* m_greenSpinBox;
	QDoubleSpinBox* m_blueSpinBox;
	float m_red;
	float m_green;
	float m_blue;
};



