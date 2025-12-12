#pragma once

#include <QDialog>
#include <QDoubleSpinBox>

class ParticleFrameKeyDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit ParticleFrameKeyDialogClass(float frame, QWidget* parent = nullptr);

    float Get_Frame() const { return m_Frame; }

private slots:
    void onOkClicked();

private:
    void initDialog();
    float m_Frame;
    QDoubleSpinBox* m_frameSpinBox;
};



