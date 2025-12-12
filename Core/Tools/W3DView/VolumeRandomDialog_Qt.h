#pragma once

#include <QDialog>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QButtonGroup>

class Vector3Randomizer;

class VolumeRandomDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit VolumeRandomDialogClass(Vector3Randomizer* randomizer, QWidget* parent = nullptr);

    Vector3Randomizer* Get_Randomizer() const { return m_Randomizer; }

private slots:
    void onOkClicked();
    void onBoxRadio();
    void onCylinderRadio();
    void onSphereRadio();
    void updateEnableState();

private:
    void initDialog();
    Vector3Randomizer* m_Randomizer;
    QButtonGroup* m_typeGroup;
    QRadioButton* m_boxRadio;
    QRadioButton* m_cylinderRadio;
    QRadioButton* m_sphereRadio;
    QCheckBox* m_sphereHollowCheck;
    QDoubleSpinBox* m_boxXSpin;
    QDoubleSpinBox* m_boxYSpin;
    QDoubleSpinBox* m_boxZSpin;
    QDoubleSpinBox* m_sphereRadiusSpin;
    QDoubleSpinBox* m_cylinderRadiusSpin;
    QDoubleSpinBox* m_cylinderHeightSpin;
};



