#include "halflifemodel.h"
#include <fstream>

struct VertexIndexer {
    MyArray<uint16_t>            indices;
    MyArray<HalfLifeModelVertex> vertices;
    AABBox                       bounds;

    VertexIndexer() {
        bounds.Reset();
    }

    void AddVertex(const vec3f& v, const vec3f& n, const vec2f& uv, const uint32_t bone) {
        const HalfLifeModelVertex vertex = { v, n, uv, bone };
        auto it = std::find(vertices.begin(), vertices.end(), vertex);
        uint16_t index;
        if (it == vertices.end()) {
            index = scast<uint16_t>(vertices.size());
            vertices.push_back(vertex);

            bounds.Absorb(v);
        } else {
            index = scast<uint16_t>(std::distance(vertices.begin(), it));
        }
        indices.push_back(index);
    }
};

PACKED_STRUCT_BEGIN
struct HLVertexCMD {
    int16_t vertex;
    int16_t normal;
    int16_t u, v;
} PACKED_STRUCT_END;


static MemStream ReadFileToMemStream(const fs::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    file.seekg(0, std::ios::end);
    const size_t fileSize = file.tellg();
    char* memory = rcast<char*>(malloc(fileSize));
    if (!memory) {
        return {};
    }

    file.seekg(0, std::ios::beg);
    file.read(memory, fileSize);
    file.close();

    return MemStream(memory, fileSize, true);
}

// based on StudioModel::CalcBoneQuaternion from the original hlmv code by Mete Ciragan
// some kind of RLE-like compression
static int16_t DecodeAnimValue(const mstudioanim_t* animPtr, int frame, const size_t channel) {
    if (!animPtr->offset[channel]) {
        return 0;
    }

    const mstudioanimvalue_t* valuePtr = rcast<const mstudioanimvalue_t*>(rcast<const char*>(animPtr) + animPtr->offset[channel]);

    while (valuePtr->num.total <= frame) {
        frame -= valuePtr->num.total;
        valuePtr += valuePtr->num.valid + 1;

        if (!valuePtr->num.total) {
            return 0;
        }
    }

    // Bah, missing blend!
    if (valuePtr->num.valid > frame) {
        return valuePtr[frame + 1].value;
    } else {
        // get last valid data block
        return valuePtr[valuePtr->num.valid].value;
    }
}


HalfLifeModel::HalfLifeModel()
    : mActiveSkin(0)
{
}
HalfLifeModel::~HalfLifeModel() {
}

bool HalfLifeModel::LoadFromPath(const fs::path& filePath) {
    MemStream stream = ReadFileToMemStream(filePath);
    if (!stream) {
        return false;
    }

    studiohdr_t stdhdr = {};
    stream.ReadStruct(stdhdr);

    if (HalfLifeModel::kIDSTMagic != stdhdr.magic || stdhdr.version < 10) {
        return false;
    }

    mSourcePath = filePath;

    if (stdhdr.numTextures == 0) {
        // try to load texture model
        fs::path tmodelPath = filePath;
        tmodelPath.replace_extension("");
        tmodelPath += "t.mdl";
        MemStream tstream = ReadFileToMemStream(tmodelPath);
        if (tstream) {
            studiohdr_t stdhdr = {};
            tstream.ReadStruct(stdhdr);

            if (HalfLifeModel::kIDSTMagic == stdhdr.magic && stdhdr.numTextures) {
                this->LoadTextures(tstream, scast<size_t>(stdhdr.numTextures), scast<size_t>(stdhdr.offsetTextures));
                if (stdhdr.numSkinFamilies > 0) {
                    this->LoadSkins(tstream, scast<size_t>(stdhdr.numSkinFamilies), scast<size_t>(stdhdr.numSkinRef), scast<size_t>(stdhdr.offsetSkins));
                }
            }
        }
    }

    stream.SetCursor(0);
    bool result = this->LoadFromMemStream(stream, stdhdr);

    return result;
}

