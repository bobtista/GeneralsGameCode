// Include Qt headers first to ensure they're available
#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QLabel>
#include <QtCore/QObject>

// Include our header FIRST (before any other project headers that might break Qt)
// This ensures the class definition is complete
#include "BabylonDlg_Qt.h"
#include "ui_BabylonDlg.h"

// Now include other headers (which might pull in Windows headers, but Qt is already included)
// Undefine Windows macros that conflict with Qt before including other headers
#ifdef _WIN32
    // Windows Sockets API defines 'connect' as a macro, which conflicts with Qt's connect()
    #ifdef connect
    #undef connect
    #endif
    // Other Windows macros that might conflict
    #ifdef SendMessage
    #undef SendMessage
    #endif
    #ifdef GetMessage
    #undef GetMessage
    #endif
#endif

#include "TransDB.h"
#include "VIEWDBSII_Qt.h"
#include "VerifyDlg_Qt.h"
#include "ExportDlg_Qt.h"
#include "Report_Qt.h"
#include "GenerateDlg_Qt.h"
#include "VerifyTextDlg_Qt.h"
#include "MatchDlg_Qt.h"
#include "RetranslateDlg_Qt.h"
#include "ProceedDlg_Qt.h"
#include "XLStuff.h"
#include "fileops.h"
#include "expimp.h"
#include "loadsave.h"
#include "transcs.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QCloseEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QPaintEvent>
#include <QtCore/QMimeData>
#include <cstdio>
#include <ctime>
#include <cstring>
#ifndef _WIN32
    #include <strings.h>  // For strcasecmp on Unix
#endif
#include <cctype>
#include <cstdlib>

extern char AppTitle[200];
extern TransDB* BabylonstrDB;
extern TransDB* MainDB;
extern char BabylonstrFilename[_MAX_PATH];
extern char MainXLSFilename[_MAX_PATH];
extern char RootPath[_MAX_PATH];
extern char DialogPath[_MAX_PATH];
extern LangID CurrentLanguage;
extern int ViewChanges;

// MainDLG is defined in Babylon_Qt.cpp

CBabylonDlg::CBabylonDlg(QWidget* parent) :
	QDialog(parent),
	ui(new Ui::BabylonDlg),
	m_pAutoProxy(nullptr),
	m_progress_pos(-1),
	m_progress_range(1),
	m_max_index(0),
	m_operate_always(FALSE)
{
	ui->setupUi(this);
	setWindowTitle(QString::fromLocal8Bit(AppTitle));
	
	connect(ui->dbsButton, &QPushButton::clicked, this, &CBabylonDlg::onViewdbs);
	connect(ui->reloadButton, &QPushButton::clicked, this, &CBabylonDlg::onReload);
	connect(ui->updateButton, &QPushButton::clicked, this, &CBabylonDlg::onUpdate);
	connect(ui->saveButton, &QPushButton::clicked, this, &CBabylonDlg::onSave);
	connect(ui->warningsButton, &QPushButton::clicked, this, &CBabylonDlg::onWarnings);
	connect(ui->errorsButton, &QPushButton::clicked, this, &CBabylonDlg::onErrors);
	connect(ui->changesButton, &QPushButton::clicked, this, &CBabylonDlg::onChanges);
	connect(ui->exportButton, &QPushButton::clicked, this, &CBabylonDlg::onExport);
	connect(ui->importButton, &QPushButton::clicked, this, &CBabylonDlg::onImport);
	connect(ui->generateButton, &QPushButton::clicked, this, &CBabylonDlg::onGenerate);
	connect(ui->dialogButton, &QPushButton::clicked, this, &CBabylonDlg::onVerifyDialog);
	connect(ui->translationsButton, &QPushButton::clicked, this, &CBabylonDlg::onTranslations);
	connect(ui->reportsButton, &QPushButton::clicked, this, &CBabylonDlg::onReports);
	connect(ui->resetButton, &QPushButton::clicked, this, &CBabylonDlg::onReset);
	connect(ui->sentButton, &QPushButton::clicked, this, &CBabylonDlg::onSent);
	connect(ui->langComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CBabylonDlg::onSelchangeCombolang);
	connect(ui->langComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), this, &CBabylonDlg::onDblclkCombolang);
	
	ui->logTextEdit->setReadOnly(true);
	ui->progressBar->setRange(0, 100);
	ui->progressBar->setValue(0);
	ui->percentLabel->setText("0%");
	
	Log(AppTitle);
	QDateTime now = QDateTime::currentDateTime();
	QString dateTimeStr = QString("Session Date: %1\n").arg(now.toString("MM/dd/yy hh:mm:ss"));
	Log(dateTimeStr.toLocal8Bit().constData());
	
	Status("Initializing dialog");
	m_operate_always = FALSE;
	
	int index = 0;
	int lang_index = 0;
	LANGINFO* info;
	while ((info = GetLangInfo(lang_index))) {
		ui->langComboBox->addItem(QString::fromLocal8Bit(info->name), QVariant::fromValue((void*)info));
		index++;
		lang_index++;
	}
	m_max_index = index;
	ui->langComboBox->setCurrentIndex(0);
	
	Ready();
	
	QTimer::singleShot(0, this, [this]() { onReload(); });
}

