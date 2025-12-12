#include "BackgroundObjectDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

CBackgroundObjectDialog::CBackgroundObjectDialog(QWidget* parent) :
	QDialog(parent)
{
	setWindowTitle("Background Object");
	resize(300, 100);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	QHBoxLayout* objectLayout = new QHBoxLayout;
	objectLayout->addWidget(new QLabel("Object Name:", this));
	m_objectCombo = new QComboBox(this);
	m_objectCombo->setEditable(true);
	objectLayout->addWidget(m_objectCombo);
	layout->addLayout(objectLayout);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &CBackgroundObjectDialog::accept);
	connect(cancelButton, &QPushButton::clicked, this, &CBackgroundObjectDialog::reject);
}

CBackgroundObjectDialog::~CBackgroundObjectDialog()
{
}

void CBackgroundObjectDialog::accept()
{
	m_objectName = m_objectCombo->currentText();
	QDialog::accept();
}



