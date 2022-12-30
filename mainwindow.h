#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include "mycommon.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class RenderView;
class HalfLifeModel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void OpenModel(const fs::path& filePath);

protected:
    void showEvent(QShowEvent* ev) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void on_WindowShown();
    void on_actionLoad_model_triggered();
    void on_actionE_xit_triggered();
    void on_actionAbout_Qt_triggered();
    void on_actionAbout_triggered();
    void on_lstTextures_currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);
    void on_btnExportTexture_clicked();
    void on_chkRenderTextured_stateChanged(int state);
    void on_chkShowBones_stateChanged(int state);
    void on_chkShowAttachments_stateChanged(int state);
    void on_chkShowHitBoxes_stateChanged(int state);
    void on_chkShowNormals_stateChanged(int state);
    void on_chkWireframeModel_stateChanged(int state);
    void on_chkWireframeoverlay_stateChanged(int state);
    void on_tabBottom_currentChanged(int index);
    void on_lstSequences_currentRowChanged(int currentRow);

private:
    void UpdateUIForModel();

private:
    Ui::MainWindow*             ui;
    StrongPtr<RenderView>       mRenderView;
    StrongPtr<HalfLifeModel>    mModel;
};
#endif // MAINWINDOW_H
