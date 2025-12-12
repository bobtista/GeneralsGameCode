#include "OpacitySettingsDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>

OpacitySettingsDialogClass::OpacitySettingsDialogClass(float opacity, QWidget* parent) :
    QDialog(parent),
    m_Opacity(opacity),
    m_opacitySlider(nullptr),
    m_opacitySpinBox(nullptr)
{
    initDialog();
}

void OpacitySettingsDialogClass::initDialog()
{
    setWindowTitle("Opacity Settings");
    resize(400, 150);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* opacityLayout = new QHBoxLayout();
    QLabel* opacityLabel = new QLabel("Opacity:", this);
    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(0, 1000);
    m_opacitySlider->setValue(static_cast<int>(m_Opacity * 1000.0f));
    m_opacitySpinBox = new QDoubleSpinBox(this);
    m_opacitySpinBox->setRange(0.0, 1.0);
    m_opacitySpinBox->setValue(m_Opacity);
    m_opacitySpinBox->setDecimals(3);
    m_opacitySpinBox->setSingleStep(0.001);
    opacityLayout->addWidget(opacityLabel);
    opacityLayout->addWidget(m_opacitySlider);
    opacityLayout->addWidget(m_opacitySpinBox);
    mainLayout->addLayout(opacityLayout);

    connect(m_opacitySlider, &QSlider::valueChanged, this, &OpacitySettingsDialogClass::onOpacityChanged);
    connect(m_opacitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value) {
        m_opacitySlider->blockSignals(true);
        m_opacitySlider->setValue(static_cast<int>(value * 1000.0));
        m_opacitySlider->blockSignals(false);
    });

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &OpacitySettingsDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void OpacitySettingsDialogClass::onOpacityChanged(int value)
{
    m_Opacity = static_cast<float>(value) / 1000.0f;
    m_opacitySpinBox->blockSignals(true);
    m_opacitySpinBox->setValue(m_Opacity);
    m_opacitySpinBox->blockSignals(false);
}

void OpacitySettingsDialogClass::onOkClicked()
{
    m_Opacity = static_cast<float>(m_opacitySpinBox->value());
    accept();
}



