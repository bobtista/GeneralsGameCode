#include "ParticleSizeDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

ParticleSizeDialogClass::ParticleSizeDialogClass(float size, QWidget* parent) :
    QDialog(parent),
    m_Size(size),
    m_sizeSpinBox(nullptr)
{
    initDialog();
}

void ParticleSizeDialogClass::initDialog()
{
    setWindowTitle("Particle Size");
    resize(300, 100);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* sizeLayout = new QHBoxLayout();
    QLabel* sizeLabel = new QLabel("Size:", this);
    m_sizeSpinBox = new QDoubleSpinBox(this);
    m_sizeSpinBox->setRange(0.0, 10000.0);
    m_sizeSpinBox->setValue(m_Size);
    m_sizeSpinBox->setDecimals(2);
    sizeLayout->addWidget(sizeLabel);
    sizeLayout->addWidget(m_sizeSpinBox);
    mainLayout->addLayout(sizeLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ParticleSizeDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void ParticleSizeDialogClass::onOkClicked()
{
    m_Size = static_cast<float>(m_sizeSpinBox->value());
    accept();
}