bool HalfLifeModel::LoadFromMemStream(MemStream& stream, const studiohdr_t& stdhdr) {
    // need to load textures first to be able to calculate UVs
    if (stdhdr.numTextures > 0) {
        this->LoadTextures(stream, scast<size_t>(stdhdr.numTextures), scast<size_t>(stdhdr.offsetTextures));
    }
    if (stdhdr.numSkinFamilies > 0) {
        this->LoadSkins(stream, scast<size_t>(stdhdr.numSkinFamilies), scast<size_t>(stdhdr.numSkinRef), scast<size_t>(stdhdr.offsetSkins));
    }

    // load main geometry
    mBodyParts.resize(stdhdr.numBodyParts);
    mActiveBodyPartSubModel.resize(stdhdr.numBodyParts, 0);
    MemStream bodypartsStream = stream.Substream(scast<size_t>(stdhdr.offsetBodyParts), stream.Length());
    for (int bodyIdx = 0; bodyIdx < stdhdr.numBodyParts; ++bodyIdx) {
        mBodyParts[bodyIdx] = MakeRefPtr<HalfLifeModelBodypart>();
        BodyPartPtr& bodyPart = mBodyParts[bodyIdx];

        mstudiobodyparts_t bparthdr = {};
        bodypartsStream.ReadStruct(bparthdr);

        bodyPart->SetName(bparthdr.name);

        MemStream modelsStream = stream.Substream(scast<size_t>(bparthdr.offsetModels), stream.Length());
        for (int mdlIdx = 0; mdlIdx < bparthdr.numModels; ++mdlIdx) {
            mstudiomodel_t mdlHdr = {};
            modelsStream.ReadStruct(mdlHdr);

            RefPtr<HalfLifeModelStudioModel> smdl = MakeRefPtr<HalfLifeModelStudioModel>();
            smdl->SetName(mdlHdr.name);
            smdl->SetType(mdlHdr.type);
            smdl->SetBoundingRadius(mdlHdr.boundingRadius);

            MyArray<vec3f> allModelVertices(mdlHdr.numVertices);
            MyArray<vec3f> allModelNormals(mdlHdr.numNormals);
            MyArray<uint8_t> allModelVBones(mdlHdr.numVertices);
            MyArray<uint8_t> allModelNBones(mdlHdr.numNormals);

            stream.Substream(mdlHdr.offsetVerices, stream.Length()).ReadToBuffer(allModelVertices.data(), sizeof(vec3f) * mdlHdr.numVertices);
            stream.Substream(mdlHdr.offsetNormals, stream.Length()).ReadToBuffer(allModelNormals.data(), sizeof(vec3f) * mdlHdr.numNormals);
            stream.Substream(mdlHdr.offsetVBonesIndices, stream.Length()).ReadToBuffer(allModelVBones.data(), allModelVBones.size());
            stream.Substream(mdlHdr.offsetNBonesIndices, stream.Length()).ReadToBuffer(allModelNBones.data(), allModelNBones.size());

            VertexIndexer indexer;

            MemStream meshesStream = stream.Substream(scast<size_t>(mdlHdr.offsetMeshes), stream.Length());
            uint32_t indicesOffset = 0;
            for (int meshIdx = 0; meshIdx < mdlHdr.numMeshes; ++meshIdx) {
                mstudiomesh_t meshHdr = {};
                meshesStream.ReadStruct(meshHdr);

                HalfLifeModelStudioMesh smesh;
                smesh.textureIndex = scast<uint32_t>(meshHdr.skinref);

                const HalfLifeModelTexture* hltexture = (smesh.textureIndex < mTextures.size()) ? &mTextures[smesh.textureIndex] : nullptr;

                const float invTexWidth = (hltexture != nullptr) ? (1.0f / scast<float>(hltexture->width)) : 1.0f;
                const float invTexHeight = (hltexture != nullptr) ? (1.0f / scast<float>(hltexture->height)) : 1.0f;

                auto makeUV = [invTexWidth, invTexHeight](const HLVertexCMD& vcmd)->vec2f {
                    return vec2f(scast<float>(vcmd.u) * invTexWidth, scast<float>(vcmd.v) * invTexHeight);
                };

                const int16_t* tricmds = rcast<const int16_t*>(stream.Data() + meshHdr.offsetTriangles);
                int numVertices;
                uint32_t numIndices = 0;
                while (numVertices = *(tricmds++)) {
                    if (numVertices < 0) {
                        // this is a triangle fan, convert it to the striangles list
                        numVertices = -numVertices;
                        const HLVertexCMD* vertexCmds = rcast<const HLVertexCMD*>(tricmds);
                        for (int i = 2; i < numVertices; ++i) {
                            DebugAssert(allModelNBones[vertexCmds[0].normal] == allModelVBones[vertexCmds[0].vertex]);
                            indexer.AddVertex(allModelVertices[vertexCmds[0].vertex], allModelNormals[vertexCmds[0].normal], makeUV(vertexCmds[0]), allModelVBones[vertexCmds[0].vertex]);

                            DebugAssert(allModelNBones[vertexCmds[i].normal] == allModelVBones[vertexCmds[i].vertex]);
                            indexer.AddVertex(allModelVertices[vertexCmds[i].vertex], allModelNormals[vertexCmds[i].normal], makeUV(vertexCmds[i]), allModelVBones[vertexCmds[i].vertex]);

                            DebugAssert(allModelNBones[vertexCmds[i-1].normal] == allModelVBones[vertexCmds[i-1].vertex]);
                            indexer.AddVertex(allModelVertices[vertexCmds[i-1].vertex], allModelNormals[vertexCmds[i-1].normal], makeUV(vertexCmds[i-1]), allModelVBones[vertexCmds[i-1].vertex]);
                        }

                        tricmds += numVertices * 4;
                        numIndices += scast<uint32_t>((numVertices - 2) * 3);
                    } else {
                        // this is a triangle strip, convert it to the striangles list
                        const HLVertexCMD* vertexCmds = rcast<const HLVertexCMD*>(tricmds);
                        for (int i = 0; i < numVertices - 2; ++i) {
                            DebugAssert(allModelNBones[vertexCmds[i].normal] == allModelVBones[vertexCmds[i].vertex]);
                            indexer.AddVertex(allModelVertices[vertexCmds[i].vertex], allModelNormals[vertexCmds[i].normal], makeUV(vertexCmds[i]), allModelVBones[vertexCmds[i].vertex]);

                            if (i & 1) {
                                DebugAssert(allModelNBones[vertexCmds[i+1].normal] == allModelVBones[vertexCmds[i+1].vertex]);
                                indexer.AddVertex(allModelVertices[vertexCmds[i+1].vertex], allModelNormals[vertexCmds[i+1].normal], makeUV(vertexCmds[i+1]), allModelVBones[vertexCmds[i+1].vertex]);

                                DebugAssert(allModelNBones[vertexCmds[i+2].normal] == allModelVBones[vertexCmds[i+2].vertex]);
                                indexer.AddVertex(allModelVertices[vertexCmds[i+2].vertex], allModelNormals[vertexCmds[i+2].normal], makeUV(vertexCmds[i+2]), allModelVBones[vertexCmds[i+2].vertex]);
                            } else {
                                DebugAssert(allModelNBones[vertexCmds[i+2].normal] == allModelVBones[vertexCmds[i+2].vertex]);
                                indexer.AddVertex(allModelVertices[vertexCmds[i+2].vertex], allModelNormals[vertexCmds[i+2].normal], makeUV(vertexCmds[i+2]), allModelVBones[vertexCmds[i+2].vertex]);

                                DebugAssert(allModelNBones[vertexCmds[i+1].normal] == allModelVBones[vertexCmds[i+1].vertex]);
                                indexer.AddVertex(allModelVertices[vertexCmds[i+1].vertex], allModelNormals[vertexCmds[i+1].normal], makeUV(vertexCmds[i+1]), allModelVBones[vertexCmds[i+1].vertex]);
                            }
                        }

                        tricmds += numVertices * 4;
                        numIndices += scast<uint32_t>((numVertices - 2) * 3);
                    }
                }

                smesh.numIndices = numIndices;
                smesh.indicesOffset = indicesOffset;

                indicesOffset += numIndices;

                smdl->AddMesh(smesh);
            }

            smdl->SetVertices(indexer.vertices);
            smdl->SetIndices(indexer.indices);

            mBounds.Absorb(indexer.bounds);

            bodyPart->AddStudioModel(smdl);
        }
    }

    // load bones
    if (stdhdr.numBones > 0) {
        mBones.resize(stdhdr.numBones);
        mSkeleton.resize(stdhdr.numBones);

        MemStream bonesStream = stream.Substream(scast<size_t>(stdhdr.offsetBones), stream.Length());
        for (int i = 0; i < stdhdr.numBones; ++i) {
            mstudiobone_t sbone = {};
            bonesStream.ReadStruct(sbone);

            HalfLifeModelBone& hlbone = mBones[i];
            hlbone.name = sbone.name;
            hlbone.parentIdx = sbone.parent;
            hlbone.pos = vec3f(sbone.value[0], sbone.value[1], sbone.value[2]);
            hlbone.rot = vec3f(sbone.value[3], sbone.value[4], sbone.value[5]);
            hlbone.scalePos = vec3f(sbone.scale[0], sbone.scale[1], sbone.scale[2]);
            hlbone.scaleRot = vec3f(sbone.scale[3], sbone.scale[4], sbone.scale[5]);
            memcpy(hlbone.controllerIdx, sbone.bonecontroller, sizeof(hlbone.controllerIdx));

            mSkeleton[i] = mat4f::identity();
        }
    }

    // load bone controllers
    if (stdhdr.numBoneControllers > 0) {
        mBoneControllers.resize(stdhdr.numBoneControllers);
        mBoneControllerValues.resize(stdhdr.numBoneControllers);

        MemStream bctrlsStream = stream.Substream(scast<size_t>(stdhdr.offsetBoneControllers), stream.Length());
        for (int i = 0; i < stdhdr.numBoneControllers; ++i) {
            mstudiobonecontroller_t hlbcontroller = {};
            bctrlsStream.ReadStruct(hlbcontroller);

            HalfLifeModelBoneController& bcontroller = mBoneControllers[i];
            bcontroller.boneIdx = hlbcontroller.bone;
            bcontroller.type = scast<uint32_t>(hlbcontroller.type);
            bcontroller.start = hlbcontroller.start;
            bcontroller.end = hlbcontroller.end;
            bcontroller.index = scast<uint32_t>(hlbcontroller.index);

            mBoneControllerValues[i] = 0.0f;
        }
    }

    MyArray<MemStream> seqGroupsFiles;

    fs::path sourceFolder = mSourcePath.parent_path();
    // load sequences groups
    if (stdhdr.numSeqGroups > 0) {
        if (stdhdr.numSeqGroups > 1) {
            seqGroupsFiles.resize(stdhdr.numSeqGroups - 1);
        }

        mSequenceGroups.resize(stdhdr.numSeqGroups);

        MemStream seqGrpsStream = stream.Substream(scast<size_t>(stdhdr.offsetSeqGroups), stream.Length());
        for (int i = 0; i < stdhdr.numSeqGroups; ++i) {
            HalfLifeModelSequenceGroup& seqGrp = mSequenceGroups[i];

            mstudioseqgroup_t hlSeqGrp = {};
            seqGrpsStream.ReadStruct(hlSeqGrp);

            seqGrp.label = hlSeqGrp.label;
            seqGrp.name = hlSeqGrp.name;
            seqGrp.data = hlSeqGrp.data;

            if (i > 0) {
                fs::path seqGroupFileName = fs::path(seqGrp.name).filename();
                MemStream seqStream = ReadFileToMemStream(sourceFolder / seqGroupFileName);
                if (seqStream) {
                    studiohdr_t seqStdhdr = {};
                    seqStream.ReadStruct(seqStdhdr);
                    if (HalfLifeModel::kIDSQMagic == seqStdhdr.magic) {
                        seqGroupsFiles[i - 1] = seqStream;
                    }
                }
            }
        }
    }

    // load sequences
    if (stdhdr.numSequences > 0) {
        mSequences.resize(stdhdr.numSequences);

        MemStream seqStream = stream.Substream(scast<size_t>(stdhdr.offsetSequences), stream.Length());
        for (size_t i = 0; i < mSequences.size(); ++i) {
            SequencePtr& sequence = mSequences[i];

            mstudioseqdesc_t seqDesc = {};
            seqStream.ReadStruct(seqDesc);

            sequence = MakeRefPtr<HalfLifeModelSequence>(mBones.size());
            sequence->SetName(seqDesc.label);
            sequence->SetFPS(seqDesc.fps);
            sequence->SetMotionType(scast<uint32_t>(seqDesc.motionType));
            sequence->SetMotionBone(scast<uint32_t>(seqDesc.motionBone));
            sequence->SetSequenceGroup(scast<uint32_t>(seqDesc.seqGroup));
            sequence->SetFramesCount(scast<uint32_t>(seqDesc.numFrames));
            sequence->SetBounds(AABBox(seqDesc.bbmin, seqDesc.bbmax));

            // first sequence group is always built-in it seems
            if (!seqDesc.seqGroup) {
                this->LoadSequenceAnim(sequence, stream, scast<size_t>(seqDesc.offsetAnimData + mSequenceGroups[0].data));
            } else if (seqGroupsFiles[seqDesc.seqGroup - 1]) {
                this->LoadSequenceAnim(sequence, seqGroupsFiles[seqDesc.seqGroup - 1], scast<size_t>(seqDesc.offsetAnimData));
            }

            if (seqDesc.numEvents > 0) {
                MyArray<HalfLifeModelAnimEvent> events(seqDesc.numEvents);
                MemStream eventsStream = stream.Substream(scast<size_t>(seqDesc.offsetEvents), stream.Length());
                for (HalfLifeModelAnimEvent& event : events) {
                    mstudioevent_t hlevent = {};
                    eventsStream.ReadStruct(hlevent);

                    event.frame = scast<uint32_t>(hlevent.frame);
                    event.event = scast<uint32_t>(hlevent.event);
                    event.type = scast<uint32_t>(hlevent.type);
                    event.options = hlevent.options;
                }

                sequence->SetEvents(events);
            }
        }
    } else {
        DebugAssert(false);
    }

    seqGroupsFiles.clear();

    // load attachments
    if (stdhdr.numAttachments > 0) {
        mAttachments.resize(stdhdr.numAttachments);

        MemStream attStream = stream.Substream(scast<size_t>(stdhdr.offsetAttachments), stream.Length());
        for (int i = 0; i < stdhdr.numAttachments; ++i) {
            mstudioattachment_t hlattachment = {};
            attStream.ReadStruct(hlattachment);

            HalfLifeModelAttachment& attachment = mAttachments[i];
            attachment.name = hlattachment.name;
            attachment.type = hlattachment.type;
            attachment.bone = hlattachment.bone;
            attachment.origin = hlattachment.org;
            attachment.vectors[0] = hlattachment.vectors[0];
            attachment.vectors[1] = hlattachment.vectors[1];
            attachment.vectors[2] = hlattachment.vectors[2];
        }
    }

    // load hitboxes
    if (stdhdr.numHitBoxes > 0) {
        mHitBoxes.resize(stdhdr.numHitBoxes);

        MemStream hbStream = stream.Substream(scast<size_t>(stdhdr.offsetHitBoxes), stream.Length());
        for (int i = 0; i < stdhdr.numHitBoxes; ++i) {
            mstudiobbox_t hlhitbox = {};
            hbStream.ReadStruct(hlhitbox);

            HalfLifeModelHitBox& hitbox = mHitBoxes[i];
            hitbox.boneIdx = scast<uint32_t>(hlhitbox.bone);
            hitbox.hitGroup = scast<uint32_t>(hlhitbox.group);
            hitbox.bounds.minimum = hlhitbox.bbmin;
            hitbox.bounds.maximum = hlhitbox.bbmax;
        }
    }

    mBounds = mSequences[0]->GetBounds();

    return true;
}

