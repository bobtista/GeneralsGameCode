#include "ResolutionDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

ResolutionDialogClass::ResolutionDialogClass(QWidget* parent) :
	QDialog(parent),
	m_width(800),
	m_height(600)
{
	setWindowTitle("Change Resolution");
	resize(250, 120);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	QHBoxLayout* widthLayout = new QHBoxLayout;
	widthLayout->addWidget(new QLabel("Width:", this));
	m_widthCombo = new QComboBox(this);
	m_widthCombo->addItems({"640", "800", "1024", "1280", "1600", "1920"});
	m_widthCombo->setCurrentText("800");
	widthLayout->addWidget(m_widthCombo);
	layout->addLayout(widthLayout);
	
	QHBoxLayout* heightLayout = new QHBoxLayout;
	heightLayout->addWidget(new QLabel("Height:", this));
	m_heightCombo = new QComboBox(this);
	m_heightCombo->addItems({"480", "600", "768", "1024", "1200", "1080"});
	m_heightCombo->setCurrentText("600");
	heightLayout->addWidget(m_heightCombo);
	layout->addLayout(heightLayout);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &ResolutionDialogClass::accept);
	connect(cancelButton, &QPushButton::clicked, this, &ResolutionDialogClass::reject);
}

ResolutionDialogClass::~ResolutionDialogClass()
{
}

void ResolutionDialogClass::accept()
{
	m_width = m_widthCombo->currentText().toInt();
	m_height = m_heightCombo->currentText().toInt();
	QDialog::accept();
}



