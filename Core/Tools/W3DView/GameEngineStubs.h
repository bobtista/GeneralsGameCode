#pragma once

// TheSuperHackers @refactor bobtista 01/01/2025 Stub header for game engine types
// Include on all platforms for Core build (Windows game engine headers not available in Core)
// This file should NOT be included when HAVE_WWVEGAS is defined (use real WWVegas headers instead)

#ifndef HAVE_WWVEGAS

#include <string>  // For std::string and StringClass

// Vector3 stub
struct Vector3 {
    float x, y, z;
    float X, Y, Z;  // Some code uses uppercase
    Vector3() : x(0), y(0), z(0), X(0), Y(0), Z(0) {}
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_), X(x_), Y(y_), Z(z_) {}
    void Set(float x_, float y_, float z_) { x = X = x_; y = Y = y_; z = Z = z_; }  // Added for ScreenCursor.cpp
    Vector3 Get_Translation() const { return Vector3(x, y, z); }  // Added for ScreenCursor.cpp
};

// Vector2 stub
struct Vector2 {
    float x, y;
    float X, Y;  // Some code uses uppercase
    Vector2() : x(0), y(0), X(0), Y(0) {}
    Vector2(float x_, float y_) : x(x_), y(y_), X(x_), Y(y_) {}
};

// Vector3i stub
struct Vector3i {
    int x, y, z;
    int I, J, K;  // Some code uses uppercase
    Vector3i() : x(0), y(0), z(0), I(0), J(0), K(0) {}
    Vector3i(int x_, int y_, int z_) : x(x_), y(y_), z(z_), I(x_), J(y_), K(z_) {}
    // Array access operator for triangle indices
    int& operator[](int index) {
        switch(index) {
            case 0: return x;
            case 1: return y;
            case 2: return z;
            default: return x;
        }
    }
    const int& operator[](int index) const {
        switch(index) {
            case 0: return x;
            case 1: return y;
            case 2: return z;
            default: return x;
        }
    }
};

// Forward declarations
class RenderObjClass;
class RenderObjIterator;
class SphereRenderObjClass;
struct SphereVectorChannelClass;

// Matrix3D stub (defined early because RenderObjClass uses it)
struct Matrix3D {
    void Get_Transform(Vector3& pos, Vector3& scale) const { pos = Vector3(0,0,0); scale = Vector3(1,1,1); }
    Vector3 Get_Translation() const { return Vector3(0,0,0); }  // Added for ScreenCursor.cpp
    Matrix3D() {}  // Default constructor
};

// Game engine class stubs
class SceneClass {
public:
    void Get_Fog_Range(float* near_range, float* far_range) { if(near_range) *near_range = 0; if(far_range) *far_range = 0; }
    void Register(RenderObjClass*, int) {}  // Added for ScreenCursor.cpp
    void Unregister(RenderObjClass*, int) {}  // Added for ScreenCursor.cpp
    // ON_FRAME_UPDATE is defined as a macro below - don't redefine here
};
class SimpleSceneClass {};
class RenderInfoClass {};
class SceneIterator {
public:
    void First() {}
    void Next() {}
    bool Is_Done() { return true; }
    RenderObjClass* Current_Item() { return nullptr; }
};
struct AABoxClass {
    Vector3 Extent;
    Vector3 Center;
    AABoxClass() : Extent(0, 0, 0), Center(0, 0, 0) {}
    AABoxClass(const Vector3& center, const Vector3& extent) : Center(center), Extent(extent) {}
    void Add_Box(const AABoxClass& other) {}  // Stub method
};
struct SphereClass {
    Vector3 Center;
    float Radius;
    SphereClass() : Center(0, 0, 0), Radius(0) {}
    SphereClass(const Vector3& center, float radius) : Center(center), Radius(radius) {}
    void Add_Sphere(const SphereClass& other) {}  // Stub method
};
class CameraClass {
public:
    bool Cull_Sphere(const SphereClass& sphere) { return false; }
};
struct EulerAngles {
    float heading, pitch, bank;
    EulerAngles() : heading(0), pitch(0), bank(0) {}
};
struct AlphaVectorStruct {
    float y, z;
    float intensity;
    float angle;  // Some code uses this
    AlphaVectorStruct() : y(0), z(0), intensity(0), angle(0) {}
};
struct V3Random {};

