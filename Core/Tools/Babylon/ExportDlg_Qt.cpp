#include "ExportDlg_Qt.h"
#include "ui_ExportDlg.h"
#include "transcs.h"
#include <QButtonGroup>
#include <QDir>
#include <cstring>

extern LANGINFO* GetLangInfo(int index);

CExportDlg::CExportDlg(QWidget* parent) :
	QDialog(parent),
	ui(new Ui::ExportDlg),
	m_langid(LANGID_UNKNOWN),
	m_got_lang(FALSE),
	m_max_index(0)
{
	ui->setupUi(this);
	
	memset(m_filename, 0, sizeof(m_filename));
	memset(&m_options, 0, sizeof(m_options));
	
	connect(ui->langComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CExportDlg::onSelchangeCombolang);
	connect(ui->langComboBox, QOverload<int>::of(&QComboBox::activated), this, &CExportDlg::onSelendokCombolang);
	connect(ui->exportButton, &QPushButton::clicked, this, &CExportDlg::accept);
	connect(ui->cancelButton, &QPushButton::clicked, this, &CExportDlg::reject);
	
	int index = 0;
	int lang_index = 0;
	LANGINFO* info;
	while ((info = GetLangInfo(lang_index))) {
		ui->langComboBox->addItem(QString::fromLocal8Bit(info->name), QVariant::fromValue((void*)info));
		index++;
		lang_index++;
	}
	m_max_index = index;
	
	if (m_max_index > 0) {
		ui->langComboBox->setCurrentIndex(0);
		onSelchangeCombolang();
	}
	
	ui->radioChangesRadioButton->setChecked(true);
	QDir currentDir = QDir::current();
	ui->filenameLineEdit->setText(currentDir.dirName());
}

CExportDlg::~CExportDlg()
{
	delete ui;
}

void CExportDlg::onSelchangeCombolang()
{
	int index = ui->langComboBox->currentIndex();
	
	if (index >= 0 && index < m_max_index) {
		LANGINFO* info = (LANGINFO*)ui->langComboBox->itemData(index).value<void*>();
		if (info) {
			m_langid = info->langid;
			m_got_lang = TRUE;
		} else {
			m_langid = LANGID_UNKNOWN;
			m_got_lang = FALSE;
		}
	} else {
		m_langid = LANGID_UNKNOWN;
		m_got_lang = FALSE;
	}
}

void CExportDlg::onSelendokCombolang()
{
	onSelchangeCombolang();
}

void CExportDlg::accept()
{
	QString buffer = ui->filenameLineEdit->text();
	QDir currentDir = QDir::current();
	QString currentPath = currentDir.absolutePath();
	
	QString filename = buffer;
	int dotIndex = filename.lastIndexOf('.');
	if (dotIndex >= 0) {
		filename = filename.left(dotIndex);
	}
	
	QString fullPath = currentPath + "/" + filename;
	strncpy(m_filename, fullPath.toLocal8Bit().constData(), sizeof(m_filename) - 1);
	m_filename[sizeof(m_filename) - 1] = '\0';
	
	if (ui->radioAllRadioButton->isChecked()) {
		m_options.filter = TR_ALL;
	} else if (ui->radioDialogRadioButton->isChecked()) {
		m_options.filter = TR_DIALOG;
	} else if (ui->radioNonDialogRadioButton->isChecked()) {
		m_options.filter = TR_NONDIALOG;
	} else if (ui->radioSampleRadioButton->isChecked()) {
		m_options.filter = TR_SAMPLE;
	} else if (ui->radioUnverifiedRadioButton->isChecked()) {
		m_options.filter = TR_UNVERIFIED;
	} else if (ui->radioMissingRadioButton->isChecked()) {
		m_options.filter = TR_MISSING_DIALOG;
	} else if (ui->radioUnsentRadioButton->isChecked()) {
		m_options.filter = TR_UNSENT;
	} else {
		m_options.filter = TR_CHANGES;
	}
	
	m_options.include_comments = FALSE;
	m_options.include_translations = ui->checkTransCheckBox->isChecked();
	
	QDialog::accept();
}



