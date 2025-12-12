#pragma once

#include <QWidget>
#include <QString>
#include <QTreeWidget>

class CHierarchyPropPage : public QWidget
{
	Q_OBJECT

public:
	explicit CHierarchyPropPage(const QString& hierarchyName, QWidget* parent = nullptr);
	~CHierarchyPropPage();

private slots:
	void onSubObjectDoubleClicked(QTreeWidgetItem* item, int column);

private:
	void initDialog();
	QString m_hierarchyName;
	QTreeWidget* m_subObjectList;
};



