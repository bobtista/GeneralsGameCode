#include "CameraSettingsDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>

CCameraSettingsDialog::CCameraSettingsDialog(QWidget* parent) :
	QDialog(parent),
	m_fov(45.0f),
	m_nearPlane(0.1f),
	m_farPlane(10000.0f),
	m_manualFOV(false),
	m_manualClipPlanes(false)
{
	setWindowTitle("Camera Settings");
	resize(300, 200);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	m_manualFOVCheckBox = new QCheckBox("Manual FOV", this);
	layout->addWidget(m_manualFOVCheckBox);
	
	QHBoxLayout* fovLayout = new QHBoxLayout;
	fovLayout->addWidget(new QLabel("FOV:", this));
	m_fovSpinBox = new QDoubleSpinBox(this);
	m_fovSpinBox->setRange(1.0, 179.0);
	m_fovSpinBox->setValue(45.0);
	m_fovSpinBox->setSingleStep(1.0);
	fovLayout->addWidget(m_fovSpinBox);
	layout->addLayout(fovLayout);
	
	m_manualClipCheckBox = new QCheckBox("Manual Clip Planes", this);
	layout->addWidget(m_manualClipCheckBox);
	
	QHBoxLayout* nearLayout = new QHBoxLayout;
	nearLayout->addWidget(new QLabel("Near Plane:", this));
	m_nearSpinBox = new QDoubleSpinBox(this);
	m_nearSpinBox->setRange(0.01, 1000.0);
	m_nearSpinBox->setValue(0.1);
	m_nearSpinBox->setSingleStep(0.1);
	nearLayout->addWidget(m_nearSpinBox);
	layout->addLayout(nearLayout);
	
	QHBoxLayout* farLayout = new QHBoxLayout;
	farLayout->addWidget(new QLabel("Far Plane:", this));
	m_farSpinBox = new QDoubleSpinBox(this);
	m_farSpinBox->setRange(1.0, 100000.0);
	m_farSpinBox->setValue(10000.0);
	m_farSpinBox->setSingleStep(100.0);
	farLayout->addWidget(m_farSpinBox);
	layout->addLayout(farLayout);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &CCameraSettingsDialog::accept);
	connect(cancelButton, &QPushButton::clicked, this, &CCameraSettingsDialog::reject);
}

CCameraSettingsDialog::~CCameraSettingsDialog()
{
}

void CCameraSettingsDialog::accept()
{
	m_fov = static_cast<float>(m_fovSpinBox->value());
	m_nearPlane = static_cast<float>(m_nearSpinBox->value());
	m_farPlane = static_cast<float>(m_farSpinBox->value());
	m_manualFOV = m_manualFOVCheckBox->isChecked();
	m_manualClipPlanes = m_manualClipCheckBox->isChecked();
	QDialog::accept();
}



