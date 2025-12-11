#include "OpacityVectorDialog_Qt.h"
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef HAVE_WWVEGAS
    // Use real WWVegas headers when available (Generals/GeneralsMD builds)
    #include "sphereobj.h"    // For AlphaVectorStruct, SphereRenderObjClass, SphereVectorChannelClass
    #include "wwmath.h"       // For Matrix3Class
#else
    // Use stubs for Core-only build
    #include "GameEngineStubs.h"
    // Stub for matrix3
    struct Matrix3Class {};
#endif
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QGroupBox>

OpacityVectorDialogClass::OpacityVectorDialogClass(QWidget* parent) :
    QDialog(parent),
    m_sliderY(nullptr),
    m_sliderZ(nullptr),
    m_intensitySpinBox(nullptr),
    m_RenderObj(nullptr),
    m_KeyIndex(0)
{
    initDialog();
}

void OpacityVectorDialogClass::initDialog()
{
    setWindowTitle("Opacity Vector");
    resize(500, 300);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QGroupBox* intensityGroup = new QGroupBox("Intensity", this);
    QHBoxLayout* intensityLayout = new QHBoxLayout();
    QLabel* intensityLabel = new QLabel("Intensity:", this);
    m_intensitySpinBox = new QDoubleSpinBox(this);
    m_intensitySpinBox->setRange(0.0, 10.0);
    m_intensitySpinBox->setValue(0.0);
    m_intensitySpinBox->setDecimals(2);
    intensityLayout->addWidget(intensityLabel);
    intensityLayout->addWidget(m_intensitySpinBox);
    intensityGroup->setLayout(intensityLayout);
    mainLayout->addWidget(intensityGroup);

    QGroupBox* angleGroup = new QGroupBox("Angle", this);
    QVBoxLayout* angleLayout = new QVBoxLayout();
    QHBoxLayout* yLayout = new QHBoxLayout();
    QLabel* yLabel = new QLabel("Y:", this);
    m_sliderY = new QSlider(Qt::Horizontal, this);
    m_sliderY->setRange(0, 179);
    m_sliderY->setValue(0);
    yLayout->addWidget(yLabel);
    yLayout->addWidget(m_sliderY);
    angleLayout->addLayout(yLayout);

    QHBoxLayout* zLayout = new QHBoxLayout();
    QLabel* zLabel = new QLabel("Z:", this);
    m_sliderZ = new QSlider(Qt::Horizontal, this);
    m_sliderZ->setRange(0, 179);
    m_sliderZ->setValue(0);
    zLayout->addWidget(zLabel);
    zLayout->addWidget(m_sliderZ);
    angleLayout->addLayout(zLayout);
    angleGroup->setLayout(angleLayout);
    mainLayout->addWidget(angleGroup);

    connect(m_sliderY, &QSlider::valueChanged, this, &OpacityVectorDialogClass::onSliderChanged);
    connect(m_sliderZ, &QSlider::valueChanged, this, &OpacityVectorDialogClass::onSliderChanged);
    connect(m_intensitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OpacityVectorDialogClass::onSliderChanged);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &OpacityVectorDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &OpacityVectorDialogClass::onCancelClicked);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void OpacityVectorDialogClass::onSliderChanged()
{
    Update_Object();
}

void OpacityVectorDialogClass::onOkClicked()
{
    m_Value = Update_Value();
    accept();
}

void OpacityVectorDialogClass::onCancelClicked()
{
    Update_Object(m_Value);
    reject();
}

AlphaVectorStruct OpacityVectorDialogClass::Update_Value()
{
    AlphaVectorStruct value;

    int y_pos = m_sliderY->value();
    int z_pos = m_sliderZ->value();

    float y_rot = DEG_TO_RADF(static_cast<float>(y_pos));
    float z_rot = DEG_TO_RADF(static_cast<float>(z_pos));

    Matrix3x3 rot_mat(true);
    rot_mat.Rotate_Y(y_rot);
    rot_mat.Rotate_Z(z_rot);

    value.angle = Build_Quaternion(rot_mat);
    value.intensity = static_cast<float>(m_intensitySpinBox->value());

    return value;
}

void OpacityVectorDialogClass::Update_Object()
{
    Update_Object(Update_Value());
}

void OpacityVectorDialogClass::Update_Object(const AlphaVectorStruct& value)
{
    if (m_RenderObj != nullptr)
    {
        switch (m_RenderObj->Class_ID())
        {
            case RenderObjClass::CLASSID_SPHERE:
            {
                SphereVectorChannelClass& vector_channel = ((SphereRenderObjClass*)m_RenderObj)->Get_Vector_Channel();
                vector_channel.Set_Key_Value(m_KeyIndex, value);
                ((SphereRenderObjClass*)m_RenderObj)->Restart_Animation();
            }
            break;
        }
    }
}