void HalfLifeModel::LoadTextures(MemStream& stream, const size_t numTextures, const size_t texturesOffset) {
    MemStream texturesStream = stream.Substream(texturesOffset, stream.Length());

    mTextures.resize(numTextures);
    for (size_t i = 0; i < numTextures; ++i) {
        mstudiotexture_t thdr = {};
        texturesStream.ReadStruct(thdr);

        HalfLifeModelTexture& tex = mTextures[i];
        tex.name = thdr.name;
        tex.width = scast<uint32_t>(thdr.width);
        tex.height = scast<uint32_t>(thdr.height);
        tex.chrome = (thdr.flags & STUDIO_NF_CHROME) == STUDIO_NF_CHROME;
        tex.additive = (thdr.flags & STUDIO_NF_ADDITIVE) == STUDIO_NF_ADDITIVE;
        tex.masked = (thdr.flags & STUDIO_NF_MASKED) == STUDIO_NF_MASKED;
        tex.data.resize(tex.width * tex.height);
        tex.palette.resize(256 * 3);

        MemStream texDataStream = stream.Substream(scast<size_t>(thdr.offset), stream.Length());
        texDataStream.ReadToBuffer(tex.data.data(), tex.data.size());
        texDataStream.ReadToBuffer(tex.palette.data(), tex.palette.size());
    }
}