// Vector3Randomizer stub
class Vector3Randomizer {
public:
    static const int CLASSID_MAXKNOWN = 4;
    static const int CLASSID_SOLIDBOX = 0;
    static const int CLASSID_SOLIDSPHERE = 1;
    static const int CLASSID_HOLLOWSPHERE = 2;
    static const int CLASSID_SOLIDCYLINDER = 3;
    Vector3Randomizer() {}
    Vector3Randomizer(const Vector3&) {}  // For constructors that take Vector3
    Vector3Randomizer(float, float) {}  // For constructors that take two floats
    Vector3Randomizer* Clone() { return new Vector3Randomizer(); }
    int Class_ID() const { return 0; }
};

// Specific randomizer types
class Vector3SolidBoxRandomizer : public Vector3Randomizer {
public:
    Vector3SolidBoxRandomizer(const Vector3& extents) : Vector3Randomizer(extents) {}
};

class Vector3HollowSphereRandomizer : public Vector3Randomizer {
public:
    Vector3HollowSphereRandomizer(float radius) : Vector3Randomizer() {}
};

class Vector3SolidSphereRandomizer : public Vector3Randomizer {
public:
    Vector3SolidSphereRandomizer(float radius) : Vector3Randomizer() {}
};

class Vector3SolidCylinderRandomizer : public Vector3Randomizer {
public:
    Vector3SolidCylinderRandomizer(float height, float radius) : Vector3Randomizer() {}
};
class AssetManagerClass {};
class W3DFileClass {};
class FileFactoryClass {};
class HTreeClass {
public:
    const char* Get_Name() const { return ""; } // Added for AssetInfo.cpp
};
class AnimatedSoundManager {};
class SaveLoadClass {};
class DefinitionManager {};
class DefinitionMgrClass {
public:
    static void Free_Definitions() {} // Added for AnimatedSoundOptionsDialog_Qt.cpp
};
// StringClass is defined in WWVegas (wwstring.h) when HAVE_WWVEGAS is defined
// Only define it here for Core-only builds
#ifndef HAVE_WWVEGAS
typedef std::string StringClass;  // Added for AnimatedSoundOptionsDialog_Qt.cpp
#endif
class WWFileClass {};
class ChunkIOClass {};
class FileClass {};  // Added for ViewerAssetMgr.cpp
typedef int MipCountType;  // Added for ViewerAssetMgr.cpp
// RestrictedFileDialogClass is defined in RestrictedFileDialog.h - don't redefine here
class AudibleSoundClass {
public:
    void Set_Volume(float vol) {}
    void Set_Pan(float pan) {}
    void Play() {}
    void Stop() {}
};

class WWAudioClass {
public:
    static WWAudioClass* Get_Instance() { return nullptr; }
    AudibleSoundClass* Create_Sound_Effect(const char* filename) { return nullptr; }
};
class SoundRenderObjClass {
public:
    void Add_Ref() {}  // Reference counting stub
};

// Texture and material stubs
class TextureClass {
public:
    void Add_Ref() {}  // Reference counting stub
    int Get_Width() const { return 0; }  // Added for ScreenCursor.cpp
    int Get_Height() const { return 0; }  // Added for ScreenCursor.cpp
};

class VertexMaterialClass {
public:
    VertexMaterialClass() {}
    void Set_Diffuse(float r, float g, float b) {}
    void Set_Emissive(float r, float g, float b) {}
    void Set_Specular(float r, float g, float b) {}
    void Set_Ambient(float r, float g, float b) {}
    void Add_Ref() {}  // Reference counting stub
};

// NEW_REF macro stub
#define NEW_REF(type, args) new type args

// Constants
#ifndef W3D_NAME_LEN
#define W3D_NAME_LEN 64
#endif

