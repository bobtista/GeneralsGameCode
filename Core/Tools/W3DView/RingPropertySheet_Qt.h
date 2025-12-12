#pragma once

#include <QDialog>
#include <QTabWidget>

class RingRenderObjClass;

class RingPropertySheetClass : public QDialog
{
	Q_OBJECT

public:
	explicit RingPropertySheetClass(RingRenderObjClass* ring, const QString& caption, QWidget* parent = nullptr, int selectPage = 0);
	~RingPropertySheetClass();

private:
	void Initialize();
	RingRenderObjClass* Create_Object();
	void Update_Object();
	void Add_Object_To_Viewer();
	void Create_New_Object();

	QTabWidget* m_tabWidget;
	QWidget* m_generalPage;
	QWidget* m_colorPage;
	QWidget* m_scalePage;
	RingRenderObjClass* m_RenderObj;
	QString m_LastSavedName;
};



