#pragma once

#include <QDialog>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QWidget>

class ColorPickerDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit ColorPickerDialogClass(int red, int green, int blue, QWidget* parent = nullptr);

    int Get_Red() const { return static_cast<int>(m_CurrentRed); }
    int Get_Green() const { return static_cast<int>(m_CurrentGreen); }
    int Get_Blue() const { return static_cast<int>(m_CurrentBlue); }
    void Set_Color(int r, int g, int b);
    void Set_Original_Color(int r, int g, int b);

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onReset();
    void onColorChanged();

private:
    void initDialog();
    void updateColor(float red, float green, float blue);
    float m_OrigRed;
    float m_OrigGreen;
    float m_OrigBlue;
    float m_CurrentRed;
    float m_CurrentGreen;
    float m_CurrentBlue;
    QSlider* m_redSlider;
    QSlider* m_greenSlider;
    QSlider* m_blueSlider;
    QDoubleSpinBox* m_redSpin;
    QDoubleSpinBox* m_greenSpin;
    QDoubleSpinBox* m_blueSpin;
    QWidget* m_currentColorWidget;
    QWidget* m_origColorWidget;
};