CBabylonDlg::~CBabylonDlg()
{
	delete ui;
}

void CBabylonDlg::Log(const char* string, LogFormat format)
{
	if (format == SAME_LINE) {
		QTextCursor cursor = ui->logTextEdit->textCursor();
		cursor.movePosition(QTextCursor::End);
		ui->logTextEdit->setTextCursor(cursor);
		ui->logTextEdit->insertPlainText(QString::fromLocal8Bit(string));
	} else {
		ui->logTextEdit->append(QString::fromLocal8Bit(string));
	}
	
	QScrollBar* scrollBar = ui->logTextEdit->verticalScrollBar();
	scrollBar->setValue(scrollBar->maximum());
}

void CBabylonDlg::Status(const char* string, int log)
{
	if (log) {
		Log(string);
	}
	
	QString statusText = QString("Status: %1").arg(QString::fromLocal8Bit(string));
	ui->statusLabel->setText(statusText);
}

int CBabylonDlg::SaveLog()
{
	QFile logFile("babylon.log");
	if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
		return FALSE;
	}
	
	QTextStream out(&logFile);
	out << "\nLOG START ******************\n\n";
	out << ui->logTextEdit->toPlainText();
	out << "\nQuiting Babylon\n\nLOG END ******************\n\n";
	
	return TRUE;
}

void CBabylonDlg::InitProgress(int range)
{
	if ((m_progress_range = range) <= 0) {
		m_progress_range = 1;
	}
	
	ui->progressBar->setRange(0, 100);
	m_progress_pos = -1;
	ui->progressBar->setValue(0);
	ui->percentLabel->setText("0%");
}

void CBabylonDlg::SetProgress(int pos)
{
	int new_pos = (pos * 100) / m_progress_range;
	
	if (new_pos > 100) {
		new_pos = 100;
	} else if (new_pos < 0) {
		new_pos = 0;
	}
	
	if (new_pos == m_progress_pos) {
		return;
	}
	
	ui->progressBar->setValue(new_pos);
	m_progress_pos = new_pos;
	ui->percentLabel->setText(QString("%1%").arg(m_progress_pos));
	QApplication::processEvents();
}

void CBabylonDlg::ProgressComplete()
{
	ui->progressBar->setValue(100);
	ui->percentLabel->setText("100%");
	m_progress_pos = 100;
}

void CBabylonDlg::onViewdbs()
{
	VIEWDBSII dlg(this);
	ViewChanges = FALSE;
	dlg.exec();
}

static int progress_count = 0;

static void progress_cb(void)
{
	progress_count++;
	if (MainDLG) {
		MainDLG->SetProgress(progress_count);
	}
}

// getLabelCount function implementation (was static in BabylonDlg.cpp)
int getLabelCount(const char* filename)
{
	int count = 0;
	FILE *fp;
	char buffer[256];  // Local buffer for reading tokens

	if (!(fp = fopen(filename, "rt")))
	{
		return 0;
	}

	while(TRUE)
	{
		if(fscanf(fp, "%s", buffer) == EOF)
			break;

		#ifdef _WIN32
			if (!stricmp(buffer, "END"))
		#else
			if (!strcasecmp(buffer, "END"))  // TheSuperHackers @refactor bobtista 01/01/2025 Use strcasecmp instead of stricmp
		#endif
		{
			count++;
		}
	}

	fclose(fp);

	return count;
}

