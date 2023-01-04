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

PACKED_STRUCT_BEGIN
struct RenderVertex {
    vec3f pos;
    vec3f normal;
    vec2f uv;
} PACKED_STRUCT_END;

PACKED_STRUCT_BEGIN
struct DebugVertex {
    vec3f    pos;
    uint32_t color;
} PACKED_STRUCT_END;

struct RenderOptions {
    bool  renderTextured;
    bool  showBones;
    bool  showBonesNames;
    bool  showAttachments;
    bool  showAttachmentsNames;
    bool  showHitBoxes;
    bool  showNormals;
    bool  showWireframe;
    bool  overlayWireframe;

    bool  imageViewerMode;
    int   textureToShow;
    float imageZoom;

    int   animSequence;

    void Reset() {
        this->renderTextured = true;
        this->showBones = false;
        this->showBonesNames = false;
        this->showAttachments = false;
        this->showAttachmentsNames = false;
        this->showHitBoxes = false;
        this->showNormals = false;
        this->showWireframe = false;
        this->overlayWireframe = false;

        this->imageViewerMode = false;
        this->textureToShow = 0;
        this->imageZoom = 1.0f;

        this->animSequence = 0;
    }
};

struct RenderTexture {
    RefPtr<QOpenGLTexture>  draw;
    RefPtr<QOpenGLTexture>  orig;   // most of the time is a pointer to `draw`, except when masked
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

    void                    MakeShader(StrongPtr<QOpenGLShaderProgram>& shader, const char* vs, const char* fs);
    void                    UpdateMatrices();
    void                    ResetView();

    void                    BeginDebugDraw();
    void                    EndDebugDraw();
    void                    EnsureDebugVertices(const size_t required);
    void                    FlushDebugVertices();
    void                    DebugDrawLine(const vec3f& pt0, const vec3f& pt1, const uint32_t color);
    void                    DebugDrawRing(const vec3f& origin, const vec3f& majorAxis, const vec3f& minorAxis, const uint32_t color);
    void                    DebugDrawSphere(const vec3f& center, const float radius, const uint32_t color);
    void                    DebugDrawTetrahedron(const vec3f& a, const vec3f& b, const float r, const uint32_t color);
    void                    DebugDrawTransformedBBox(const mat4f& xform, const AABBox& bbox, const uint32_t color);

public:
    void                    SetModel(HalfLifeModel* mdl);
    void                    SetRenderOptions(const RenderOptions& options);
    const RenderOptions&    GetRenderOptions() const;

private:
    QOpenGLContext*                 mGLContext;
    QBasicTimer                     mTimer;

    HalfLifeModel*                  mModel;
    MyArray<RenderVertex>           mRenderVertices;
    MyArray<RenderTexture>          mTextures;
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

    StrongPtr<QOpenGLShaderProgram> mShaderModel;
    StrongPtr<QOpenGLShaderProgram> mShaderImage;
    StrongPtr<QOpenGLShaderProgram> mShaderDebug;

    StrongPtr<QOpenGLTexture>       mWhiteTexture;
    int                             mLightPosLocation;
    int                             mModelViewLocation;
    int                             mModelViewProjLocation;
    int                             mIsChromeLocation;
    int                             mForcedColorLocation;
    int                             mAlphaTestLocation;

    RenderOptions                   mRenderOptions;

    MyArray<DebugVertex>            mDebugVertices;
    size_t                          mDebugVerticesCount;
};

#endif // RENDERVIEW_H
