#include "GenerateDlg_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QGroupBox>
#include <QLabel>
#include <QButtonGroup>

CGenerateDlg::CGenerateDlg(QWidget* parent) :
	QDialog(parent),
	m_num_langs(0)
{
	setWindowTitle("Generate Game Files");
	resize(249, 185);
	
	memset(m_filename, 0, sizeof(m_filename));
	memset(&m_options, 0, sizeof(m_options));
	memset(m_langids, 0, sizeof(m_langids));
	memset(m_langindices, 0, sizeof(m_langindices));
	
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	
	QHBoxLayout* topLayout = new QHBoxLayout;
	QListWidget* langList = new QListWidget(this);
	langList->setSelectionMode(QAbstractItemView::MultiSelection);
	topLayout->addWidget(langList);
	
	QVBoxLayout* optionsLayout = new QVBoxLayout;
	QGroupBox* fileOptionsGroup = new QGroupBox("File Options", this);
	QVBoxLayout* fileLayout = new QVBoxLayout;
	
	QRadioButton* unicodeRadio = new QRadioButton("CSF Format", this);
	QRadioButton* strRadio = new QRadioButton("\".str\" Format", this);
	unicodeRadio->setChecked(true);
	fileLayout->addWidget(unicodeRadio);
	fileLayout->addWidget(strRadio);
	
	QHBoxLayout* prefixLayout = new QHBoxLayout;
	prefixLayout->addWidget(new QLabel("Prefix:", this));
	QLineEdit* prefixEdit = new QLineEdit(this);
	prefixLayout->addWidget(prefixEdit);
	fileLayout->addLayout(prefixLayout);
	
	fileOptionsGroup->setLayout(fileLayout);
	optionsLayout->addWidget(fileOptionsGroup);
	
	QGroupBox* missingGroup = new QGroupBox("For missing translations", this);
	QVBoxLayout* missingLayout = new QVBoxLayout;
	QRadioButton* idsRadio = new QRadioButton("Show string IDs", this);
	QRadioButton* originalRadio = new QRadioButton("Show original text", this);
	idsRadio->setChecked(true);
	missingLayout->addWidget(idsRadio);
	missingLayout->addWidget(originalRadio);
	missingGroup->setLayout(missingLayout);
	optionsLayout->addWidget(missingGroup);
	
	topLayout->addLayout(optionsLayout);
	mainLayout->addLayout(topLayout);
	
	QHBoxLayout* buttonLayout = new QHBoxLayout;
	QPushButton* invertButton = new QPushButton("&Invert", this);
	QPushButton* selectAllButton = new QPushButton("Select &All", this);
	buttonLayout->addWidget(invertButton);
	buttonLayout->addWidget(selectAllButton);
	buttonLayout->addStretch();
	QPushButton* generateButton = new QPushButton("Generate Files", this);
	QPushButton* cancelButton = new QPushButton("Cancel", this);
	buttonLayout->addWidget(generateButton);
	buttonLayout->addWidget(cancelButton);
	mainLayout->addLayout(buttonLayout);
	
	connect(invertButton, &QPushButton::clicked, this, &CGenerateDlg::onInvert);
	connect(selectAllButton, &QPushButton::clicked, this, &CGenerateDlg::onSelectAll);
	connect(unicodeRadio, &QRadioButton::clicked, this, &CGenerateDlg::onUnicode);
	connect(strRadio, &QRadioButton::clicked, this, &CGenerateDlg::onBabylonstr);
	connect(idsRadio, &QRadioButton::clicked, this, &CGenerateDlg::onIds);
	connect(originalRadio, &QRadioButton::clicked, this, &CGenerateDlg::onOriginal);
	connect(generateButton, &QPushButton::clicked, this, &CGenerateDlg::accept);
	connect(cancelButton, &QPushButton::clicked, this, &CGenerateDlg::reject);
}

CGenerateDlg::~CGenerateDlg()
{
}

void CGenerateDlg::onSelectAll()
{
}

void CGenerateDlg::onInvert()
{
}

void CGenerateDlg::onUnicode()
{
	m_options.format = GN_UNICODE;  // TheSuperHackers @refactor bobtista 01/01/2025 Use GN_UNICODE instead of GN_CSF
}

void CGenerateDlg::onBabylonstr()
{
	m_options.format = GN_BABYLONSTR;  // TheSuperHackers @refactor bobtista 01/01/2025 Use GN_BABYLONSTR instead of GN_STR
}

void CGenerateDlg::onIds()
{
	m_options.untranslated = GN_USEIDS;  // TheSuperHackers @refactor bobtista 01/01/2025 Use untranslated instead of missing, GN_USEIDS instead of GN_IDS
}

void CGenerateDlg::onOriginal()
{
	m_options.untranslated = GN_USEORIGINAL;  // TheSuperHackers @refactor bobtista 01/01/2025 Use untranslated instead of missing, GN_USEORIGINAL instead of GN_ORIGINAL
}

void CGenerateDlg::accept()
{
	QDialog::accept();
}


