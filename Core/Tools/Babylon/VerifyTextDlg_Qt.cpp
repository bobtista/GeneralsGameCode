#include "VerifyTextDlg_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialog>  // For QDialog::Accepted/Rejected

CVerifyTextDlg::CVerifyTextDlg(const char* expectedText, const char* actualText, QWidget* parent) :
	QDialog(parent),
	m_db(nullptr),
	m_langid(LANGID_UNKNOWN)
{
	setWindowTitle("Verify Text");
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	QLabel* expectedLabel = new QLabel(QString("Expected: %1").arg(expectedText ? expectedText : ""), this);
	QLabel* actualLabel = new QLabel(QString("Actual: %1").arg(actualText ? actualText : ""), this);
	layout->addWidget(expectedLabel);
	layout->addWidget(actualLabel);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	QPushButton* matchButton = new QPushButton("Matches", this);
	QPushButton* noMatchButton = new QPushButton("No Match", this);
	buttonLayout->addWidget(noMatchButton);
	buttonLayout->addStretch();
	buttonLayout->addWidget(matchButton);
	layout->addLayout(buttonLayout);
	
	connect(matchButton, &QPushButton::clicked, this, &CVerifyTextDlg::onMatch);
	connect(noMatchButton, &QPushButton::clicked, this, &CVerifyTextDlg::onNoMatch);
}

CVerifyTextDlg::~CVerifyTextDlg()
{
}

void CVerifyTextDlg::onMatch()
{
	accept();
}

void CVerifyTextDlg::onNoMatch()
{
	reject();
}


