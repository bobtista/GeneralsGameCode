#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QScrollBar>
#include <QtCore/QObject>
#include <string>

namespace Ui {
    class BabylonDlg;
}

// Forward declarations to avoid circular dependencies
class TransDB;
class BabylonText;
class BabylonLabel;
// LangID is an enum defined in TransDB.h - we can't forward declare it easily
// So we'll include TransDB.h here to get the enum definition
// This is safe because TransDB.h only forward declares CBabylonDlg, not the full class
#include "TransDB.h"

typedef enum
{
	SAME_LINE,
	NEW_LINE
} LogFormat;

typedef struct
{
	int new_strings;
	int deleted_strings;
	int modified_strings;
	int new_labels;
	int deleted_labels;
	int modified_labels;
	int skipped_labels;
	int updated_comments;
	int updated_contexts;
	int updated_waves;
	int updated_speakers;
	int updated_listeners;
	int updated_maxlen;
	int changes;
} UPDATEINFO;

class CBabylonDlgAutoProxy;

class CBabylonDlg : public QDialog
{
	Q_OBJECT

public:
	explicit CBabylonDlg(QWidget* parent = nullptr);
	~CBabylonDlg();

	int ValidateStrFile(const char* filename);
	int MatchText(BabylonText* text, BabylonLabel* label, BabylonText** match);
	int RetranslateText(BabylonText* text, BabylonText* label);
	void VerifyDialog(TransDB* db, LangID langid);
	void VerifyTranslations(TransDB* db, LangID langid);
	int CanProceed(void);
	int CanOperate(void);
	int SaveMainDB(void);
	int UpdateLabel(BabylonLabel* source, BabylonLabel* destination, UPDATEINFO& info, int update = TRUE, int skip = FALSE);
	int UpdateDB(TransDB* source, TransDB* destination, int update = TRUE);
	void ProgressComplete(void);
	void SetProgress(int pos);
	void InitProgress(int range);
	int SaveLog(void);
	void Status(const char* string, int log = TRUE);
	void Log(const char* string, LogFormat format = NEW_LINE);
	int LoadStrFile(TransDB* db, const char* filename, void (*cb)(void) = NULL);
	void Ready(void) { Status("Ready", FALSE); ProgressComplete(); }

protected:
	void closeEvent(QCloseEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* event) override;
	void paintEvent(QPaintEvent* event) override;

private slots:
	void onViewdbs();
	void onReload();
	void onUpdate();
	void onSave();
	void onWarnings();
	void onErrors();
	void onChanges();
	void onExport();
	void onImport();
	void onGenerate();
	void onVerifyDialog();
	void onTranslations();
	void onSelchangeCombolang();
	void onReports();
	void onDblclkCombolang();
	void onReset();
	void onSent();

private:
	Ui::BabylonDlg* ui;
	CBabylonDlgAutoProxy* m_pAutoProxy;
	int m_progress_pos;
	int m_progress_range;
	int m_max_index;
	int m_operate_always;

	bool CanExit();
};

extern CBabylonDlg* MainDLG;