void CBabylonDlg::onReload()
{
	if (!CanProceed()) {
		return;
	}
	
	BabylonstrDB->Clear();
	BabylonstrDB->ClearChanges();
	MainDB->Clear();
	MainDB->ClearChanges();
	
	int count = getLabelCount(BabylonstrFilename);
	count += GetLabelCountDB(MainXLSFilename);
	progress_count = 0;
	
	ui->errorsButton->setEnabled(false);
	ui->warningsButton->setEnabled(false);
	ui->updateButton->setEnabled(true);
	ui->saveButton->setEnabled(true);
	ui->importButton->setEnabled(true);
	ui->exportButton->setEnabled(true);
	
	InitProgress(count);
	Log("");
	
	int num_errors = 0;
	int num_warnings = 0;
	int str_loaded = FALSE;
	int db_loaded = FALSE;
	int db_readonly = FALSE;
	int db_error = FALSE;
	int do_update = FALSE;
	int errors = 0;
	char buffer[200];
	
	if (FileExists(BabylonstrFilename)) {
		if ((errors = ValidateStrFile(BabylonstrFilename))) {
			if (errors == -1) {
				if (QMessageBox::question(this, "Validation Warning", 
					"Unable to verify string file!\n\nMake sure \"strcheck.exe\" is in your path and \"strcheck.rst\" is writeable.\n\nDo you wish to continue loading?\n\nWarning: Any errors in the string file could cause inappropriate updates to the database.") == QMessageBox::Yes) {
					errors = 0;
				}
			} else {
				sprintf(buffer, "\"%s\" has %d formatting error%s!\n\nFile will not be loaded.", BabylonstrFilename, errors, errors == 1 ? "" : "s");
				QMessageBox::warning(this, "Validation Error", QString::fromLocal8Bit(buffer));
			}
		}
		if (!errors) {
			sprintf(buffer, "Loading \"%s\"...", BabylonstrFilename);
			Status(buffer);
			
			if (!(str_loaded = LoadStrFile(BabylonstrDB, BabylonstrFilename, progress_cb))) {
				Log("FAILED", SAME_LINE);
				BabylonstrDB->Clear();
				BabylonstrDB->ClearChanges();
			}
		} else {
			sprintf(buffer, "Loading \"%s\"...NOT LOADED", BabylonstrFilename);
			Log(buffer);
		}
	} else {
		sprintf(buffer, "Loading \"%s\"...", BabylonstrFilename);
		Status(buffer);
		Log("FILE NOT FOUND", SAME_LINE);
	}
	
	if (str_loaded) {
		sprintf(buffer, "Validating \"%s\"...", BabylonstrFilename);
		Status(buffer, FALSE);
		
		// Check for errors and log them immediately (pass 'this' to get detailed error messages)
		if ((num_errors = BabylonstrDB->Errors(this))) {
			sprintf(buffer, "Generals.str has %d error(s):\n\nSee log area below for details.\n\nAll errors must be fixed before \"Update\" will be enabled.", num_errors);
			QMessageBox::warning(this, "Validation Errors", QString::fromLocal8Bit(buffer));
			ui->updateButton->setEnabled(false);
		} else {
			ui->updateButton->setEnabled(true);
		}
		
		if ((num_warnings = BabylonstrDB->Warnings())) {
			ui->warningsButton->setEnabled(true);
		}
		
		if (num_errors) {
			ui->errorsButton->setEnabled(true);
		}
		
		if (!num_errors && !num_warnings) {
			Log("OK", SAME_LINE);
		} else {
			sprintf(buffer, "%d errors, %d warnings OK", num_errors, num_warnings);
			Log(buffer, SAME_LINE);
		}
	}
	
	sprintf(buffer, "Loading \"%s\"...", MainXLSFilename);
	Status(buffer);
	
	if (FileExists(MainXLSFilename)) {
		if (!(db_loaded = LoadMainDB(MainDB, MainXLSFilename, progress_cb))) {
			Log("FAILED", SAME_LINE);
			MainDB->Clear();
			MainDB->ClearChanges();
			db_error = TRUE;
			ui->updateButton->setEnabled(false);
			ui->saveButton->setEnabled(false);
			ui->exportButton->setEnabled(false);
			ui->importButton->setEnabled(false);
		} else {
			if (FileAttribs(MainXLSFilename) & FA_READONLY) {
				QMessageBox::warning(this, "Read Only", "Database file is readonly!\n\nNo updates will be allowed.\n\nCheckout the database file and reload.");
				db_readonly = TRUE;
				ui->updateButton->setEnabled(false);
				ui->saveButton->setEnabled(false);
				ui->importButton->setEnabled(false);
				Log("READONLY", SAME_LINE);
			} else {
				Log("OK", SAME_LINE);
			}
		}
	} else {
		Log("FILE NOT FOUND", SAME_LINE);
	}
	
	if (str_loaded && !db_error && !num_errors) {
		if (UpdateDB(BabylonstrDB, MainDB, FALSE)) {
			BabylonstrDB->Changed();
			if (db_loaded) {
				if (db_readonly) {
					sprintf(buffer, "\"%s\" has changed!\n\nHowever, as the database is READ ONLY you cannot update the changes.", BabylonstrFilename);
					QMessageBox::information(this, "Database Changed", QString::fromLocal8Bit(buffer));
					do_update = FALSE;
				} else {
					sprintf(buffer, "\"%s\" has changed!\n\nRecommended that you update the database with these new changes.\n\nDo you wish to update now?", BabylonstrFilename);
					do_update = (QMessageBox::question(this, "Database Changed", QString::fromLocal8Bit(buffer)) == QMessageBox::Yes);
				}
			} else {
				sprintf(buffer, "New Database!\n\nRecommended that you update the new database.\n\nDo you wish to update now?");
				do_update = (QMessageBox::question(this, "New Database", QString::fromLocal8Bit(buffer)) == QMessageBox::Yes);
			}
		}
	}
	
	ProgressComplete();
	Ready();
	
	if (do_update) {
		onUpdate();
	}
}

void CBabylonDlg::onUpdate()
{
	if (!CanOperate()) {
		return;
	}
	
	Status("Updating databases...");
	UpdateDB(BabylonstrDB, MainDB, TRUE);
	Ready();
}

void CBabylonDlg::onSave()
{
	if (CanOperate()) {
		SaveMainDB();
	}
}

