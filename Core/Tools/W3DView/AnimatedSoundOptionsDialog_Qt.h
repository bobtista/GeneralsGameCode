#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QString>

class AnimatedSoundOptionsDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit AnimatedSoundOptionsDialogClass(QWidget* parent = nullptr);

    static void Load_Animated_Sound_Settings();

private slots:
    void onSoundDefinitionLibraryBrowse();
    void onSoundIniBrowse();
    void onSoundPathBrowse();
    void onOkClicked();

private:
    void initDialog();
    QLineEdit* m_soundDefLibEdit;
    QLineEdit* m_soundIniEdit;
    QLineEdit* m_soundPathEdit;
};