void HalfLifeModel::LoadSkins(MemStream& stream, const size_t numSkins, const size_t numTexturesPerSkin, const size_t skinsOffset) {
    MemStream skinsStream = stream.Substream(skinsOffset, stream.Length());

    mSkins.resize(numSkins);
    for (HalfLifeModelSkin& skin : mSkins) {
        skin.remapTable.resize(numTexturesPerSkin);
        skinsStream.ReadToBuffer(skin.remapTable.data(), numTexturesPerSkin * sizeof(uint16_t));
    }
}

size_t HalfLifeModel::GetBodyPartsCount() const {
    return mBodyParts.size();
}

HalfLifeModelBodypart* HalfLifeModel::GetBodyPart(const size_t idx) const {
    return mBodyParts[idx].get();
}

void HalfLifeModel::SetBodyPartActiveSubModel(const size_t bodyPartIdx, const size_t subModelIdx) {
    mActiveBodyPartSubModel[bodyPartIdx] = subModelIdx;
}

size_t HalfLifeModel::GetBodyPartActiveSubModel(const size_t bodyPartIdx) const {
    return mActiveBodyPartSubModel[bodyPartIdx];
}

size_t HalfLifeModel::GetBonesCount() const {
    return mBones.size();
}

const HalfLifeModelBone& HalfLifeModel::GetBone(const size_t idx) const {
    return mBones[idx];
}

