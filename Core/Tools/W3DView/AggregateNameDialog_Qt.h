#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QString>

class AggregateNameDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit AggregateNameDialogClass(QWidget* parent = nullptr);
    AggregateNameDialogClass(const QString& def_name, QWidget* parent = nullptr);

    QString Get_Name() const { return m_Name; }
    void Set_Name(const QString& name) { m_Name = name; }

private slots:
    void onOkClicked();

private:
    void initDialog();
    QString m_Name;
    QLineEdit* m_nameEdit;
};



