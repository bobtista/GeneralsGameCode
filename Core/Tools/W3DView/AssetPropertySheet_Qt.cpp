#include "AssetPropertySheet_Qt.h"
#include "MeshPropPage_Qt.h"
#include "AnimationPropPage_Qt.h"
#include "HierarchyPropPage_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

CAssetPropertySheet::CAssetPropertySheet(const QString& title, QWidget* parent) :
	QDialog(parent),
	m_tabWidget(nullptr),
	m_meshPage(nullptr),
	m_animationPage(nullptr),
	m_hierarchyPage(nullptr)
{
	setWindowTitle(title.isEmpty() ? "Object Properties" : title);
	resize(500, 400);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	m_tabWidget = new QTabWidget(this);
	
	m_meshPage = new CMeshPropPage("", this);
	m_tabWidget->addTab(m_meshPage, "Mesh");
	
	m_animationPage = new CAnimationPropPage(this);
	m_tabWidget->addTab(m_animationPage, "Animation");
	
	m_hierarchyPage = new CHierarchyPropPage("", this);
	m_tabWidget->addTab(m_hierarchyPage, "Hierarchy");
	
	layout->addWidget(m_tabWidget);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &CAssetPropertySheet::accept);
	connect(cancelButton, &QPushButton::clicked, this, &CAssetPropertySheet::reject);
}

void CAssetPropertySheet::SetMeshPage(const QString& meshName)
{
	if (m_meshPage)
	{
		m_tabWidget->removeTab(m_tabWidget->indexOf(m_meshPage));
		delete m_meshPage;
	}
	m_meshPage = new CMeshPropPage(meshName, this);
	m_tabWidget->insertTab(0, m_meshPage, "Mesh");
	m_tabWidget->setCurrentIndex(0);
}

void CAssetPropertySheet::SetHierarchyPage(const QString& hierarchyName)
{
	if (m_hierarchyPage)
	{
		m_tabWidget->removeTab(m_tabWidget->indexOf(m_hierarchyPage));
		delete m_hierarchyPage;
	}
	m_hierarchyPage = new CHierarchyPropPage(hierarchyName, this);
	int index = m_tabWidget->count();
	m_tabWidget->insertTab(index, m_hierarchyPage, "Hierarchy");
	m_tabWidget->setCurrentIndex(index);
}

void CAssetPropertySheet::SetAnimationPage()
{
	if (m_animationPage)
	{
		m_tabWidget->removeTab(m_tabWidget->indexOf(m_animationPage));
		delete m_animationPage;
	}
	m_animationPage = new CAnimationPropPage(this);
	int index = m_tabWidget->count();
	m_tabWidget->insertTab(index, m_animationPage, "Animation");
	m_tabWidget->setCurrentIndex(index);
}

CAssetPropertySheet::~CAssetPropertySheet()
{
}

