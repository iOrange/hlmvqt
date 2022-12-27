#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "renderview.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QMimeData>
#include <QDragEnterEvent>

#include "aboutdlg.h"
#include "halflifemodel.h"


// settings
static const QString kLastOpenPath("LastOpenPath");
static const QString kLastSavePath("LastSavePath");


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mRenderView{}
    , mModel{}
{
    ui->setupUi(this);

    mRenderView = MakeStrongPtr<RenderView>(ui->pnlUpper);
    ui->verticalLayout_2->addWidget(mRenderView.get());
    mRenderView->setSizePolicy(ui->pnlUpper->sizePolicy());
    mRenderView->setGeometry(QRect(0, 0, ui->pnlUpper->width(), ui->pnlUpper->height()));

    this->setAcceptDrops(true);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::OpenModel(const fs::path& filePath) {
    StrongPtr<HalfLifeModel> mdl = MakeStrongPtr<HalfLifeModel>();
    if (mdl->LoadFromPath(filePath)) {
        mRenderView->SetModel(nullptr);
        mModel.swap(mdl);
        mRenderView->SetModel(mModel.get());

        QSettings registry;
        QString lastOpenDir = registry.value(kLastOpenPath).toString();

        fs::path folderPath = fs::absolute(filePath.parent_path());
        lastOpenDir = QString::fromStdString(folderPath.u8string());
        registry.setValue(kLastOpenPath, lastOpenDir);

        this->UpdateUIForModel();
    }
}


void MainWindow::showEvent(QShowEvent* ev) {
    QMainWindow::showEvent(ev);
    // Call slot via queued connection so it's called from the UI thread after this method has returned and the window has been shown
    QMetaObject::invokeMethod(this, &MainWindow::on_WindowShown, Qt::ConnectionType::QueuedConnection);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    const QMimeData* md = event->mimeData();
    if (md->hasUrls()) {
        const QList<QUrl>& urls = md->urls();
        if (urls.size() > 0) {
            const QUrl& url = urls[0];
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                if (filePath.endsWith(".mdl")) {
                    event->acceptProposedAction();
                }
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QList<QUrl>& urls = event->mimeData()->urls();
    const QUrl & url = urls[0];
    if (url.isLocalFile()) {
        QString filePath = url.toLocalFile();
        if (filePath.endsWith(".mdl")) {
            this->OpenModel(filePath.toStdString());

            event->acceptProposedAction();
        }
    }
}


void MainWindow::on_WindowShown() {
}

void MainWindow::on_actionLoad_model_triggered() {
    QSettings registry;
    QString lastOpenDir = registry.value(kLastOpenPath).toString();

    QString path = QFileDialog::getOpenFileName(this, tr("Select Half-Life model..."), lastOpenDir, tr("Half-Life model (*.mdl)"));
    if (!path.isEmpty()) {
        fs::path mdlPath = path.toStdString();
        this->OpenModel(mdlPath);
    }
}


void MainWindow::on_actionE_xit_triggered() {
    this->close();
}


void MainWindow::on_actionAbout_Qt_triggered() {
    QMessageBox::aboutQt(this, "About Qt...");
}


void MainWindow::on_actionAbout_triggered() {
    AboutDlg dlg(this);
    dlg.exec();
}


void MainWindow::on_lstTextures_currentRowChanged(int currentRow) {
    if (mModel && currentRow >= 0 && currentRow < mModel->GetTexturesCount()) {
        const HalfLifeModelTexture& hltexture = mModel->GetTexture(currentRow);
        ui->chkIsChromeTexture->setChecked(hltexture.chrome);
        ui->lblTextureSize->setText(QString("%1 x %2 %3").arg(hltexture.width).arg(hltexture.height).arg(tr("pixels")));
    }
}

void MainWindow::on_btnExportTexture_clicked() {
    if (!mModel) {
        return;
    }

    const int selectedTextureIdx = ui->lstTextures->currentRow();
    if (selectedTextureIdx < 0 || selectedTextureIdx > mModel->GetTexturesCount()) {
        return;
    }

    QSettings registry;
    QString lastSaveDir = registry.value(kLastSavePath).toString();

    QString path = QFileDialog::getSaveFileName(this, tr("Where to save texture..."), lastSaveDir, tr("BMP image (*.bmp)"));
    if (!path.isEmpty()) {
        const HalfLifeModelTexture& hltexture = mModel->GetTexture(selectedTextureIdx);

        QImage img(hltexture.data.data(), hltexture.width, hltexture.height, hltexture.width, QImage::Format_Indexed8);
        QList<uint32_t> imgPal(256);
        for (qsizetype i = 0; i < imgPal.size(); ++i) {
            imgPal[i] = qRgb(hltexture.palette[i * 3], hltexture.palette[i * 3 + 1], hltexture.palette[i * 3 + 2]);
        }
        img.setColorTable(imgPal);

        if (!img.save(path, "BMP")) {
            QMessageBox::critical(this, this->windowTitle(), tr("Failed to export texture!"));
        }

        fs::path folderPath = fs::absolute(fs::path(path.toStdString()).parent_path());
        lastSaveDir = QString::fromStdString(folderPath.u8string());
        registry.setValue(kLastSavePath, lastSaveDir);
    }
}

void MainWindow::UpdateUIForModel() {
    if (mModel) {
        // model tab
        RenderOptions options;
        options.Reset();
        mRenderView->SetRenderOptions(options);
        ui->chkRenderTextured->setChecked(true);


        ui->lstTextures->clear();

        const size_t numTextures = mModel->GetTexturesCount();
        for (size_t i = 0; i < numTextures; ++i) {
            const HalfLifeModelTexture& hltexture = mModel->GetTexture(i);
            ui->lstTextures->addItem(QString::fromStdString(hltexture.name));
        }
    }
}


void MainWindow::on_chkRenderTextured_stateChanged(int state) {
    if (mModel) {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.renderTextured = (Qt::Checked == state);
        mRenderView->SetRenderOptions(options);
    }
}


void MainWindow::on_chkShowBones_stateChanged(int state) {

}


void MainWindow::on_chkShowAttachments_stateChanged(int state) {

}


void MainWindow::on_chkShowHitBoxes_stateChanged(int state) {

}


void MainWindow::on_chkShowNormals_stateChanged(int state) {

}


void MainWindow::on_chkWireframeModel_stateChanged(int state) {
    if (mModel) {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.showWireframe = (Qt::Checked == state);
        if (options.showWireframe) {
            ui->chkWireframeoverlay->setChecked(false);
            options.overlayWireframe = false;
        }
        mRenderView->SetRenderOptions(options);
    }
}


void MainWindow::on_chkWireframeoverlay_stateChanged(int state) {
    if (mModel) {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.overlayWireframe = (Qt::Checked == state);
        if (options.overlayWireframe) {
            ui->chkWireframeModel->setChecked(false);
            options.showWireframe = false;
        }
        mRenderView->SetRenderOptions(options);
    }
}