const mat4f& HalfLifeModel::GetBoneMat(const size_t idx) const {
    return mSkeleton[idx];
}

size_t HalfLifeModel::GetBoneControllersCount() const {
    return mBoneControllers.size();
}

const HalfLifeModelBoneController& HalfLifeModel::GetBoneController(const size_t idx) const {
    return mBoneControllers[idx];
}

void HalfLifeModel::SetBoneControllerValue(const size_t idx, const float value) {
    mBoneControllerValues[idx] = value;
}

float HalfLifeModel::GetBoneControllerValue(const size_t idx) {
    const HalfLifeModelBoneController& controller = this->GetBoneController(idx);
    return controller.IsRotation() ? Deg2Rad(mBoneControllerValues[idx]) : mBoneControllerValues[idx];
}

size_t HalfLifeModel::GetTexturesCount() const {
    return mTextures.size();
}

const HalfLifeModelTexture& HalfLifeModel::GetTexture(const size_t idx) const {
    return mTextures[idx];
}

size_t HalfLifeModel::GetSkinsCount() const {
    return mSkins.size();
}

void HalfLifeModel::SetActiveSkin(const size_t skinIdx) {
    mActiveSkin = skinIdx;
}

size_t HalfLifeModel::GetActiveSkin() const {
    return mActiveSkin;
}

