#pragma once

#include <QDialog>
#include <QDoubleSpinBox>

class ParticleBlurTimeKeyDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit ParticleBlurTimeKeyDialogClass(float blur_time, QWidget* parent = nullptr);

    float Get_Blur_Time() const { return m_BlurTime; }

private slots:
    void onOkClicked();

private:
    void initDialog();
    float m_BlurTime;
    QDoubleSpinBox* m_blurTimeSpinBox;
};



