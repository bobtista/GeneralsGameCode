#pragma once

#include <QDialog>
#include <QComboBox>
#include <QTreeWidget>
#include <QPushButton>

class RenderObjClass;

class BoneMgrDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit BoneMgrDialogClass(RenderObjClass* prender_obj, QWidget* parent = nullptr);

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onAttachButton();
    void onObjectComboChanged();
    void onBoneTreeSelectionChanged();

private:
    void initDialog();
    void fillBoneItem(QTreeWidgetItem* bone_item, int bone_index);
    void updateControls(QTreeWidgetItem* selected_item);
    QTreeWidgetItem* getCurrentBoneItem();
    RenderObjClass* m_pBaseModel;
    RenderObjClass* m_pBackupModel;
    bool m_bAttach;
    QString m_BoneName;
    QComboBox* m_objectCombo;
    QTreeWidget* m_boneTree;
    QPushButton* m_attachButton;
};



