#include "AnimationPropPage_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>

CAnimationPropPage::CAnimationPropPage(QWidget* parent) :
	QWidget(parent)
{
	initDialog();
}

CAnimationPropPage::~CAnimationPropPage()
{
}

void CAnimationPropPage::initDialog()
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	
	QGroupBox* infoGroup = new QGroupBox("Animation Information", this);
	QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);
	
	QLabel* nameLabel = new QLabel("Animation properties will be displayed here", this);
	infoLayout->addWidget(nameLabel);
	
	mainLayout->addWidget(infoGroup);
	mainLayout->addStretch();
	
	setLayout(mainLayout);
}



