#pragma once

#include <QDialog>
#include <QComboBox>
#include <QString>

class ViewerSceneClass;

class CAddToLineupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CAddToLineupDialog(ViewerSceneClass* scene, QWidget* parent = nullptr);

    QString GetObject() const { return m_Object; }

private slots:
    void onOkClicked();

private:
    void initDialog();
    ViewerSceneClass* m_pCScene;
    QString m_Object;
    QComboBox* m_objectCombo;
};



