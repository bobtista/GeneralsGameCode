#pragma once

#include <QDialog>
#include <QDoubleSpinBox>
#include <QTreeWidget>
#include <QPushButton>

class CEditLODDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CEditLODDialog(QWidget* parent = nullptr);

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onRecalc();
    void onItemChanged();

private:
    void initDialog();
    void resetControls(int index);
    void enableControls(bool enable);
    QTreeWidget* m_hierarchyList;
    QDoubleSpinBox* m_switchUpSpin;
    QDoubleSpinBox* m_switchDownSpin;
    QPushButton* m_recalcButton;
    float m_spinIncrement;
};



