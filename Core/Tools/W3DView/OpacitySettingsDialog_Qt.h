#pragma once

#include <QDialog>
#include <QSlider>
#include <QDoubleSpinBox>

class OpacitySettingsDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit OpacitySettingsDialogClass(float opacity, QWidget* parent = nullptr);

    float Get_Opacity() const { return m_Opacity; }

private slots:
    void onOkClicked();
    void onOpacityChanged(int value);

private:
    void initDialog();
    float m_Opacity;
    QSlider* m_opacitySlider;
    QDoubleSpinBox* m_opacitySpinBox;
};



