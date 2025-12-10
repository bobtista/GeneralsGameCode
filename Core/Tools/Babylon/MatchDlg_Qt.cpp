#include "MatchDlg_Qt.h"
#include "TransDB.h"  // TheSuperHackers @refactor bobtista 01/01/2025 Include TransDB.h for BabylonText and BabylonLabel types
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>

// Global variables for match dialog (defined here for non-Windows builds)
// On Windows, these are defined in MatchDlg.cpp
BabylonText* MatchingBabylonText = NULL;
BabylonText* MatchOriginalText = NULL;
BabylonLabel* MatchLabel = NULL;

CMatchDlg::CMatchDlg(QWidget* parent) : QDialog(parent)
{
	setWindowTitle("Verify Dialog");
	resize(340, 213);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	QLabel* textLabel = new QLabel("Text", this);
	layout->addWidget(textLabel);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	QPushButton* matchButton = new QPushButton("Matches", this);
	QPushButton* noMatchButton = new QPushButton("No Match", this);
	QPushButton* skipButton = new QPushButton("Skip", this);
	buttonLayout->addWidget(noMatchButton);
	buttonLayout->addWidget(skipButton);
	buttonLayout->addStretch();
	buttonLayout->addWidget(matchButton);
	layout->addLayout(buttonLayout);
	
	connect(matchButton, &QPushButton::clicked, this, &CMatchDlg::accept);
	connect(noMatchButton, &QPushButton::clicked, this, &CMatchDlg::reject);
	connect(skipButton, &QPushButton::clicked, this, [this]() { done(100); });
}

CMatchDlg::~CMatchDlg()
{
}


