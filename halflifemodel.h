#pragma once
#include "mycommon.h"
#include "mymath.h"

#include "halflifemodel_structs.inl"

class HalfLifeModelBodypart;
class HalfLifeModelStudioModel;
class HalfLifeModelSequence;

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
    bool                additive;
    bool                masked;
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
    int32_t     parentIdx;
    vec3f       pos;
    vec3f       rot;
    // scales used for decoding anims
    vec3f       scalePos;
    vec3f       scaleRot;
    // 
    int32_t     controllerIdx[6]; // rot + pos
};

struct HalfLifeModelBoneController {
    static const uint32_t kMouthIndex = 4;

    int32_t     boneIdx;
    uint32_t    type;
    float       start;
    float       end;
    uint32_t    index;

    inline bool IsRotation() const {
        return 0 != (this->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR));
    }
};

struct HalfLifeModelAttachment {
    CharString  name;
    int32_t     type;
    int32_t     bone;
    vec3f       origin;
    vec3f       vectors[3];
};

struct HalfLifeModelAnimEvent {
    uint32_t    frame;
    uint32_t    event;
    uint32_t    type;
    CharString  options;
};

struct HalfLifeModelSequenceGroup {
    CharString  label;
    CharString  name;   // file name
    int32_t     data;
};

struct HalfLifeModelAnimFrame {
    int16_t offset[3];
    int16_t rotation[3];
};

struct HalfLifeModelAnimLine {
    MyArray<HalfLifeModelAnimFrame> frames;
};

struct HalfLifeModelHitBox {
    uint32_t    boneIdx;
    uint32_t    hitGroup;
    AABBox      bounds;
};

class HalfLifeModel {
    static const uint32_t kIDSTMagic = MakeFourcc<'I','D','S','T'>();
    static const uint32_t kIDSQMagic = MakeFourcc<'I','D','S','Q'>();

    using BodyPartPtr = RefPtr<HalfLifeModelBodypart>;
    using SequencePtr = RefPtr<HalfLifeModelSequence>;

public:
    HalfLifeModel();
    ~HalfLifeModel();

    bool                                    LoadFromPath(const fs::path& filePath);
    bool                                    LoadFromMemStream(MemStream& stream, const studiohdr_t& stdhdr);

    void                                    LoadTextures(MemStream& stream, const size_t numTextures, const size_t texturesOffset);

    size_t                                  GetBodyPartsCount() const;
    HalfLifeModelBodypart*                  GetBodyPart(const size_t idx) const;

    size_t                                  GetBonesCount() const;
    const HalfLifeModelBone&                GetBone(const size_t idx) const;
    const mat4f&                            GetBoneMat(const size_t idx) const;

    size_t                                  GetBoneControllersCount() const;
    const HalfLifeModelBoneController&      GetBoneController(const size_t idx) const;
    void                                    SetBoneControllerValue(const size_t idx, const float value);
    float                                   GetBoneControllerValue(const size_t idx);

    size_t                                  GetTexturesCount() const;
    const HalfLifeModelTexture&             GetTexture(const size_t idx) const;

    const AABBox&                           GetBounds() const;

    size_t                                  GetSequencesCount() const;
    HalfLifeModelSequence*                  GetSequence(const size_t idx) const;

    size_t                                  GetAttachmentsCount() const;
    const HalfLifeModelAttachment&          GetAttachment(const size_t idx) const;

    size_t                                  GetHitBoxesCount() const;
    const HalfLifeModelHitBox&              GetHitBox(const size_t idx) const;

    void                                    CalculateSkeleton(const float frame, const size_t sequenceIdx);

private:
    void                                    LoadSequenceAnim(SequencePtr& sequence, MemStream& stream, const size_t offsetAnim);

private:
    fs::path                                mSourcePath;
    MyArray<BodyPartPtr>                    mBodyParts;
    MyArray<HalfLifeModelTexture>           mTextures;
    MyArray<HalfLifeModelBone>              mBones;
    MyArray<HalfLifeModelBoneController>    mBoneControllers;
    MyArray<float>                          mBoneControllerValues;
    MyArray<mat4f>                          mSkeleton;
    AABBox                                  mBounds;
    MyArray<SequencePtr>                    mSequences;
    MyArray<HalfLifeModelSequenceGroup>     mSequenceGroups;
    MyArray<HalfLifeModelAttachment>        mAttachments;
    MyArray<HalfLifeModelHitBox>            mHitBoxes;
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
    CharString                  mName;
    MyArray<StudioModelPtr>     mModels;
};

class HalfLifeModelStudioModel {
public:
    HalfLifeModelStudioModel();
    ~HalfLifeModelStudioModel();

    void                                SetName(const CharString& name);
    const CharString&                   GetName() const;
    void                                SetType(const int type);
    int                                 GetType() const;
    void                                SetBoundingRadius(const float r);
    float                               GetBoundingRadius() const;

    void                                SetVertices(const MyArray<HalfLifeModelVertex>& vertices);
    size_t                              GetVerticesCount() const;
    const HalfLifeModelVertex*          GetVertices() const;

    void                                SetIndices(const MyArray<uint16_t>& indices);
    size_t                              GetIndicesCount() const;
    const uint16_t*                     GetIndices();

    void                                AddMesh(const HalfLifeModelStudioMesh& mesh);
    size_t                              GetMeshesCount() const;
    const HalfLifeModelStudioMesh&      GetMesh(const size_t idx) const;

private:
    CharString                          mName;
    int                                 mType;
    float                               mBoundingRadius;
    MyArray<HalfLifeModelVertex>        mVertices;
    MyArray<uint16_t>                   mIndices;
    MyArray<HalfLifeModelStudioMesh>    mMeshes;
};

class HalfLifeModelSequence {
public:
    HalfLifeModelSequence(const size_t numBones);
    ~HalfLifeModelSequence();

    void                            SetName(const CharString& name);
    const CharString&               GetName() const;
    void                            SetFPS(const float fps);
    float                           GetFPS() const;
    void                            SetMotionType(const uint32_t motionType);
    uint32_t                        GetMotionType() const;
    void                            SetMotionBone(const uint32_t motionBone);
    uint32_t                        GetMotionBone() const;
    void                            SetFramesCount(const uint32_t frames);
    uint32_t                        GetFramesCount() const;
    void                            SetSequenceGroup(const uint32_t group);
    uint32_t                        GetSequenceGroup() const;
    void                            SetBounds(const AABBox& bounds);
    const AABBox&                   GetBounds() const;
    void                            SetAnimLine(const size_t boneIdx, const HalfLifeModelAnimLine& animLine);
    const HalfLifeModelAnimLine&    GetAnimLine(const size_t boneIdx) const;
    void                            SetEvents(MyArray<HalfLifeModelAnimEvent>& events);
    size_t                          GetEventsCount() const;
    const HalfLifeModelAnimEvent&   GetEvent(const size_t idx) const;

private:
    CharString                      mName;
    float                           mFPS;
    uint32_t                        mMotionType;
    uint32_t                        mMotionBone;
    uint32_t                        mSequenceGroup;
    uint32_t                        mNumFrames;
    AABBox                          mBounds;
    MyArray<HalfLifeModelAnimEvent> mEvents;
    MyArray<HalfLifeModelAnimLine>  mAnimLines;
};