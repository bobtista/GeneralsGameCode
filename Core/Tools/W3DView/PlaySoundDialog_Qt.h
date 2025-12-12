#pragma once

#include <QDialog>
#include <QString>

class AudibleSoundClass;

class PlaySoundDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit PlaySoundDialogClass(const QString& filename, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onPlaySoundEffect();
    void onStopSoundEffect();

private:
    void initDialog();
    QString m_filename;
    AudibleSoundClass* m_soundObj;
};



