#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class TexturePathDialogClass : public QDialog
{
	Q_OBJECT

public:
	explicit TexturePathDialogClass(QWidget* parent = nullptr);
	~TexturePathDialogClass();
	
	QString GetPath1() const { return m_path1; }
	QString GetPath2() const { return m_path2; }

private slots:
	void onBrowse1();
	void onBrowse2();
	void accept() override;

private:
	QLineEdit* m_path1Edit;
	QLineEdit* m_path2Edit;
	QString m_path1;
	QString m_path2;
};



