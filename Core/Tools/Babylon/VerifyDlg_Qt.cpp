#include "VerifyDlg_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>

VerifyDlg::VerifyDlg(QWidget* parent) :
	QDialog(parent),
	m_db(nullptr),
	m_langid(LANGID_UNKNOWN),
	m_babylon_text(nullptr)
{
	setWindowTitle("Verify Dialog");
	resize(340, 213);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	QLabel* textLabel = new QLabel("Text", this);
	layout->addWidget(textLabel);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	QPushButton* matchButton = new QPushButton("Matches", this);
	QPushButton* stopButton = new QPushButton("Stop Verifying", this);
	buttonLayout->addWidget(stopButton);
	buttonLayout->addStretch();
	buttonLayout->addWidget(matchButton);
	layout->addLayout(buttonLayout);
	
	connect(matchButton, &QPushButton::clicked, this, &VerifyDlg::onMatch);
	connect(stopButton, &QPushButton::clicked, this, &VerifyDlg::reject);
}

VerifyDlg::~VerifyDlg()
{
}

void VerifyDlg::onMatch()
{
	accept();
}

void VerifyDlg::onNoMatch()
{
	reject();
}

void VerifyDlg::onStop()
{
	reject();
}

void VerifyDlg::onPlay()
{
}

void VerifyDlg::onPause()
{
}



