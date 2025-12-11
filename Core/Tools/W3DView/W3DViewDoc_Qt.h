#pragma once

#include <QObject>
#include <QString>
#include "GameEngineStubs.h"  // Include on all platforms for Vector3

class CameraClass;
class SceneClass;
class ViewerSceneClass;
class LightClass;
class RenderObjClass;
class HAnimClass;
class HTreeClass;
class ParticleEmitterClass;
class CGraphicView;
class CDataTreeView;

class CW3DViewDoc : public QObject
{
	Q_OBJECT

public:
	explicit CW3DViewDoc(QObject* parent = nullptr);
	~CW3DViewDoc();

	bool OnNewDocument();
	bool OnOpenDocument(const QString& pathName);
	void Serialize();

	CameraClass* Get2DCamera() const { return m_pC2DCamera; }
	CameraClass* GetBackObjectCamera() const { return m_pCBackObjectCamera; }
	SceneClass* Get2DScene() const { return m_pC2DScene; }
	SceneClass* GetCursorScene() const { return m_pCursorScene; }
	ViewerSceneClass* GetScene() const { return m_pCScene; }
	SceneClass* GetBackObjectScene() const { return m_pCBackObjectScene; }
	LightClass* GetSceneLight() const { return m_pCSceneLight; }
	RenderObjClass* GetDisplayedObject() const { return m_pCRenderObj; }
	HAnimClass* GetCurrentAnimation() const { return m_pCAnimation; }
	const HTreeClass* Get_Current_HTree() const;

	void InitScene();
	void LoadAssetsFromFile(const QString& pathName);
	void CleanupResources();
	bool Is_Initialized() { return m_IsInitialized; }

	void Reload_Displayed_Object();
	void Display_Emitter(ParticleEmitterClass* pemitter = nullptr, bool use_global_reset_flag = true, bool allow_reset = true);
	void DisplayObject(RenderObjClass* pCModel = nullptr, bool use_global_reset_flag = true, bool allow_reset = true, bool add_ghost = false);
	bool SaveSettings(const QString& filename, unsigned int dwSettingsMask);
	bool LoadSettings(const QString& fileName);
	CGraphicView* GetGraphicView();
	CDataTreeView* GetDataTreeView();
	void SetViews(CGraphicView* graphicView, CDataTreeView* dataTreeView);

	void ResetAnimation();
	void StepAnimation(int frame_inc = 1);
	void PlayAnimation(RenderObjClass* pobj, const QString& panim_name = QString(), bool use_global_reset_flag = true, bool allow_reset = true);
	void UpdateFrame(float time_slice);
	void SetAnimationBlend(bool bBlend) { m_bAnimBlend = bBlend; }
	bool GetAnimationBlend() const { return m_bAnimBlend; }
	bool GetChannelQCompression() const { return m_bCompress_channel_Q; }
	void SetChannelQCompression(bool bCompress) { m_bCompress_channel_Q = bCompress; }

	void SetBackgroundColor(const Vector3& backgroundColor);
	const Vector3& GetBackgroundColor() const { return m_backgroundColor; }

	void SetBackgroundBMP(const QString& pszBackgroundBMP);
	const QString& GetBackgroundBMP() const { return m_stringBackgroundBMP; }

	void SetBackgroundObject(const QString& pszBackgroundObjectName);
	const QString& GetBackgroundObjectName() const { return m_stringBackgroundObject; }

	void EnableFog(bool enable = true);
	bool IsFogEnabled() const { return m_bFogEnabled; }

	void Remove_Object_From_Scene(RenderObjClass* prender_obj = nullptr);

	void Show_Cursor(bool onoff);
	void Set_Cursor(const QString& resource_name);
	bool Is_Cursor_Shown() const;
	void Create_Cursor();

signals:
	void DocumentChanged();
	void SceneUpdated();

private:
	void InitializeMembers();

	CameraClass* m_pC2DCamera;
	CameraClass* m_pCBackObjectCamera;
	SceneClass* m_pC2DScene;
	SceneClass* m_pCursorScene;
	ViewerSceneClass* m_pCScene;
	SceneClass* m_pCBackObjectScene;
	LightClass* m_pCSceneLight;
	RenderObjClass* m_pCRenderObj;
	HAnimClass* m_pCAnimation;
	
	CGraphicView* m_pGraphicView;
	CDataTreeView* m_pDataTreeView;

	Vector3 m_backgroundColor;
	QString m_stringBackgroundBMP;
	QString m_stringBackgroundObject;
	bool m_bCompress_channel_Q;
	
	bool m_bFogEnabled;
	bool m_bAnimBlend;
	bool m_IsInitialized;
};



