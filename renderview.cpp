#include <QtGui>
#include <QtOpenGL>

#include "renderview.h"
#include "render_shaders.inl"

#include "halflifemodel.h"

enum : int {
    k_AttribPosition = 0,
    k_AttribNormal,
    k_AttribUV
};


RenderView::RenderView(QWidget* parent)
    : QOpenGLWidget(parent)
    , QOpenGLFunctions_2_0()
    , mGLContext(nullptr)
    , mModel(nullptr)
    , mModelBounds{}
    , mIsFirstFrame(false)
    , mShaderModel(nullptr)
    , mLightPos(250.0f, 250.0f, 1000.0f)
    , mRenderOptions{}
{
    QTimer* timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(33);

    mRenderOptions.Reset();
}

RenderView::~RenderView() {
    delete mShaderModel;
    mTextures.clear();
}



void RenderView::initializeGL() {
    initializeOpenGLFunctions();

    glClearColor(40.0f / 255.0f, 113.0f / 255.0f, 134.0f / 255.0f, 0.0f);
    glClearDepth(1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    this->MakeShader(mShaderModel, g_VS_DrawModel, g_FS_DrawModel);

    mWhiteTexture = MakeStrongPtr<QOpenGLTexture>(QOpenGLTexture::Target2D);
    mWhiteTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    if (mWhiteTexture->create()) {
        mWhiteTexture->setSize(1, 1);
        mWhiteTexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
        mWhiteTexture->allocateStorage();

        const uint32_t whitePixel = ~0u;
        mWhiteTexture->setData(0, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, &whitePixel, nullptr);
    }

    mModelMat.setToIdentity();
    mViewMat.setToIdentity();
    mProjectionMat.setToIdentity();
    this->UpdateMatrices();
}

void RenderView::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mModel) {
        mShaderModel->bind();
        mShaderModel->enableAttributeArray(k_AttribPosition);
        mShaderModel->enableAttributeArray(k_AttribNormal);
        mShaderModel->enableAttributeArray(k_AttribUV);

        mShaderModel->setUniformValue("lightPos", mLightPos.x, mLightPos.y, mLightPos.z);
        mShaderModel->setUniformValue("mv", mModelView);
        mShaderModel->setUniformValue("mvp", mModelViewProj);

        const bool renderTextured = mRenderOptions.renderTextured && !mRenderOptions.showWireframe;

        if (mRenderOptions.showWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        const size_t numBodyParts = mModel->GetBodyPartsCount();
        for (size_t i = 0; i < numBodyParts; ++i) {
            HalfLifeModelBodypart* bodyPart = mModel->GetBodyPart(i);
            const size_t numStudioModels = bodyPart->GetStudioModelsCount();

            for (size_t j = 0; j < 1/*numStudioModels*/; ++j) {
                HalfLifeModelStudioModel* smdl = bodyPart->GetStudioModel(j);

                const uint16_t* indices = smdl->GetIndices();
                const HalfLifeModelVertex* srcVertices = smdl->GetVertices();

                if (mModel->GetBonesCount() > 0) {
                    const size_t numVertices = smdl->GetVerticesCount();
                    mRenderVertices.resize(numVertices);
                    RenderVertex* renderVertices = mRenderVertices.data();
                    for (size_t k = 0; k < numVertices; ++k) {
                        const mat4f& boneMat = mModel->GetBoneMat(srcVertices[k].boneIdx);
                        renderVertices[k].pos = boneMat.transformPos(srcVertices[k].pos);
                        renderVertices[k].normal = boneMat.transformDir(srcVertices[k].normal);
                        renderVertices[k].uv = srcVertices[k].uv;

                        mModelBounds.Absorb(renderVertices[k].pos);
                    }

                    mShaderModel->setAttributeArray(k_AttribPosition, &renderVertices->pos.x, 3, sizeof(RenderVertex));
                    mShaderModel->setAttributeArray(k_AttribNormal, &renderVertices->normal.x, 3, sizeof(RenderVertex));
                    mShaderModel->setAttributeArray(k_AttribUV, &renderVertices->uv.x, 2, sizeof(RenderVertex));
                } else {
                    mShaderModel->setAttributeArray(k_AttribPosition, &srcVertices->pos.x, 3, sizeof(HalfLifeModelVertex));
                    mShaderModel->setAttributeArray(k_AttribNormal, &srcVertices->normal.x, 3, sizeof(HalfLifeModelVertex));
                    mShaderModel->setAttributeArray(k_AttribUV, &srcVertices->uv.x, 2, sizeof(HalfLifeModelVertex));
                }

                const size_t numMeshes = smdl->GetMeshesCount();
                for (size_t k = 0; k < numMeshes; ++k) {
                    const HalfLifeModelStudioMesh& mesh = smdl->GetMesh(k);
                    if (mesh.textureIndex < mTextures.size()) {
                        if (renderTextured) {
                            mTextures[mesh.textureIndex]->bind();
                        } else {
                            mWhiteTexture->bind();
                        }

                        const HalfLifeModelTexture& hltexture = mModel->GetTexture(mesh.textureIndex);
                        mShaderModel->setUniformValue(mIsChromeLocation, hltexture.chrome);
                    } else {
                        mWhiteTexture->bind();
                        mShaderModel->setUniformValue(mIsChromeLocation, false);
                    }

                    glDrawElements(GL_TRIANGLES, scast<GLsizei>(mesh.numIndices), GL_UNSIGNED_SHORT, indices + mesh.indicesOffset);
                }
            }
        }

        if (mIsFirstFrame) {
            this->ResetView();
            mIsFirstFrame = false;
        }
    }
}

