#include "DialogToolbar_Qt.h"
#include <QAction>

DialogToolbarClass::DialogToolbarClass(QWidget* parent) :
	QToolBar(parent)
{
}

DialogToolbarClass::~DialogToolbarClass()
{
}

void DialogToolbarClass::Enable_Button(int id, bool benable)
{
	QAction* action = findChild<QAction*>(QString::number(id));
	if (action)
	{
		action->setEnabled(benable);
	}
}



