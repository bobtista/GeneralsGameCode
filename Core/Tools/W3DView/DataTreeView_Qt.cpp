#include "DataTreeView_Qt.h"
#include <QContextMenuEvent>
#include <QMenu>

CDataTreeView::CDataTreeView(QWidget* parent) :
	QTreeWidget(parent)
{
	setHeaderLabel("Assets");
	setSelectionMode(QAbstractItemView::SingleSelection);
	
	connect(this, &QTreeWidget::itemSelectionChanged, this, &CDataTreeView::onItemSelectionChanged);
}

CDataTreeView::~CDataTreeView()
{
}

void CDataTreeView::contextMenuEvent(QContextMenuEvent* event)
{
	QMenu menu(this);
	menu.addAction("Properties");
	menu.addAction("Delete");
	menu.exec(event->globalPos());
}

void CDataTreeView::onItemSelectionChanged()
{
}



