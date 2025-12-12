#include "MeshPropPage_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QGroupBox>

CMeshPropPage::CMeshPropPage(const QString& meshName, QWidget* parent) :
	QWidget(parent),
	m_meshName(meshName)
{
	initDialog();
}

CMeshPropPage::~CMeshPropPage()
{
}

void CMeshPropPage::initDialog()
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	
	QGroupBox* infoGroup = new QGroupBox("Mesh Information", this);
	QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);
	
	QLabel* nameLabel = new QLabel("Name:", this);
	QLineEdit* nameEdit = new QLineEdit(m_meshName, this);
	nameEdit->setReadOnly(true);
	
	infoLayout->addWidget(nameLabel);
	infoLayout->addWidget(nameEdit);
	
	mainLayout->addWidget(infoGroup);
	mainLayout->addStretch();
	
	setLayout(mainLayout);
}