size_t HalfLifeModel::GetSkinTexture(const size_t textureIdx) const {
    const HalfLifeModelSkin& skin = mSkins[mActiveSkin];
    if (textureIdx < skin.remapTable.size()) {
        return skin.remapTable[textureIdx];
    } else {
        return textureIdx;
    }
}

const AABBox& HalfLifeModel::GetBounds() const {
    return mBounds;
}

size_t HalfLifeModel::GetSequencesCount() const {
    return mSequences.size();
}

HalfLifeModelSequence* HalfLifeModel::GetSequence(const size_t idx) const {
    return mSequences[idx].get();
}

size_t HalfLifeModel::GetAttachmentsCount() const {
    return mAttachments.size();
}

const HalfLifeModelAttachment& HalfLifeModel::GetAttachment(const size_t idx) const {
    return mAttachments[idx];
}

size_t HalfLifeModel::GetHitBoxesCount() const {
    return mHitBoxes.size();
}

const HalfLifeModelHitBox& HalfLifeModel::GetHitBox(const size_t idx) const {
    return mHitBoxes[idx];
}

void HalfLifeModel::CalculateSkeleton(const float frame, const size_t sequenceIdx) {
    if (!mBones.empty() && !mSequences.empty()) {
        const SequencePtr& sequence = mSequences[sequenceIdx];

        const uint32_t motionType = sequence->GetMotionType();
        const uint32_t motionBone = sequence->GetMotionBone();

        const uint32_t frameA = scast<uint32_t>(Floori(frame)) % sequence->GetFramesCount();
        const uint32_t frameB = (frameA + 1u) % sequence->GetFramesCount();
        const float frameLerp = frame - scast<float>(frameA);

        for (size_t boneIdx = 0, numBones = mBones.size(); boneIdx < numBones; ++boneIdx) {
            vec3f pos;
            quatf rot;

            const HalfLifeModelBone& bone = mBones[boneIdx];
            const HalfLifeModelAnimLine& animLine = sequence->GetAnimLine(boneIdx);
            if (!animLine.frames.empty()) {
                const HalfLifeModelAnimFrame& valueA = animLine.frames[frameA];
                const HalfLifeModelAnimFrame& valueB = animLine.frames[frameB];

                vec3f offsetA(scast<float>(valueA.offset[0]) * bone.scalePos.x,
                              scast<float>(valueA.offset[1]) * bone.scalePos.y,
                              scast<float>(valueA.offset[2]) * bone.scalePos.z);
                vec3f offsetB(scast<float>(valueB.offset[0]) * bone.scalePos.x,
                              scast<float>(valueB.offset[1]) * bone.scalePos.y,
                              scast<float>(valueB.offset[2]) * bone.scalePos.z);

                vec3f rotationA(scast<float>(valueA.rotation[0]) * bone.scaleRot.x,
                                scast<float>(valueA.rotation[1]) * bone.scaleRot.y,
                                scast<float>(valueA.rotation[2]) * bone.scaleRot.z);
                vec3f rotationB(scast<float>(valueB.rotation[0]) * bone.scaleRot.x,
                                scast<float>(valueB.rotation[1]) * bone.scaleRot.y,
                                scast<float>(valueB.rotation[2]) * bone.scaleRot.z);

                // add bone controllers to the pos if any
                if (bone.controllerIdx[0] >= 0) {
                    offsetA.x += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[0]));
                    offsetB.x += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[0]));
                }
                if (bone.controllerIdx[1] >= 0) {
                    offsetA.y += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[1]));
                    offsetB.y += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[1]));
                }
                if (bone.controllerIdx[2] >= 0) {
                    offsetA.z += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[2]));
                    offsetB.z += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[2]));
                }

                // add bone controllers to the rotation if any
                if (bone.controllerIdx[3] >= 0) {
                    rotationA.x += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[3]));
                    rotationB.x += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[3]));
                }
                if (bone.controllerIdx[4] >= 0) {
                    rotationA.y += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[4]));
                    rotationB.y += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[4]));
                }
                if (bone.controllerIdx[5] >= 0) {
                    rotationA.z += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[5]));
                    rotationB.z += this->GetBoneControllerValue(scast<size_t>(bone.controllerIdx[5]));
                }

                pos = bone.pos + Lerp(offsetA, offsetB, frameLerp);
                rot = quatf::slerp(quatf::fromEuler(bone.rot + rotationA), quatf::fromEuler(bone.rot + rotationB), frameLerp);

                if (motionBone == boneIdx) {
                    if (motionType & STUDIO_X) {
                        pos.x = 0.0f;
                    }
                    if (motionType & STUDIO_Y) {
                        pos.y = 0.0f;
                    }
                    if (motionType & STUDIO_Z) {
                        pos.z = 0.0f;
                    }
                }
            } else {
                pos = bone.pos;
                rot = quatf::fromEuler(bone.rot);
            }

            mat4f& boneMat = mSkeleton[boneIdx];
            boneMat = mat4f::fromQuatAndPos(rot, pos);
            if (bone.parentIdx >= 0) {
                boneMat = mSkeleton[bone.parentIdx] * boneMat;
            }
        }
    }
}

