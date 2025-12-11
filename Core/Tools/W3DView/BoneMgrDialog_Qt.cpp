#include "BoneMgrDialog_Qt.h"
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
// TheSuperHackers @refactor bobtista 01/01/2025 Use GameEngineStubs for all platforms (Core build)
#include "GameEngineStubs.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QHeaderView>

BoneMgrDialogClass::BoneMgrDialogClass(RenderObjClass* prender_obj, QWidget* parent) :
    QDialog(parent),
    m_pBaseModel(prender_obj),
    m_pBackupModel(nullptr),
    m_bAttach(false),
    m_objectCombo(nullptr),
    m_boneTree(nullptr),
    m_attachButton(nullptr)
{
    initDialog();
}

void BoneMgrDialogClass::initDialog()
{
    setWindowTitle("Bone Management");
    resize(600, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* objectLayout = new QHBoxLayout();
    objectLayout->addWidget(new QLabel("Object:", this));
    m_objectCombo = new QComboBox(this);
    connect(m_objectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BoneMgrDialogClass::onObjectComboChanged);
    objectLayout->addWidget(m_objectCombo);
    mainLayout->addLayout(objectLayout);

    m_boneTree = new QTreeWidget(this);
    m_boneTree->setHeaderLabels(QStringList() << "Bone" << "Attached Objects");
    m_boneTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(m_boneTree, &QTreeWidget::itemSelectionChanged, this, &BoneMgrDialogClass::onBoneTreeSelectionChanged);
    mainLayout->addWidget(m_boneTree);

    m_attachButton = new QPushButton("Attach", this);
    connect(m_attachButton, &QPushButton::clicked, this, &BoneMgrDialogClass::onAttachButton);
    mainLayout->addWidget(m_attachButton);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &BoneMgrDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &BoneMgrDialogClass::onCancelClicked);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void BoneMgrDialogClass::onOkClicked()
{
    accept();
}

void BoneMgrDialogClass::onCancelClicked()
{
    reject();
}

void BoneMgrDialogClass::onAttachButton()
{
}

void BoneMgrDialogClass::onObjectComboChanged()
{
}

void BoneMgrDialogClass::onBoneTreeSelectionChanged()
{
    updateControls(m_boneTree->currentItem());
}

void BoneMgrDialogClass::fillBoneItem(QTreeWidgetItem* bone_item, int bone_index)
{
}

void BoneMgrDialogClass::updateControls(QTreeWidgetItem* selected_item)
{
    m_attachButton->setEnabled(selected_item != nullptr);
}

QTreeWidgetItem* BoneMgrDialogClass::getCurrentBoneItem()
{
    return m_boneTree->currentItem();
}


