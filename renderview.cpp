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
    , mAnimationFrame(0.0f)
    , mShaderModel(nullptr)
    , mShaderImage(nullptr)
    , mLightPos(250.0f, 250.0f, 1000.0f)
    , mLightPosLocation(0)
    , mModelViewLocation(0)
    , mModelViewProjLocation(0)
    , mIsChromeLocation(0)
    , mForcedColorLocation(0)
    , mAlphaTestLocation(0)
    , mRenderOptions{}
{
    mRenderOptions.Reset();
}

RenderView::~RenderView() {
    delete mShaderModel;
    delete mShaderImage;
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
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1);

    this->MakeShader(mShaderModel, g_VS_DrawModel, g_FS_DrawModel);
    this->MakeShader(mShaderImage, g_VS_DrawImage, g_FS_DrawImage);

    if (mShaderModel) {
        mShaderModel->bind();
        mLightPosLocation = mShaderModel->uniformLocation("lightPos");
        mModelViewLocation = mShaderModel->uniformLocation("mv");
        mModelViewProjLocation = mShaderModel->uniformLocation("mvp");
        mIsChromeLocation = mShaderModel->uniformLocation("isChrome");
        mForcedColorLocation = mShaderModel->uniformLocation("forcedColor");
        mAlphaTestLocation = mShaderModel->uniformLocation("alphaTest");
        mShaderModel->setUniformValue(mIsChromeLocation, false);
        mShaderModel->setUniformValue(mForcedColorLocation, 1.0f, 1.0f, 1.0f, 1.0f);
        mShaderModel->setUniformValue(mAlphaTestLocation, -1.0f, -1.0f, -1.0f, -1.0f);
        mShaderModel->release();
    }

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

    mTimer.start(16, this);
}

