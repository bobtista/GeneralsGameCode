#include "AdvancedAnimSheet_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <cstring>

CAdvancedAnimSheet::CAdvancedAnimSheet(QWidget* parent, int selectPage) :
	QDialog(parent),
	m_tabWidget(nullptr),
	m_mixingPage(nullptr),
	m_reportPage(nullptr),
	AnimsValid(false),
	AnimCount(0)
{
	setWindowTitle("Advanced Animation");
	resize(500, 400);
	
	memset(Anims, 0, sizeof(Anims));
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	
	m_tabWidget = new QTabWidget(this);
	
	m_mixingPage = new QWidget();
	m_tabWidget->addTab(m_mixingPage, "Mixing");
	
	m_reportPage = new QWidget();
	m_tabWidget->addTab(m_reportPage, "Report");
	
	layout->addWidget(m_tabWidget);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(okButton, &QPushButton::clicked, this, &CAdvancedAnimSheet::accept);
	connect(cancelButton, &QPushButton::clicked, this, &CAdvancedAnimSheet::reject);
	
	if (selectPage >= 0 && selectPage < m_tabWidget->count())
	{
		m_tabWidget->setCurrentIndex(selectPage);
	}
}

CAdvancedAnimSheet::~CAdvancedAnimSheet()
{
	if (AnimsValid)
	{
		for (int i = 0; i < AnimCount; i++)
		{
			Anims[i] = nullptr;
		}
		AnimsValid = false;
		AnimCount = 0;
	}
}

int CAdvancedAnimSheet::GetAnimCount()
{
	if (AnimsValid)
	{
		return AnimCount;
	}
	LoadAnims();
	return AnimCount;
}

HAnimClass** CAdvancedAnimSheet::GetAnims()
{
	if (!AnimsValid)
	{
		LoadAnims();
	}
	return Anims;
}

void CAdvancedAnimSheet::LoadAnims()
{
	AnimsValid = true;
	AnimCount = 0;
}



