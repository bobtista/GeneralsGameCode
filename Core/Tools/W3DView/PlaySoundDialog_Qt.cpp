#include "PlaySoundDialog_Qt.h"
// TheSuperHackers @refactor bobtista 01/01/2025 Use GameEngineStubs for all platforms (Core build)
#include "GameEngineStubs.h"
#include "Utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>

PlaySoundDialogClass::PlaySoundDialogClass(const QString& filename, QWidget* parent) :
    QDialog(parent),
    m_filename(filename),
    m_soundObj(nullptr)
{
    initDialog();
}

void PlaySoundDialogClass::initDialog()
{
    setWindowTitle("Play Sound Effect");
    resize(400, 150);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* filenameLabel = new QLabel("Filename:", this);
    mainLayout->addWidget(filenameLabel);

    QLabel* filenameValue = new QLabel(m_filename, this);
    mainLayout->addWidget(filenameValue);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* playButton = new QPushButton("Play", this);
    QPushButton* stopButton = new QPushButton("Stop", this);
    buttonLayout->addWidget(playButton);
    buttonLayout->addWidget(stopButton);
    mainLayout->addLayout(buttonLayout);

    connect(playButton, &QPushButton::clicked, this, &PlaySoundDialogClass::onPlaySoundEffect);
    connect(stopButton, &QPushButton::clicked, this, &PlaySoundDialogClass::onStopSoundEffect);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    // TheSuperHackers @refactor bobtista 01/01/2025 Conditionally compile audio functionality
    #ifdef _WIN32
    m_soundObj = WWAudioClass::Get_Instance()->Create_Sound_Effect(m_filename.toLocal8Bit().constData());
    #else
    m_soundObj = nullptr;  // Audio not available on non-Windows
    #endif
    if (m_soundObj == nullptr)
    {
        QMessageBox::warning(this, "File Not Found", QString("Cannot find sound file: %1!").arg(m_filename));
        reject();
    }
    else
    {
        onPlaySoundEffect();
    }
}

void PlaySoundDialogClass::onPlaySoundEffect()
{
    if (m_soundObj != nullptr)
    {
        m_soundObj->Stop();
        m_soundObj->Play();
    }
}

void PlaySoundDialogClass::onStopSoundEffect()
{
    if (m_soundObj != nullptr)
    {
        m_soundObj->Stop();
    }
}

void PlaySoundDialogClass::closeEvent(QCloseEvent* event)
{
    if (m_soundObj != nullptr)
    {
        m_soundObj->Stop();
        // TheSuperHackers @refactor bobtista 01/01/2025 Conditionally compile REF_PTR_RELEASE
        #ifdef _WIN32
        REF_PTR_RELEASE(m_soundObj);
        #else
        if (m_soundObj) { delete m_soundObj; m_soundObj = nullptr; }
        #endif
    }
    QDialog::closeEvent(event);
}


