#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "renderview.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDir>
#include <QColorDialog>

#include "aboutdlg.h"
#include "halflifemodel.h"



static void HLTextureToQImage(const HalfLifeModelTexture& hltexture, QImage& result) {
    result = QImage(hltexture.data.data(), hltexture.width, hltexture.height, hltexture.width, QImage::Format_Indexed8);
    QList<uint32_t> imgPal(256);
    for (qsizetype i = 0; i < imgPal.size(); ++i) {
        imgPal[i] = qRgb(hltexture.palette[i * 3], hltexture.palette[i * 3 + 1], hltexture.palette[i * 3 + 2]);
    }
    result.setColorTable(imgPal);
}

// makes path to have consistent separators '/'
static fs::path FixPath(const fs::path& pathToFix) {
    std::string str = pathToFix.u8string();
    std::string::size_type pos = 0;
    while (std::string::npos != (pos = str.find_first_of('\\'))) {
        str[pos] = '/';
    }
    return fs::path{ str };
}


// settings
static const QString kLastOpenPath("LastOpenPath");
static const QString kLastSavePath("LastSavePath");
static const QString kRecentModelTemplate("RecentModel_");

constexpr size_t kMaxRecentModels = 6;


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

    this->UpdateRecentModelsList();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::OpenModel(const fs::path& filePath, const bool addToRecent) {
    fs::path fixedPath = FixPath(filePath);

    StrongPtr<HalfLifeModel> mdl = MakeStrongPtr<HalfLifeModel>();
    if (mdl->LoadFromPath(fixedPath)) {
        mRenderView->SetModel(nullptr);
        mModel.swap(mdl);
        mRenderView->SetModel(mModel.get());

        QSettings registry;
        QString lastOpenDir = registry.value(kLastOpenPath).toString();

        fs::path folderPath = fs::absolute(fixedPath.parent_path());
        lastOpenDir = QString::fromStdString(folderPath.u8string());
        registry.setValue(kLastOpenPath, lastOpenDir);

        this->UpdateUIForModel();

        if (addToRecent) {
            this->AddToRecentModelsList(QString::fromStdString(fixedPath.u8string()));
        }
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
            this->OpenModel(filePath.toStdString(), true);

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
        this->OpenModel(mdlPath, true);
    }
}

void MainWindow::on_actionRecentModel_triggered(const size_t recentModelIdx) {
    const QList<QAction*>& actions = ui->menuRecent_models->actions();
    if (recentModelIdx < scast<size_t>(actions.size())) {
        fs::path modelPath = actions[recentModelIdx]->text().toStdString();
        this->OpenModel(modelPath, false);
    }
}

void MainWindow::on_actionE_xit_triggered() {
    this->close();
}

void MainWindow::on_actionReset_view_triggered() {
    if (mModel) {
        mRenderView->ResetView();
    }
}

void MainWindow::on_actionShow_stats_toggled(bool b) {
    mRenderView->SetShowStats(b);
}

void MainWindow::on_actionBackground_color_triggered() {
    vec4f colorF = mRenderView->GetBackgroundColor();
    QColor newColor = QColorDialog::getColor(QColor::fromRgbF(colorF.x, colorF.y, colorF.z), this, tr("Choose background color"));
    mRenderView->SetBackgroundColor(vec4f(newColor.redF(), newColor.greenF(), newColor.blueF(), 0.0f));
}

void MainWindow::on_actionAbout_Qt_triggered() {
    QMessageBox::aboutQt(this, tr("About Qt..."));
}


void MainWindow::on_actionAbout_triggered() {
    AboutDlg dlg(this);
    dlg.exec();
}


void MainWindow::on_lstTextures_currentItemChanged(QListWidgetItem* current, QListWidgetItem* /*previous*/) {
    if (mModel && current) {
        const int textureIdx = current->data(Qt::UserRole).toInt();
        const HalfLifeModelTexture& hltexture = mModel->GetTexture(textureIdx);
        ui->lblTextureSize->setText(QString("%1 x %2 %3").arg(hltexture.width).arg(hltexture.height).arg(tr("pixels")));
        ui->chkIsChromeTexture->setChecked(hltexture.chrome);
        ui->chkIsAdditiveTexture->setChecked(hltexture.additive);
        ui->chkIsMaskedTexture->setChecked(hltexture.masked);

        RenderOptions options = mRenderView->GetRenderOptions();
        options.imageViewerMode = true;
        options.textureToShow = textureIdx;
        mRenderView->SetRenderOptions(options);
    }
}

