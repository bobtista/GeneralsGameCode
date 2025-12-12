#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QToolBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QFileDialog>

class CGraphicView;
class CDataTreeView;
class CW3DViewDoc;

enum ASSET_TYPE {
	ASSET_TYPE_UNKNOWN,
	ASSET_TYPE_MESH,
	ASSET_TYPE_ANIMATION,
	ASSET_TYPE_AGGREGATE,
	ASSET_TYPE_HLOD,
	ASSET_TYPE_EMITTER,
	ASSET_TYPE_SPHERE,
	ASSET_TYPE_RING,
	ASSET_TYPE_SOUND
};

class CMainFrame : public QMainWindow
{
	Q_OBJECT

public:
	explicit CMainFrame(QWidget* parent = nullptr);
	~CMainFrame();

	CGraphicView* GetPane(int iRow, int iCol) const;
	CGraphicView* GetGraphicView() const { return m_graphicView; }
	CW3DViewDoc* GetActiveDocument() const { return m_pDoc; }
	void ShowObjectProperties(void);
	void OnSelectionChanged(ASSET_TYPE newAssetType);
	void Update_Frame_Time(unsigned long milliseconds);
	void UpdatePolygonCount(int iPolygons);
	void Update_Particle_Count(int particles);
	void UpdateCameraDistance(float cameraDistance);
	void UpdateFrameCount(int iCurrentFrame, int iTotalFrames, float frame_rate);
	void RestoreOriginalSize(void);
	void Select_Device(bool show_dlg = true);
	QMenu* Get_Emitters_List_Menu(void) const { return m_emittersSubMenu; }
	void Update_Emitters_List(void);

protected:
	void closeEvent(QCloseEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void showEvent(QShowEvent* event) override;

private slots:
	void onObjectProperties();
	void onLodGenerate();
	void onFileOpen();
	void onAniSpeed();
	void onAniStop();
	void onAniStart();
	void onAniPause();
	void onCameraBack();
	void onCameraBottom();
	void onCameraFront();
	void onCameraLeft();
	void onCameraReset();
	void onCameraRight();
	void onCameraTop();
	void onObjectRotateZ();
	void onObjectRotateY();
	void onObjectRotateX();
	void onLightAmbient();
	void onLightScene();
	void onBackgroundColor();
	void onBackgroundBMP();
	void onSaveSettings();
	void onLoadSettings();
	void onLODSetSwitch();
	void onLODSave();
	void onLODSaveAll();
	void onBackgroundObject();
	void onViewAnimationBar();
	void onViewObjectBar();
	void onAniStepFwd();
	void onAniStepBkwd();
	void onObjectReset();
	void onCameraAllowRotateX();
	void onCameraAllowRotateY();
	void onCameraAllowRotateZ();
	void onViewFullscreen();
	void onDeviceChange();
	void onCreateEmitter();
	void onEditEmitter();
	void onSaveEmitter();
	void onBoneAutoAssign();
	void onBoneManagement();
	void onSaveAggregate();
	void onCameraAnimate();
	void onCameraResetOnLoad();
	void onObjectRotateYBack();
	void onObjectRotateZBack();
	void onLightRotateY();
	void onLightRotateYBack();
	void onLightRotateZ();
	void onLightRotateZBack();
	void onDecLight();
	void onIncLight();
	void onDecAmbientLight();
	void onIncAmbientLight();
	void onMakeAggregate();
	void onRenameAggregate();
	void onLODRecordScreenArea();
	void onLODIncludeNull();
	void onLodPrevLevel();
	void onLodNextLevel();
	void onLodAutoswitch();
	void onMakeMovie();
	void onSaveScreenshot();
	void onSlideshowDown();
	void onSlideshowUp();
	void onAdvancedAnim();
	void onCameraSettings();
	void onCopyScreenSize();
	void onListMissingTextures();
	void onCopyAssets();
	void onLightingExpose();
	void onTexturePath();
	void onChangeResolution();
	void onCreateSphere();
	void onCreateRing();
	void onEditPrimitive();
	void onExportPrimitive();
	void onKillSceneLight();
	void onPrelitMultipass();
	void onPrelitMultitex();
	void onPrelitVertex();
	void onAddToLineup();
	void onImportFacialAnims();
	void onRestrictAnims();
	void onBindSubobjectLod();
	void onSetCameraDistance();
	void onObjectAlternateMaterials();
	void onCreateSoundObject();
	void onEditSoundObject();
	void onExportSoundObj();
	void onWireframeMode();
	void onBackgroundFog();
	void onScaleEmitter();
	void onToggleSorting();
	void onCameraBonePosX();
	void onViewPatchGapFill();
	void onViewSubdivision1();
	void onViewSubdivision2();
	void onViewSubdivision3();
	void onViewSubdivision4();
	void onViewSubdivision5();
	void onViewSubdivision6();
	void onViewSubdivision7();
	void onViewSubdivision8();
	void onMungeSortOnLoad();
	void onEnableGammaCorrection();
	void onSetGamma();
	void onEditAnimatedSoundsOptions();

private:
	void Restore_Window_State(void);
	void setupMenuBar();
	void setupToolBars();
	void setupStatusBar();
	void setupSplitter();

	QSplitter* m_splitter;
	QToolBar* m_objectToolBar;
	QToolBar* m_animationToolBar;
	QStatusBar* m_statusBar;
	QMenu* m_emittersSubMenu;
	
	ASSET_TYPE m_currentAssetType;
	bool m_bShowAnimationBar;
	QRect m_origRect;
	bool m_bInitialized;
	
	CGraphicView* m_graphicView;
	CDataTreeView* m_dataTreeView;
	CW3DViewDoc* m_pDoc;
};

