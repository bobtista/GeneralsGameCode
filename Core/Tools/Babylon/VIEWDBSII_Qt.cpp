#include "VIEWDBSII_Qt.h"
#include "TransDB.h"
#include "BabylonDlg_Qt.h"
#include "list.h"
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QTreeWidgetItem>
#include <QHeaderView>

extern int ViewChanges;
extern TransDB* MainDB;
extern TransDB* BabylonstrDB;
extern CBabylonDlg* MainDLG;

static int label_count;
static void progress_cb(void)
{
	label_count++;
	if (MainDLG)
	{
		MainDLG->SetProgress(label_count);
	}
}

VIEWDBSII::VIEWDBSII(QWidget* parent) : QDialog(parent)
{
	setWindowTitle("View Internal Databases");
	resize(600, 400);
	
	QVBoxLayout* layout = new QVBoxLayout(this);
	m_treeWidget = new QTreeWidget(this);
	m_treeWidget->setHeaderLabels(QStringList() << "Database" << "Labels" << "Obsolete");
	m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	layout->addWidget(m_treeWidget);
	
	if (ViewChanges) {
		create_changes_view();
	} else {
		create_full_view();
	}
}

VIEWDBSII::~VIEWDBSII()
{
}

void VIEWDBSII::closeEvent(QCloseEvent* event)
{
	event->accept();
}

void VIEWDBSII::create_changes_view()
{
	m_treeWidget->clear();
	QTreeWidgetItem* root = new QTreeWidgetItem(m_treeWidget);
	root->setText(0, "Changes");
	
	if (MainDB)
	{
		QTreeWidgetItem* mainItem = new QTreeWidgetItem(root);
		mainItem->setText(0, "Main DB");
		mainItem->setText(1, QString::number(MainDB->NumLabels()));
		mainItem->setText(2, QString::number(MainDB->NumObsolete()));
	}
	
	if (BabylonstrDB)
	{
		QTreeWidgetItem* babylonItem = new QTreeWidgetItem(root);
		babylonItem->setText(0, "Babylon.str");
		babylonItem->setText(1, QString::number(BabylonstrDB->NumLabels()));
		babylonItem->setText(2, QString::number(BabylonstrDB->NumObsolete()));
	}
	
	root->setExpanded(true);
}

void VIEWDBSII::create_full_view()
{
	m_treeWidget->clear();
	
	if (MainDLG)
	{
		MainDLG->Log("");
		MainDLG->Status("Building database view...");
	}
	
	QTreeWidgetItem* root = new QTreeWidgetItem(m_treeWidget);
	root->setText(0, "DBs");
	
	TransDB* db = FirstTransDB();
	int totalCount = 0;
	
	while (db)
	{
		totalCount += db->NumLabels();
		totalCount += db->NumObsolete();
		db = db->Next();
	}
	
	if (MainDLG)
	{
		MainDLG->InitProgress(totalCount);
	}
	
	label_count = 0;
	db = FirstTransDB();
	
	while (db)
	{
		QTreeWidgetItem* dbItem = new QTreeWidgetItem(root);
                const char* dbName = db->Name();  // TheSuperHackers @refactor bobtista 01/01/2025 Use Name() instead of NameSB()
		if (dbName)
		{
			dbItem->setText(0, QString::fromLocal8Bit(dbName));
		}
		dbItem->setText(1, QString::number(db->NumLabels()));
		dbItem->setText(2, QString::number(db->NumObsolete()));
		
		ListSearch sh;
		BabylonLabel* label = db->FirstLabel(sh);
		while (label)
		{
			QTreeWidgetItem* labelItem = new QTreeWidgetItem(dbItem);
			const char* labelName = label->NameSB();  // TheSuperHackers @refactor bobtista 01/01/2025 Use NameSB() to get narrow string
			if (labelName)
			{
				labelItem->setText(0, QString::fromLocal8Bit(labelName));
			}
			
			ListSearch textSh;
			BabylonText* text = label->FirstText(textSh);
			int textCount = 0;
			while (text)
			{
				textCount++;
				text = label->NextText(textSh);
			}
			labelItem->setText(1, QString::number(textCount));
			
			progress_cb();
			label = db->NextLabel(sh);
		}
		
		db = db->Next();
	}
	
	if (MainDLG)
	{
		MainDLG->ProgressComplete();
		MainDLG->Status("Ready", false);
	}
	
	root->setExpanded(true);
}

