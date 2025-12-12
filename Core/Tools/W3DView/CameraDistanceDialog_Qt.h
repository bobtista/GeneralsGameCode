#pragma once

#include <QDialog>
#include <QDoubleSpinBox>

class CameraDistanceDialogClass : public QDialog
{
	Q_OBJECT

public:
	explicit CameraDistanceDialogClass(QWidget* parent = nullptr);
	~CameraDistanceDialogClass();
	
	float GetDistance() const { return m_distance; }

private slots:
	void accept() override;

private:
	QDoubleSpinBox* m_distanceSpinBox;
	float m_distance;
};