int CBabylonDlg::SaveMainDB()
{
	TransDB* db = MainDB;
	const char* filename = MainXLSFilename;
	
	if (!db) {
		return TRUE;
	}
	
	if (!db->IsChanged()) {
		return TRUE;
	}
	
	int attribs = FileAttribs(filename);
	
	if (attribs & FA_READONLY) {
		char buffer[100];
		sprintf(buffer, "Cannot save changes!\n\nFile \"%s\" is read only", filename);
		QMessageBox::warning(this, "Save Failed", QString::fromLocal8Bit(buffer));
		sprintf(buffer, "Cannot save to \"%s\". File is read only", filename);
		Log(buffer);
		Status("Save failed", FALSE);
		return FALSE;
	}
	
	Log("");
	Status("Saving main database...");
	
	if (attribs != FA_NOFILE) {
		MakeBackupFile(filename);
	}
	
	if (!WriteMainDB(db, filename, this)) {
		RestoreBackupFile(filename);
		Log("FAILED", SAME_LINE);
		Status("Save failed", FALSE);
		return FALSE;
	} else {
		Log("OK", SAME_LINE);
	}
	
	Ready();
	return TRUE;
}

void CBabylonDlg::onWarnings()
{
	if (BabylonstrDB) {
		BabylonstrDB->Warnings(this);
	}
}

void CBabylonDlg::onErrors()
{
	if (BabylonstrDB) {
		BabylonstrDB->Errors(this);
	}
}

void CBabylonDlg::onChanges()
{
	ViewChanges = TRUE;
	onViewdbs();
}

void CBabylonDlg::onExport()
{
	if (CanOperate()) {
		CExportDlg dlg(this);
		if (dlg.exec() == QDialog::Accepted) {
			ExportTranslations(MainDB, dlg.Filename(), dlg.Language(), dlg.Options(), this);
		}
	}
}

void CBabylonDlg::onImport()
{
	if (CanOperate()) {
		QString filename = QFileDialog::getOpenFileName(this, "Import Translations", "", "Translation Files (*.xls *.str)");
		if (!filename.isEmpty()) {
			ImportTranslations(MainDB, filename.toLocal8Bit().constData(), this);
		}
	}
}

void CBabylonDlg::onGenerate()
{
	if (CanOperate()) {
		CGenerateDlg dlg(this);
		if (dlg.exec() == QDialog::Accepted) {
			GenerateGameFiles(MainDB, dlg.FilePrefix(), dlg.Options(), dlg.Languages(), this);  // TheSuperHackers @refactor bobtista 01/01/2025 Use FilePrefix() instead of Filename()
		}
	}
}

void CBabylonDlg::onVerifyDialog()
{
	if (CanOperate()) {
		VerifyDialog(MainDB, CurrentLanguage);
	}
}

void CBabylonDlg::VerifyDialog(TransDB* db, LangID langid)
{
	VerifyDlg dlg(this);
	dlg.SetDB(db);
	dlg.SetLang(langid);
	dlg.exec();
}

void CBabylonDlg::VerifyTranslations(TransDB* db, LangID langid)
{
	CVerifyTextDlg dlg("", "", this);  // TheSuperHackers @refactor bobtista 01/01/2025 CVerifyTextDlg constructor requires expectedText and actualText parameters
	dlg.SetDB(db);
	dlg.SetLang(langid);
	dlg.exec();
}

void CBabylonDlg::onTranslations()
{
	if (CanOperate()) {
		VerifyTranslations(MainDB, CurrentLanguage);
	}
}

void CBabylonDlg::onSelchangeCombolang()
{
	int index = ui->langComboBox->currentIndex();
	
	if (index >= 0 && index < m_max_index) {
		LANGINFO* info = (LANGINFO*)ui->langComboBox->itemData(index).value<void*>();
		if (info) {
			CurrentLanguage = info->langid;
		} else {
			CurrentLanguage = LANGID_UNKNOWN;
		}
	} else {
		CurrentLanguage = LANGID_UNKNOWN;
	}
}

void CBabylonDlg::onReports()
{
	if (CanOperate()) {
		CReport dlg(this);
		if (dlg.exec() == QDialog::Accepted) {
			GenerateReport(MainDB, dlg.Filename(), dlg.Options(), dlg.Languages(), this);
		}
	}
}

void CBabylonDlg::onDblclkCombolang()
{
}

void CBabylonDlg::onReset()
{
	if (CurrentLanguage != LANGID_UNKNOWN) {
		char buffer[200];
		sprintf(buffer, "Are you sure you want to invalidate all %s dialog?", GetLangName(CurrentLanguage));
		if (QMessageBox::question(this, "Confirm Reset", QString::fromLocal8Bit(buffer)) == QMessageBox::Yes) {
			MainDB->InvalidateDialog(CurrentLanguage);
		}
	}
}

void CBabylonDlg::onSent()
{
	if (CanOperate()) {
		QString filename = QFileDialog::getOpenFileName(this, "Update Sent Translations", "", "Translation Files (*.xls)");
		if (!filename.isEmpty()) {
			UpdateSentTranslations(MainDB, filename.toLocal8Bit().constData(), this);
		}
	}
}

void CBabylonDlg::closeEvent(QCloseEvent* event)
{
	if (CanExit()) {
		if (SaveLog()) {
			event->accept();
		} else {
			QMessageBox::warning(this, "Error", "Failed to save log file!");
			event->accept();
		}
	} else {
		event->ignore();
	}
}

void CBabylonDlg::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
	}
}

void CBabylonDlg::dropEvent(QDropEvent* event)
{
	QList<QUrl> urls = event->mimeData()->urls();
	if (!urls.isEmpty()) {
		QString filePath = urls.first().toLocalFile();
	}
}