void MainWindow::on_btnExportTexture_clicked() {
    if (!mModel) {
        return;
    }

    const auto& items = ui->lstTextures->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    const int selectedTextureIdx = items[0]->data(Qt::UserRole).toInt();
    if (selectedTextureIdx < 0 || selectedTextureIdx > mModel->GetTexturesCount()) {
        return;
    }

    const HalfLifeModelTexture& hltexture = mModel->GetTexture(selectedTextureIdx);

    QSettings registry;
    QString lastSaveDir = registry.value(kLastSavePath).toString();

    QString proposedName = QDir(lastSaveDir).filePath(QString::fromStdString(hltexture.name));

    QString path = QFileDialog::getSaveFileName(this, tr("Where to save texture..."), proposedName, tr("BMP image (*.bmp)"));
    if (!path.isEmpty()) {
        QImage img;
        HLTextureToQImage(hltexture, img);

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
        ui->chkRenderTextured->setChecked(options.renderTextured);
        ui->chkShowBones->setChecked(options.showBones);
        ui->chkShowBonesNames->setChecked(options.showBonesNames);
        ui->chkShowAttachments->setChecked(options.showAttachments);
        ui->chkShowAttachmentsNames->setChecked(options.showAttachmentsNames);
        ui->chkShowHitBoxes->setChecked(options.showHitBoxes);
        ui->chkShowNormals->setChecked(options.showNormals);
        ui->chkWireframeModel->setChecked(options.showWireframe);
        ui->chkWireframeoverlay->setChecked(options.overlayWireframe);
        ui->lblTexturesCount->setText(QString::number(mModel->GetTexturesCount()));
        ui->lblBodypartsCount->setText(QString::number(mModel->GetBodyPartsCount()));
        ui->lblSkinsCount->setText(QString::number(mModel->GetSkinsCount()));
        ui->lblBonesCount->setText(QString::number(mModel->GetBonesCount()));
        ui->lblAttachmentsCount->setText(QString::number(mModel->GetAttachmentsCount()));
        ui->lblSequencesCount->setText(QString::number(mModel->GetSequencesCount()));

        ui->comboBoneControllers->clear();
        if (mModel->GetBoneControllersCount() == 0) {
            ui->comboBoneControllers->setEnabled(false);
            ui->sliderBoneControllerValue->setEnabled(false);
            ui->lblBoneControllerBoneName->setText(QString());
        } else {
            ui->comboBoneControllers->setEnabled(true);
            ui->sliderBoneControllerValue->setEnabled(true);

            for (size_t i = 0; i < mModel->GetBoneControllersCount(); ++i) {
                const HalfLifeModelBoneController& controller = mModel->GetBoneController(i);
                QString text;
                if (controller.index == HalfLifeModelBoneController::kMouthIndex) {
                    text = tr("Mouth");
                } else {
                    text = QString("%1 %2").arg(tr("Controller")).arg(controller.index);
                }
                ui->comboBoneControllers->addItem(text);
            }

            ui->comboBoneControllers->setCurrentIndex(0);
        }

        // body tab
        ui->lstBodyParts->clear();
        ui->lstBodySubModels->clear();
        ui->lstSkins->clear();
        for (size_t i = 0; i < mModel->GetBodyPartsCount(); ++i) {
            const HalfLifeModelBodypart* bodyPart = mModel->GetBodyPart(i);
            ui->lstBodyParts->addItem(QString::fromStdString(bodyPart->GetName()));
        }
        ui->lstBodyParts->setCurrentRow(0);
        for (size_t i = 0; i < mModel->GetSkinsCount(); ++i) {
            ui->lstSkins->addItem(QString("%1 %2").arg(tr("Skin")).arg(i));
        }
        ui->lstSkins->setCurrentRow(0);

        // textures tab
        ui->lstTextures->clear();
        const size_t numTextures = mModel->GetTexturesCount();
        for (size_t i = 0; i < numTextures; ++i) {
            const HalfLifeModelTexture& hltexture = mModel->GetTexture(i);
            QImage img;
            HLTextureToQImage(hltexture, img);

            QListWidgetItem* item = new QListWidgetItem();
            item->setText(QString::fromStdString(hltexture.name));
            item->setToolTip(item->text());
            item->setIcon(QIcon(QPixmap::fromImage(img)));
            item->setData(Qt::UserRole, scast<int>(i));
            ui->lstTextures->addItem(item);
        }
        ui->spinImageZoom->setValue(1.0);

        // sequences tab
        ui->lstSequences->clear();
        const size_t numSequences = mModel->GetSequencesCount();
        for (size_t i = 0; i < numSequences; ++i) {
            ui->lstSequences->addItem(QString::fromStdString(mModel->GetSequence(i)->GetName()));
        }
        ui->lstSequences->setCurrentRow(0);
    }
}

