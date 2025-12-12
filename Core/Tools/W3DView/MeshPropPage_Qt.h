#pragma once

#include <QWidget>
#include <QString>

class CMeshPropPage : public QWidget
{
	Q_OBJECT

public:
	explicit CMeshPropPage(const QString& meshName, QWidget* parent = nullptr);
	~CMeshPropPage();

private:
	void initDialog();
	QString m_meshName;
};