void HalfLifeModel::LoadSequenceAnim(SequencePtr& sequence, MemStream& stream, const size_t offsetAnim) {
    MemStream animStream = stream.Substream(offsetAnim, stream.Length());
    const mstudioanim_t* animPtr = rcast<const mstudioanim_t*>(animStream.GetDataAtCursor());

    const uint32_t numFrames = sequence->GetFramesCount();

    for (size_t boneIdx = 0, numBones = mBones.size(); boneIdx < numBones; ++boneIdx, ++animPtr) {
        const HalfLifeModelBone& bone = mBones[boneIdx];

        HalfLifeModelAnimLine animLine;
        animLine.frames.resize(numFrames);
        for (int frame = 0; frame < scast<int>(numFrames); ++frame) {
            HalfLifeModelAnimFrame& animFrame = animLine.frames[frame];

            animFrame.offset[0] = DecodeAnimValue(animPtr, frame, 0);
            animFrame.offset[1] = DecodeAnimValue(animPtr, frame, 1);
            animFrame.offset[2] = DecodeAnimValue(animPtr, frame, 2);
            animFrame.rotation[0] = DecodeAnimValue(animPtr, frame, 3);
            animFrame.rotation[1] = DecodeAnimValue(animPtr, frame, 4);
            animFrame.rotation[2] = DecodeAnimValue(animPtr, frame, 5);
        }

        sequence->SetAnimLine(boneIdx, animLine);
    }
}


/////////////////////

HalfLifeModelBodypart::HalfLifeModelBodypart()
{}
HalfLifeModelBodypart::~HalfLifeModelBodypart()
{}

void HalfLifeModelBodypart::SetName(const CharString& name) {
    mName = name;
}

const CharString& HalfLifeModelBodypart::GetName() const {
    return mName;
}

void HalfLifeModelBodypart::AddStudioModel(const HalfLifeModelBodypart::StudioModelPtr& smdl) {
    mModels.push_back(smdl);
}

size_t HalfLifeModelBodypart::GetStudioModelsCount() const {
    return mModels.size();
}

