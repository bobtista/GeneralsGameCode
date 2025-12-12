#include "SaveSettingsDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QFileDialog>

CSaveSettingsDialog::CSaveSettingsDialog(QWidget* parent) :
	QDialog(parent),
	m_settingsMask(0)
{
	setWindowTitle("Save Settings");
	resize(350, 150);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	QHBoxLayout* filenameLayout = new QHBoxLayout;
	filenameLayout->addWidget(new QLabel("Filename:", this));
	m_filenameEdit = new QLineEdit(this);
	filenameLayout->addWidget(m_filenameEdit);
	QPushButton* browseButton = new QPushButton("Browse...", this);
	filenameLayout->addWidget(browseButton);
	layout->addLayout(filenameLayout);
	
	m_lightCheckBox = new QCheckBox("Save Light Settings", this);
	m_backgroundCheckBox = new QCheckBox("Save Background Settings", this);
	m_cameraCheckBox = new QCheckBox("Save Camera Settings", this);
	
	layout->addWidget(m_lightCheckBox);
	layout->addWidget(m_backgroundCheckBox);
	layout->addWidget(m_cameraCheckBox);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(browseButton, &QPushButton::clicked, this, &CSaveSettingsDialog::onBrowse);
	connect(okButton, &QPushButton::clicked, this, &CSaveSettingsDialog::accept);
	connect(cancelButton, &QPushButton::clicked, this, &CSaveSettingsDialog::reject);
}

CSaveSettingsDialog::~CSaveSettingsDialog()
{
}

void CSaveSettingsDialog::onBrowse()
{
	QString filename = QFileDialog::getSaveFileName(this, "Save Settings", "", "Settings Files (*.ini)");
	if (!filename.isEmpty()) {
		m_filenameEdit->setText(filename);
	}
}

void CSaveSettingsDialog::accept()
{
	m_filename = m_filenameEdit->text();
	m_settingsMask = 0;
	if (m_lightCheckBox->isChecked()) {
		m_settingsMask |= 0x00000001;
	}
	if (m_backgroundCheckBox->isChecked()) {
		m_settingsMask |= 0x00000002;
	}
	if (m_cameraCheckBox->isChecked()) {
		m_settingsMask |= 0x00000004;
	}
	QDialog::accept();
}



