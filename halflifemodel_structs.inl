#define MAXSTUDIOTRIANGLES      65535   // TODO: tune this
#define MAXSTUDIOVERTS          2048    // TODO: tune this
#define MAXSTUDIOSEQUENCES      256     // total animation sequences
#define MAXSTUDIOSKINS          128     // total textures
#define MAXSTUDIOSRCBONES       512     // bones allowed at source movement
#define MAXSTUDIOBONES          128     // total bones actually used
#define MAXSTUDIOMODELS         32      // sub-models per model
#define MAXSTUDIOBODYPARTS      32
#define MAXSTUDIOGROUPS         4
#define MAXSTUDIOANIMATIONS     512     // per sequence
#define MAXSTUDIOMESHES         256
#define MAXSTUDIOEVENTS         1024
#define MAXSTUDIOPIVOTS         256
#define MAXSTUDIOCONTROLLERS    8


PACKED_STRUCT_BEGIN
struct studiohdr_t {
    int     magic;
    int     version;

    char    name[64];
    int     length;

    vec3f   eyePosition;        // ideal eye position
    vec3f   min;                // ideal movement hull size
    vec3f   max;

    vec3f   bbmin;              // clipping bounding box
    vec3f   bbmax;

    int     flags;

    int     numBones;
    int     offsetBones;

    int     numbonecontrollers; // bone controllers
    int     offsetbonecontroller;

    int     numHitBoxes;        // complex bounding boxes
    int     offsetHitBoxes;

    int     numSequences;
    int     offsetSequences;

    int     numSeqGroups;       // demand loaded sequences
    int     offsetSeqGroups;

    int     numTextures;        // raw textures
    int     offsetTextures;
    int     offsetTexturesData;

    int     numskinref;         // replaceable textures
    int     numskinfamilies;
    int     offsetskin;

    int     numbodyparts;
    int     offsetbodypart;

    int     numAttachments;     // queryable attachable points
    int     offsetAttachments;

    int     soundtable;
    int     soundindex;
    int     soundgroups;
    int     offsetsoundgroup;

    int     numtransitions;     // animation node to animation node transition graph
    int     offsettransition;
} PACKED_STRUCT_END;

// header for demand loaded sequence group data
PACKED_STRUCT_BEGIN
struct studioseqhdr_t {
    int     id;
    int     version;

    char    name[64];
    int     length;
} PACKED_STRUCT_END;

// bones
PACKED_STRUCT_BEGIN
struct mstudiobone_t {
    char    name[32];           // bone name for symbolic links
    int     parent;             // parent bone
    int     flags;              // ??
    int     bonecontroller[6];  // bone controller index, -1 == none
    float   value[6];           // default DoF values
    float   scale[6];           // scale for delta DoF values
} PACKED_STRUCT_END;


// bone controllers
PACKED_STRUCT_BEGIN
struct mstudiobonecontroller_t {
    int     bone;               // -1 == 0
    int     type;               // X, Y, Z, XR, YR, ZR, M
    float   start;
    float   end;
    int     rest;               // byte index value at rest
    int     index;              // 0-3 user set controller, 4 mouth
} PACKED_STRUCT_END;

// intersection boxes
PACKED_STRUCT_BEGIN
struct mstudiobbox_t {
    int     bone;
    int     group;              // intersection group
    vec3f   bbmin;              // bounding box
    vec3f   bbmax;
} PACKED_STRUCT_END;

// demand loaded sequence groups
PACKED_STRUCT_BEGIN
struct mstudioseqgroup_t {
    char    label[32];          // textual name
    char    name[64];           // file name
    uint32_t cache;             // cache index pointer  (original code uses void*)
    int     data;               // hack for group 0
} PACKED_STRUCT_END;

// sequence descriptions
PACKED_STRUCT_BEGIN
struct mstudioseqdesc_t {
    char    label[32];          // sequence label

    float   fps;                // frames per second
    int     flags;              // looping/non-looping flags

    int     activity;
    int     actweight;

    int     numEvents;
    int     offsetEvents;

    int     numFrames;          // number of frames per sequence

    int     numpivots;          // number of foot pivots
    int     pivotindex;

    int     motionType;
    int     motionBone;
    vec3f   linearmovement;
    int     automoveposindex;
    int     automoveangleindex;

    vec3f   bbmin;              // per sequence bounding box
    vec3f   bbmax;

    int     numblends;
    int     offsetAnimData;    // mstudioanim_t pointer relative to start of sequence group data
    // [blend][bone][X, Y, Z, XR, YR, ZR]

