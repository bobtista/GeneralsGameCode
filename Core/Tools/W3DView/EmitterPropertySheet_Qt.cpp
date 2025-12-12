#include "EmitterPropertySheet_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

CEmitterPropertySheet::CEmitterPropertySheet(QWidget* parent) :
	QDialog(parent)
{
	setWindowTitle("Emitter Properties");
	resize(600, 500);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	m_tabWidget = new QTabWidget(this);
	
	QWidget* generalPage = new QWidget();
	m_tabWidget->addTab(generalPage, "General");
	
	QWidget* colorPage = new QWidget();
	m_tabWidget->addTab(colorPage, "Color");
	
	QWidget* sizePage = new QWidget();
	m_tabWidget->addTab(sizePage, "Size");
	
	QWidget* rotationPage = new QWidget();
	m_tabWidget->addTab(rotationPage, "Rotation");
	
	QWidget* framePage = new QWidget();
	m_tabWidget->addTab(framePage, "Frame");
	
	QWidget* particlePage = new QWidget();
	m_tabWidget->addTab(particlePage, "Particle");
	
	QWidget* physicsPage = new QWidget();
	m_tabWidget->addTab(physicsPage, "Physics");
	
	QWidget* linePage = new QWidget();
	m_tabWidget->addTab(linePage, "Line");
	
	QWidget* lineGroupPage = new QWidget();
	m_tabWidget->addTab(lineGroupPage, "Line Group");
	
	QWidget* userPage = new QWidget();
	m_tabWidget->addTab(userPage, "User");
	
	layout->addWidget(m_tabWidget);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &CEmitterPropertySheet::accept);
	connect(cancelButton, &QPushButton::clicked, this, &CEmitterPropertySheet::reject);
}

CEmitterPropertySheet::~CEmitterPropertySheet()
{
}



