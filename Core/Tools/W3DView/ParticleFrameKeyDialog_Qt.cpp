#include "ParticleFrameKeyDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>

ParticleFrameKeyDialogClass::ParticleFrameKeyDialogClass(float frame, QWidget* parent) :
    QDialog(parent),
    m_Frame(frame),
    m_frameSpinBox(nullptr)
{
    initDialog();
}

void ParticleFrameKeyDialogClass::initDialog()
{
    setWindowTitle("Particle Frame Key");
    resize(300, 100);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* frameLayout = new QHBoxLayout();
    QLabel* frameLabel = new QLabel("Frame:", this);
    m_frameSpinBox = new QDoubleSpinBox(this);
    m_frameSpinBox->setRange(-1024.0, 1024.0);
    m_frameSpinBox->setValue(m_Frame);
    m_frameSpinBox->setDecimals(2);
    frameLayout->addWidget(frameLabel);
    frameLayout->addWidget(m_frameSpinBox);
    mainLayout->addLayout(frameLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ParticleFrameKeyDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void ParticleFrameKeyDialogClass::onOkClicked()
{
    m_Frame = static_cast<float>(m_frameSpinBox->value());
    accept();
}



