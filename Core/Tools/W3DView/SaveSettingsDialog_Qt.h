#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>

class CSaveSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CSaveSettingsDialog(QWidget* parent = nullptr);
	~CSaveSettingsDialog();
	
	QString GetFilename() const { return m_filename; }
	unsigned int GetSettingsMask() const { return m_settingsMask; }

private slots:
	void onBrowse();
	void accept() override;

private:
	QLineEdit* m_filenameEdit;
	QCheckBox* m_lightCheckBox;
	QCheckBox* m_backgroundCheckBox;
	QCheckBox* m_cameraCheckBox;
	QString m_filename;
	unsigned int m_settingsMask;
};



