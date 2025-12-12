#include "TexturePathDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>

TexturePathDialogClass::TexturePathDialogClass(QWidget* parent) :
	QDialog(parent)
{
	setWindowTitle("Texture Paths");
	resize(500, 150);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	QHBoxLayout* path1Layout = new QHBoxLayout;
	path1Layout->addWidget(new QLabel("Path 1:", this));
	m_path1Edit = new QLineEdit(this);
	path1Layout->addWidget(m_path1Edit);
	QPushButton* browse1Button = new QPushButton("Browse...", this);
	path1Layout->addWidget(browse1Button);
	layout->addLayout(path1Layout);
	
	QHBoxLayout* path2Layout = new QHBoxLayout;
	path2Layout->addWidget(new QLabel("Path 2:", this));
	m_path2Edit = new QLineEdit(this);
	path2Layout->addWidget(m_path2Edit);
	QPushButton* browse2Button = new QPushButton("Browse...", this);
	path2Layout->addWidget(browse2Button);
	layout->addLayout(path2Layout);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(browse1Button, &QPushButton::clicked, this, &TexturePathDialogClass::onBrowse1);
	connect(browse2Button, &QPushButton::clicked, this, &TexturePathDialogClass::onBrowse2);
	connect(okButton, &QPushButton::clicked, this, &TexturePathDialogClass::accept);
	connect(cancelButton, &QPushButton::clicked, this, &TexturePathDialogClass::reject);
}

TexturePathDialogClass::~TexturePathDialogClass()
{
}

void TexturePathDialogClass::onBrowse1()
{
	QString path = QFileDialog::getExistingDirectory(this, "Select Texture Path 1");
	if (!path.isEmpty()) {
		m_path1Edit->setText(path);
	}
}

void TexturePathDialogClass::onBrowse2()
{
	QString path = QFileDialog::getExistingDirectory(this, "Select Texture Path 2");
	if (!path.isEmpty()) {
		m_path2Edit->setText(path);
	}
}

void TexturePathDialogClass::accept()
{
	m_path1 = m_path1Edit->text();
	m_path2 = m_path2Edit->text();
	QDialog::accept();
}



