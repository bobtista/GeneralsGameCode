#include "ProceedDlg_Qt.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDialog>

ProceedDlg::ProceedDlg(const QString& message, QWidget* parent) :
    QDialog(parent),
    m_message(message)
{
    initDialog();
}

void ProceedDlg::initDialog()
{
    setWindowTitle("Proceed");
    resize(400, 150);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* messageLabel = new QLabel(m_message, this);
    messageLabel->setWordWrap(true);
    mainLayout->addWidget(messageLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* yesButton = new QPushButton("Yes", this);
    QPushButton* alwaysButton = new QPushButton("Always", this);
    QPushButton* noButton = new QPushButton("No", this);
    buttonLayout->addWidget(yesButton);
    buttonLayout->addWidget(alwaysButton);
    buttonLayout->addWidget(noButton);
    mainLayout->addLayout(buttonLayout);

    connect(yesButton, &QPushButton::clicked, this, &ProceedDlg::onYes);
    connect(alwaysButton, &QPushButton::clicked, this, &ProceedDlg::onAlways);
    connect(noButton, &QPushButton::clicked, this, &ProceedDlg::onNo);

    setLayout(mainLayout);
}

void ProceedDlg::onYes()
{
    done(QMessageBox::Yes);
}

void ProceedDlg::onAlways()
{
    done(IDALWAYS);
}

void ProceedDlg::onNo()
{
    done(QMessageBox::No);
}

void ProceedDlg::closeEvent(QCloseEvent* event)
{
    done(QMessageBox::No);
    event->accept();
}

