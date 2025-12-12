#include "AnimatedSoundOptionsDialog_Qt.h"
#include "Globals.h"
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef _WIN32
#include "ffactory.h"
#include "animatedsoundmgr.h"
#include "wwsaveload.h"
#include "definitionmgr.h"
#include "WWFILE.h"
#include "chunkio.h"
#include "wwdebug.h"
#include "RestrictedFileDialog.h"
#else
#include "GameEngineStubs.h"
#endif
#include "Utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QSettings>
#include <QDir>
#include <QFileInfo>

AnimatedSoundOptionsDialogClass::AnimatedSoundOptionsDialogClass(QWidget* parent) :
    QDialog(parent),
    m_soundDefLibEdit(nullptr),
    m_soundIniEdit(nullptr),
    m_soundPathEdit(nullptr)
{
    initDialog();
}

void AnimatedSoundOptionsDialogClass::initDialog()
{
    setWindowTitle("Animated Sound Options");
    resize(600, 200);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* defLibLayout = new QHBoxLayout();
    QLabel* defLibLabel = new QLabel("Sound Definition Library:", this);
    m_soundDefLibEdit = new QLineEdit(this);
    QPushButton* defLibBrowse = new QPushButton("Browse...", this);
    defLibLayout->addWidget(defLibLabel);
    defLibLayout->addWidget(m_soundDefLibEdit);
    defLibLayout->addWidget(defLibBrowse);
    mainLayout->addLayout(defLibLayout);
    connect(defLibBrowse, &QPushButton::clicked, this, &AnimatedSoundOptionsDialogClass::onSoundDefinitionLibraryBrowse);

    QHBoxLayout* iniLayout = new QHBoxLayout();
    QLabel* iniLabel = new QLabel("Sound INI:", this);
    m_soundIniEdit = new QLineEdit(this);
    QPushButton* iniBrowse = new QPushButton("Browse...", this);
    iniLayout->addWidget(iniLabel);
    iniLayout->addWidget(m_soundIniEdit);
    iniLayout->addWidget(iniBrowse);
    mainLayout->addLayout(iniLayout);
    connect(iniBrowse, &QPushButton::clicked, this, &AnimatedSoundOptionsDialogClass::onSoundIniBrowse);

    QHBoxLayout* pathLayout = new QHBoxLayout();
    QLabel* pathLabel = new QLabel("Sound Path:", this);
    m_soundPathEdit = new QLineEdit(this);
    QPushButton* pathBrowse = new QPushButton("Browse...", this);
    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(m_soundPathEdit);
    pathLayout->addWidget(pathBrowse);
    mainLayout->addLayout(pathLayout);
    connect(pathBrowse, &QPushButton::clicked, this, &AnimatedSoundOptionsDialogClass::onSoundPathBrowse);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &AnimatedSoundOptionsDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    QSettings settings;
    QString soundDefLibPath = settings.value("Config/SoundDefLibPath", "").toString();
    QString soundIniPath = settings.value("Config/AnimSoundINIPath", "").toString();
    QString soundDataPath = settings.value("Config/AnimSoundDataPath", "").toString();

    m_soundDefLibEdit->setText(soundDefLibPath);
    m_soundIniEdit->setText(soundIniPath);
    m_soundPathEdit->setText(soundDataPath);
}

void AnimatedSoundOptionsDialogClass::onSoundDefinitionLibraryBrowse()
{
    QString filename = QFileDialog::getOpenFileName(this,
        "Select Definition Database File",
        m_soundDefLibEdit->text(),
        "Definition Database Files (*.ddb);;All Files (*.*)");
    if (!filename.isEmpty())
    {
        m_soundDefLibEdit->setText(filename);
    }
}

void AnimatedSoundOptionsDialogClass::onSoundIniBrowse()
{
    QString filename = QFileDialog::getOpenFileName(this,
        "Select INI File",
        m_soundIniEdit->text(),
        "INI Files (*.ini);;All Files (*.*)");
    if (!filename.isEmpty())
    {
        m_soundIniEdit->setText(filename);
    }
}

void AnimatedSoundOptionsDialogClass::onSoundPathBrowse()
{
    QString filename = QFileDialog::getOpenFileName(this,
        "Pick Sound Path",
        m_soundPathEdit->text(),
        "All Files (*.*)");
    if (!filename.isEmpty())
    {
        QFileInfo fileInfo(filename);
        QString path = fileInfo.absolutePath();
        m_soundPathEdit->setText(path);
    }
}

void AnimatedSoundOptionsDialogClass::onOkClicked()
{
    QString soundDefLibPath = m_soundDefLibEdit->text();
    QString soundIniPath = m_soundIniEdit->text();
    QString soundDataPath = m_soundPathEdit->text();

    QSettings settings;
    settings.setValue("Config/SoundDefLibPath", soundDefLibPath);
    settings.setValue("Config/AnimSoundINIPath", soundIniPath);
    settings.setValue("Config/AnimSoundDataPath", soundDataPath);

    Load_Animated_Sound_Settings();
    accept();
}

void AnimatedSoundOptionsDialogClass::Load_Animated_Sound_Settings()
{
#ifdef _WIN32
    DefinitionMgrClass::Free_Definitions();

    QSettings settings;
    StringClass soundDefLibPath = settings.value("Config/SoundDefLibPath", "").toString().toLocal8Bit().constData();
    StringClass soundIniPath = settings.value("Config/AnimSoundINIPath", "").toString().toLocal8Bit().constData();
    StringClass soundDataPath = settings.value("Config/AnimSoundDataPath", "").toString().toLocal8Bit().constData();

    FileClass* file = _TheFileFactory->Get_File(soundDefLibPath);
    if (file != nullptr)
    {
        file->Open(FileClass::READ);
        ChunkLoadClass cload(file);
        SaveLoadSystemClass::Load(cload);
        file->Close();
        _TheFileFactory->Return_File(file);
    }
    else
    {
        WWDEBUG_SAY(("Failed to load file %s", soundDefLibPath.str()));
    }

    AnimatedSoundMgrClass::Shutdown();
    AnimatedSoundMgrClass::Initialize(soundIniPath);

    _TheSimpleFileFactory->Append_Sub_Directory(soundDataPath);
#else
    // Stub for non-Windows
    (void)0;
#endif
}


