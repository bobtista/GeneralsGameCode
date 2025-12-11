#include "SoundEditDialog_Qt.h"
// TheSuperHackers @refactor bobtista 01/01/2025 Use GameEngineStubs for all platforms (Core build)
#include "GameEngineStubs.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>

SoundEditDialogClass::SoundEditDialogClass(QWidget* parent) :
    QDialog(parent),
    SoundRObj(nullptr),
    m_filenameEdit(nullptr),
    m_volumeSlider(nullptr),
    m_prioritySlider(nullptr),
    m_2DRadio(nullptr),
    m_3DRadio(nullptr),
    m_playButton(nullptr)
{
    initDialog();
}

SoundEditDialogClass::~SoundEditDialogClass()
{
    if (SoundRObj)
    {
        REF_PTR_RELEASE(SoundRObj);
    }
}

void SoundEditDialogClass::initDialog()
{
    setWindowTitle("Sound Edit");
    resize(500, 300);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* filenameLayout = new QHBoxLayout();
    filenameLayout->addWidget(new QLabel("Filename:", this));
    m_filenameEdit = new QLineEdit(this);
    filenameLayout->addWidget(m_filenameEdit);
    QPushButton* browseButton = new QPushButton("Browse...", this);
    connect(browseButton, &QPushButton::clicked, this, &SoundEditDialogClass::onBrowse);
    filenameLayout->addWidget(browseButton);
    mainLayout->addLayout(filenameLayout);

    QGroupBox* typeGroup = new QGroupBox("Type", this);
    QHBoxLayout* typeLayout = new QHBoxLayout();
    m_2DRadio = new QRadioButton("2D", this);
    m_3DRadio = new QRadioButton("3D", this);
    typeLayout->addWidget(m_2DRadio);
    typeLayout->addWidget(m_3DRadio);
    typeGroup->setLayout(typeLayout);
    mainLayout->addWidget(typeGroup);
    connect(m_2DRadio, &QRadioButton::toggled, this, &SoundEditDialogClass::updateEnableState);
    connect(m_3DRadio, &QRadioButton::toggled, this, &SoundEditDialogClass::updateEnableState);

    QHBoxLayout* volumeLayout = new QHBoxLayout();
    volumeLayout->addWidget(new QLabel("Volume:", this));
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    volumeLayout->addWidget(m_volumeSlider);
    mainLayout->addLayout(volumeLayout);

    QHBoxLayout* priorityLayout = new QHBoxLayout();
    priorityLayout->addWidget(new QLabel("Priority:", this));
    m_prioritySlider = new QSlider(Qt::Horizontal, this);
    m_prioritySlider->setRange(0, 100);
    m_prioritySlider->setValue(50);
    priorityLayout->addWidget(m_prioritySlider);
    mainLayout->addLayout(priorityLayout);

    m_playButton = new QPushButton("Play", this);
    connect(m_playButton, &QPushButton::clicked, this, &SoundEditDialogClass::onPlay);
    mainLayout->addWidget(m_playButton);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SoundEditDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SoundEditDialogClass::onCancelClicked);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    m_2DRadio->setChecked(true);
    updateEnableState();
}

void SoundEditDialogClass::Set_Sound(SoundRenderObjClass* sound)
{
    REF_PTR_SET(SoundRObj, sound);
}

SoundRenderObjClass* SoundEditDialogClass::Get_Sound() const
{
    if (SoundRObj != nullptr)
    {
        SoundRObj->Add_Ref();
    }
    return SoundRObj;
}

void SoundEditDialogClass::onBrowse()
{
    QString filename = QFileDialog::getOpenFileName(this, "Select Sound File", m_filenameEdit->text(), "Sound Files (*.wav *.mp3);;All Files (*.*)");
    if (!filename.isEmpty())
    {
        m_filenameEdit->setText(filename);
    }
}

void SoundEditDialogClass::on2DRadio()
{
    updateEnableState();
}

void SoundEditDialogClass::on3DRadio()
{
    updateEnableState();
}

void SoundEditDialogClass::onPlay()
{
}

void SoundEditDialogClass::onOkClicked()
{
    accept();
}

void SoundEditDialogClass::onCancelClicked()
{
    reject();
}

void SoundEditDialogClass::updateEnableState()
{
}


