#include "ParticleBlurTimeKeyDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>

ParticleBlurTimeKeyDialogClass::ParticleBlurTimeKeyDialogClass(float blur_time, QWidget* parent) :
    QDialog(parent),
    m_BlurTime(blur_time),
    m_blurTimeSpinBox(nullptr)
{
    initDialog();
}

void ParticleBlurTimeKeyDialogClass::initDialog()
{
    setWindowTitle("Particle Blur Time Key");
    resize(300, 100);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* blurTimeLayout = new QHBoxLayout();
    QLabel* blurTimeLabel = new QLabel("Blur Time:", this);
    m_blurTimeSpinBox = new QDoubleSpinBox(this);
    m_blurTimeSpinBox->setRange(-1024.0, 1024.0);
    m_blurTimeSpinBox->setValue(m_BlurTime);
    m_blurTimeSpinBox->setDecimals(2);
    blurTimeLayout->addWidget(blurTimeLabel);
    blurTimeLayout->addWidget(m_blurTimeSpinBox);
    mainLayout->addLayout(blurTimeLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ParticleBlurTimeKeyDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void ParticleBlurTimeKeyDialogClass::onOkClicked()
{
    m_BlurTime = static_cast<float>(m_blurTimeSpinBox->value());
    accept();
}



