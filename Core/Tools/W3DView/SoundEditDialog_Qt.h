#pragma once

#include <QDialog>
#include <QSlider>
#include <QLineEdit>
#include <QRadioButton>
#include <QPushButton>

class SoundRenderObjClass;

class SoundEditDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit SoundEditDialogClass(QWidget* parent = nullptr);
    ~SoundEditDialogClass();

    void Set_Sound(SoundRenderObjClass* sound);
    SoundRenderObjClass* Get_Sound() const;

private slots:
    void onBrowse();
    void on2DRadio();
    void on3DRadio();
    void onPlay();
    void onOkClicked();
    void onCancelClicked();

private:
    void initDialog();
    void updateEnableState();
    SoundRenderObjClass* SoundRObj;
    QString OldName;
    QLineEdit* m_filenameEdit;
    QSlider* m_volumeSlider;
    QSlider* m_prioritySlider;
    QRadioButton* m_2DRadio;
    QRadioButton* m_3DRadio;
    QPushButton* m_playButton;
};