HalfLifeModelStudioModel* HalfLifeModelBodypart::GetStudioModel(const size_t idx) const {
    return mModels[idx].get();
}



/////////////////////

HalfLifeModelStudioModel::HalfLifeModelStudioModel()
    : mName{}
    , mType(0)
    , mBoundingRadius(0.0f)
{
}
HalfLifeModelStudioModel::~HalfLifeModelStudioModel() {
}

void HalfLifeModelStudioModel::SetName(const CharString& name) {
    mName = name;
}

const CharString& HalfLifeModelStudioModel::GetName() const {
    return mName;
}

void HalfLifeModelStudioModel::SetType(const int type) {
    mType = type;
}

int HalfLifeModelStudioModel::GetType() const {
    return mType;
}

void HalfLifeModelStudioModel::SetBoundingRadius(const float r) {
    mBoundingRadius = r;
}

float HalfLifeModelStudioModel::GetBoundingRadius() const {
    return mBoundingRadius;
}

void HalfLifeModelStudioModel::SetVertices(const MyArray<HalfLifeModelVertex>& vertices) {
    mVertices = vertices;
}

size_t HalfLifeModelStudioModel::GetVerticesCount() const {
    return mVertices.size();
}

const HalfLifeModelVertex* HalfLifeModelStudioModel::GetVertices() const {
    return mVertices.data();
}

void HalfLifeModelStudioModel::SetIndices(const MyArray<uint16_t>& indices) {
    mIndices = indices;
}

size_t HalfLifeModelStudioModel::GetIndicesCount() const {
    return mIndices.size();
}

const uint16_t* HalfLifeModelStudioModel::GetIndices() {
    return mIndices.data();
}

void HalfLifeModelStudioModel::AddMesh(const HalfLifeModelStudioMesh& mesh) {
    mMeshes.push_back(mesh);
}

size_t HalfLifeModelStudioModel::GetMeshesCount() const {
    return mMeshes.size();
}

const HalfLifeModelStudioMesh& HalfLifeModelStudioModel::GetMesh(const size_t idx) const {
    return mMeshes[idx];
}



HalfLifeModelSequence::HalfLifeModelSequence(const size_t numBones) {
    mAnimLines.resize(numBones);
}
HalfLifeModelSequence::~HalfLifeModelSequence() {
}

void HalfLifeModelSequence::SetName(const CharString& name) {
    mName = name;
}

const CharString& HalfLifeModelSequence::GetName() const {
    return mName;
}

void HalfLifeModelSequence::SetFPS(const float fps) {
    mFPS = fps;
}

float HalfLifeModelSequence::GetFPS() const {
    return mFPS;
}

void HalfLifeModelSequence::SetMotionType(const uint32_t motionType) {
    mMotionType = motionType;
}

uint32_t HalfLifeModelSequence::GetMotionType() const {
    return mMotionType;
}

void HalfLifeModelSequence::SetMotionBone(const uint32_t motionBone) {
    mMotionBone = motionBone;
}

uint32_t HalfLifeModelSequence::GetMotionBone() const {
    return mMotionBone;
}

void HalfLifeModelSequence::SetFramesCount(const uint32_t frames) {
    mNumFrames = frames;
}

uint32_t HalfLifeModelSequence::GetFramesCount() const {
    return mNumFrames;
}

void HalfLifeModelSequence::SetSequenceGroup(const uint32_t group) {
    mSequenceGroup = group;
}

uint32_t HalfLifeModelSequence::GetSequenceGroup() const {
    return mSequenceGroup;
}

void HalfLifeModelSequence::SetBounds(const AABBox& bounds) {
    mBounds = bounds;
}

const AABBox& HalfLifeModelSequence::GetBounds() const {
    return mBounds;
}

void HalfLifeModelSequence::SetAnimLine(const size_t boneIdx, const HalfLifeModelAnimLine& animLine) {
    mAnimLines[boneIdx] = animLine;
}

const HalfLifeModelAnimLine& HalfLifeModelSequence::GetAnimLine(const size_t boneIdx) const {
    return mAnimLines[boneIdx];
}

void HalfLifeModelSequence::SetEvents(MyArray<HalfLifeModelAnimEvent>& events) {
    mEvents.swap(events);
}

size_t HalfLifeModelSequence::GetEventsCount() const {
    return mEvents.size();
}

const HalfLifeModelAnimEvent& HalfLifeModelSequence::GetEvent(const size_t idx) const {
    return mEvents[idx];
}
