#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QString>

class CMeshPropPage;
class CHierarchyPropPage;
class CAnimationPropPage;

class CAssetPropertySheet : public QDialog
{
	Q_OBJECT

public:
	explicit CAssetPropertySheet(const QString& title = "", QWidget* parent = nullptr);
	~CAssetPropertySheet();

	void SetMeshPage(const QString& meshName);
	void SetHierarchyPage(const QString& hierarchyName);
	void SetAnimationPage();

private:
	QTabWidget* m_tabWidget;
	CMeshPropPage* m_meshPage;
	CHierarchyPropPage* m_hierarchyPage;
	CAnimationPropPage* m_animationPage;
};

