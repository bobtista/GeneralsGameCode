#include "VolumeRandomDialog_Qt.h"
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef _WIN32
#include "v3_rnd.h"
#include "vector3.h"
#else
#include "GameEngineStubs.h"
#endif
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>

VolumeRandomDialogClass::VolumeRandomDialogClass(Vector3Randomizer* randomizer, QWidget* parent) :
    QDialog(parent),
    m_Randomizer(nullptr),
    m_typeGroup(nullptr),
    m_boxRadio(nullptr),
    m_cylinderRadio(nullptr),
    m_sphereRadio(nullptr),
    m_sphereHollowCheck(nullptr),
    m_boxXSpin(nullptr),
    m_boxYSpin(nullptr),
    m_boxZSpin(nullptr),
    m_sphereRadiusSpin(nullptr),
    m_cylinderRadiusSpin(nullptr),
    m_cylinderHeightSpin(nullptr)
{
    initDialog();
    if (randomizer)
    {
        m_Randomizer = randomizer;
    }
}

void VolumeRandomDialogClass::initDialog()
{
    setWindowTitle("Volume Randomizer");
    resize(500, 400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_typeGroup = new QButtonGroup(this);
    m_boxRadio = new QRadioButton("Box", this);
    m_cylinderRadio = new QRadioButton("Cylinder", this);
    m_sphereRadio = new QRadioButton("Sphere", this);
    m_typeGroup->addButton(m_boxRadio, 0);
    m_typeGroup->addButton(m_cylinderRadio, 1);
    m_typeGroup->addButton(m_sphereRadio, 2);
    mainLayout->addWidget(m_boxRadio);
    mainLayout->addWidget(m_cylinderRadio);
    mainLayout->addWidget(m_sphereRadio);

    connect(m_boxRadio, &QRadioButton::toggled, this, &VolumeRandomDialogClass::updateEnableState);
    connect(m_cylinderRadio, &QRadioButton::toggled, this, &VolumeRandomDialogClass::updateEnableState);
    connect(m_sphereRadio, &QRadioButton::toggled, this, &VolumeRandomDialogClass::updateEnableState);

    QGroupBox* boxGroup = new QGroupBox("Box", this);
    QVBoxLayout* boxLayout = new QVBoxLayout();
    QHBoxLayout* xLayout = new QHBoxLayout();
    xLayout->addWidget(new QLabel("X:", this));
    m_boxXSpin = new QDoubleSpinBox(this);
    m_boxXSpin->setRange(-10000.0, 10000.0);
    m_boxXSpin->setValue(1.0);
    xLayout->addWidget(m_boxXSpin);
    boxLayout->addLayout(xLayout);
    QHBoxLayout* yLayout = new QHBoxLayout();
    yLayout->addWidget(new QLabel("Y:", this));
    m_boxYSpin = new QDoubleSpinBox(this);
    m_boxYSpin->setRange(-10000.0, 10000.0);
    m_boxYSpin->setValue(1.0);
    yLayout->addWidget(m_boxYSpin);
    boxLayout->addLayout(yLayout);
    QHBoxLayout* zLayout = new QHBoxLayout();
    zLayout->addWidget(new QLabel("Z:", this));
    m_boxZSpin = new QDoubleSpinBox(this);
    m_boxZSpin->setRange(-10000.0, 10000.0);
    m_boxZSpin->setValue(1.0);
    zLayout->addWidget(m_boxZSpin);
    boxLayout->addLayout(zLayout);
    boxGroup->setLayout(boxLayout);
    mainLayout->addWidget(boxGroup);

    QGroupBox* sphereGroup = new QGroupBox("Sphere", this);
    QVBoxLayout* sphereLayout = new QVBoxLayout();
    QHBoxLayout* radiusLayout = new QHBoxLayout();
    radiusLayout->addWidget(new QLabel("Radius:", this));
    m_sphereRadiusSpin = new QDoubleSpinBox(this);
    m_sphereRadiusSpin->setRange(0.0, 10000.0);
    m_sphereRadiusSpin->setValue(1.0);
    radiusLayout->addWidget(m_sphereRadiusSpin);
    sphereLayout->addLayout(radiusLayout);
    m_sphereHollowCheck = new QCheckBox("Hollow", this);
    sphereLayout->addWidget(m_sphereHollowCheck);
    sphereGroup->setLayout(sphereLayout);
    mainLayout->addWidget(sphereGroup);

    QGroupBox* cylinderGroup = new QGroupBox("Cylinder", this);
    QVBoxLayout* cylinderLayout = new QVBoxLayout();
    QHBoxLayout* cylRadiusLayout = new QHBoxLayout();
    cylRadiusLayout->addWidget(new QLabel("Radius:", this));
    m_cylinderRadiusSpin = new QDoubleSpinBox(this);
    m_cylinderRadiusSpin->setRange(0.0, 10000.0);
    m_cylinderRadiusSpin->setValue(1.0);
    cylRadiusLayout->addWidget(m_cylinderRadiusSpin);
    cylinderLayout->addLayout(cylRadiusLayout);
    QHBoxLayout* cylHeightLayout = new QHBoxLayout();
    cylHeightLayout->addWidget(new QLabel("Height:", this));
    m_cylinderHeightSpin = new QDoubleSpinBox(this);
    m_cylinderHeightSpin->setRange(0.0, 10000.0);
    m_cylinderHeightSpin->setValue(1.0);
    cylHeightLayout->addWidget(m_cylinderHeightSpin);
    cylinderLayout->addLayout(cylHeightLayout);
    cylinderGroup->setLayout(cylinderLayout);
    mainLayout->addWidget(cylinderGroup);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &VolumeRandomDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    m_boxRadio->setChecked(true);
    updateEnableState();
}

void VolumeRandomDialogClass::updateEnableState()
{
    bool boxEnabled = m_boxRadio->isChecked();
    bool cylinderEnabled = m_cylinderRadio->isChecked();
    bool sphereEnabled = m_sphereRadio->isChecked();

    m_boxXSpin->setEnabled(boxEnabled);
    m_boxYSpin->setEnabled(boxEnabled);
    m_boxZSpin->setEnabled(boxEnabled);
    m_sphereRadiusSpin->setEnabled(sphereEnabled);
    m_sphereHollowCheck->setEnabled(sphereEnabled);
    m_cylinderRadiusSpin->setEnabled(cylinderEnabled);
    m_cylinderHeightSpin->setEnabled(cylinderEnabled);
}

void VolumeRandomDialogClass::onBoxRadio()
{
    updateEnableState();
}

void VolumeRandomDialogClass::onCylinderRadio()
{
    updateEnableState();
}

void VolumeRandomDialogClass::onSphereRadio()
{
    updateEnableState();
}

void VolumeRandomDialogClass::onOkClicked()
{
    if (m_boxRadio->isChecked())
    {
        Vector3 extents(static_cast<float>(m_boxXSpin->value()),
                       static_cast<float>(m_boxYSpin->value()),
                       static_cast<float>(m_boxZSpin->value()));
        m_Randomizer = new Vector3SolidBoxRandomizer(extents);
    }
    else if (m_sphereRadio->isChecked())
    {
        float radius = static_cast<float>(m_sphereRadiusSpin->value());
        if (m_sphereHollowCheck->isChecked())
        {
            m_Randomizer = new Vector3HollowSphereRandomizer(radius);
        }
        else
        {
            m_Randomizer = new Vector3SolidSphereRandomizer(radius);
        }
    }
    else if (m_cylinderRadio->isChecked())
    {
        float radius = static_cast<float>(m_cylinderRadiusSpin->value());
        float height = static_cast<float>(m_cylinderHeightSpin->value());
        m_Randomizer = new Vector3SolidCylinderRandomizer(height, radius);
    }
    accept();
}


