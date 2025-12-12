#pragma once

#include <QDialog>
#include <QComboBox>
#include <QPushButton>

class DeviceSelectionDialog : public QDialog
{
	Q_OBJECT

public:
	explicit DeviceSelectionDialog(QWidget* parent = nullptr);
	~DeviceSelectionDialog();
	
	int GetSelectedDevice() const { return m_selectedDevice; }

private slots:
	void accept() override;

private:
	QComboBox* m_deviceCombo;
	int m_selectedDevice;
};



