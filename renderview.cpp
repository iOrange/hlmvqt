#include <QtGui>
#include <QtOpenGL>

#include "renderview.h"
#include "render_shaders.inl"

#include "halflifemodel.h"

enum : int {
    k_AttribPosition = 0,
    k_AttribNormal,
    k_AttribUV,
    k_AttribColor
};


constexpr size_t kMaxDebugDrawVertices = 4096;


RenderView::RenderView(QWidget* parent)
    : QOpenGLWidget(parent)
    , QOpenGLFunctions_2_0()
    , mGLContext(nullptr)
    , mModel(nullptr)
    , mAnimationFrame(0.0f)
    , mShaderModel{}
    , mShaderImage{}
    , mShaderDebug{}
    , mLightPos(250.0f, 250.0f, 1000.0f)
    , mLightPosLocation(0)
    , mModelViewLocation(0)
    , mModelViewProjLocation(0)
    , mIsChromeLocation(0)
    , mForcedColorLocation(0)
    , mAlphaTestLocation(0)
    , mRenderOptions{}
    , mDebugVerticesCount(0)
{
    mRenderOptions.Reset();

    mDebugVertices.resize(kMaxDebugDrawVertices);
}

RenderView::~RenderView() {
    mTextures.clear();
}



void RenderView::initializeGL() {
    initializeOpenGLFunctions();

    glClearColor(40.0f / 255.0f, 113.0f / 255.0f, 134.0f / 255.0f, 0.0f);
    glClearDepth(1.0);

    this->MakeShader(mShaderModel, g_VS_DrawModel, g_FS_DrawModel);
    this->MakeShader(mShaderImage, g_VS_DrawImage, g_FS_DrawImage);
    this->MakeShader(mShaderDebug, g_VS_DrawDebug, g_FS_DrawDebug);

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

            MyArray<std::pair<QString, vec2f>> debugStrings;

            if (mRenderOptions.showBones && mModel->GetBonesCount() > 0) {
                this->BeginDebugDraw();

                const size_t numBones = mModel->GetBonesCount();
                constexpr uint32_t colorPoints = 0xFF00D9FF;
                constexpr uint32_t colorParentPoint = 0xFFFF0000;
                constexpr uint32_t colorTets = 0xFF3299FF;
                constexpr float r = 1.0f;
                for (size_t i = 0; i < numBones; ++i) {
                    const HalfLifeModelBone& bone = mModel->GetBone(i);
                    const mat4f& boneTransform = mModel->GetBoneMat(i);
                    vec3f pos(boneTransform[0][3], boneTransform[1][3], boneTransform[2][3]);

                    this->DebugDrawSphere(pos, r, (bone.parentIdx >= 0) ? colorPoints : colorParentPoint);

                    if (bone.parentIdx >= 0) {
                        const mat4f& parentTransform = mModel->GetBoneMat(scast<size_t>(bone.parentIdx));
                        vec3f parentPos(parentTransform[0][3], parentTransform[1][3], parentTransform[2][3]);
                        this->DebugDrawTetrahedron(parentPos, pos, r, colorTets);
                    }

                    if (mRenderOptions.showBonesNames) {
                        QVector4D ap = mModelViewProj * QVector4D(pos.x, pos.y, pos.z, 1.0f);
                        vec2f ap2 = vec2f(ap.x() / ap.w(), ap.y() / ap.w()) * vec2f(0.5f, -0.5f) + vec2f(0.5f, 0.5f);

                        ap2.x *= scast<float>(this->width());
                        ap2.y *= scast<float>(this->height());
                        debugStrings.push_back({ QString::fromStdString(bone.name), ap2 });
                    }
                }

                this->EndDebugDraw();
            }

            if (mRenderOptions.showAttachments && mModel->GetAttachmentsCount() > 0) {
                this->BeginDebugDraw();

                const size_t numAttachments = mModel->GetAttachmentsCount();
                constexpr uint32_t colorAttach = 0xFF00FF00;
                constexpr float r = 1.3f;
                for (size_t i = 0; i < numAttachments; ++i) {
                    const HalfLifeModelAttachment& attachment = mModel->GetAttachment(i);
                    const mat4f& boneTransform = mModel->GetBoneMat(scast<size_t>(attachment.bone));
                    vec3f pos = boneTransform.transformPos(attachment.origin);

                    this->DebugDrawSphere(pos, r, colorAttach);

                    if (mRenderOptions.showAttachmentsNames && !attachment.name.empty()) {
                        QVector4D ap = mModelViewProj * QVector4D(pos.x, pos.y, pos.z, 1.0f);
                        vec2f ap2 = vec2f(ap.x() / ap.w(), ap.y() / ap.w()) * vec2f(0.5f, -0.5f) + vec2f(0.5f, 0.5f);

                        ap2.x *= scast<float>(this->width());
                        ap2.y *= scast<float>(this->height());
                        debugStrings.push_back({ QString::fromStdString(attachment.name), ap2 });
                    }
                }

                this->EndDebugDraw();
            }

            if (mRenderOptions.showHitBoxes && mModel->GetHitBoxesCount() > 0) {
                this->BeginDebugDraw();

                const size_t numHitBoxes = mModel->GetHitBoxesCount();
                constexpr uint32_t colorBox = 0xFF0000FF;
                for (size_t i = 0; i < numHitBoxes; ++i) {
                    const HalfLifeModelHitBox& hitbox = mModel->GetHitBox(i);
                    const mat4f& boneTransform = mModel->GetBoneMat(hitbox.boneIdx);

                    this->DebugDrawTransformedBBox(boneTransform, hitbox.bounds, colorBox);
                }

                this->EndDebugDraw();
            }

            if (!debugStrings.empty()) {
                for (const auto& p : debugStrings) {
                    QPainter painter;
                    painter.begin(this);

                    painter.setPen(Qt::cyan);
                    painter.drawText(Floori(p.second.x), Floori(p.second.y), p.first);
                    painter.end();
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


void RenderView::MakeShader(StrongPtr<QOpenGLShaderProgram>& shader, const char* vs, const char* fs) {
    shader = MakeStrongPtr<QOpenGLShaderProgram>();
    if (shader->addShaderFromSourceCode(QOpenGLShader::Vertex, vs) &&
        shader->addShaderFromSourceCode(QOpenGLShader::Fragment, fs)) {
        shader->bindAttributeLocation("inPos", k_AttribPosition);
        shader->bindAttributeLocation("inNormal", k_AttribNormal);
        shader->bindAttributeLocation("inUV", k_AttribUV);
        shader->bindAttributeLocation("inColor", k_AttribColor);
        shader->link();
        shader->bind();
        shader->setUniformValue("texDiffuse", 0);
        shader->release();
    } else {
        QString log = shader->log();
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

void RenderView::BeginDebugDraw() {
    // nothing here just yet
}
void RenderView::EndDebugDraw() {
    this->FlushDebugVertices();
}

void RenderView::EnsureDebugVertices(const size_t required) {
    if (mDebugVerticesCount + required >= kMaxDebugDrawVertices) {
        this->FlushDebugVertices();
        mDebugVerticesCount = 0;
    }
}

void RenderView::FlushDebugVertices() {
    if (mDebugVerticesCount) {
        mShaderDebug->bind();
        mShaderDebug->setUniformValue("mvp", mModelViewProj);

        mShaderDebug->enableAttributeArray(k_AttribPosition);
        mShaderDebug->enableAttributeArray(k_AttribColor);

        const DebugVertex* vbStart = mDebugVertices.data();

        mShaderDebug->setAttributeArray(k_AttribPosition, &vbStart->pos.x, 3, sizeof(DebugVertex));
        mShaderDebug->setAttributeArray(k_AttribColor, GL_UNSIGNED_BYTE, &vbStart->color, 4, sizeof(DebugVertex));

        glDepthMask(GL_FALSE);
        glDepthFunc(GL_ALWAYS);

        glDrawArrays(GL_LINES, 0, scast<GLsizei>(mDebugVerticesCount));

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);

        mShaderDebug->release();
    }

    mDebugVerticesCount = 0;
}

void RenderView::DebugDrawLine(const vec3f& pt0, const vec3f& pt1, const uint32_t color) {
    this->EnsureDebugVertices(2);

    DebugVertex* vbStart = &mDebugVertices[mDebugVerticesCount];
    vbStart[0] = { pt0, color };
    vbStart[1] = { pt1, color };

    mDebugVerticesCount += 2;
}

void RenderView::DebugDrawRing(const vec3f& origin, const vec3f& majorAxis, const vec3f& minorAxis, const uint32_t color) {
    constexpr size_t kNumRingSegments = 32;
    constexpr size_t kNumRingVertices = kNumRingSegments * 2;

    this->EnsureDebugVertices(kNumRingVertices);

    DebugVertex* vbStart = &mDebugVertices[mDebugVerticesCount];
    size_t vbOffset = 0;

    constexpr float angleDelta = MM_TwoPi / scast<float>(kNumRingSegments);
    // Instead of calling cos/sin for each segment we calculate
    // the sign of the angle delta and then incrementally calculate sin
    // and cosine from then on.
    float cosDelta = Cos(angleDelta);
    float sinDelta = Sin(angleDelta);
    float incrementalSin = 0.0f;
    float incrementalCos = 1.0f;

    vec3f firstPoint = majorAxis + origin;
    vec3f prevPoint = firstPoint;

    for (size_t i = 1; i < kNumRingSegments; ++i) {
        const float newCos = incrementalCos * cosDelta - incrementalSin * sinDelta;
        const float newSin = incrementalCos * sinDelta + incrementalSin * cosDelta;
        incrementalCos = newCos;
        incrementalSin = newSin;

        vec3f point = (majorAxis * incrementalCos + origin) + (minorAxis * incrementalSin);

        vbStart[vbOffset++] = { prevPoint, color };
        vbStart[vbOffset++] = { point, color };

        prevPoint = point;
    }

    vbStart[vbOffset++] = { prevPoint, color };
    vbStart[vbOffset++] = { firstPoint, color };

    mDebugVerticesCount += kNumRingVertices;
}

void RenderView::DebugDrawSphere(const vec3f& center, const float radius, const uint32_t color) {
    const vec3f xaxis = vec3f(radius, 0.0f, 0.0f);
    const vec3f yaxis = vec3f(0.0f, radius, 0.0f);
    const vec3f zaxis = vec3f(0.0f, 0.0f, radius);

    this->DebugDrawRing(center, xaxis, zaxis, color);
    this->DebugDrawRing(center, xaxis, yaxis, color);
    this->DebugDrawRing(center, yaxis, zaxis, color);
}

void RenderView::DebugDrawTetrahedron(const vec3f& a, const vec3f& b, const float r, const uint32_t color) {
    constexpr size_t kNumTetrahedronLines = 6;
    constexpr size_t kNumTetrahedronVertices = kNumTetrahedronLines * 2;

    this->EnsureDebugVertices(kNumTetrahedronVertices);

    vec3f dir = vec3f::normalize(b - a);
    vec3f axisX, axisZ;
    OrthonormalBasis(dir, axisX, axisZ);

    const vec3f p0 = a - vec3f(axisX * r + axisZ * r);
    const vec3f p1 = a + vec3f(axisX * r + (-axisZ * r));
    const vec3f p2 = a + vec3f(axisZ * r);

    DebugVertex* vbStart = &mDebugVertices[mDebugVerticesCount];

    // base
    vbStart[ 0] = { p0, color }; vbStart[ 1] = { p1, color };
    vbStart[ 2] = { p1, color }; vbStart[ 3] = { p2, color };
    vbStart[ 4] = { p2, color }; vbStart[ 5] = { p0, color };
    // top
    vbStart[ 6] = { p0, color }; vbStart[ 7] = { b, color };
    vbStart[ 8] = { p1, color }; vbStart[ 9] = { b, color };
    vbStart[10] = { p2, color }; vbStart[11] = { b, color };

    mDebugVerticesCount += kNumTetrahedronVertices;
}

void RenderView::DebugDrawTransformedBBox(const mat4f& xform, const AABBox& bbox, const uint32_t color) {
    constexpr size_t kNumBoxVertices = 24;

    this->EnsureDebugVertices(kNumBoxVertices);

    vec3f points[8] = { bbox.minimum,
                        vec3f(bbox.maximum.x, bbox.minimum.y, bbox.minimum.z),
                        vec3f(bbox.maximum.x, bbox.minimum.y, bbox.maximum.z),
                        vec3f(bbox.minimum.x, bbox.minimum.y, bbox.maximum.z),
                        vec3f(bbox.minimum.x, bbox.maximum.y, bbox.minimum.z),
                        vec3f(bbox.maximum.x, bbox.maximum.y, bbox.minimum.z),
                        bbox.maximum,
                        vec3f(bbox.minimum.x, bbox.maximum.y, bbox.maximum.z) };

    for (size_t i = 0; i < 8; ++i) {
        points[i] = xform.transformPos(points[i]);
    }

    DebugVertex* vbStart = &mDebugVertices[mDebugVerticesCount];
    vbStart[ 0] = { points[0], color };  vbStart[ 1] = { points[1], color };
    vbStart[ 2] = { points[1], color };  vbStart[ 3] = { points[2], color };
    vbStart[ 4] = { points[2], color };  vbStart[ 5] = { points[3], color };
    vbStart[ 6] = { points[3], color };  vbStart[ 7] = { points[0], color };
    vbStart[ 8] = { points[4], color };  vbStart[ 9] = { points[5], color };
    vbStart[10] = { points[5], color };  vbStart[11] = { points[6], color };
    vbStart[12] = { points[6], color };  vbStart[13] = { points[7], color };
    vbStart[14] = { points[7], color };  vbStart[15] = { points[4], color };
    vbStart[16] = { points[0], color };  vbStart[17] = { points[4], color };
    vbStart[18] = { points[1], color };  vbStart[19] = { points[5], color };
    vbStart[20] = { points[2], color };  vbStart[21] = { points[6], color };
    vbStart[22] = { points[3], color };  vbStart[23] = { points[7], color };

    mDebugVerticesCount += kNumBoxVertices;
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