void CBabylonDlg::paintEvent(QPaintEvent* event)
{
	QDialog::paintEvent(event);
}

bool CBabylonDlg::CanExit()
{
	if (m_pAutoProxy != NULL) {
		hide();
		return FALSE;
	}
	return TRUE;
}

int CBabylonDlg::CanProceed(void)
{
	if (MainDB && MainDB->IsChanged()) {
		if (QMessageBox::question(this, "Unsaved Changes", 
			"The database has been modified.\n\nDo you wish to save your changes?") == QMessageBox::Yes) {
			if (!SaveMainDB()) {
				return FALSE;
			}
		}
	}
	return TRUE;
}

int CBabylonDlg::CanOperate(void)
{
	if (m_operate_always) {
		return TRUE;
	}
	
	if (!MainDB || !BabylonstrDB) {
		return FALSE;
	}
	
	if (BabylonstrDB->IsChanged() || BabylonstrDB->HasErrors()) {
		const char* string = "Unknown problem!\n\n\nProceed anyway?";
		
		if (BabylonstrDB->HasErrors()) {
			string = "Generals.str has errors! As a result the translation database is not up to date!\n\nRecommend you fix problems in Generals.str before proceeding.\n\n\n\nDo you wish to continue anyway?";
		}
		
		if (BabylonstrDB->IsChanged()) {
			string = "The translation database is not up to date! Generals.str has changed since the last time the database was updated.\n\nRecommend you update the database before proceeding.\n\n\n\nDo you wish to continue anyway?";
		}
		
		ProceedDlg dlg(QString::fromLocal8Bit(string), this);
		int result = dlg.exec();
		
		if (result == IDALWAYS) {
			m_operate_always = TRUE;
		}
		
		return (result != QMessageBox::No);
	}
	
	return TRUE;
}

extern int getLabelCount(const char* filename);
extern BabylonText* MatchingBabylonText;
extern BabylonText* MatchOriginalText;
extern BabylonLabel* MatchLabel;

#define MAX_INFO_LEN (2*1024)

typedef struct {
	char comment[MAX_INFO_LEN+1];
	char context[MAX_INFO_LEN+1];
	char speaker[MAX_INFO_LEN+1];
	char listener[MAX_INFO_LEN+1];
	char wave[MAX_INFO_LEN+1];
	int maxlen;
} INFO;

static INFO global_info_qt;
static INFO local_info_qt;

static void init_info_qt(INFO* info)
{
	memset(info, 0, sizeof(INFO));
	info->maxlen = -1;
}

static void removeLeadingAndTrailing_qt(char* buffer)
{
	char* start = buffer;
	while (*start && isspace(*start)) {
		start++;
	}
	
	char* end = start + strlen(start) - 1;
	while (end > start && isspace(*end)) {
		*end = '\0';
		end--;
	}
	
	if (start != buffer) {
		memmove(buffer, start, strlen(start) + 1);
	}
}

