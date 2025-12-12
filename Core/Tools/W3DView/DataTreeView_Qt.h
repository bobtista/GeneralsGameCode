#pragma once

#include <QTreeWidget>
#include <QTreeWidgetItem>

class CDataTreeView : public QTreeWidget
{
	Q_OBJECT

public:
	explicit CDataTreeView(QWidget* parent = nullptr);
	~CDataTreeView();

protected:
	void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
	void onItemSelectionChanged();
};



