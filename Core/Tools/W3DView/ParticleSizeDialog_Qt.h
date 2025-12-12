#pragma once

#include <QDialog>
#include <QDoubleSpinBox>

class ParticleSizeDialogClass : public QDialog
{
    Q_OBJECT

public:
    explicit ParticleSizeDialogClass(float size, QWidget* parent = nullptr);

    float Get_Size() const { return m_Size; }

private slots:
    void onOkClicked();

private:
    void initDialog();
    float m_Size;
    QDoubleSpinBox* m_sizeSpinBox;
};



