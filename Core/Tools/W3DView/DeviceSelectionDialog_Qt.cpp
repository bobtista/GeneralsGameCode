#include "DeviceSelectionDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

DeviceSelectionDialog::DeviceSelectionDialog(QWidget* parent) :
	QDialog(parent),
	m_selectedDevice(0)
{
	setWindowTitle("Select Device");
	resize(300, 100);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	layout->addWidget(new QLabel("Select rendering device:", this));
	
	m_deviceCombo = new QComboBox(this);
	m_deviceCombo->addItem("Default Device");
	m_deviceCombo->addItem("Software Device");
	layout->addWidget(m_deviceCombo);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &DeviceSelectionDialog::accept);
	connect(cancelButton, &QPushButton::clicked, this, &DeviceSelectionDialog::reject);
}

DeviceSelectionDialog::~DeviceSelectionDialog()
{
}

void DeviceSelectionDialog::accept()
{
	m_selectedDevice = m_deviceCombo->currentIndex();
	QDialog::accept();
}