    int     blendtype[2];       // X, Y, Z, XR, YR, ZR
    float   blendstart[2];      // starting value
    float   blendend[2];        // ending value
    int     blendparent;

    int     seqGroup;           // sequence group for demand loading

    int     entrynode;          // transition node at entry
    int     exitnode;           // transition node at exit
    int     nodeflags;          // transition rules

    int     nextseq;            // auto advancing sequences
} PACKED_STRUCT_END;

// events
PACKED_STRUCT_BEGIN
struct mstudioevent_t {
    int     frame;
    int     event;
    int     type;
    char    options[64];
} PACKED_STRUCT_END;


// pivots
PACKED_STRUCT_BEGIN
struct mstudiopivot_t {
    vec3f   org;                // pivot point
    int     start;
    int     end;
} PACKED_STRUCT_END;

// attachment
PACKED_STRUCT_BEGIN
struct mstudioattachment_t {
    char    name[32];
    int     type;
    int     bone;
    vec3f   org;                // attachment point
    vec3f   vectors[3];
} PACKED_STRUCT_END;

PACKED_STRUCT_BEGIN
struct mstudioanim_t {
    unsigned short  offset[6];
} PACKED_STRUCT_END;

// animation frames
typedef union
{
    struct {
        unsigned char   valid;
        unsigned char   total;
    } num;
    short               value;
} mstudioanimvalue_t;



// body part index
PACKED_STRUCT_BEGIN
struct mstudiobodyparts_t {
    char    name[64];
    int     numModels;
    int     base;
    int     offsetModels;
} PACKED_STRUCT_END;



// skin info
PACKED_STRUCT_BEGIN
struct mstudiotexture_t {
    char    name[64];
    int     flags;
    int     width;
    int     height;
    int     offset;
} PACKED_STRUCT_END;


// skin families
// short	index[skinfamilies][skinref]

// studio models
PACKED_STRUCT_BEGIN
struct mstudiomodel_t {
    char    name[64];

    int     type;               // Unused ??
    float   boundingRadius;     // Unused ??

    int     numMeshes;
    int     offsetMeshes;

    int     numVertices;        // number of unique vertices
    int     offsetVBonesIndices;// vertex bone info
    int     offsetVerices;      // vertex vec3_t
    int     numNormals;         // number of unique surface normals
    int     offsetNBonesIndices;// normal bone info
    int     offsetNormals;      // normal vec3_t

    int     numgroups;          // deformation groups
    int     groupindex;
} PACKED_STRUCT_END;


// meshes
PACKED_STRUCT_BEGIN
struct mstudiomesh_t {
    int     numTriangles;
    int     offsetTriangles;
    int     skinref;
    int     numNormals;         // per mesh normals
    int     offsetNormals;      // Unused ??
} PACKED_STRUCT_END;


// lighting options
#define STUDIO_NF_FLATSHADE     0x0001
#define STUDIO_NF_CHROME        0x0002
#define STUDIO_NF_FULLBRIGHT    0x0004
#define STUDIO_NF_NOMIPS        0x0008
#define STUDIO_NF_SMOOTH        0x0010
#define STUDIO_NF_ADDITIVE      0x0020
#define STUDIO_NF_MASKED        0x0040

// motion flags
#define STUDIO_X                0x0001
#define STUDIO_Y                0x0002
#define STUDIO_Z                0x0004
#define STUDIO_XR               0x0008
#define STUDIO_YR               0x0010
#define STUDIO_ZR               0x0020
#define STUDIO_LX               0x0040
#define STUDIO_LY               0x0080
#define STUDIO_LZ               0x0100
#define STUDIO_AX               0x0200
#define STUDIO_AY               0x0400
#define STUDIO_AZ               0x0800
#define STUDIO_AXR              0x1000
#define STUDIO_AYR              0x2000
#define STUDIO_AZR              0x4000
#define STUDIO_TYPES            0x7FFF
#define STUDIO_RLOOP            0x8000  // controller that wraps shortest distance

// sequence flags
#define STUDIO_LOOPING          0x0001

// bone flags
#define STUDIO_HAS_NORMALS      0x0001
#define STUDIO_HAS_VERTICES     0x0002
#define STUDIO_HAS_BBOX         0x0004
#define STUDIO_HAS_CHROME       0x0008	// if any of the textures have chrome on them

#define RAD_TO_STUDIO           (32768.0/M_PI)
#define STUDIO_TO_RAD           (M_PI/32768.0)
