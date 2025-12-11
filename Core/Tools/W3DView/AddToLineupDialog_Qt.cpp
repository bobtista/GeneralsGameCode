#include "AddToLineupDialog_Qt.h"
#include "ViewerScene.h"
#include <QLineEdit>  // TheSuperHackers @refactor bobtista 01/01/2025 Needed for lineEdit()
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef HAVE_WWVEGAS
    // Use real WWVegas headers when available (Generals/GeneralsMD builds)
    #include "rendobj.h"
    #include "assetmgr.h"  // For WW3DAssetManager
#else
    // Use stubs for Core-only build
    #include "GameEngineStubs.h"
#endif
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>

CAddToLineupDialog::CAddToLineupDialog(ViewerSceneClass* scene, QWidget* parent) :
    QDialog(parent),
    m_pCScene(scene),
    m_Object(""),
    m_objectCombo(nullptr)
{
    initDialog();
}

void CAddToLineupDialog::initDialog()
{
    setWindowTitle("Add To Lineup");
    resize(400, 100);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* label = new QLabel("Object:", this);
    mainLayout->addWidget(label);

    m_objectCombo = new QComboBox(this);
    m_objectCombo->setEditable(true);
    // TheSuperHackers @refactor bobtista 01/01/2025 QComboBox doesn't have setMaxLength, use QLineEdit's setMaxLength via lineEdit()
    if (m_objectCombo->lineEdit()) {
        m_objectCombo->lineEdit()->setMaxLength(64);
    }
    mainLayout->addWidget(m_objectCombo);

    if (m_pCScene)
    {
        WW3DAssetManager* assets = WW3DAssetManager::Get_Instance();
        if (assets != nullptr)
        {
            RenderObjIterator* it = assets->Create_Render_Obj_Iterator();
            if (it != nullptr)
            {
                for (; !it->Is_Done(); it->Next())
                {
                    if (m_pCScene->Can_Line_Up(it->Current_Item_Class_ID()))
                    {
                        m_objectCombo->addItem(QString::fromLocal8Bit(it->Current_Item_Name()));
                    }
                }
                assets->Release_Render_Obj_Iterator(it);
            }
        }
    }

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &CAddToLineupDialog::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void CAddToLineupDialog::onOkClicked()
{
    QString text = m_objectCombo->currentText();
    if (text.isEmpty())
    {
        QMessageBox::warning(this, "Add To Lineup", "Please select an object, or type in an object name.");
        return;
    }

    m_Object = text;
    accept();
}


