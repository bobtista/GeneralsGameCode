#include "Report_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>

CReport::CReport(QWidget* parent) :
	QDialog(parent),
	m_num_langs(0)
{
	setWindowTitle("Create Reports");
	
	memset(m_filename, 0, sizeof(m_filename));
	memset(&m_options, 0, sizeof(m_options));
	memset(m_langids, 0, sizeof(m_langids));
	memset(m_langindices, 0, sizeof(m_langindices));
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	QListWidget* langList = new QListWidget(this);
	langList->setSelectionMode(QAbstractItemView::MultiSelection);
	layout->addWidget(langList);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	QPushButton* invertButton = new QPushButton("&Invert", this);
	QPushButton* selectAllButton = new QPushButton("Select &All", this);
	buttonLayout->addWidget(invertButton);
	buttonLayout->addWidget(selectAllButton);
	buttonLayout->addStretch();
	QPushButton* okButton = new QPushButton("OK", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);
	
	connect(invertButton, &QPushButton::clicked, this, &CReport::onInvert);
	connect(selectAllButton, &QPushButton::clicked, this, &CReport::onSelectAll);
	connect(okButton, &QPushButton::clicked, this, &CReport::accept);
	connect(cancelButton, &QPushButton::clicked, this, &CReport::reject);
}

CReport::~CReport()
{
}

void CReport::onSelectAll()
{
}

void CReport::onInvert()
{
}

void CReport::onShowDetails()
{
}

void CReport::accept()
{
	QDialog::accept();
}



