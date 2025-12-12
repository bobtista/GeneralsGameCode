#pragma once

#include <QDialog>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef _WIN32
#include "vector3.h"
#else
#include "GameEngineStubs.h"
#endif

class CSceneLightDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CSceneLightDialog(QWidget* parent = nullptr);

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onSliderChanged();
    void onGrayscaleCheck();
    void onChannelChanged();
    void onAttenuationCheck();

private:
    void initDialog();
    void setColorControlState(const Vector3& color);
    void updateLight(const Vector3& color);
    void updateDistance(float distance);
    void updateAttenuation();
    void updateAttenuationControls();

    QSlider* m_redSlider;
    QSlider* m_greenSlider;
    QSlider* m_blueSlider;
    QSlider* m_intensitySlider;
    QDoubleSpinBox* m_distanceSpin;
    QDoubleSpinBox* m_startAttenSpin;
    QDoubleSpinBox* m_endAttenSpin;
    QCheckBox* m_grayscaleCheck;
    QCheckBox* m_attenuationCheck;
    QButtonGroup* m_channelGroup;
    QRadioButton* m_diffuseRadio;
    QRadioButton* m_specularRadio;
    QRadioButton* m_bothRadio;
};


