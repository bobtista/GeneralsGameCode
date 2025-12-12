#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>

class CBackgroundObjectDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CBackgroundObjectDialog(QWidget* parent = nullptr);
	~CBackgroundObjectDialog();
	
	QString GetObjectName() const { return m_objectName; }

private slots:
	void accept() override;

private:
	QComboBox* m_objectCombo;
	QString m_objectName;
};



