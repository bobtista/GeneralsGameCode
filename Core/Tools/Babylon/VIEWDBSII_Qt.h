#pragma once

#include <QDialog>
#include <QTreeWidget>

class VIEWDBSII : public QDialog
{
	Q_OBJECT

public:
	explicit VIEWDBSII(QWidget* parent = nullptr);
	~VIEWDBSII();

protected:
	void closeEvent(QCloseEvent* event) override;

private:
	QTreeWidget* m_treeWidget;
	void create_changes_view();
	void create_full_view();
};



