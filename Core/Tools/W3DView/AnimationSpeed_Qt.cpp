#include "AnimationSpeed_Qt.h"
#include "MainFrm_Qt.h"
#include "GraphicView_Qt.h"
#include "Utils.h"
#include "W3DViewDoc_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QButtonGroup>
#include <QApplication>

CAnimationSpeed::CAnimationSpeed(QWidget* parent) :
    QDialog(parent),
    m_speedSlider(nullptr),
    m_blendCheckBox(nullptr),
    m_compressQCheckBox(nullptr),
    m_16BitRadio(nullptr),
    m_8BitRadio(nullptr),
    m_speedLabel(nullptr),
    m_iInitialPercent(0)
{
    initDialog();
}

void CAnimationSpeed::initDialog()
{
    setWindowTitle("Animation Speed");
    resize(400, 200);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_speedLabel = new QLabel("Speed: 100%", this);
    mainLayout->addWidget(m_speedLabel);

    m_speedSlider = new QSlider(Qt::Horizontal, this);
    m_speedSlider->setRange(1, 200);
    mainLayout->addWidget(m_speedSlider);
    connect(m_speedSlider, &QSlider::valueChanged, this, &CAnimationSpeed::onSpeedSliderValueChanged);

    m_blendCheckBox = new QCheckBox("Blend", this);
    mainLayout->addWidget(m_blendCheckBox);
    connect(m_blendCheckBox, &QCheckBox::toggled, this, &CAnimationSpeed::onBlendToggled);

    m_compressQCheckBox = new QCheckBox("Compress Q", this);
    mainLayout->addWidget(m_compressQCheckBox);
    connect(m_compressQCheckBox, &QCheckBox::toggled, this, &CAnimationSpeed::onCompressQToggled);

    QButtonGroup* bitGroup = new QButtonGroup(this);
    m_16BitRadio = new QRadioButton("16 Bit", this);
    m_8BitRadio = new QRadioButton("8 Bit", this);
    bitGroup->addButton(m_16BitRadio, 0);
    bitGroup->addButton(m_8BitRadio, 1);
    mainLayout->addWidget(m_16BitRadio);
    mainLayout->addWidget(m_8BitRadio);
    connect(m_16BitRadio, &QRadioButton::toggled, this, &CAnimationSpeed::on16BitToggled);
    connect(m_8BitRadio, &QRadioButton::toggled, this, &CAnimationSpeed::on8BitToggled);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    CW3DViewDoc* pCDoc = ::GetCurrentDocument();
    if (pCDoc)
    {
        m_blendCheckBox->setChecked(pCDoc->GetAnimationBlend());
        m_compressQCheckBox->setChecked(pCDoc->GetChannelQCompression());
        if (pCDoc->GetChannelQCompression())
        {
            m_16BitRadio->setEnabled(true);
            m_8BitRadio->setEnabled(true);
        }
        else
        {
            m_16BitRadio->setEnabled(false);
            m_8BitRadio->setEnabled(false);
        }
    }

    CMainFrame* pCMainWnd = qobject_cast<CMainFrame*>(parent());
    if (!pCMainWnd)
    {
        QWidget* topLevel = QApplication::activeWindow();
        if (topLevel)
        {
            pCMainWnd = qobject_cast<CMainFrame*>(topLevel);
        }
    }
    if (pCMainWnd)
    {
        CGraphicView* pCGraphicView = pCMainWnd->GetGraphicView();
        if (pCGraphicView)
        {
            float animationSpeed = pCGraphicView->GetAnimationSpeed();
            m_iInitialPercent = int(animationSpeed * 100.00F);
        }
    }

    m_speedSlider->setValue(m_iInitialPercent);
    onSpeedSliderValueChanged(m_iInitialPercent);
}

void CAnimationSpeed::onSpeedSliderValueChanged(int value)
{
    m_iInitialPercent = value;
    m_speedLabel->setText(QString("Speed: %1%").arg(value));

    CMainFrame* pCMainWnd = qobject_cast<CMainFrame*>(parent());
    if (!pCMainWnd)
    {
        QWidget* topLevel = QApplication::activeWindow();
        if (topLevel)
        {
            pCMainWnd = qobject_cast<CMainFrame*>(topLevel);
        }
    }
    if (pCMainWnd)
    {
        CGraphicView* pCGraphicView = pCMainWnd->GetGraphicView();
        if (pCGraphicView)
        {
            pCGraphicView->SetAnimationSpeed(((float)m_iInitialPercent) / 100.00F);
        }
    }
}

void CAnimationSpeed::onBlendToggled(bool checked)
{
    CW3DViewDoc* pCDoc = ::GetCurrentDocument();
    if (pCDoc)
    {
        pCDoc->SetAnimationBlend(checked);
    }
}

void CAnimationSpeed::onCompressQToggled(bool checked)
{
    m_16BitRadio->setEnabled(checked);
    m_8BitRadio->setEnabled(checked);
}

void CAnimationSpeed::on16BitToggled(bool checked)
{
    if (checked)
    {
    }
}

void CAnimationSpeed::on8BitToggled(bool checked)
{
    if (checked)
    {
    }
}

