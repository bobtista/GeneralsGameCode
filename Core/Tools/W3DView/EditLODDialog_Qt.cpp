#include "EditLODDialog_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QHeaderView>

CEditLODDialog::CEditLODDialog(QWidget* parent) :
    QDialog(parent),
    m_hierarchyList(nullptr),
    m_switchUpSpin(nullptr),
    m_switchDownSpin(nullptr),
    m_recalcButton(nullptr),
    m_spinIncrement(0.5f)
{
    initDialog();
}

void CEditLODDialog::initDialog()
{
    setWindowTitle("Edit LOD");
    resize(600, 400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_hierarchyList = new QTreeWidget(this);
    m_hierarchyList->setHeaderLabels(QStringList() << "Name" << "Switch Up" << "Switch Down");
    m_hierarchyList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(m_hierarchyList, &QTreeWidget::itemChanged, this, &CEditLODDialog::onItemChanged);
    mainLayout->addWidget(m_hierarchyList);

    QHBoxLayout* switchLayout = new QHBoxLayout();
    switchLayout->addWidget(new QLabel("Switch Up:", this));
    m_switchUpSpin = new QDoubleSpinBox(this);
    m_switchUpSpin->setRange(0.0, 10000.0);
    m_switchUpSpin->setValue(0.0);
    switchLayout->addWidget(m_switchUpSpin);
    switchLayout->addWidget(new QLabel("Switch Down:", this));
    m_switchDownSpin = new QDoubleSpinBox(this);
    m_switchDownSpin->setRange(0.0, 10000.0);
    m_switchDownSpin->setValue(0.0);
    switchLayout->addWidget(m_switchDownSpin);
    mainLayout->addLayout(switchLayout);

    m_recalcButton = new QPushButton("Recalculate", this);
    connect(m_recalcButton, &QPushButton::clicked, this, &CEditLODDialog::onRecalc);
    mainLayout->addWidget(m_recalcButton);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &CEditLODDialog::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &CEditLODDialog::onCancelClicked);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void CEditLODDialog::onOkClicked()
{
    accept();
}

void CEditLODDialog::onCancelClicked()
{
    reject();
}

void CEditLODDialog::onRecalc()
{
}

void CEditLODDialog::onItemChanged()
{
}

void CEditLODDialog::resetControls(int index)
{
}

void CEditLODDialog::enableControls(bool enable)
{
    m_switchUpSpin->setEnabled(enable);
    m_switchDownSpin->setEnabled(enable);
    m_recalcButton->setEnabled(enable);
}



