#include "CameraDistanceDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>

CameraDistanceDialogClass::CameraDistanceDialogClass(QWidget* parent) :
	QDialog(parent),
	m_distance(100.0f)
{
	setWindowTitle("Set Camera Distance");
	resize(250, 100);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	QHBoxLayout* distanceLayout = new QHBoxLayout;
	distanceLayout->addWidget(new QLabel("Distance:", this));
	m_distanceSpinBox = new QDoubleSpinBox(this);
	m_distanceSpinBox->setRange(0.1, 10000.0);
	m_distanceSpinBox->setValue(100.0);
	m_distanceSpinBox->setSingleStep(10.0);
	distanceLayout->addWidget(m_distanceSpinBox);
	layout->addLayout(distanceLayout);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &CameraDistanceDialogClass::accept);
	connect(cancelButton, &QPushButton::clicked, this, &CameraDistanceDialogClass::reject);
}

CameraDistanceDialogClass::~CameraDistanceDialogClass()
{
}

void CameraDistanceDialogClass::accept()
{
	m_distance = static_cast<float>(m_distanceSpinBox->value());
	QDialog::accept();
}



