#include "ColorPickerDialogClass_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPushButton>

ColorPickerDialogClass::ColorPickerDialogClass(int red, int green, int blue, QWidget* parent) :
    QDialog(parent),
    m_OrigRed(static_cast<float>(red)),
    m_OrigGreen(static_cast<float>(green)),
    m_OrigBlue(static_cast<float>(blue)),
    m_CurrentRed(static_cast<float>(red)),
    m_CurrentGreen(static_cast<float>(green)),
    m_CurrentBlue(static_cast<float>(blue)),
    m_redSlider(nullptr),
    m_greenSlider(nullptr),
    m_blueSlider(nullptr),
    m_redSpin(nullptr),
    m_greenSpin(nullptr),
    m_blueSpin(nullptr),
    m_currentColorWidget(nullptr),
    m_origColorWidget(nullptr)
{
    initDialog();
}

void ColorPickerDialogClass::initDialog()
{
    setWindowTitle("Color Picker");
    resize(500, 400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* colorPreviewLayout = new QHBoxLayout();
    QLabel* currentLabel = new QLabel("Current:", this);
    m_currentColorWidget = new QWidget(this);
    m_currentColorWidget->setMinimumSize(100, 50);
    m_currentColorWidget->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                        .arg(static_cast<int>(m_CurrentRed))
                                        .arg(static_cast<int>(m_CurrentGreen))
                                        .arg(static_cast<int>(m_CurrentBlue)));
    colorPreviewLayout->addWidget(currentLabel);
    colorPreviewLayout->addWidget(m_currentColorWidget);
    QLabel* origLabel = new QLabel("Original:", this);
    m_origColorWidget = new QWidget(this);
    m_origColorWidget->setMinimumSize(100, 50);
    m_origColorWidget->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                     .arg(static_cast<int>(m_OrigRed))
                                     .arg(static_cast<int>(m_OrigGreen))
                                     .arg(static_cast<int>(m_OrigBlue)));
    colorPreviewLayout->addWidget(origLabel);
    colorPreviewLayout->addWidget(m_origColorWidget);
    mainLayout->addLayout(colorPreviewLayout);

    QGroupBox* colorGroup = new QGroupBox("Color", this);
    QVBoxLayout* colorLayout = new QVBoxLayout();
    QHBoxLayout* redLayout = new QHBoxLayout();
    redLayout->addWidget(new QLabel("Red:", this));
    m_redSlider = new QSlider(Qt::Horizontal, this);
    m_redSlider->setRange(0, 255);
    m_redSlider->setValue(static_cast<int>(m_CurrentRed));
    redLayout->addWidget(m_redSlider);
    m_redSpin = new QDoubleSpinBox(this);
    m_redSpin->setRange(0.0, 255.0);
    m_redSpin->setValue(m_CurrentRed);
    redLayout->addWidget(m_redSpin);
    colorLayout->addLayout(redLayout);
    connect(m_redSlider, &QSlider::valueChanged, this, &ColorPickerDialogClass::onColorChanged);
    connect(m_redSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColorPickerDialogClass::onColorChanged);

    QHBoxLayout* greenLayout = new QHBoxLayout();
    greenLayout->addWidget(new QLabel("Green:", this));
    m_greenSlider = new QSlider(Qt::Horizontal, this);
    m_greenSlider->setRange(0, 255);
    m_greenSlider->setValue(static_cast<int>(m_CurrentGreen));
    greenLayout->addWidget(m_greenSlider);
    m_greenSpin = new QDoubleSpinBox(this);
    m_greenSpin->setRange(0.0, 255.0);
    m_greenSpin->setValue(m_CurrentGreen);
    greenLayout->addWidget(m_greenSpin);
    colorLayout->addLayout(greenLayout);
    connect(m_greenSlider, &QSlider::valueChanged, this, &ColorPickerDialogClass::onColorChanged);
    connect(m_greenSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColorPickerDialogClass::onColorChanged);

    QHBoxLayout* blueLayout = new QHBoxLayout();
    blueLayout->addWidget(new QLabel("Blue:", this));
    m_blueSlider = new QSlider(Qt::Horizontal, this);
    m_blueSlider->setRange(0, 255);
    m_blueSlider->setValue(static_cast<int>(m_CurrentBlue));
    blueLayout->addWidget(m_blueSlider);
    m_blueSpin = new QDoubleSpinBox(this);
    m_blueSpin->setRange(0.0, 255.0);
    m_blueSpin->setValue(m_CurrentBlue);
    blueLayout->addWidget(m_blueSpin);
    colorLayout->addLayout(blueLayout);
    connect(m_blueSlider, &QSlider::valueChanged, this, &ColorPickerDialogClass::onColorChanged);
    connect(m_blueSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColorPickerDialogClass::onColorChanged);

    colorGroup->setLayout(colorLayout);
    mainLayout->addWidget(colorGroup);

    QPushButton* resetButton = new QPushButton("Reset", this);
    connect(resetButton, &QPushButton::clicked, this, &ColorPickerDialogClass::onReset);
    mainLayout->addWidget(resetButton);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ColorPickerDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ColorPickerDialogClass::onCancelClicked);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void ColorPickerDialogClass::Set_Color(int r, int g, int b)
{
    updateColor(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b));
}

void ColorPickerDialogClass::Set_Original_Color(int r, int g, int b)
{
    m_OrigRed = static_cast<float>(r);
    m_OrigGreen = static_cast<float>(g);
    m_OrigBlue = static_cast<float>(b);
    m_origColorWidget->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                     .arg(r).arg(g).arg(b));
}

void ColorPickerDialogClass::onOkClicked()
{
    m_CurrentRed = static_cast<float>(m_redSpin->value());
    m_CurrentGreen = static_cast<float>(m_greenSpin->value());
    m_CurrentBlue = static_cast<float>(m_blueSpin->value());
    accept();
}

void ColorPickerDialogClass::onCancelClicked()
{
    reject();
}

void ColorPickerDialogClass::onReset()
{
    updateColor(m_OrigRed, m_OrigGreen, m_OrigBlue);
}

void ColorPickerDialogClass::onColorChanged()
{
    m_CurrentRed = static_cast<float>(m_redSpin->value());
    m_CurrentGreen = static_cast<float>(m_greenSpin->value());
    m_CurrentBlue = static_cast<float>(m_blueSpin->value());
    m_currentColorWidget->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                       .arg(static_cast<int>(m_CurrentRed))
                                       .arg(static_cast<int>(m_CurrentGreen))
                                       .arg(static_cast<int>(m_CurrentBlue)));
}

void ColorPickerDialogClass::updateColor(float red, float green, float blue)
{
    m_CurrentRed = red;
    m_CurrentGreen = green;
    m_CurrentBlue = blue;
    m_redSlider->setValue(static_cast<int>(red));
    m_greenSlider->setValue(static_cast<int>(green));
    m_blueSlider->setValue(static_cast<int>(blue));
    m_redSpin->setValue(red);
    m_greenSpin->setValue(green);
    m_blueSpin->setValue(blue);
    onColorChanged();
}



