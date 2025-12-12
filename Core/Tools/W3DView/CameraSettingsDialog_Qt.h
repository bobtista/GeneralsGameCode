#pragma once

#include <QDialog>
#include <QDoubleSpinBox>
#include <QCheckBox>

class CCameraSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CCameraSettingsDialog(QWidget* parent = nullptr);
	~CCameraSettingsDialog();
	
	float GetFOV() const { return m_fov; }
	float GetNearPlane() const { return m_nearPlane; }
	float GetFarPlane() const { return m_farPlane; }
	bool GetManualFOV() const { return m_manualFOV; }
	bool GetManualClipPlanes() const { return m_manualClipPlanes; }

private slots:
	void accept() override;

private:
	QDoubleSpinBox* m_fovSpinBox;
	QDoubleSpinBox* m_nearSpinBox;
	QDoubleSpinBox* m_farSpinBox;
	QCheckBox* m_manualFOVCheckBox;
	QCheckBox* m_manualClipCheckBox;
	float m_fov;
	float m_nearPlane;
	float m_farPlane;
	bool m_manualFOV;
	bool m_manualClipPlanes;
};



