#pragma once

// TheSuperHackers @refactor bobtista 01/01/2025 Stub header for game engine types on non-Windows platforms

#ifndef _WIN32

#include <string>  // For std::string and StringClass

// Vector3 stub
struct Vector3 {
    float x, y, z;
    float X, Y, Z;  // Some code uses uppercase
    Vector3() : x(0), y(0), z(0), X(0), Y(0), Z(0) {}
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_), X(x_), Y(y_), Z(z_) {}
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
};

// Forward declarations
class RenderObjClass;
class RenderObjIterator;
class SphereRenderObjClass;
struct SphereVectorChannelClass;

// Game engine class stubs
class SceneClass {
public:
    void Get_Fog_Range(float* near_range, float* far_range) { if(near_range) *near_range = 0; if(far_range) *far_range = 0; }
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
typedef std::string StringClass;  // Added for AnimatedSoundOptionsDialog_Qt.cpp
class WWFileClass {};
class ChunkIOClass {};
class RestrictedFileDialogClass {};
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
    struct SphereClass Get_Bounding_Sphere();
    void Prepare_LOD(const CameraClass& camera) {}
    void Set_LOD_Level(int lod_level) {}
    AABoxClass Get_Obj_Space_Bounding_Box() { return AABoxClass(); }
    void Get_Obj_Space_Bounding_Box(AABoxClass& box) { box = AABoxClass(); }  // Overload for non-Windows
    AABoxClass Get_Bounding_Box() { return AABoxClass(); }
    Vector3 Get_Position() { return Vector3(); }
    void Set_Position(const Vector3& pos) {}
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

template<typename T> class RefRenderObjListClass {
public:
    void Add(T* obj) {}
    int Get_Count() { return 0; }
    T* Remove_Head() { return nullptr; }
};

// RefRenderObjListIterator stub
template<typename T> class RefRenderObjListIterator {
public:
    RefRenderObjListIterator(RefRenderObjListClass<T>* list) {}
    void First() {}
    void Next() {}
    bool Is_Done() { return true; }
    T* Peek_Obj() { return nullptr; }
    RenderObjClass* Current_Item() { return nullptr; }
};

// Note: DynamicVectorClass is defined in Utils.h for non-Windows
// We don't redefine it here to avoid conflicts

// Particle system stubs
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

// ASSERT macro stub
#ifndef ASSERT
#define ASSERT(x) ((void)0)
#endif

#endif  // _WIN32

