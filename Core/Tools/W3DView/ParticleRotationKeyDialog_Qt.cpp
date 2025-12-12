#include "ParticleRotationKeyDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>

ParticleRotationKeyDialogClass::ParticleRotationKeyDialogClass(float rotation, QWidget* parent) :
    QDialog(parent),
    m_Rotation(rotation),
    m_rotationSpinBox(nullptr)
{
    initDialog();
}

void ParticleRotationKeyDialogClass::initDialog()
{
    setWindowTitle("Particle Rotation Key");
    resize(300, 100);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* rotationLayout = new QHBoxLayout();
    QLabel* rotationLabel = new QLabel("Rotation:", this);
    m_rotationSpinBox = new QDoubleSpinBox(this);
    m_rotationSpinBox->setRange(-10000.0, 10000.0);
    m_rotationSpinBox->setValue(m_Rotation);
    m_rotationSpinBox->setDecimals(2);
    rotationLayout->addWidget(rotationLabel);
    rotationLayout->addWidget(m_rotationSpinBox);
    mainLayout->addLayout(rotationLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ParticleRotationKeyDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void ParticleRotationKeyDialogClass::onOkClicked()
{
    m_Rotation = static_cast<float>(m_rotationSpinBox->value());
    accept();
}



