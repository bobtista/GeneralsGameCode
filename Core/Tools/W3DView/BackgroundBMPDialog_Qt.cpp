#include "BackgroundBMPDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>

CBackgroundBMPDialog::CBackgroundBMPDialog(QWidget* parent) :
	QDialog(parent)
{
	setWindowTitle("Background BMP");
	resize(400, 100);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	QHBoxLayout* filenameLayout = new QHBoxLayout;
	filenameLayout->addWidget(new QLabel("Filename:", this));
	m_filenameEdit = new QLineEdit(this);
	filenameLayout->addWidget(m_filenameEdit);
	QPushButton* browseButton = new QPushButton("Browse...", this);
	filenameLayout->addWidget(browseButton);
	layout->addLayout(filenameLayout);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(browseButton, &QPushButton::clicked, this, &CBackgroundBMPDialog::onBrowse);
	connect(okButton, &QPushButton::clicked, this, &CBackgroundBMPDialog::accept);
	connect(cancelButton, &QPushButton::clicked, this, &CBackgroundBMPDialog::reject);
}

CBackgroundBMPDialog::~CBackgroundBMPDialog()
{
}

void CBackgroundBMPDialog::onBrowse()
{
	QString filename = QFileDialog::getOpenFileName(this, "Select Background BMP", "", "Bitmap Files (*.bmp)");
	if (!filename.isEmpty()) {
		m_filenameEdit->setText(filename);
	}
}

void CBackgroundBMPDialog::accept()
{
	m_filename = m_filenameEdit->text();
	QDialog::accept();
}



