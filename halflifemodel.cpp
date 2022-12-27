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
    short vertex;
    short normal;
    short u, v;
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


HalfLifeModel::HalfLifeModel() {

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

    if (HalfLifeModel::kIDSTMagic != stdhdr.magic && HalfLifeModel::kIDSQMagic != stdhdr.magic) {
        return false;
    }

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
                this->LoadTextures(tstream, stdhdr.numTextures, stdhdr.offsetTextures);
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

    mBodyParts.resize(stdhdr.numbodyparts);
    MemStream bodypartsStream = stream.Substream(scast<size_t>(stdhdr.offsetbodypart), stream.Length());
    for (int bodyIdx = 0; bodyIdx < stdhdr.numbodyparts; ++bodyIdx) {
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

                const short* tricmds = rcast<const short*>(stream.Data() + meshHdr.offsetTriangles);
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
            hlbone.rot = quatf::fromEuler(vec3f(sbone.value[3], sbone.value[4], sbone.value[5]));

            mat4f& boneMat = mSkeleton[i];
            boneMat = mat4f::fromQuatAndPos(hlbone.rot, hlbone.pos);
            if (hlbone.parentIdx >= 0) {
                boneMat = mSkeleton[hlbone.parentIdx] * boneMat;
            }
        }
    }

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
        tex.data.resize(tex.width * tex.height);
        tex.palette.resize(256 * 3);

        MemStream texDataStream = stream.Substream(scast<size_t>(thdr.offset), stream.Length());
        texDataStream.ReadToBuffer(tex.data.data(), tex.data.size());
        texDataStream.ReadToBuffer(tex.palette.data(), tex.palette.size());
    }
}

size_t HalfLifeModel::GetBodyPartsCount() const {
    return mBodyParts.size();
}

HalfLifeModelBodypart* HalfLifeModel::GetBodyPart(const size_t idx) const {
    return mBodyParts[idx].get();
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

size_t HalfLifeModel::GetTexturesCount() const {
    return mTextures.size();
}

const HalfLifeModelTexture& HalfLifeModel::GetTexture(const size_t idx) const {
    return mTextures[idx];
}

const AABBox& HalfLifeModel::GetBounds() const {
    return mBounds;
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

