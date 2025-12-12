#include "GammaDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>

GammaDialogClass::GammaDialogClass(QWidget* parent) :
	QDialog(parent),
	m_gamma(1.0f)
{
	setWindowTitle("Set Gamma");
	resize(200, 100);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	QHBoxLayout* gammaLayout = new QHBoxLayout;
	gammaLayout->addWidget(new QLabel("Gamma:", this));
	m_gammaSpinBox = new QDoubleSpinBox(this);
	m_gammaSpinBox->setRange(0.1, 3.0);
	m_gammaSpinBox->setValue(1.0);
	m_gammaSpinBox->setSingleStep(0.1);
	gammaLayout->addWidget(m_gammaSpinBox);
	layout->addLayout(gammaLayout);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &GammaDialogClass::accept);
	connect(cancelButton, &QPushButton::clicked, this, &GammaDialogClass::reject);
}

GammaDialogClass::~GammaDialogClass()
{
}

void GammaDialogClass::accept()
{
	m_gamma = static_cast<float>(m_gammaSpinBox->value());
	QDialog::accept();
}



