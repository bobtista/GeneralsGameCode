#include "SceneLightDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QGroupBox>

CSceneLightDialog::CSceneLightDialog(QWidget* parent) :
    QDialog(parent),
    m_redSlider(nullptr),
    m_greenSlider(nullptr),
    m_blueSlider(nullptr),
    m_intensitySlider(nullptr),
    m_distanceSpin(nullptr),
    m_startAttenSpin(nullptr),
    m_endAttenSpin(nullptr),
    m_grayscaleCheck(nullptr),
    m_attenuationCheck(nullptr),
    m_channelGroup(nullptr),
    m_diffuseRadio(nullptr),
    m_specularRadio(nullptr),
    m_bothRadio(nullptr)
{
    initDialog();
}

void CSceneLightDialog::initDialog()
{
    setWindowTitle("Scene Light");
    resize(500, 400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QGroupBox* colorGroup = new QGroupBox("Color", this);
    QVBoxLayout* colorLayout = new QVBoxLayout();
    QHBoxLayout* redLayout = new QHBoxLayout();
    redLayout->addWidget(new QLabel("Red:", this));
    m_redSlider = new QSlider(Qt::Horizontal, this);
    m_redSlider->setRange(0, 255);
    m_redSlider->setValue(255);
    redLayout->addWidget(m_redSlider);
    colorLayout->addLayout(redLayout);
    QHBoxLayout* greenLayout = new QHBoxLayout();
    greenLayout->addWidget(new QLabel("Green:", this));
    m_greenSlider = new QSlider(Qt::Horizontal, this);
    m_greenSlider->setRange(0, 255);
    m_greenSlider->setValue(255);
    greenLayout->addWidget(m_greenSlider);
    colorLayout->addLayout(greenLayout);
    QHBoxLayout* blueLayout = new QHBoxLayout();
    blueLayout->addWidget(new QLabel("Blue:", this));
    m_blueSlider = new QSlider(Qt::Horizontal, this);
    m_blueSlider->setRange(0, 255);
    m_blueSlider->setValue(255);
    blueLayout->addWidget(m_blueSlider);
    colorLayout->addLayout(blueLayout);
    colorGroup->setLayout(colorLayout);
    mainLayout->addWidget(colorGroup);

    QHBoxLayout* intensityLayout = new QHBoxLayout();
    intensityLayout->addWidget(new QLabel("Intensity:", this));
    m_intensitySlider = new QSlider(Qt::Horizontal, this);
    m_intensitySlider->setRange(0, 100);
    m_intensitySlider->setValue(100);
    intensityLayout->addWidget(m_intensitySlider);
    mainLayout->addLayout(intensityLayout);

    QHBoxLayout* distanceLayout = new QHBoxLayout();
    distanceLayout->addWidget(new QLabel("Distance:", this));
    m_distanceSpin = new QDoubleSpinBox(this);
    m_distanceSpin->setRange(0.0, 10000.0);
    m_distanceSpin->setValue(100.0);
    distanceLayout->addWidget(m_distanceSpin);
    mainLayout->addLayout(distanceLayout);

    m_attenuationCheck = new QCheckBox("Attenuation", this);
    mainLayout->addWidget(m_attenuationCheck);
    connect(m_attenuationCheck, &QCheckBox::toggled, this, &CSceneLightDialog::onAttenuationCheck);

    QHBoxLayout* startAttenLayout = new QHBoxLayout();
    startAttenLayout->addWidget(new QLabel("Start:", this));
    m_startAttenSpin = new QDoubleSpinBox(this);
    m_startAttenSpin->setRange(0.0, 10000.0);
    startAttenLayout->addWidget(m_startAttenSpin);
    mainLayout->addLayout(startAttenLayout);

    QHBoxLayout* endAttenLayout = new QHBoxLayout();
    endAttenLayout->addWidget(new QLabel("End:", this));
    m_endAttenSpin = new QDoubleSpinBox(this);
    m_endAttenSpin->setRange(0.0, 10000.0);
    endAttenLayout->addWidget(m_endAttenSpin);
    mainLayout->addLayout(endAttenLayout);

    m_grayscaleCheck = new QCheckBox("Grayscale", this);
    mainLayout->addWidget(m_grayscaleCheck);
    connect(m_grayscaleCheck, &QCheckBox::toggled, this, &CSceneLightDialog::onGrayscaleCheck);

    m_channelGroup = new QButtonGroup(this);
    m_diffuseRadio = new QRadioButton("Diffuse", this);
    m_specularRadio = new QRadioButton("Specular", this);
    m_bothRadio = new QRadioButton("Both", this);
    m_channelGroup->addButton(m_diffuseRadio, 0);
    m_channelGroup->addButton(m_specularRadio, 1);
    m_channelGroup->addButton(m_bothRadio, 2);
    mainLayout->addWidget(m_diffuseRadio);
    mainLayout->addWidget(m_specularRadio);
    mainLayout->addWidget(m_bothRadio);
    // TheSuperHackers @refactor bobtista 01/01/2025 Use Qt 6 compatible overload syntax
    connect(m_channelGroup, &QButtonGroup::idClicked, this, &CSceneLightDialog::onChannelChanged);
    m_bothRadio->setChecked(true);

    connect(m_redSlider, &QSlider::valueChanged, this, &CSceneLightDialog::onSliderChanged);
    connect(m_greenSlider, &QSlider::valueChanged, this, &CSceneLightDialog::onSliderChanged);
    connect(m_blueSlider, &QSlider::valueChanged, this, &CSceneLightDialog::onSliderChanged);
    connect(m_intensitySlider, &QSlider::valueChanged, this, &CSceneLightDialog::onSliderChanged);
    connect(m_distanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CSceneLightDialog::onSliderChanged);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &CSceneLightDialog::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &CSceneLightDialog::onCancelClicked);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    updateAttenuationControls();
}

void CSceneLightDialog::onOkClicked()
{
    accept();
}

void CSceneLightDialog::onCancelClicked()
{
    reject();
}

void CSceneLightDialog::onSliderChanged()
{
    Vector3 color(static_cast<float>(m_redSlider->value()) / 255.0f,
                  static_cast<float>(m_greenSlider->value()) / 255.0f,
                  static_cast<float>(m_blueSlider->value()) / 255.0f);
    updateLight(color);
    updateDistance(static_cast<float>(m_distanceSpin->value()));
}

void CSceneLightDialog::onGrayscaleCheck()
{
}

void CSceneLightDialog::onChannelChanged()
{
}

void CSceneLightDialog::onAttenuationCheck()
{
    updateAttenuationControls();
}

void CSceneLightDialog::setColorControlState(const Vector3& color)
{
    m_redSlider->setValue(static_cast<int>(color.X * 255.0f));
    m_greenSlider->setValue(static_cast<int>(color.Y * 255.0f));
    m_blueSlider->setValue(static_cast<int>(color.Z * 255.0f));
}

void CSceneLightDialog::updateLight(const Vector3& color)
{
}

void CSceneLightDialog::updateDistance(float distance)
{
}

void CSceneLightDialog::updateAttenuation()
{
}

void CSceneLightDialog::updateAttenuationControls()
{
    bool enabled = m_attenuationCheck->isChecked();
    m_startAttenSpin->setEnabled(enabled);
    m_endAttenSpin->setEnabled(enabled);
}


