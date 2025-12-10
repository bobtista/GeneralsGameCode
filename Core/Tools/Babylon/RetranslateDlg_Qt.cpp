#include "RetranslateDlg_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

RetranslateDlg::RetranslateDlg(QWidget* parent) :
	QDialog(parent),
	newtext(nullptr),
	oldtext(nullptr)
{
	setWindowTitle("Retranslate");
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	QLabel* textLabel = new QLabel("Text", this);
	layout->addWidget(textLabel);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	QPushButton* retranslateButton = new QPushButton("Retranslate", this);
	QPushButton* noRetranslateButton = new QPushButton("No Retranslate", this);
	QPushButton* skipButton = new QPushButton("Skip", this);
	QPushButton* skipAllButton = new QPushButton("Skip All", this);
	buttonLayout->addWidget(noRetranslateButton);
	buttonLayout->addWidget(skipButton);
	buttonLayout->addWidget(skipAllButton);
	buttonLayout->addStretch();
	buttonLayout->addWidget(retranslateButton);
	layout->addLayout(buttonLayout);
	
	connect(retranslateButton, &QPushButton::clicked, this, &RetranslateDlg::onRetranslate);
	connect(noRetranslateButton, &QPushButton::clicked, this, &RetranslateDlg::onNoRetranslate);
	connect(skipButton, &QPushButton::clicked, this, &RetranslateDlg::onSkip);
	connect(skipAllButton, &QPushButton::clicked, this, &RetranslateDlg::onSkipAll);
}

RetranslateDlg::~RetranslateDlg()
{
}

void RetranslateDlg::onRetranslate()
{
	accept();
}

void RetranslateDlg::onNoRetranslate()
{
	done(101);
}

void RetranslateDlg::onSkip()
{
	done(100);
}

void RetranslateDlg::onSkipAll()
{
	done(102);
}



