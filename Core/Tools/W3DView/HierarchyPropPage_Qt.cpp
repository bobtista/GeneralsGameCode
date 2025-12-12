#include "HierarchyPropPage_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include <QHeaderView>

CHierarchyPropPage::CHierarchyPropPage(const QString& hierarchyName, QWidget* parent) :
	QWidget(parent),
	m_hierarchyName(hierarchyName),
	m_subObjectList(nullptr)
{
	initDialog();
}

CHierarchyPropPage::~CHierarchyPropPage()
{
}

void CHierarchyPropPage::initDialog()
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	
	QGroupBox* infoGroup = new QGroupBox("Hierarchy Information", this);
	QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);
	
	QLabel* nameLabel = new QLabel("Name:", this);
	QLineEdit* nameEdit = new QLineEdit(m_hierarchyName, this);
	nameEdit->setReadOnly(true);
	
	infoLayout->addWidget(nameLabel);
	infoLayout->addWidget(nameEdit);
	
	mainLayout->addWidget(infoGroup);
	
	QGroupBox* subObjectGroup = new QGroupBox("Sub Objects", this);
	QVBoxLayout* subObjectLayout = new QVBoxLayout(subObjectGroup);
	
	m_subObjectList = new QTreeWidget(this);
	m_subObjectList->setHeaderLabels(QStringList() << "Name" << "Type");
	m_subObjectList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_subObjectList->setRootIsDecorated(false);
	
	connect(m_subObjectList, &QTreeWidget::itemDoubleClicked, this, &CHierarchyPropPage::onSubObjectDoubleClicked);
	
	subObjectLayout->addWidget(m_subObjectList);
	mainLayout->addWidget(subObjectGroup);
	
	setLayout(mainLayout);
}

void CHierarchyPropPage::onSubObjectDoubleClicked(QTreeWidgetItem* item, int column)
{
	Q_UNUSED(column);
	if (item)
	{
		QString name = item->text(0);
	}
}



