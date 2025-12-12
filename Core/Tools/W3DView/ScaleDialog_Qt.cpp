#include "ScaleDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>

ScaleDialogClass::ScaleDialogClass(QWidget* parent) :
	QDialog(parent),
	m_scale(1.0f)
{
	setWindowTitle("Scale Emitter");
	resize(200, 100);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	QHBoxLayout* scaleLayout = new QHBoxLayout;
	scaleLayout->addWidget(new QLabel("Scale:", this));
	m_scaleSpinBox = new QDoubleSpinBox(this);
	m_scaleSpinBox->setRange(0.01, 100.0);
	m_scaleSpinBox->setValue(1.0);
	m_scaleSpinBox->setSingleStep(0.1);
	scaleLayout->addWidget(m_scaleSpinBox);
	layout->addLayout(scaleLayout);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &ScaleDialogClass::accept);
	connect(cancelButton, &QPushButton::clicked, this, &ScaleDialogClass::reject);
}

ScaleDialogClass::~ScaleDialogClass()
{
}

void ScaleDialogClass::accept()
{
	m_scale = static_cast<float>(m_scaleSpinBox->value());
	QDialog::accept();
}