// Math constants
#ifndef DEG_TO_RADF
#define DEG_TO_RADF(x) ((x) * (3.14159265358979323846f / 180.0f))
#endif

// Matrix types
struct Matrix3x3 {
    float m[9];
    Matrix3x3() { for(int i = 0; i < 9; i++) m[i] = 0; }
    Matrix3x3(bool identity) {
        for(int i = 0; i < 9; i++) m[i] = 0;
        if (identity) {
            m[0] = m[4] = m[8] = 1.0f;  // Identity matrix
        }
    }
    void Rotate_Y(float angle) {}
    void Rotate_Z(float angle) {}
};

// Build quaternion stub - returns angle
inline float Build_Quaternion(Matrix3x3& m) { return 0.0f; }

// Render object constants and methods (defined after forward declaration)
// Note: RenderObjClass is forward declared above, full definition here
class RenderObjClass {
public:
    int Class_ID() const { return 0; }
    static const int CLASSID_SPHERE = 0;
    static const int CLASSID_LIGHT = 1;
    static const int CLASSID_HMODEL = 2;
    static const int CLASSID_HLOD = 3;
    void Add_Ref() {}  // Reference counting stub
    bool Is_Force_Visible() { return false; }
    void Set_Visible(bool visible) {}
    bool Is_Really_Visible() { return false; }
    int Get_LOD_Level() { return 0; }
    struct SphereClass Get_Bounding_Sphere() { return SphereClass(); }  // Implement inline
    void Prepare_LOD(const CameraClass& camera) {}
    void Set_LOD_Level(int lod_level) {}
    AABoxClass Get_Obj_Space_Bounding_Box() { return AABoxClass(); }
    void Get_Obj_Space_Bounding_Box(AABoxClass& box) { box = AABoxClass(); }  // Overload for non-Windows
    AABoxClass Get_Bounding_Box() { return AABoxClass(); }
    Vector3 Get_Position() { return Vector3(); }
    void Set_Position(const Vector3& pos) {}
    Matrix3D Get_Transform() const { return Matrix3D(); }  // Added for ScreenCursor.cpp
    const HTreeClass* Get_HTree() const { return nullptr; } // Added for AssetInfo.cpp
    const char* Get_Name() const { return ""; } // Added for MainFrm_Qt.cpp
};

// Forward declaration
struct SphereVectorChannelClass;

// Sphere render object
class SphereRenderObjClass : public RenderObjClass {
public:
    SphereVectorChannelClass* Get_Opacity_Vector_Channel() { return nullptr; }
    SphereVectorChannelClass& Get_Vector_Channel();
    void Restart_Animation() {}
};

struct SphereVectorChannelClass {
    void Set_Value(const AlphaVectorStruct& value) {}
    void Set_Key_Value(int keyIndex, const AlphaVectorStruct& value) {}
};

// Implementation of Get_Vector_Channel (defined after struct)
inline SphereVectorChannelClass& SphereRenderObjClass::Get_Vector_Channel() {
    static SphereVectorChannelClass dummy;
    return dummy;
}

// Asset manager
class WW3DAssetManager {
public:
    static WW3DAssetManager* Get_Instance() { return nullptr; }
    RenderObjIterator* Create_Render_Obj_Iterator() { return nullptr; }
    void Release_Render_Obj_Iterator(RenderObjIterator* it) { if(it) delete it; }
    RenderObjClass* Create_Render_Obj(const char* name) { return nullptr; } // Added for AssetInfo.cpp
    bool Load_3D_Assets(FileClass& file) { return false; } // Added for ViewerAssetMgr.cpp
    TextureClass* Get_Texture(const char* filename, MipCountType mip_level_count = 0) { return nullptr; } // Added for ViewerAssetMgr.cpp
};

// Render object iterator
class RenderObjIterator {
public:
    RenderObjClass* Get_Current() { return nullptr; }
    void First() {}
    bool Is_Done() { return true; }
    void Next() {}
    int Current_Item_Class_ID() { return 0; }
    const char* Current_Item_Name() { return ""; }
};

