#pragma once
#include "mycommon.h"
#include "mymath.h"

#include "halflifemodel_structs.inl"

class HalfLifeModelBodypart;
class HalfLifeModelStudioModel;

PACKED_STRUCT_BEGIN
struct HalfLifeModelVertex {
    vec3f       pos;
    vec3f       normal;
    vec2f       uv;
    uint32_t    boneIdx;

    inline bool operator==(const HalfLifeModelVertex& other) const {
        return pos == other.pos && normal == other.normal && uv == other.uv && boneIdx == other.boneIdx;
    }
    inline bool operator!=(const HalfLifeModelVertex& other) const {
        return pos != other.pos || normal != other.normal || uv != other.uv || boneIdx != other.boneIdx;
    }
} PACKED_STRUCT_END;

struct HalfLifeModelTexture {
    CharString          name;
    uint32_t            width;
    uint32_t            height;
    bool                chrome;
    MyArray<uint8_t>    data;
    MyArray<uint8_t>    palette;
};

struct HalfLifeModelStudioMesh {
    uint32_t    numIndices;
    uint32_t    indicesOffset;
    uint32_t    textureIndex;
};

struct HalfLifeModelBone {
    CharString  name;
    int         parentIdx;
    vec3f       pos;
    quatf       rot;
};

class HalfLifeModel {
    static const uint32_t kIDSTMagic = MakeFourcc<'I','D','S','T'>();
    static const uint32_t kIDSQMagic = MakeFourcc<'I','D','S','Q'>();

    using BodyPartPtr = RefPtr<HalfLifeModelBodypart>;

public:
    HalfLifeModel();
    ~HalfLifeModel();

    bool                            LoadFromPath(const fs::path& filePath);
    bool                            LoadFromMemStream(MemStream& stream, const studiohdr_t& stdhdr);

    void                            LoadTextures(MemStream& stream, const size_t numTextures, const size_t texturesOffset);

    size_t                          GetBodyPartsCount() const;
    HalfLifeModelBodypart*          GetBodyPart(const size_t idx) const;

    size_t                          GetBonesCount() const;
    const HalfLifeModelBone&        GetBone(const size_t idx) const;
    const mat4f&                    GetBoneMat(const size_t idx) const;

    size_t                          GetTexturesCount() const;
    const HalfLifeModelTexture&     GetTexture(const size_t idx) const;

    const AABBox&                   GetBounds() const;

private:
    MyArray<BodyPartPtr>            mBodyParts;
    MyArray<HalfLifeModelTexture>   mTextures;
    MyArray<HalfLifeModelBone>      mBones;
    MyArray<mat4f>                  mSkeleton;
    AABBox                          mBounds;
};

class HalfLifeModelBodypart {
    using StudioModelPtr = RefPtr<HalfLifeModelStudioModel>;

public:
    HalfLifeModelBodypart();
    ~HalfLifeModelBodypart();

    void                        SetName(const CharString& name);
    const CharString&           GetName() const;

    void                        AddStudioModel(const StudioModelPtr& smdl);
    size_t                      GetStudioModelsCount() const;
    HalfLifeModelStudioModel*   GetStudioModel(const size_t idx) const;

private:
    CharString              mName;
    MyArray<StudioModelPtr> mModels;
};

class HalfLifeModelStudioModel {
public:
    HalfLifeModelStudioModel();
    ~HalfLifeModelStudioModel();

    void                            SetName(const CharString& name);
    const CharString&               GetName() const;
    void                            SetType(const int type);
    int                             GetType() const;
    void                            SetBoundingRadius(const float r);
    float                           GetBoundingRadius() const;

    void                            SetVertices(const MyArray<HalfLifeModelVertex>& vertices);
    size_t                          GetVerticesCount() const;
    const HalfLifeModelVertex*      GetVertices() const;

    void                            SetIndices(const MyArray<uint16_t>& indices);
    size_t                          GetIndicesCount() const;
    const uint16_t*                 GetIndices();

    void                            AddMesh(const HalfLifeModelStudioMesh& mesh);
    size_t                          GetMeshesCount() const;
    const HalfLifeModelStudioMesh&  GetMesh(const size_t idx) const;

private:
    CharString                          mName;
    int                                 mType;
    float                               mBoundingRadius;
    MyArray<HalfLifeModelVertex>        mVertices;
    MyArray<uint16_t>                   mIndices;
    MyArray<HalfLifeModelStudioMesh>    mMeshes;
};
