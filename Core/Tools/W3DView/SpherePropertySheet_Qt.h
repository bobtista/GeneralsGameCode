#pragma once

#include <QDialog>
#include <QTabWidget>

class SphereRenderObjClass;

class SpherePropertySheetClass : public QDialog
{
	Q_OBJECT

public:
	explicit SpherePropertySheetClass(SphereRenderObjClass* sphere, const QString& caption, QWidget* parent = nullptr, int selectPage = 0);
	~SpherePropertySheetClass();

private:
	void Initialize();
	SphereRenderObjClass* Create_Object();
	void Update_Object();
	void Add_Object_To_Viewer();
	void Create_New_Object();

	QTabWidget* m_tabWidget;
	QWidget* m_generalPage;
	QWidget* m_colorPage;
	QWidget* m_scalePage;
	SphereRenderObjClass* m_RenderObj;
	QString m_LastSavedName;
};



