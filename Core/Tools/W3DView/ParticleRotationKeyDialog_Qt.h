#pragma once

#include <QDialog>
#include <QDoubleSpinBox>

class ParticleRotationKeyDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit ParticleRotationKeyDialogClass(float rotation, QWidget* parent = nullptr);

    float Get_Rotation() const { return m_Rotation; }

private slots:
    void onOkClicked();

private:
    void initDialog();
    float m_Rotation;
    QDoubleSpinBox* m_rotationSpinBox;
};



