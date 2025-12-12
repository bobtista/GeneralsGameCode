#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class CBackgroundBMPDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CBackgroundBMPDialog(QWidget* parent = nullptr);
	~CBackgroundBMPDialog();
	
	QString GetFilename() const { return m_filename; }

private slots:
	void onBrowse();
	void accept() override;

private:
	QLineEdit* m_filenameEdit;
	QString m_filename;
};



