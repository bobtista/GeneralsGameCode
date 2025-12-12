#pragma once

#include <QDialog>
#include <QSlider>
#include <QCheckBox>
#include <QRadioButton>
#include <QLabel>

class CAnimationSpeed : public QDialog
{
    Q_OBJECT

public:
    explicit CAnimationSpeed(QWidget* parent = nullptr);

private slots:
    void onSpeedSliderValueChanged(int value);
    void onBlendToggled(bool checked);
    void onCompressQToggled(bool checked);
    void on16BitToggled(bool checked);
    void on8BitToggled(bool checked);

private:
    void initDialog();
    QSlider* m_speedSlider;
    QCheckBox* m_blendCheckBox;
    QCheckBox* m_compressQCheckBox;
    QRadioButton* m_16BitRadio;
    QRadioButton* m_8BitRadio;
    QLabel* m_speedLabel;
    int m_iInitialPercent;
};



