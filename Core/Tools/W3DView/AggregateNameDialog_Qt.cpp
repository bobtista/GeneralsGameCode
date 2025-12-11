#include "AggregateNameDialog_Qt.h"
// TheSuperHackers @refactor bobtista 01/01/2025 Conditionally include game engine headers
#ifdef HAVE_WWVEGAS
    // Use real WWVegas headers when available (Generals/GeneralsMD builds)
    #include "w3d_file.h"  // For W3D_NAME_LEN
#else
    // Use stubs for Core-only build
    #include "GameEngineStubs.h"
#endif
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

AggregateNameDialogClass::AggregateNameDialogClass(QWidget* parent) :
    QDialog(parent),
    m_Name(""),
    m_nameEdit(nullptr)
{
    initDialog();
}

AggregateNameDialogClass::AggregateNameDialogClass(const QString& def_name, QWidget* parent) :
    QDialog(parent),
    m_Name(def_name),
    m_nameEdit(nullptr)
{
    initDialog();
}

void AggregateNameDialogClass::initDialog()
{
    setWindowTitle("Aggregate Name");
    resize(300, 100);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* label = new QLabel("Aggregate Name:", this);
    mainLayout->addWidget(label);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setMaxLength(W3D_NAME_LEN - 1);
    m_nameEdit->setText(m_Name);
    mainLayout->addWidget(m_nameEdit);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &AggregateNameDialogClass::onOkClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void AggregateNameDialogClass::onOkClicked()
{
    m_Name = m_nameEdit->text();
    accept();
}


