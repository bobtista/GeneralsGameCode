#include "ColorSelectionDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPainter>

ColorSelectionDialogClass::ColorSelectionDialogClass(const Vector3& def_color, QWidget* parent) :
    QDialog(parent),
    m_Color(def_color),
    m_PaintColor(def_color),
    m_redSlider(nullptr),
    m_greenSlider(nullptr),
    m_blueSlider(nullptr),
    m_redSpin(nullptr),
    m_greenSpin(nullptr),
    m_blueSpin(nullptr),
    m_grayscaleCheck(nullptr),
    m_colorWindow(nullptr)
{
    initDialog();
}

void ColorSelectionDialogClass::initDialog()
{
    setWindowTitle("Color Selection");
    resize(500, 350);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_colorWindow = new QWidget(this);
    m_colorWindow->setMinimumHeight(100);
    m_colorWindow->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                 .arg(static_cast<int>(m_Color.X * 255))
                                 .arg(static_cast<int>(m_Color.Y * 255))
                                 .arg(static_cast<int>(m_Color.Z * 255)));
    mainLayout->addWidget(m_colorWindow);

    QGroupBox* colorGroup = new QGroupBox("Color", this);
    QVBoxLayout* colorLayout = new QVBoxLayout();
    QHBoxLayout* redLayout = new QHBoxLayout();
    redLayout->addWidget(new QLabel("Red:", this));
    m_redSlider = new QSlider(Qt::Horizontal, this);
    m_redSlider->setRange(0, 255);
    m_redSlider->setValue(static_cast<int>(m_Color.X * 255));
    redLayout->addWidget(m_redSlider);
    m_redSpin = new QDoubleSpinBox(this);
    m_redSpin->setRange(0.0, 255.0);
    m_redSpin->setValue(m_Color.X * 255.0);
    redLayout->addWidget(m_redSpin);
    colorLayout->addLayout(redLayout);
    connect(m_redSlider, &QSlider::valueChanged, this, &ColorSelectionDialogClass::onSliderChanged);
    connect(m_redSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColorSelectionDialogClass::onSpinBoxChanged);

    QHBoxLayout* greenLayout = new QHBoxLayout();
    greenLayout->addWidget(new QLabel("Green:", this));
    m_greenSlider = new QSlider(Qt::Horizontal, this);
    m_greenSlider->setRange(0, 255);
    m_greenSlider->setValue(static_cast<int>(m_Color.Y * 255));
    greenLayout->addWidget(m_greenSlider);
    m_greenSpin = new QDoubleSpinBox(this);
    m_greenSpin->setRange(0.0, 255.0);
    m_greenSpin->setValue(m_Color.Y * 255.0);
    greenLayout->addWidget(m_greenSpin);
    colorLayout->addLayout(greenLayout);
    connect(m_greenSlider, &QSlider::valueChanged, this, &ColorSelectionDialogClass::onSliderChanged);
    connect(m_greenSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColorSelectionDialogClass::onSpinBoxChanged);

    QHBoxLayout* blueLayout = new QHBoxLayout();
    blueLayout->addWidget(new QLabel("Blue:", this));
    m_blueSlider = new QSlider(Qt::Horizontal, this);
    m_blueSlider->setRange(0, 255);
    m_blueSlider->setValue(static_cast<int>(m_Color.Z * 255));
    blueLayout->addWidget(m_blueSlider);
    m_blueSpin = new QDoubleSpinBox(this);
    m_blueSpin->setRange(0.0, 255.0);
    m_blueSpin->setValue(m_Color.Z * 255.0);
    blueLayout->addWidget(m_blueSpin);
    colorLayout->addLayout(blueLayout);
    connect(m_blueSlider, &QSlider::valueChanged, this, &ColorSelectionDialogClass::onSliderChanged);
    connect(m_blueSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColorSelectionDialogClass::onSpinBoxChanged);

    colorGroup->setLayout(colorLayout);
    mainLayout->addWidget(colorGroup);

    m_grayscaleCheck = new QCheckBox("Grayscale", this);
    connect(m_grayscaleCheck, &QCheckBox::toggled, this, &ColorSelectionDialogClass::onGrayscaleCheck);
    mainLayout->addWidget(m_grayscaleCheck);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ColorSelectionDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void ColorSelectionDialogClass::onOkClicked()
{
    m_Color.X = static_cast<float>(m_redSpin->value() / 255.0);
    m_Color.Y = static_cast<float>(m_greenSpin->value() / 255.0);
    m_Color.Z = static_cast<float>(m_blueSpin->value() / 255.0);
    accept();
}

void ColorSelectionDialogClass::onSliderChanged()
{
    m_redSpin->blockSignals(true);
    m_redSpin->setValue(m_redSlider->value());
    m_redSpin->blockSignals(false);
    m_greenSpin->blockSignals(true);
    m_greenSpin->setValue(m_greenSlider->value());
    m_greenSpin->blockSignals(false);
    m_blueSpin->blockSignals(true);
    m_blueSpin->setValue(m_blueSlider->value());
    m_blueSpin->blockSignals(false);
    paintColorWindow();
}

void ColorSelectionDialogClass::onSpinBoxChanged()
{
    m_redSlider->blockSignals(true);
    m_redSlider->setValue(static_cast<int>(m_redSpin->value()));
    m_redSlider->blockSignals(false);
    m_greenSlider->blockSignals(true);
    m_greenSlider->setValue(static_cast<int>(m_greenSpin->value()));
    m_greenSlider->blockSignals(false);
    m_blueSlider->blockSignals(true);
    m_blueSlider->setValue(static_cast<int>(m_blueSpin->value()));
    m_blueSlider->blockSignals(false);
    paintColorWindow();
}

void ColorSelectionDialogClass::onGrayscaleCheck()
{
    if (m_grayscaleCheck->isChecked())
    {
        float gray = (m_Color.X + m_Color.Y + m_Color.Z) / 3.0f;
        m_Color.X = m_Color.Y = m_Color.Z = gray;
        updateSliders();
    }
}

void ColorSelectionDialogClass::paintColorWindow()
{
    m_colorWindow->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                 .arg(static_cast<int>(m_redSpin->value()))
                                 .arg(static_cast<int>(m_greenSpin->value()))
                                 .arg(static_cast<int>(m_blueSpin->value())));
}

void ColorSelectionDialogClass::updateSliders()
{
    m_redSlider->setValue(static_cast<int>(m_Color.X * 255));
    m_greenSlider->setValue(static_cast<int>(m_Color.Y * 255));
    m_blueSlider->setValue(static_cast<int>(m_Color.Z * 255));
    m_redSpin->setValue(m_Color.X * 255.0);
    m_greenSpin->setValue(m_Color.Y * 255.0);
    m_blueSpin->setValue(m_Color.Z * 255.0);
    paintColorWindow();
}