// REF_PTR macros
#define REF_PTR_RELEASE(ptr) { if(ptr) { delete ptr; ptr = nullptr; } }
#define REF_PTR_SET(dest, src) { if(dest) delete dest; dest = src; if(src) src->Add_Ref(); }

// RefRenderObjListClass is a typedef in WWVegas, not a template
// For stubs, define it as a simple class
class RefRenderObjListClass {
public:
    void Add(RenderObjClass* obj) {}
    int Get_Count() { return 0; }
    RenderObjClass* Remove_Head() { return nullptr; }
};

// RefRenderObjListIterator stub
class RefRenderObjListIterator {
public:
    RefRenderObjListIterator(RefRenderObjListClass* list) {}
    void First() {}
    void Next() {}
    bool Is_Done() { return true; }
    RenderObjClass* Peek_Obj() { return nullptr; }
    RenderObjClass* Current_Item() { return nullptr; }
};

// Note: DynamicVectorClass is defined in Utils.h for non-Windows
// We don't redefine it here to avoid conflicts

// Particle system stubs
// Forward declare ParticlePropertyStruct (defined below)
template<typename T> struct ParticlePropertyStruct;

class ParticleEmitterDefClass {
public:
    ParticleEmitterDefClass() {}
    ParticleEmitterDefClass(const ParticleEmitterDefClass&) {}
    ParticleEmitterDefClass& operator=(const ParticleEmitterDefClass&) { return *this; }
    void Set_Velocity(const Vector3&) {}
    void Set_Acceleration(const Vector3&) {}
    void Set_Burst_Size(unsigned int) {}
    void Set_Outward_Vel(float) {}
    void Set_Vel_Inherit(float) {}
    void Set_Velocity_Random(Vector3Randomizer*) {}
    void Set_Color_Keyframes(ParticlePropertyStruct<Vector3>&) {}  // Added for EmitterInstanceList.cpp
    void Set_Opacity_Keyframes(ParticlePropertyStruct<float>&) {}  // Added for EmitterInstanceList.cpp
    void Set_Size_Keyframes(ParticlePropertyStruct<float>&) {}  // Added for EmitterInstanceList.cpp
    void Set_Rotation_Keyframes(ParticlePropertyStruct<float>&, float) {}  // Added for EmitterInstanceList.cpp
    void Set_Frame_Keyframes(ParticlePropertyStruct<float>&) {}  // Added for EmitterInstanceList.cpp
    void Set_Blur_Time_Keyframes(ParticlePropertyStruct<float>&) {}  // Added for EmitterInstanceList.cpp
    void Get_Color_Keyframes(ParticlePropertyStruct<Vector3>&) const {}  // Added for EmitterInstanceList.cpp
    void Get_Opacity_Keyframes(ParticlePropertyStruct<float>&) const {}  // Added for EmitterInstanceList.cpp
    void Get_Size_Keyframes(ParticlePropertyStruct<float>&) const {}  // Added for EmitterInstanceList.cpp
};

class ParticleEmitterClass {
public:
    ParticleEmitterDefClass* Build_Definition() { return nullptr; }
    void Add_Ref() {}  // Reference counting stub
    void Set_Base_Velocity(const Vector3& value) {} // Added for EmitterInstanceList.cpp
    void Set_Acceleration(const Vector3& value) {} // Added for EmitterInstanceList.cpp
    void Set_Burst_Size(unsigned int count) {} // Added for EmitterInstanceList.cpp
    void Set_Outwards_Velocity(float value) {} // Added for EmitterInstanceList.cpp
    void Set_Velocity_Inheritance_Factor(float value) {} // Added for EmitterInstanceList.cpp
    void Set_Velocity_Randomizer(Vector3Randomizer* randomizer) {} // Added for EmitterInstanceList.cpp
    void Reset_Blur_Times(ParticlePropertyStruct<float>&) {}  // Added for EmitterInstanceList.cpp
    void Reset_Colors(ParticlePropertyStruct<Vector3>&) {}  // Added for EmitterInstanceList.cpp
    void Reset_Opacity(ParticlePropertyStruct<float>&) {}  // Added for EmitterInstanceList.cpp
    void Reset_Size(ParticlePropertyStruct<float>&) {}  // Added for EmitterInstanceList.cpp
    void Reset_Rotations(ParticlePropertyStruct<float>&, float) {}  // Added for EmitterInstanceList.cpp
    void Reset_Frames(ParticlePropertyStruct<float>&) {}  // Added for EmitterInstanceList.cpp
};