QList<QString> MainWindow::GetRecentModelsList() const {
    QList<QString> result;

    QSettings registry;
    for (size_t i = 0; i < kMaxRecentModels; ++i) {
        QString keyName = QString("%1%2").arg(kRecentModelTemplate).arg(i);
        QString keyValue = registry.value(keyName).toString();
        if (keyValue.isEmpty()) {
            break;
        }

        result.push_back(keyValue);
    }

    return result;
}

void MainWindow::AddToRecentModelsList(const QString& entry) {
    QList<QString> currentList = this->GetRecentModelsList();
    const qsizetype idx = currentList.indexOf(entry);
    if (idx != -1) {
        currentList.removeAt(idx);
    }

    QList<QString> newList;
    newList.push_back(entry);
    for (size_t i = 0; i < kMaxRecentModels - 1; ++i) {
        if (i < scast<size_t>(currentList.size())) {
            newList.push_back(currentList[i]);
        } else {
            break;
        }
    }

    QSettings registry;
    for (size_t i = 0; i < scast<size_t>(newList.size()); ++i) {
        QString keyName = QString("%1%2").arg(kRecentModelTemplate).arg(i);
        registry.setValue(keyName, newList[i]);
    }

    this->UpdateRecentModelsList();
}

void MainWindow::UpdateRecentModelsList() {
    ui->menuRecent_models->clear();

    const QList<QString>& list = this->GetRecentModelsList();

    if (list.isEmpty()) {
        ui->menuRecent_models->setDisabled(true);
    } else {
        ui->menuRecent_models->setDisabled(false);

        for (size_t i = 0; i < scast<size_t>(list.size()); ++i) {
            QAction* action = ui->menuRecent_models->addAction(list[i]);
            connect(action, &QAction::triggered, this, [this, i]() { this->on_actionRecentModel_triggered(i); });
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
    if (mModel) {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.showBones = (Qt::Checked == state);
        mRenderView->SetRenderOptions(options);
    }
}

void MainWindow::on_chkShowBonesNames_stateChanged(int state) {
    if (mModel) {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.showBonesNames = (Qt::Checked == state);
        mRenderView->SetRenderOptions(options);
    }
}

void MainWindow::on_chkShowAttachments_stateChanged(int state) {
    if (mModel) {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.showAttachments = (Qt::Checked == state);
        mRenderView->SetRenderOptions(options);
    }
}

void MainWindow::on_chkShowAttachmentsNames_stateChanged(int state) {
    if (mModel) {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.showAttachmentsNames = (Qt::Checked == state);
        mRenderView->SetRenderOptions(options);
    }
}

void MainWindow::on_chkShowHitBoxes_stateChanged(int state) {
    if (mModel) {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.showHitBoxes = (Qt::Checked == state);
        mRenderView->SetRenderOptions(options);
    }
}

void MainWindow::on_chkShowNormals_stateChanged(int state) {
    if (mModel) {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.showNormals = (Qt::Checked == state);
        mRenderView->SetRenderOptions(options);
    }
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

void MainWindow::on_tabBottom_currentChanged(int index) {
    if (index == 2) { // textures
        if (!ui->lstTextures->currentItem()) {
            ui->lstTextures->setCurrentRow(0);
        } else {
            this->on_lstTextures_currentItemChanged(ui->lstTextures->currentItem(), nullptr);
        }
    } else {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.imageViewerMode = false;
        options.textureToShow = 0;
        mRenderView->SetRenderOptions(options);
    }
}

void MainWindow::on_lstSequences_currentRowChanged(int currentRow) {
    if (mModel && currentRow >= 0 && currentRow < mModel->GetSequencesCount()) {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.animSequence = currentRow;
        mRenderView->SetRenderOptions(options);

        const HalfLifeModelSequence* sequence = mModel->GetSequence(scast<size_t>(currentRow));

        ui->lblSequenceName->setText(QString::fromStdString(sequence->GetName()));
        ui->lblSequenceFPS->setText(QString("%1 %2").arg(sequence->GetFPS()).arg(tr("FPS")));
        ui->lblSequenceFrames->setText(QString("%1 %2").arg(sequence->GetFramesCount()).arg(tr("frames")));
        ui->lblSequenceEvents->setText(QString("%1 %2").arg(sequence->GetEventsCount()).arg(tr("events")));

        ui->lstEvents->clear();
        if (sequence->GetEventsCount() > 0) {
            for (size_t i = 0; i < sequence->GetEventsCount(); ++i) {
                ui->lstEvents->addItem(QString("%1 %2").arg(tr("Event")).arg(i));
            }
            ui->lstEvents->setCurrentRow(0);
        }
    }
}

void MainWindow::on_lstEvents_currentRowChanged(int currentRow) {
    if (mModel && currentRow >= 0) {
        const int seqIdx = ui->lstSequences->currentRow();
        if (seqIdx >= 0 && seqIdx < mModel->GetSequencesCount()) {
            const HalfLifeModelSequence* sequence = mModel->GetSequence(scast<size_t>(seqIdx));
            if (currentRow < sequence->GetEventsCount()) {
                const HalfLifeModelAnimEvent& event = sequence->GetEvent(scast<size_t>(currentRow));

                ui->lblEventFrame->setText(QString("%1 %2").arg(tr("Frame")).arg(event.frame));
                ui->lblEventEvent->setText(QString("%1 %2").arg(tr("Event")).arg(event.event));
                ui->lblEventType->setText(QString("%1 %2").arg(tr("Type")).arg(event.type));
                ui->lblEventOptions->setText(event.options.empty() ? tr("No options") : QString::fromStdString(event.options));
                ui->lblEventOptions->setToolTip(ui->lblEventOptions->text());
            }
        }
    }
}

void MainWindow::on_spinImageZoom_valueChanged(double value) {
    if (mModel) {
        RenderOptions options = mRenderView->GetRenderOptions();
        options.imageZoom = scast<float>(value);
        mRenderView->SetRenderOptions(options);
    }
}

void MainWindow::on_comboBoneControllers_currentIndexChanged(int index) {
    if (index >= 0 && mModel && index < mModel->GetBoneControllersCount()) {
        const HalfLifeModelBoneController& controller = mModel->GetBoneController(index);

        ui->sliderBoneControllerValue->setMinimum(scast<int>(controller.start));
        ui->sliderBoneControllerValue->setMaximum(scast<int>(controller.end));
        ui->sliderBoneControllerValue->setValue(0);

        if (controller.boneIdx >= 0 && controller.boneIdx < mModel->GetBonesCount()) {
            const HalfLifeModelBone& bone = mModel->GetBone(scast<size_t>(controller.boneIdx));
            ui->lblBoneControllerBoneName->setText(QString::fromStdString(bone.name));
        }
    }
}

void MainWindow::on_sliderBoneControllerValue_valueChanged(int value) {
    if (mModel) {
        const int index = ui->comboBoneControllers->currentIndex();

        if (index >= 0 && index < mModel->GetBoneControllersCount()) {
            mModel->SetBoneControllerValue(scast<size_t>(index), scast<float>(value));
        }
    }
}

void MainWindow::on_lstBodyParts_currentRowChanged(int currentRow) {
    if (mModel && currentRow >= 0 && currentRow < mModel->GetBodyPartsCount()) {
        ui->lstBodySubModels->clear();

        const HalfLifeModelBodypart* bodyPart = mModel->GetBodyPart(scast<size_t>(currentRow));
        for (size_t i = 0; i < bodyPart->GetStudioModelsCount(); ++i) {
            const HalfLifeModelStudioModel* smdl = bodyPart->GetStudioModel(i);
            ui->lstBodySubModels->addItem(QString::fromStdString(smdl->GetName()));
        }
    }
}


void MainWindow::on_lstBodySubModels_currentRowChanged(int currentRow) {
    if (mModel) {
        const int bodyPartIdx = ui->lstBodyParts->currentRow();
        if (bodyPartIdx >= 0 && bodyPartIdx < mModel->GetBodyPartsCount()) {
            const HalfLifeModelBodypart* bodyPart = mModel->GetBodyPart(scast<size_t>(bodyPartIdx));

            if (currentRow >= 0 && currentRow < bodyPart->GetStudioModelsCount()) {
                mModel->SetBodyPartActiveSubModel(bodyPartIdx, scast<size_t>(currentRow));
            }
        }
    }
}


void MainWindow::on_lstSkins_currentRowChanged(int currentRow) {
    if (mModel && currentRow >= 0 && currentRow < mModel->GetSkinsCount()) {
        mModel->SetActiveSkin(scast<size_t>(currentRow));
    }
}
