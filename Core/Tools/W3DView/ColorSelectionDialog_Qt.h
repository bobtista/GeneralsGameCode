#pragma once

#include <QDialog>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QWidget>
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef HAVE_WWVEGAS
    // Use real WWVegas headers when available (Generals/GeneralsMD builds)
    #include "vector3.h"  // For Vector3
#else
    // Use stubs for Core-only build
    #include "GameEngineStubs.h"
#endif

class ColorSelectionDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit ColorSelectionDialogClass(const Vector3& def_color, QWidget* parent = nullptr);

    const Vector3& Get_Color() const { return m_Color; }
    void Set_Color(const Vector3& color) { m_Color = color; }

private slots:
    void onOkClicked();
    void onSliderChanged();
    void onSpinBoxChanged();
    void onGrayscaleCheck();

private:
    void initDialog();
    void paintColorWindow();
    void updateSliders();
    Vector3 m_Color;
    Vector3 m_PaintColor;
    QSlider* m_redSlider;
    QSlider* m_greenSlider;
    QSlider* m_blueSlider;
    QDoubleSpinBox* m_redSpin;
    QDoubleSpinBox* m_greenSpin;
    QDoubleSpinBox* m_blueSpin;
    QCheckBox* m_grayscaleCheck;
    QWidget* m_colorWindow;
};


