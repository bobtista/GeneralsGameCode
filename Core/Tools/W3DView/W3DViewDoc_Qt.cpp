#include "W3DViewDoc_Qt.h"
#include "GraphicView_Qt.h"
#include "DataTreeView_Qt.h"
#include <cstring>

CW3DViewDoc::CW3DViewDoc(QObject* parent) :
	QObject(parent),
	m_pC2DCamera(nullptr),
	m_pCBackObjectCamera(nullptr),
	m_pC2DScene(nullptr),
	m_pCursorScene(nullptr),
	m_pCScene(nullptr),
	m_pCBackObjectScene(nullptr),
	m_pCSceneLight(nullptr),
	m_pCRenderObj(nullptr),
	m_pCAnimation(nullptr),
	m_pGraphicView(nullptr),
	m_pDataTreeView(nullptr),
	m_bFogEnabled(false),
	m_bAnimBlend(false),
	m_IsInitialized(false)
{
	InitializeMembers();
}

CW3DViewDoc::~CW3DViewDoc()
{
	CleanupResources();
}

void CW3DViewDoc::InitializeMembers()
{
	m_backgroundColor = Vector3(0.0f, 0.0f, 0.0f);
	m_stringBackgroundBMP.clear();
	m_stringBackgroundObject.clear();
}

bool CW3DViewDoc::OnNewDocument()
{
	CleanupResources();
	InitScene();
	return true;
}

bool CW3DViewDoc::OnOpenDocument(const QString& pathName)
{
	if (pathName.isEmpty())
		return false;
	
	LoadAssetsFromFile(pathName);
	return true;
}

void CW3DViewDoc::Serialize()
{
}

void CW3DViewDoc::InitScene()
{
	m_IsInitialized = true;
	emit SceneUpdated();
}

void CW3DViewDoc::LoadAssetsFromFile(const QString& pathName)
{
	Q_UNUSED(pathName);
	emit DocumentChanged();
}

void CW3DViewDoc::CleanupResources()
{
	m_pC2DCamera = nullptr;
	m_pCBackObjectCamera = nullptr;
	m_pC2DScene = nullptr;
	m_pCursorScene = nullptr;
	m_pCScene = nullptr;
	m_pCBackObjectScene = nullptr;
	m_pCSceneLight = nullptr;
	m_pCRenderObj = nullptr;
	m_pCAnimation = nullptr;
	m_IsInitialized = false;
}

void CW3DViewDoc::Reload_Displayed_Object()
{
	emit SceneUpdated();
}

void CW3DViewDoc::Display_Emitter(ParticleEmitterClass* pemitter, bool use_global_reset_flag, bool allow_reset)
{
	Q_UNUSED(pemitter);
	Q_UNUSED(use_global_reset_flag);
	Q_UNUSED(allow_reset);
	emit SceneUpdated();
}

void CW3DViewDoc::DisplayObject(RenderObjClass* pCModel, bool use_global_reset_flag, bool allow_reset, bool add_ghost)
{
	Q_UNUSED(pCModel);
	Q_UNUSED(use_global_reset_flag);
	Q_UNUSED(allow_reset);
	Q_UNUSED(add_ghost);
	m_pCRenderObj = pCModel;
	emit SceneUpdated();
}

bool CW3DViewDoc::SaveSettings(const QString& filename, unsigned int dwSettingsMask)
{
	Q_UNUSED(filename);
	Q_UNUSED(dwSettingsMask);
	return true;
}

bool CW3DViewDoc::LoadSettings(const QString& fileName)
{
	Q_UNUSED(fileName);
	return true;
}

CGraphicView* CW3DViewDoc::GetGraphicView()
{
	return m_pGraphicView;
}

CDataTreeView* CW3DViewDoc::GetDataTreeView()
{
	return m_pDataTreeView;
}

void CW3DViewDoc::SetViews(CGraphicView* graphicView, CDataTreeView* dataTreeView)
{
	m_pGraphicView = graphicView;
	m_pDataTreeView = dataTreeView;
}

void CW3DViewDoc::ResetAnimation()
{
	emit SceneUpdated();
}

void CW3DViewDoc::StepAnimation(int frame_inc)
{
	Q_UNUSED(frame_inc);
	emit SceneUpdated();
}

void CW3DViewDoc::PlayAnimation(RenderObjClass* pobj, const QString& panim_name, bool use_global_reset_flag, bool allow_reset)
{
	Q_UNUSED(pobj);
	Q_UNUSED(panim_name);
	Q_UNUSED(use_global_reset_flag);
	Q_UNUSED(allow_reset);
	emit SceneUpdated();
}

void CW3DViewDoc::UpdateFrame(float time_slice)
{
	Q_UNUSED(time_slice);
	emit SceneUpdated();
}

void CW3DViewDoc::SetBackgroundColor(const class Vector3& backgroundColor)
{
	m_backgroundColor = backgroundColor;
	emit SceneUpdated();
}

void CW3DViewDoc::SetBackgroundBMP(const QString& pszBackgroundBMP)
{
	m_stringBackgroundBMP = pszBackgroundBMP;
	emit SceneUpdated();
}

void CW3DViewDoc::SetBackgroundObject(const QString& pszBackgroundObjectName)
{
	m_stringBackgroundObject = pszBackgroundObjectName;
	emit SceneUpdated();
}

void CW3DViewDoc::EnableFog(bool enable)
{
	m_bFogEnabled = enable;
	emit SceneUpdated();
}

void CW3DViewDoc::Remove_Object_From_Scene(RenderObjClass* prender_obj)
{
	Q_UNUSED(prender_obj);
	emit SceneUpdated();
}

void CW3DViewDoc::Show_Cursor(bool onoff)
{
	Q_UNUSED(onoff);
}

void CW3DViewDoc::Set_Cursor(const QString& resource_name)
{
	Q_UNUSED(resource_name);
}

bool CW3DViewDoc::Is_Cursor_Shown() const
{
	return false;
}

void CW3DViewDoc::Create_Cursor()
{
}

const HTreeClass* CW3DViewDoc::Get_Current_HTree() const
{
	return nullptr;
}

