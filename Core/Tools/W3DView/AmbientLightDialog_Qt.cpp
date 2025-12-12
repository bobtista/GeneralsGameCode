#include "AmbientLightDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>

CAmbientLightDialog::CAmbientLightDialog(QWidget* parent) :
	QDialog(parent),
	m_red(0.5f),
	m_green(0.5f),
	m_blue(0.5f)
{
	setWindowTitle("Ambient Light");
	resize(250, 150);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	QHBoxLayout* redLayout = new QHBoxLayout;
	redLayout->addWidget(new QLabel("Red:", this));
	m_redSpinBox = new QDoubleSpinBox(this);
	m_redSpinBox->setRange(0.0, 1.0);
	m_redSpinBox->setValue(0.5);
	m_redSpinBox->setSingleStep(0.1);
	redLayout->addWidget(m_redSpinBox);
	layout->addLayout(redLayout);
	
	QHBoxLayout* greenLayout = new QHBoxLayout;
	greenLayout->addWidget(new QLabel("Green:", this));
	m_greenSpinBox = new QDoubleSpinBox(this);
	m_greenSpinBox->setRange(0.0, 1.0);
	m_greenSpinBox->setValue(0.5);
	m_greenSpinBox->setSingleStep(0.1);
	greenLayout->addWidget(m_greenSpinBox);
	layout->addLayout(greenLayout);
	
	QHBoxLayout* blueLayout = new QHBoxLayout;
	blueLayout->addWidget(new QLabel("Blue:", this));
	m_blueSpinBox = new QDoubleSpinBox(this);
	m_blueSpinBox->setRange(0.0, 1.0);
	m_blueSpinBox->setValue(0.5);
	m_blueSpinBox->setSingleStep(0.1);
	blueLayout->addWidget(m_blueSpinBox);
	layout->addLayout(blueLayout);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &CAmbientLightDialog::accept);
	connect(cancelButton, &QPushButton::clicked, this, &CAmbientLightDialog::reject);
}

CAmbientLightDialog::~CAmbientLightDialog()
{
}

void CAmbientLightDialog::accept()
{
	m_red = static_cast<float>(m_redSpinBox->value());
	m_green = static_cast<float>(m_greenSpinBox->value());
	m_blue = static_cast<float>(m_blueSpinBox->value());
	QDialog::accept();
}