void RenderView::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mModel) {
        if (mRenderOptions.imageViewerMode) {
            glDisable(GL_CULL_FACE);

            mShaderImage->bind();
            mShaderImage->enableAttributeArray(k_AttribPosition);
            mShaderImage->enableAttributeArray(k_AttribUV);

            QRectF rc = this->rect();

            QMatrix4x4 mvp;
            mvp.ortho(rc);
            mShaderImage->setUniformValue("mvp", mvp);

            const auto& texture = mTextures[mRenderOptions.textureToShow];
            texture.orig->bind();

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            const float tw = scast<float>(texture.orig->width()) * mRenderOptions.imageZoom;
            const float th = scast<float>(texture.orig->height()) * mRenderOptions.imageZoom;
            const float tx = (rc.width() - tw) * 0.5f;
            const float ty = (rc.height() - th) * 0.5f;

            const RenderVertex vertices[4] = {
                { vec3f(     tx,      ty, 0.0f), {}, vec2f(0.0f, 0.0f)},
                { vec3f(tx + tw,      ty, 0.0f), {}, vec2f(1.0f, 0.0f)},
                { vec3f(     tx, ty + th, 0.0f), {}, vec2f(0.0f, 1.0f)},
                { vec3f(tx + tw, ty + th, 0.0f), {}, vec2f(1.0f, 1.0f)},
            };

            mShaderImage->setAttributeArray(k_AttribPosition, &vertices[0].pos.x, 3, sizeof(RenderVertex));
            mShaderImage->setAttributeArray(k_AttribUV, &vertices[0].uv.x, 2, sizeof(RenderVertex));

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        } else {
            glEnable(GL_CULL_FACE);

            if (mModel->GetBonesCount() > 0 && mModel->GetSequencesCount() > 0) {
                const HalfLifeModelSequence* sequence = mModel->GetSequence(scast<size_t>(mRenderOptions.animSequence));

                QDateTime curTime = QDateTime::currentDateTime();
                const int deltaMs = qAbs(curTime.time().msecsTo(mLastTime.time()));
                const float deltaSec = scast<float>(deltaMs) / 1000.0f;
                mLastTime = curTime;

                mAnimationFrame += deltaSec * sequence->GetFPS();
                if (mAnimationFrame >= scast<float>(sequence->GetFramesCount())) {
                    mAnimationFrame -= scast<float>(sequence->GetFramesCount());
                }

                mModel->CalculateSkeleton(mAnimationFrame, scast<size_t>(mRenderOptions.animSequence));
            }

            mShaderModel->bind();
            mShaderModel->enableAttributeArray(k_AttribPosition);
            mShaderModel->enableAttributeArray(k_AttribNormal);
            mShaderModel->enableAttributeArray(k_AttribUV);

            mShaderModel->setUniformValue(mLightPosLocation, mLightPos.x, mLightPos.y, mLightPos.z);
            mShaderModel->setUniformValue(mModelViewLocation, mModelView);
            mShaderModel->setUniformValue(mModelViewProjLocation, mModelViewProj);
            mShaderModel->setUniformValue(mForcedColorLocation, 1.0f, 1.0f, 1.0f, 1.0f);

            bool renderTextured = mRenderOptions.renderTextured;
            const size_t numCycles = mRenderOptions.overlayWireframe ? 2 : 1;
            for (size_t drawCycle = 0; drawCycle < numCycles; ++drawCycle) {
                glDisable(GL_POLYGON_OFFSET_FILL);

                if (mRenderOptions.showWireframe || drawCycle > 0) {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    renderTextured = false;
                    if (drawCycle > 0) {
                        glEnable(GL_POLYGON_OFFSET_FILL);
                        glPolygonOffset(1.0f, 0.1f);
                    }
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
                                    mTextures[mesh.textureIndex].draw->bind();
                                } else {
                                    mWhiteTexture->bind();
                                }

                                const HalfLifeModelTexture& hltexture = mModel->GetTexture(mesh.textureIndex);
                                mShaderModel->setUniformValue(mIsChromeLocation, hltexture.chrome);
                                if (renderTextured && hltexture.masked) {
                                    mShaderModel->setUniformValue(mAlphaTestLocation, 0.5f, 0.5f, 0.5f, 0.5f);
                                } else {
                                    mShaderModel->setUniformValue(mAlphaTestLocation, -1.0f, -1.0f, -1.0f, -1.0f);
                                }
                            } else {
                                mWhiteTexture->bind();
                                mShaderModel->setUniformValue(mIsChromeLocation, false);
                                mShaderModel->setUniformValue(mAlphaTestLocation, -1.0f, -1.0f, -1.0f, -1.0f);
                            }

                            if (drawCycle > 0) {
                                mShaderModel->setUniformValue(mIsChromeLocation, true);
                                mShaderModel->setUniformValue(mForcedColorLocation, 1.0f, 0.0f, 0.95f, 1.0f);
                            }

                            glDrawElements(GL_TRIANGLES, scast<GLsizei>(mesh.numIndices), GL_UNSIGNED_SHORT, indices + mesh.indicesOffset);
                        }
                    }
                }
            }
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
    const float posX = event->position().x();
    const float posY = event->position().y();

    if (event->buttons() & Qt::LeftButton) {
        float dx = mLastRotPos.x() - posX;
        float dy = mLastRotPos.y() - posY;
        mLastRotPos = event->pos();

        mRotAngles.x -= dy;
        mRotAngles.y -= dx;
    } else if (event->buttons() & Qt::RightButton) {
        float dx = 0.0f, dy = 0.0f, dz = 0.0f;

        if (event->modifiers() & Qt::ControlModifier) {
            dz = mLastMovePos.y() - posY;
        } else {
            dx = mLastMovePos.x() - posX;
            dy = -(mLastMovePos.y() - posY);
        }
        mLastMovePos = event->position().toPoint();

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

void RenderView::timerEvent(QTimerEvent* event) {
    if (mModel) {
        this->update();
    }
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
        shader->release();
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
        const AABBox& bounds = mModel->GetBounds();

        const float dx = bounds.maximum.x - bounds.minimum.x;
        const float dy = bounds.maximum.y - bounds.minimum.y;
        const float dz = bounds.maximum.z - bounds.minimum.z;
        const float d = Max3(dx, dy, dz);

        mOffset = vec3f(0.0f, -(bounds.minimum.z + dz * 0.5f), -d);
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

    mAnimationFrame = 0.0f;
    mLastTime = QDateTime::currentDateTime();

    if (mModel) {
        if (mModel->GetBonesCount() > 0) {
            mRenderVertices.resize(16384); // reserve
        }

        if (mModel->GetTexturesCount() > 0) {
            const size_t numTextures = mModel->GetTexturesCount();
            mTextures.resize(numTextures);

            for (size_t i = 0; i < numTextures; ++i) {
                const HalfLifeModelTexture& hltexture = mModel->GetTexture(i);
                RenderTexture& renderTexture = mTextures[i];

                const size_t numTextureVariants = hltexture.masked ? 2 : 1;
                for (size_t variant = 0; variant < numTextureVariants; ++variant) {
                    RefPtr<QOpenGLTexture>& gltexture = variant ? renderTexture.orig : renderTexture.draw;

                    gltexture = MakeRefPtr<QOpenGLTexture>(QOpenGLTexture::Target2D);
                    gltexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
                    if (gltexture->create()) {
                        gltexture->setSize(scast<int>(hltexture.width), scast<int>(hltexture.height));
                        gltexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
                        gltexture->allocateStorage();

                        MyArray<uint32_t> rgbaData(hltexture.width * hltexture.height);
                        for (size_t j = 0, numPixels = hltexture.data.size(); j < numPixels; ++j) {
                            const size_t idx = hltexture.data[j];
                            rgbaData[j] = (hltexture.masked && idx == 255 && !variant) ? 0u : ((hltexture.palette[idx * 3 + 0] <<  0) |
                                                                                               (hltexture.palette[idx * 3 + 1] <<  8) |
                                                                                               (hltexture.palette[idx * 3 + 2] << 16) |
                                                                                                0xFF000000);
                        }
                        gltexture->setData(0, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, rgbaData.data(), nullptr);
                    }
                }

                if (!hltexture.masked) {
                    renderTexture.orig = renderTexture.draw;
                }
            }
        }
    }

    this->ResetView();
}

void RenderView::SetRenderOptions(const RenderOptions& options) {
    if (mRenderOptions.animSequence != options.animSequence) {
        mAnimationFrame = 0.0f;
    }

    mRenderOptions = options;
}

const RenderOptions& RenderView::GetRenderOptions() const {
    return mRenderOptions;
}