static int readToEndOfQuote_qt(FILE* file, char* in, char* out, char* wavefile, int maxBufLen, int in_comment)
{
	int slash = FALSE;
	int state = 0;
	int line_start = FALSE;
	char ch;
	int new_lines = 0;
	int ccount = 0;
	
	while (maxBufLen) {
		if (in) {
			if (!(ch = *in++)) {
				in = NULL;
				ch = getc(file);
			}
		} else {
			ch = getc(file);
		}
		
		if (ch == EOF) {
			QMessageBox::warning(nullptr, "Error", "Missing terminating quote");
			return new_lines;
		}
		
		if (ch == '\n') {
			line_start = TRUE;
			if (!in) {
				new_lines++;
			}
			slash = FALSE;
			ccount = 0;
			ch = ' ';
		} else if (line_start && (ch == '/' || isspace(ch))) {
			continue;
		} else if (ch == '\\' && !slash) {
			slash = TRUE;
		} else if (ch == '\\' && slash) {
			slash = FALSE;
		} else if (ch == '"' && !slash) {
			break;
		} else {
			slash = FALSE;
		}
		
		if (isspace(ch)) {
			ch = ' ';
		} else {
			line_start = FALSE;
		}
		
		*out++ = ch;
		maxBufLen--;
	}
	
	*out = 0;
	
	if (!wavefile) {
		return new_lines;
	}
	
	int len = 0;
	while (TRUE) {
		if (in) {
			if (!(ch = *in++)) {
				in = NULL;
				ch = getc(file);
			}
		} else {
			ch = getc(file);
		}
		
		if (ch == '\n' || ch == EOF) {
			if (!in) {
				new_lines++;
			}
			break;
		}
		
		switch (state) {
			case 0:
				if (isspace(ch) || ch == '=') {
					break;
				}
				state = 1;
			case 1:
				if (((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')) {
					*wavefile++ = ch;
					len++;
					break;
				}
				state = 2;
			case 2:
				break;
		}
	}
	
	*wavefile = 0;
	if (len) {
		if ((ch = *(wavefile-1)) < '0' || ch > '9') {
			*(wavefile-1) = 0;
		}
	}
	
	return new_lines;
}

static void StripSpaces_qt(char* string)
{
	char* start = string;
	while (*start && isspace(*start)) {
		start++;
	}
	
	char* end = start + strlen(start) - 1;
	while (end > start && isspace(*end)) {
		*end = '\0';
		end--;
	}
	
	if (start != string) {
		memmove(string, start, strlen(start) + 1);
	}
}

static void ConvertMetaChars_qt(char* string)
{
	char* src = string;
	char* dst = string;
	
	while (*src) {
		if (*src == '\\' && src[1]) {
			switch (src[1]) {
				case 'n':
					*dst++ = '\n';
					src += 2;
					break;
				case 't':
					*dst++ = '\t';
					src += 2;
					break;
				case 'r':
					*dst++ = '\r';
					src += 2;
					break;
				case '\\':
					*dst++ = '\\';
					src += 2;
					break;
				default:
					*dst++ = *src++;
					break;
			}
		} else {
			*dst++ = *src++;
		}
	}
	*dst = '\0';
}

static int getString_qt(FILE* file, char* in, char* out)
{
	int bytes = 2*1024;
	int new_lines = 0;
	char ch;
	char* ptr = out;
	
	while ((ch = *in++) && ch != '\n' && bytes) {
		*ptr++ = ch;
		bytes--;
	}
	
	*ptr = 0;
	ConvertMetaChars_qt(out);
	StripSpaces_qt(out);
	return new_lines;
}

static char* getToken_qt(char* buffer, char* token, int bytes)
{
	char ch;
	int state = 0;
	*token = 0;
	
	while ((ch = *buffer) && ch != '\n' && bytes) {
		switch (state) {
			case 0:
				if (ch == '/' || isspace(ch)) {
					break;
				}
				state = 1;
			case 1:
				if (!isspace(ch) && ch != ':') {
					*token++ = ch;
					bytes--;
					break;
				}
				*token = 0;
				state = 2;
			case 2:
				if (ch != ':') {
					break;
				}
				state = 3;
				break;
			case 3:
				if (isspace(ch)) {
					break;
				}
				return buffer;
		}
		buffer++;
	}
	
	*token = 0;
	return buffer;
}

static int parseComment_qt(FILE* file, char* buffer, INFO* info)
{
	char token[256];
	int new_lines = 0;
	
	buffer = getToken_qt(buffer, token, sizeof(token)-1);
	
	if (!token[0]) {
		return new_lines;
	}
	
        #ifdef _WIN32
            if (!stricmp(token, "COMMENT")) {
        #else
            if (!strcasecmp(token, "COMMENT")) {  // TheSuperHackers @refactor bobtista 01/01/2025 Use strcasecmp instead of stricmp
        #endif
		new_lines += getString_qt(file, buffer, info->comment);
	#ifdef _WIN32
	} else if (!stricmp(token, "CONTEXT")) {
	#else
	} else if (!strcasecmp(token, "CONTEXT")) {
	#endif
		new_lines += getString_qt(file, buffer, info->context);
	#ifdef _WIN32
	} else if (!stricmp(token, "SPEAKER")) {
	#else
	} else if (!strcasecmp(token, "SPEAKER")) {
	#endif
		new_lines += getString_qt(file, buffer, info->speaker);
	#ifdef _WIN32
	} else if (!stricmp(token, "LISTENER")) {
	#else
	} else if (!strcasecmp(token, "LISTENER")) {
	#endif
		new_lines += getString_qt(file, buffer, info->listener);
	#ifdef _WIN32
	} else if (!stricmp(token, "MAXLEN")) {
	#else
	} else if (!strcasecmp(token, "MAXLEN")) {
	#endif
		info->maxlen = atoi(buffer);
	#ifdef _WIN32
	} else if (!stricmp(token, "WAVE")) {
	#else
	} else if (!strcasecmp(token, "WAVE")) {
	#endif
		new_lines += getString_qt(file, buffer, info->wave);
	}
	
	return new_lines;
}

int CBabylonDlg::LoadStrFile(TransDB* db, const char* filename, void (*cb)(void))
{
	FILE* file = NULL;
	BabylonLabel* label = NULL;
	int status = FALSE;
	int line_number = 0;
	
	static char buffer[100*1024];
	static char buffer2[100*1024];
	static char buffer3[100*1024];
	
	init_info_qt(&global_info_qt);
	
	if (!(file = fopen(filename, "rt"))) {
		goto exit;
	}
	
	while (fgets(buffer, sizeof(buffer)-1, file)) {
		line_number++;
		removeLeadingAndTrailing_qt(buffer);
		
		if (!buffer[0] || (buffer[0] == '/' && buffer[1] == '/')) {
			line_number += parseComment_qt(file, buffer, &global_info_qt);
			continue;
		}
		
		label = new BabylonLabel();
		label->SetName(buffer);
		label->LockName();
		label->SetLineNumber(line_number);
		db->AddLabel(label);
		
		local_info_qt = global_info_qt;
		local_info_qt.wave[0] = 0;
		
		while (TRUE) {
			if (!fgets(buffer, sizeof(buffer)-1, file)) {
				QMessageBox::warning(this, "Error", "Unexpected end of file");
				goto exit;
			}
			
			line_number++;
			removeLeadingAndTrailing_qt(buffer);
			
			#ifdef _WIN32
				if (!stricmp(buffer, "END")) {
			#else
				if (!strcasecmp(buffer, "END")) {
			#endif
				break;
			}
			
			if (!buffer[0] || (buffer[0] == '/' && buffer[1] == '/')) {
				line_number += parseComment_qt(file, buffer, &local_info_qt);
				continue;
			}
			
			if (buffer[0] == '"') {
				int line = line_number;
				strcat(buffer, "\n");
				line_number += readToEndOfQuote_qt(file, &buffer[1], buffer2, buffer3, sizeof(buffer2)-1, FALSE);
				BabylonText* text = new BabylonText();
				text->Set(buffer2);
				text->FormatMetaString();
				text->LockText();
				if (buffer3[0]) {
					text->SetWave(buffer3);
				} else {
					text->SetWave(local_info_qt.wave);
				}
				text->SetLineNumber(line);
				label->AddText(text);
			}
		}
		
		label->SetComment(local_info_qt.comment);
		label->SetContext(local_info_qt.context);
		label->SetSpeaker(local_info_qt.speaker);
		label->SetListener(local_info_qt.listener);
		// Only set MaxLen if it was explicitly set (>= 0), otherwise leave it at 0 (no limit)
		if (local_info_qt.maxlen >= 0) {
			label->SetMaxLen(local_info_qt.maxlen);
		}
		
		if (cb) {
			cb();
		}
		label = NULL;
	}
	
	status = TRUE;
	
exit:
	db->ClearChanges();
	delete label;
	
	if (file) {
		fclose(file);
	}
	
	return status;
}

int CBabylonDlg::ValidateStrFile(const char* filename)
{
	return 0;
}

int CBabylonDlg::MatchText(BabylonText* text, BabylonLabel* label, BabylonText** match)
{
	CMatchDlg dlg(this);
	*match = NULL;
	
	MatchOriginalText = text;
	MatchLabel = label;
	
	int result = dlg.exec();
	
	if (result == QDialog::Accepted) {
		*match = MatchingBabylonText;
	}
	
	return result;
}

int CBabylonDlg::RetranslateText(BabylonText* text, BabylonText* label)
{
	RetranslateDlg dlg(this);
	dlg.newtext = text;
	dlg.oldtext = label;
	
	int result = dlg.exec();
	if (result == 101) {
		return QDialog::Accepted;
	}
	return result;
}

int CBabylonDlg::UpdateLabel(BabylonLabel* source, BabylonLabel* destination, UPDATEINFO& info, int update, int skip)
{
	BabylonText* stext, *dtext;
	ListSearch sh;
	TransDB* destDB, *srcDB;
	int label_modified = FALSE;
	
	destination->ClearMatched();
	source->ClearMatched();
	destDB = destination->DB();
	srcDB = source->DB();
	
	stext = source->FirstText(sh);
	
	while (stext) {
		dtext = destDB->FindText(stext->Get());
		
		while (dtext && (dtext->Label() != destination)) {
			dtext = destDB->FindNextText();
		}
		
		if (dtext && dtext->Matched()) {
			QMessageBox::critical(this, "Error", "Fatal error: substring already matched");
			return QDialog::Rejected;
		}
		
		if (dtext) {
			dtext->Match(stext);
			stext->Match(dtext);
		}
		
		stext = source->NextText(sh);
	}
	
	stext = source->FirstText(sh);
	
	while (stext) {
		if (destination->AllMatched()) {
			break;
		}
		
		if (!stext->Matched()) {
			int result;
			BabylonText* match = NULL;
			
			if (update && !skip) {
				if (destination->DB()->MultiTextAllowed()) {
					result = MatchText(stext, destination, &match);
				} else {
					ListSearch tsh;
					BabylonText* oldtext = destination->FirstText(tsh);
					if (!oldtext) {
						break;
					}
					result = RetranslateText(stext, oldtext);
					match = oldtext;
				}
			} else {
				result = 100;
			}
			
			if (result == QDialog::Rejected || result == 100) {
				return result;
			}
			
			if (match) {
				stext->Match(match);
				match->Match(stext);
			}
			stext->Processed();
		}
		stext = source->NextText(sh);
	}
	
	dtext = destination->FirstText(sh);
	
	while (dtext) {
		if ((stext = (BabylonText*)dtext->Matched())) {
			if (wcscmp(dtext->Get(), stext->Get())) {
				if (update) {
					dtext->Set(stext->Get());
				}
				info.modified_strings++;
				label_modified = TRUE;
				info.changes++;
			}
			if (wcsicmp(dtext->Wave(), stext->Wave())) {
				if (update) {
					dtext->SetWave(stext->Wave());
				}
				info.updated_waves++;
				label_modified = TRUE;
				info.changes++;
			}
			if (dtext->Retranslate()) {
				if (update) {
					dtext->IncRevision();
				}
				label_modified = TRUE;
			}
			dtext->SetRetranslate(FALSE);
		}
		
		dtext = destination->NextText(sh);
	}
	
	dtext = destination->FirstText(sh);
	
	while (dtext) {
		BabylonText* next = destination->NextText(sh);
		
		if (!dtext->Matched()) {
			if (update) {
				dtext->Remove();
				destDB->AddObsolete(dtext);
			}
			info.deleted_strings++;
			label_modified = TRUE;
			info.changes++;
		}
		
		dtext = next;
	}
	
	stext = source->FirstText(sh);
	
	while (stext) {
		if (!stext->Matched()) {
			if (update) {
				dtext = stext->Clone();
				destination->AddText(dtext);
			}
			info.new_strings++;
			label_modified = TRUE;
			info.changes++;
		}
		
		stext = source->NextText(sh);
	}
	
	if (wcscmp(destination->Comment(), source->Comment())) {  // TheSuperHackers @refactor bobtista 01/01/2025 Use wcscmp for wide strings
		if (update) {
			destination->SetComment(source->Comment());
		}
		info.updated_comments++;
		label_modified = TRUE;
		info.changes++;
	}
	
	if (wcscmp(destination->Context(), source->Context())) {  // TheSuperHackers @refactor bobtista 01/01/2025 Use wcscmp for wide strings
		if (update) {
			destination->SetContext(source->Context());
		}
		info.updated_contexts++;
		label_modified = TRUE;
		info.changes++;
	}
	
	if (wcscmp(destination->Speaker(), source->Speaker())) {  // TheSuperHackers @refactor bobtista 01/01/2025 Use wcscmp for wide strings
		if (update) {
			destination->SetSpeaker(source->Speaker());
		}
		info.updated_speakers++;
		label_modified = TRUE;
		info.changes++;
	}
	
	if (wcscmp(destination->Listener(), source->Listener())) {  // TheSuperHackers @refactor bobtista 01/01/2025 Use wcscmp for wide strings
		if (update) {
			destination->SetListener(source->Listener());
		}
		info.updated_listeners++;
		label_modified = TRUE;
		info.changes++;
	}
	
	if (destination->MaxLen() != source->MaxLen()) {
		if (update) {
			destination->SetMaxLen(source->MaxLen());
		}
		label_modified = TRUE;
		info.updated_maxlen++;
		info.changes++;
	}
	
	if (label_modified) {
		if (update) {
			source->ClearChanges();
		} else {
			source->Changed();
		}
		info.modified_labels++;
	}
	
	return QDialog::Accepted;
}

int CBabylonDlg::UpdateDB(TransDB* source, TransDB* destination, int update)
{
	char buffer[200];
	memset(buffer, 0, sizeof(buffer));
	
	if (update) {
		sprintf(buffer, "Updating \"%s\" from \"%s\"...", destination->Name(), source->Name());
		Log("");
		Status(buffer);
	} else {
		Status("Checking for changes...", FALSE);
	}
	
	source->ClearProcessed();
	destination->ClearProcessed();
	
	if (update) {
		InitProgress(source->NumLabels());
	}
	
	UPDATEINFO info;
	memset(&info, 0, sizeof(info));
	
	ListSearch sh;
	BabylonLabel* slabel = source->FirstLabel(sh);
	int count = 0;
	
	while (slabel) {
		if (update) {
			SetProgress(count++);
		}
		
		BabylonLabel* dlabel = destination->FindLabel(slabel->Name());
		
		if (!dlabel) {
			info.new_labels++;
			if (update) {
				dlabel = slabel->Clone();
				destination->AddLabel(dlabel);
			}
		} else {
			int result = UpdateLabel(slabel, dlabel, info, update, FALSE);
			if (result == QDialog::Rejected) {
				return result;
			}
		}
		
		slabel->Processed();
		slabel = source->NextLabel(sh);
	}
	
	// TheSuperHackers @refactor bobtista 01/01/2025 ListSearch doesn't have Reset(), use FirstNode to reset
	sh.FirstNode(destination->GetLabelsList());
	BabylonLabel* dlabel = destination->FirstLabel(sh);
	
	while (dlabel) {
		// TheSuperHackers @refactor bobtista 01/01/2025 Get next label BEFORE removing current one to avoid iterator invalidation
		BabylonLabel* next_label = destination->NextLabel(sh);
		
		if (!dlabel->IsProcessed()) {  // TheSuperHackers @refactor bobtista 01/01/2025 Use IsProcessed() instead of Processed() which returns void
			info.deleted_labels++;
			if (update) {
				// TheSuperHackers @refactor bobtista 01/01/2025 Remove and delete label (matches original MFC code)
				dlabel->Remove();
				delete dlabel;
			}
		}
		
		dlabel = next_label;  // Use saved next_label instead of calling NextLabel() again
	}
	
	if (update) {
		ProgressComplete();
		sprintf(buffer, "Update complete: %d new, %d deleted, %d modified labels", 
			info.new_labels, info.deleted_labels, info.modified_labels);
		Log(buffer);
		Status("Update complete", FALSE);
		
		// TheSuperHackers @refactor bobtista 01/01/2025 Clear changes on source database after successful update (matches original MFC code)
		// This prevents the "database is out of date" prompt from appearing on every action
		// Only clear if no labels were skipped (matches original behavior)
		if (!info.skipped_labels) {
			source->ClearChanges();
		}
	}
	
	return QDialog::Accepted;
}


