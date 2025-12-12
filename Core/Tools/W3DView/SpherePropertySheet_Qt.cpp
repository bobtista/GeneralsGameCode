#include "SpherePropertySheet_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

SpherePropertySheetClass::SpherePropertySheetClass(SphereRenderObjClass* sphere, const QString& caption, QWidget* parent, int selectPage) :
	QDialog(parent),
	m_RenderObj(sphere),
	m_tabWidget(nullptr),
	m_generalPage(nullptr),
	m_colorPage(nullptr),
	m_scalePage(nullptr)
{
	setWindowTitle(caption);
	resize(500, 400);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	m_tabWidget = new QTabWidget(this);
	
	m_generalPage = new QWidget();
	m_tabWidget->addTab(m_generalPage, "General");
	
	m_colorPage = new QWidget();
	m_tabWidget->addTab(m_colorPage, "Color");
	
	m_scalePage = new QWidget();
	m_tabWidget->addTab(m_scalePage, "Scale");
	
	layout->addWidget(m_tabWidget);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &SpherePropertySheetClass::accept);
	connect(cancelButton, &QPushButton::clicked, this, &SpherePropertySheetClass::reject);
	
	if (selectPage >= 0 && selectPage < m_tabWidget->count())
	{
		m_tabWidget->setCurrentIndex(selectPage);
	}
	
	Initialize();
}

SpherePropertySheetClass::~SpherePropertySheetClass()
{
}

void SpherePropertySheetClass::Initialize()
{
}

SphereRenderObjClass* SpherePropertySheetClass::Create_Object()
{
	return nullptr;
}

void SpherePropertySheetClass::Update_Object()
{
}

void SpherePropertySheetClass::Add_Object_To_Viewer()
{
}

void SpherePropertySheetClass::Create_New_Object()
{
}



