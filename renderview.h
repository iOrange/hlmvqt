#ifndef RENDERVIEW_H
#define RENDERVIEW_H

#include <QBasicTimer>
#include <QDateTime>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_2_0>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMatrix4x4>

#include "mycommon.h"
#include "mymath.h"

class HalfLifeModel;

struct RenderVertex {
    vec3f pos;
    vec3f normal;
    vec2f uv;
};

struct RenderOptions {
    bool renderTextured;
    bool showBones;
    bool showAttachments;
    bool showHitBoxes;
    bool showNormals;
    bool showWireframe;
    bool overlayWireframe;

    bool imageViewerMode;
    int  textureToShow;

    int  animSequence;

    void Reset() {
        this->renderTextured = true;
        this->showBones = false;
        this->showAttachments = false;
        this->showHitBoxes = false;
        this->showNormals = false;
        this->showWireframe = false;
        this->overlayWireframe = false;

        this->imageViewerMode = false;
        this->textureToShow = 0;

        this->animSequence = 0;
    }
};

class RenderView : public QOpenGLWidget, protected QOpenGLFunctions_2_0 {
    Q_OBJECT

public:
    explicit RenderView(QWidget* parent = nullptr);
    ~RenderView();

protected:
    void                    initializeGL() override;
    void                    paintGL() override;
    void                    resizeGL(int w, int h) override;
    void                    mousePressEvent(QMouseEvent* event) override;
    void                    mouseMoveEvent(QMouseEvent* event) override;
    void                    wheelEvent(QWheelEvent* event) override;
    void                    timerEvent(QTimerEvent* event) override;

    void                    MakeShader(QOpenGLShaderProgram*& shader, const char* vs, const char* fs);
    void                    UpdateMatrices();
    void                    ResetView();

public:
    void                    SetModel(HalfLifeModel* mdl);
    void                    SetRenderOptions(const RenderOptions& options);
    const RenderOptions&    GetRenderOptions() const;

private:
    QOpenGLContext*                 mGLContext;
    QBasicTimer                     mTimer;

    HalfLifeModel*                  mModel;
    MyArray<RenderVertex>           mRenderVertices;
    MyArray<RefPtr<QOpenGLTexture>> mTextures;
    AABBox                          mModelBounds;
    bool                            mIsFirstFrame;
    QDateTime                       mLastTime;
    float                           mAnimationFrame;

    QMatrix4x4                      mModelMat;
    QMatrix4x4                      mViewMat;
    QMatrix4x4                      mProjectionMat;
    QMatrix4x4                      mModelView;
    QMatrix4x4                      mModelViewProj;
    vec3f                           mRotAngles;
    vec3f                           mOffset;
    vec3f                           mLightPos;
    QPoint                          mLastRotPos;
    QPoint                          mLastMovePos;

    QOpenGLShaderProgram*           mShaderModel;
    QOpenGLShaderProgram*           mShaderImage;
    StrongPtr<QOpenGLTexture>       mWhiteTexture;
    int                             mIsChromeLocation;

    RenderOptions                   mRenderOptions;
};

#endif // RENDERVIEW_H
