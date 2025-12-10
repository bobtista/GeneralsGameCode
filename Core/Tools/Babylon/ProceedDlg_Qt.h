#pragma once

#include <QDialog>
#include <QString>

#define IDALWAYS 101

class ProceedDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ProceedDlg(const QString& message, QWidget* parent = nullptr);

private slots:
    void onYes();
    void onAlways();
    void onNo();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void initDialog();
    QString m_message;
};



