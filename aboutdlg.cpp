#include "aboutdlg.h"
#include "ui_aboutdlg.h"

AboutDlg::AboutDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDlg)
{
    ui->setupUi(this);
}

AboutDlg::~AboutDlg() {
    delete ui;
}

void AboutDlg::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);

    this->setFixedSize(this->size());
}

void AboutDlg::on_pushButton_clicked() {
    this->close();
}