void RenderView::resizeGL(int w, int h) {
    mProjectionMat.setToIdentity();
    mProjectionMat.perspective(60.0f, scast<GLfloat>(w) / scast<GLfloat>(h), 1.0f, 4096.0f);
    this->UpdateMatrices();
}

void RenderView::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        mLastRotPos = event->pos();
    } else if (event->buttons() & Qt::RightButton) {
        mLastMovePos = event->pos();
    }

    event->accept();
}

void RenderView::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        float dx = mLastRotPos.x() - event->x();
        float dy = mLastRotPos.y() - event->y();
        mLastRotPos = event->pos();

        mRotAngles.x -= dy;
        mRotAngles.y -= dx;
    } else if (event->buttons() & Qt::RightButton) {
        float dx = 0.0f, dy = 0.0f, dz = 0.0f;

        if (event->modifiers() & Qt::ControlModifier) {
            dz = mLastMovePos.y() - event->y();
        } else {
            dx = mLastMovePos.x() - event->x();
            dy = -(mLastMovePos.y() - event->y());
        }
        mLastMovePos = event->pos();

        mOffset.x -= dx * 0.1f;
        mOffset.y -= dy * 0.1f;
        mOffset.z -= dz * 0.1f;
    }

    mRotAngles.x = NormalizeAngle(mRotAngles.x);
    mRotAngles.y = NormalizeAngle(mRotAngles.y);

    this->UpdateMatrices();

    event->accept();
}

void RenderView::wheelEvent(QWheelEvent* event) {
    mOffset.z -= scast<float>(event->angleDelta().y()) / 16.0f;

    this->UpdateMatrices();

    event->accept();
}


void RenderView::MakeShader(QOpenGLShaderProgram*& shader, const char* vs, const char* fs) {
    shader = new QOpenGLShaderProgram();
    if (shader->addShaderFromSourceCode(QOpenGLShader::Vertex, vs) &&
        shader->addShaderFromSourceCode(QOpenGLShader::Fragment, fs)) {
        shader->bindAttributeLocation("inPos", k_AttribPosition);
        shader->bindAttributeLocation("inNormal", k_AttribNormal);
        shader->bindAttributeLocation("inUV", k_AttribUV);
        shader->link();
        shader->bind();
        shader->setUniformValue("texDiffuse", 0);

        mIsChromeLocation = shader->uniformLocation("isChrome");
        shader->setUniformValue(mIsChromeLocation, false);
    } else {
        QString log = shader->log();
        delete shader;
        shader = nullptr;
    }
}

void RenderView::UpdateMatrices() {
    mModelMat.setToIdentity();
    mModelMat.translate(mOffset.x, mOffset.y, mOffset.z);
    mModelMat.rotate(mRotAngles.x, 1.0f, 0.0f, 0.0f);
    mModelMat.rotate(mRotAngles.y, 0.0f, 0.0f, 1.0f);

    mModelView = mViewMat * mModelMat;
    mModelViewProj = mProjectionMat * mModelView;
}

void RenderView::ResetView() {
    if (mModel) {
        const float dx = mModelBounds.maximum.x - mModelBounds.minimum.x;
        const float dy = mModelBounds.maximum.y - mModelBounds.minimum.y;
        const float dz = mModelBounds.maximum.z - mModelBounds.minimum.z;
        const float d = Max3(dx, dy, dz);

        mOffset = vec3f(0.0f, -(mModelBounds.minimum.z + dz * 0.5f), -d);
        mRotAngles = vec3f(-90.0f, -90.0f, 0.0f);
    } else {
        mOffset = vec3f(0.0f, 0.0f, 0.0f);
        mRotAngles = vec3f(0.0f, 0.0f, 0.0f);
    }

    this->UpdateMatrices();
}

void RenderView::SetModel(HalfLifeModel* mdl) {
    mModel = mdl;
    mTextures.clear();

    if (mModel) {
        if (mModel->GetBonesCount() > 0) {
            mRenderVertices.resize(16384); // reserve
            mModelBounds.Reset(true);
        } else {
            mModelBounds = mModel->GetBounds();
        }

        if (mModel->GetTexturesCount() > 0) {
            const size_t numTextures = mModel->GetTexturesCount();
            mTextures.resize(numTextures);

            for (size_t i = 0; i < numTextures; ++i) {
                const HalfLifeModelTexture& hltexture = mModel->GetTexture(i);
                RefPtr<QOpenGLTexture> gltexture = MakeRefPtr<QOpenGLTexture>(QOpenGLTexture::Target2D);
                gltexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
                if (gltexture->create()) {
                    gltexture->setSize(scast<int>(hltexture.width), scast<int>(hltexture.height));
                    gltexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
                    gltexture->allocateStorage();

                    MyArray<uint32_t> rgbaData(hltexture.width * hltexture.height);
                    for (size_t j = 0, numPixels = hltexture.data.size(); j < numPixels; ++j) {
                        const size_t idx = hltexture.data[j];
                        rgbaData[j] = (hltexture.palette[idx * 3 + 0] <<  0) |
                                      (hltexture.palette[idx * 3 + 1] <<  8) |
                                      (hltexture.palette[idx * 3 + 2] << 16) |
                                       0xFF000000;
                    }
                    gltexture->setData(0, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, rgbaData.data(), nullptr);
                }

                mTextures[i] = gltexture;
            }
        }

        mIsFirstFrame = true;
    }

    this->ResetView();
}

void RenderView::SetRenderOptions(const RenderOptions& options) {
    mRenderOptions = options;
}

const RenderOptions& RenderView::GetRenderOptions() const {
    return mRenderOptions;
}

