#pragma once

#include <QDialog>
#include <QDoubleSpinBox>

class GammaDialogClass : public QDialog
{
	Q_OBJECT

public:
	explicit GammaDialogClass(QWidget* parent = nullptr);
	~GammaDialogClass();
	
	float GetGamma() const { return m_gamma; }

private slots:
	void accept() override;

private:
	QDoubleSpinBox* m_gammaSpinBox;
	float m_gamma;
};



