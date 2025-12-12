#pragma once

#include <QDialog>
#include <QDoubleSpinBox>

class ScaleDialogClass : public QDialog
{
	Q_OBJECT

public:
	explicit ScaleDialogClass(QWidget* parent = nullptr);
	~ScaleDialogClass();
	
	float GetScale() const { return m_scale; }

private slots:
	void accept() override;

private:
	QDoubleSpinBox* m_scaleSpinBox;
	float m_scale;
};



