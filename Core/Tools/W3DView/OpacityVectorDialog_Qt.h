#pragma once

#include <QDialog>
#include <QSlider>
#include <QDoubleSpinBox>
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef _WIN32
#include "sphereobj.h"
#else
#include "GameEngineStubs.h"
#endif

class ColorBarClass;

class OpacityVectorDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit OpacityVectorDialogClass(QWidget* parent = nullptr);

    void Set_Render_Obj(RenderObjClass* render_obj) { m_RenderObj = render_obj; }
    void Set_Vector(const AlphaVectorStruct& def_vector) { m_Value = def_vector; }
    const AlphaVectorStruct& Get_Vector() const { return m_Value; }
    void Set_Key_Index(int index) { m_KeyIndex = index; }

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onSliderChanged();

private:
    void initDialog();
    void Update_Object(const AlphaVectorStruct& value);
    void Update_Object();
    AlphaVectorStruct Update_Value();

    QSlider* m_sliderY;
    QSlider* m_sliderZ;
    QDoubleSpinBox* m_intensitySpinBox;
    RenderObjClass* m_RenderObj;
    AlphaVectorStruct m_Value;
    int m_KeyIndex;
};