// ParticlePropertyStruct template stub
template<typename T> struct ParticlePropertyStruct {
    T value;
    T Rand;  // Randomizer value
    unsigned int NumKeyFrames;  // Number of keyframes
    T* Values;  // Array of keyframe values
    ParticlePropertyStruct() : NumKeyFrames(0), Values(nullptr) {}
    ParticlePropertyStruct(const T& v) : value(v), NumKeyFrames(0), Values(nullptr) {}
};

// WW3D namespace/class stub
namespace WW3D {
    inline void Get_Device_Resolution(int& cx, int& cy, int& bits, bool& windowed) {
        cx = cy = 1024; bits = 32; windowed = true;  // Stub values
    }
}

// DirectX/rendering stubs
enum BufferType { BUFFER_TYPE_DYNAMIC_SORTING };
typedef int dynamic_fvf_type;
const int dynamic_fvf_type_value = 0;  // Stub value

// Forward declare VertexFormatXYZNDUV2
struct VertexFormatXYZNDUV2;

class DynamicVBAccessClass {
public:
    DynamicVBAccessClass(BufferType, int, int) {}
    class WriteLockClass {
    public:
        WriteLockClass(DynamicVBAccessClass*) {}
        VertexFormatXYZNDUV2* Get_Formatted_Vertex_Array();
    };
};

class DynamicIBAccessClass {
public:
    DynamicIBAccessClass(BufferType, int) {}
    class WriteLockClass {
    public:
        WriteLockClass(DynamicIBAccessClass*) {}
        unsigned short* Get_Index_Array() { static unsigned short dummy[6]; return dummy; }
    };
};

// Define VertexFormatXYZNDUV2 after forward declaration
struct VertexFormatXYZNDUV2 {
    float x, y, z;
    float nx, ny, nz;
    unsigned int diffuse;
    float u1, v1;  // First UV set
    float u2, v2;  // Second UV set
};

// Implement Get_Formatted_Vertex_Array after VertexFormatXYZNDUV2 is defined
inline VertexFormatXYZNDUV2* DynamicVBAccessClass::WriteLockClass::Get_Formatted_Vertex_Array() {
    static VertexFormatXYZNDUV2 dummy[4];
    return dummy;
}

// Forward declarations
class DynamicVBAccessClass;
class DynamicIBAccessClass;

// DirectX wrapper stubs
namespace DX8Wrapper {
    inline void Set_Material(void*) {}
    inline void Set_Shader(void*) {}
    inline void Set_Texture(int, void*) {}  // Added for ScreenCursor.cpp
    inline void Set_Vertex_Buffer(DynamicVBAccessClass&) {}  // Added for ScreenCursor.cpp
    inline void Set_Index_Buffer(DynamicIBAccessClass&, int) {}  // Added for ScreenCursor.cpp
}

class ShaderClass {
public:
    static ShaderClass* _PresetATestBlend2DShader;  // Added for ScreenCursor.cpp - declared, defined in .cpp
};

// Shader preset stub - declare as extern, define in Utils.cpp to avoid multiple definition
extern ShaderClass* g_PresetATestBlend2DShader;

class SortingRendererClass {
public:
    static void Insert_Triangles(const SphereClass&, int, int, int, int) {}  // Added for ScreenCursor.cpp (5 args)
};

// SceneClass update constants and methods
// (Matrix3D is now defined earlier, before RenderObjClass)
#define ON_FRAME_UPDATE 0

// ASSERT macro stub
#ifndef ASSERT
#define ASSERT(x) ((void)0)
#endif

#endif // !HAVE_WWVEGAS

