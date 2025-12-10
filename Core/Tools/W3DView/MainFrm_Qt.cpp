#include "MainFrm_Qt.h"
#include "GraphicView_Qt.h"
#include "DataTreeView_Qt.h"
#include "DeviceSelectionDialog_Qt.h"
#include "ResolutionDialog_Qt.h"
#include "ScaleDialog_Qt.h"
#include "GammaDialog_Qt.h"
#include "SaveSettingsDialog_Qt.h"
#include "CameraDistanceDialog_Qt.h"
#include "AmbientLightDialog_Qt.h"
#include "BackgroundColorDialog_Qt.h"
#include "BackgroundBMPDialog_Qt.h"
#include "BackgroundObjectDialog_Qt.h"
#include "CameraSettingsDialog_Qt.h"
#include "TexturePathDialog_Qt.h"
#include "AssetPropertySheet_Qt.h"
#include "EmitterPropertySheet_Qt.h"
#include "AggregateNameDialog_Qt.h"
#include "AddToLineupDialog_Qt.h"
#include "AnimationSpeed_Qt.h"
#include "PlaySoundDialog_Qt.h"
#include "AnimatedSoundOptionsDialog_Qt.h"
#include "ParticleSizeDialog_Qt.h"
#include "ParticleFrameKeyDialog_Qt.h"
#include "ParticleRotationKeyDialog_Qt.h"
#include "ParticleBlurTimeKeyDialog_Qt.h"
#include "OpacitySettingsDialog_Qt.h"
#include "OpacityVectorDialog_Qt.h"
#include "VolumeRandomDialog_Qt.h"
#include "SoundEditDialog_Qt.h"
#include "EditLODDialog_Qt.h"
#include "SceneLightDialog_Qt.h"
#include "BoneMgrDialog_Qt.h"
#include "ColorSelectionDialog_Qt.h"
#include "ColorPickerDialogClass_Qt.h"
#include "W3DViewDoc_Qt.h"
#include "Utils.h"
#include "GraphicView_Qt.h"
#include "DataTreeView_Qt.h"
#include <QCloseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>

CMainFrame::CMainFrame(QWidget* parent) :
	QMainWindow(parent),
	m_splitter(nullptr),
	m_objectToolBar(nullptr),
	m_animationToolBar(nullptr),
	m_statusBar(nullptr),
	m_emittersSubMenu(nullptr),
	m_currentAssetType(ASSET_TYPE_UNKNOWN),
	m_bShowAnimationBar(false),
	m_bInitialized(false),
	m_graphicView(nullptr),
	m_dataTreeView(nullptr),
	m_pDoc(nullptr)
{
	setWindowTitle("W3DView");
	resize(800, 600);
	
	m_pDoc = new CW3DViewDoc(this);
	m_pDoc->OnNewDocument();
	
	setupMenuBar();
	setupToolBars();
	setupStatusBar();
	setupSplitter();
	
	m_origRect = geometry();
	Restore_Window_State();
}

CMainFrame::~CMainFrame()
{
}

void CMainFrame::setupMenuBar()
{
	QMenuBar* menuBar = this->menuBar();
	
	QMenu* fileMenu = menuBar->addMenu("&File");
	fileMenu->addAction("&Open", this, &CMainFrame::onFileOpen, QKeySequence::Open);
	fileMenu->addSeparator();
	fileMenu->addAction("E&xit", this, &QMainWindow::close, QKeySequence::Quit);
	
	QMenu* viewMenu = menuBar->addMenu("&View");
	viewMenu->addAction("&Object Bar", this, &CMainFrame::onViewObjectBar);
	viewMenu->addAction("&Animation Bar", this, &CMainFrame::onViewAnimationBar);
	viewMenu->addAction("&Fullscreen", this, &CMainFrame::onViewFullscreen);
	
	QMenu* objectMenu = menuBar->addMenu("&Object");
	objectMenu->addAction("&Properties", this, &CMainFrame::onObjectProperties);
	objectMenu->addSeparator();
	objectMenu->addAction("Rotate &X", this, &CMainFrame::onObjectRotateX);
	objectMenu->addAction("Rotate &Y", this, &CMainFrame::onObjectRotateY);
	objectMenu->addAction("Rotate &Z", this, &CMainFrame::onObjectRotateZ);
	objectMenu->addAction("&Reset", this, &CMainFrame::onObjectReset);
	
	QMenu* cameraMenu = menuBar->addMenu("&Camera");
	cameraMenu->addAction("&Front", this, &CMainFrame::onCameraFront);
	cameraMenu->addAction("&Back", this, &CMainFrame::onCameraBack);
	cameraMenu->addAction("&Left", this, &CMainFrame::onCameraLeft);
	cameraMenu->addAction("&Right", this, &CMainFrame::onCameraRight);
	cameraMenu->addAction("&Top", this, &CMainFrame::onCameraTop);
	cameraMenu->addAction("&Bottom", this, &CMainFrame::onCameraBottom);
	cameraMenu->addSeparator();
	cameraMenu->addAction("&Reset", this, &CMainFrame::onCameraReset);
	cameraMenu->addAction("&Settings", this, &CMainFrame::onCameraSettings);
	
	QMenu* animationMenu = menuBar->addMenu("&Animation");
	animationMenu->addAction("&Start", this, &CMainFrame::onAniStart);
	animationMenu->addAction("&Stop", this, &CMainFrame::onAniStop);
	animationMenu->addAction("&Pause", this, &CMainFrame::onAniPause);
	animationMenu->addAction("&Speed", this, &CMainFrame::onAniSpeed);
	animationMenu->addSeparator();
	animationMenu->addAction("Step &Forward", this, &CMainFrame::onAniStepFwd);
	animationMenu->addAction("Step &Backward", this, &CMainFrame::onAniStepBkwd);
	
	QMenu* lightMenu = menuBar->addMenu("&Light");
	lightMenu->addAction("&Ambient", this, &CMainFrame::onLightAmbient);
	lightMenu->addAction("&Scene", this, &CMainFrame::onLightScene);
	
	QMenu* backgroundMenu = menuBar->addMenu("&Background");
	backgroundMenu->addAction("&Color", this, &CMainFrame::onBackgroundColor);
	backgroundMenu->addAction("&BMP", this, &CMainFrame::onBackgroundBMP);
	backgroundMenu->addAction("&Object", this, &CMainFrame::onBackgroundObject);
	
	QMenu* settingsMenu = menuBar->addMenu("&Settings");
	settingsMenu->addAction("&Save", this, &CMainFrame::onSaveSettings);
	settingsMenu->addAction("&Load", this, &CMainFrame::onLoadSettings);
	
	QMenu* helpMenu = menuBar->addMenu("&Help");
	helpMenu->addAction("&About", this, [this]() {
		QMessageBox::about(this, "About W3DView", "W3DView - 3D Model Viewer");
	});
}

void CMainFrame::setupToolBars()
{
	m_objectToolBar = addToolBar("Object");
	m_objectToolBar->setObjectName("ObjectToolBar");
	
	m_animationToolBar = addToolBar("Animation");
	m_animationToolBar->setObjectName("AnimationToolBar");
	m_animationToolBar->setVisible(false);
}

void CMainFrame::setupStatusBar()
{
	m_statusBar = statusBar();
	m_statusBar->showMessage("Ready");
}

void CMainFrame::setupSplitter()
{
	QWidget* centralWidget = new QWidget(this);
	setCentralWidget(centralWidget);
	
	QVBoxLayout* layout = new QVBoxLayout(centralWidget);
	layout->setContentsMargins(0, 0, 0, 0);
	
	m_splitter = new QSplitter(Qt::Horizontal, centralWidget);
	layout->addWidget(m_splitter);
	
	m_dataTreeView = new CDataTreeView(m_splitter);
	m_graphicView = new CGraphicView(m_splitter);
	
	m_splitter->addWidget(m_dataTreeView);
	m_splitter->addWidget(m_graphicView);
	
	m_splitter->setStretchFactor(0, 0);
	m_splitter->setStretchFactor(1, 1);
	m_splitter->setSizes({200, 600});
	
	if (m_pDoc) {
		m_pDoc->SetViews(m_graphicView, m_dataTreeView);
	}
}

void CMainFrame::Restore_Window_State(void)
{
	m_bInitialized = true;
}

CGraphicView* CMainFrame::GetPane(int iRow, int iCol) const
{
	if (iRow == 0 && iCol == 1) {
		return m_graphicView;
	}
	return nullptr;
}

void CMainFrame::ShowObjectProperties(void)
{
	onObjectProperties();
}

void CMainFrame::OnSelectionChanged(ASSET_TYPE newAssetType)
{
	m_currentAssetType = newAssetType;
}

void CMainFrame::Update_Frame_Time(unsigned long milliseconds)
{
	if (m_statusBar) {
		m_statusBar->showMessage(QString("Frame Time: %1 ms").arg(milliseconds));
	}
}

void CMainFrame::UpdatePolygonCount(int iPolygons)
{
	if (m_statusBar) {
		QString status = m_statusBar->currentMessage();
		status += QString(" | Polygons: %1").arg(iPolygons);
		m_statusBar->showMessage(status);
	}
}

void CMainFrame::Update_Particle_Count(int particles)
{
	if (m_statusBar) {
		QString status = m_statusBar->currentMessage();
		status += QString(" | Particles: %1").arg(particles);
		m_statusBar->showMessage(status);
	}
}

void CMainFrame::UpdateCameraDistance(float cameraDistance)
{
	if (m_statusBar) {
		QString status = m_statusBar->currentMessage();
		status += QString(" | Camera Distance: %1").arg(cameraDistance);
		m_statusBar->showMessage(status);
	}
}

void CMainFrame::UpdateFrameCount(int iCurrentFrame, int iTotalFrames, float frame_rate)
{
	if (m_statusBar) {
		m_statusBar->showMessage(QString("Frame: %1/%2 (%.2f fps)").arg(iCurrentFrame).arg(iTotalFrames).arg(frame_rate));
	}
}

void CMainFrame::RestoreOriginalSize(void)
{
	setGeometry(m_origRect);
}

void CMainFrame::Select_Device(bool show_dlg)
{
	if (show_dlg) {
		DeviceSelectionDialog dlg(this);
		dlg.exec();
	}
}

void CMainFrame::Update_Emitters_List(void)
{
}

void CMainFrame::closeEvent(QCloseEvent* event)
{
	QMainWindow::closeEvent(event);
}

void CMainFrame::resizeEvent(QResizeEvent* event)
{
	QMainWindow::resizeEvent(event);
}

void CMainFrame::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);
}

void CMainFrame::onObjectProperties() 
{ 
        CAssetPropertySheet dlg("", this);
	dlg.exec();
}
void CMainFrame::onLodGenerate() { }
void CMainFrame::onFileOpen() { QFileDialog::getOpenFileName(this, "Open File", "", "W3D Files (*.w3d)"); }
void CMainFrame::onAniSpeed()
{
    CAnimationSpeed dlg(this);
    dlg.exec();
}
void CMainFrame::onAniStop() { }
void CMainFrame::onAniStart() { }
void CMainFrame::onAniPause() { }
void CMainFrame::onCameraBack() { }
void CMainFrame::onCameraBottom() { }
void CMainFrame::onCameraFront() { }
void CMainFrame::onCameraLeft() { }
void CMainFrame::onCameraReset() { }
void CMainFrame::onCameraRight() { }
void CMainFrame::onCameraTop() { }
void CMainFrame::onObjectRotateZ() { }
void CMainFrame::onObjectRotateY() { }
void CMainFrame::onObjectRotateX() { }
void CMainFrame::onLightAmbient() 
{ 
	CAmbientLightDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
	}
}
void CMainFrame::onLightScene()
{
    CSceneLightDialog dlg(this);
    dlg.exec();
}
void CMainFrame::onBackgroundColor() 
{ 
	CBackgroundColorDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
	}
}

void CMainFrame::onBackgroundBMP() 
{ 
	CBackgroundBMPDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
	}
}
void CMainFrame::onSaveSettings() 
{ 
	CSaveSettingsDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
	}
}
void CMainFrame::onLoadSettings() 
{ 
	QString filename = QFileDialog::getOpenFileName(this, "Load Settings", "", "Settings Files (*.ini)");
	if (!filename.isEmpty()) {
	}
}
void CMainFrame::onLODSetSwitch()
{
    CEditLODDialog dlg(this);
    dlg.exec();
}
void CMainFrame::onLODSave() { }
void CMainFrame::onLODSaveAll() { }
void CMainFrame::onBackgroundObject() 
{ 
	CBackgroundObjectDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
	}
}
void CMainFrame::onViewAnimationBar() { m_bShowAnimationBar = !m_bShowAnimationBar; m_animationToolBar->setVisible(m_bShowAnimationBar); }
void CMainFrame::onViewObjectBar() { m_objectToolBar->setVisible(!m_objectToolBar->isVisible()); }
void CMainFrame::onAniStepFwd() { }
void CMainFrame::onAniStepBkwd() { }
void CMainFrame::onObjectReset() { }
void CMainFrame::onCameraAllowRotateX() { }
void CMainFrame::onCameraAllowRotateY() { }
void CMainFrame::onCameraAllowRotateZ() { }
void CMainFrame::onViewFullscreen() { setWindowState(windowState() ^ Qt::WindowFullScreen); }
void CMainFrame::onDeviceChange() { }
void CMainFrame::onCreateEmitter() { }
void CMainFrame::onEditEmitter() 
{ 
	CEmitterPropertySheet dlg(this);
	dlg.exec();
}
void CMainFrame::onSaveEmitter() { }
void CMainFrame::onBoneAutoAssign() { }
void CMainFrame::onBoneManagement()
{
    CW3DViewDoc* pDoc = ::GetCurrentDocument();
    if (pDoc)
    {
        RenderObjClass* prender_obj = pDoc->GetDisplayedObject();
        if (prender_obj)
        {
            BoneMgrDialogClass dlg(prender_obj, this);
            dlg.exec();
            Update_Emitters_List();
        }
    }
}
void CMainFrame::onSaveAggregate() { }
void CMainFrame::onCameraAnimate() { }
void CMainFrame::onCameraResetOnLoad() { }
void CMainFrame::onObjectRotateYBack() { }
void CMainFrame::onObjectRotateZBack() { }
void CMainFrame::onLightRotateY() { }
void CMainFrame::onLightRotateYBack() { }
void CMainFrame::onLightRotateZ() { }
void CMainFrame::onLightRotateZBack() { }
void CMainFrame::onDecLight() { }
void CMainFrame::onIncLight() { }
void CMainFrame::onDecAmbientLight() { }
void CMainFrame::onIncAmbientLight() { }
void CMainFrame::onMakeAggregate()
{
    AggregateNameDialogClass dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
    }
}

void CMainFrame::onRenameAggregate()
{
    CW3DViewDoc* pDoc = ::GetCurrentDocument();
    if (pDoc)
    {
        RenderObjClass* prender_obj = pDoc->GetDisplayedObject();
        if (prender_obj != nullptr)
        {
            QString old_name = QString::fromLocal8Bit(prender_obj->Get_Name());
            AggregateNameDialogClass dialog(old_name, this);
            if (dialog.exec() == QDialog::Accepted)
            {
            }
        }
    }
}
void CMainFrame::onLODRecordScreenArea() { }
void CMainFrame::onLODIncludeNull() { }
void CMainFrame::onLodPrevLevel() { }
void CMainFrame::onLodNextLevel() { }
void CMainFrame::onLodAutoswitch() { }
void CMainFrame::onMakeMovie() { }
void CMainFrame::onSaveScreenshot() { }
void CMainFrame::onSlideshowDown() { }
void CMainFrame::onSlideshowUp() { }
void CMainFrame::onAdvancedAnim() { }
void CMainFrame::onCameraSettings() 
{ 
	CCameraSettingsDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
	}
}
void CMainFrame::onCopyScreenSize() { }
void CMainFrame::onListMissingTextures() { }
void CMainFrame::onCopyAssets() { }
void CMainFrame::onLightingExpose() { }
void CMainFrame::onTexturePath() 
{ 
	TexturePathDialogClass dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
	}
}
void CMainFrame::onChangeResolution() 
{ 
	ResolutionDialogClass dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
	}
}
void CMainFrame::onCreateSphere() { }
void CMainFrame::onCreateRing() { }
void CMainFrame::onEditPrimitive() { }
void CMainFrame::onExportPrimitive() { }
void CMainFrame::onKillSceneLight() { }
void CMainFrame::onPrelitMultipass() { }
void CMainFrame::onPrelitMultitex() { }
void CMainFrame::onPrelitVertex() { }
void CMainFrame::onAddToLineup()
{
    CW3DViewDoc* pDoc = ::GetCurrentDocument();
    ViewerSceneClass* pScene = nullptr;
    if (pDoc)
    {
        pScene = pDoc->GetScene();
    }
    CAddToLineupDialog dlg(pScene, this);
    if (dlg.exec() == QDialog::Accepted)
    {
    }
}
void CMainFrame::onImportFacialAnims() { }
void CMainFrame::onRestrictAnims() { }
void CMainFrame::onBindSubobjectLod() { }
void CMainFrame::onSetCameraDistance() 
{ 
	CameraDistanceDialogClass dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
	}
}
void CMainFrame::onObjectAlternateMaterials() { }
void CMainFrame::onCreateSoundObject() { }
void CMainFrame::onEditSoundObject()
{
    SoundEditDialogClass dlg(this);
    dlg.exec();
}
void CMainFrame::onExportSoundObj() { }
void CMainFrame::onWireframeMode() { }
void CMainFrame::onBackgroundFog() { }
void CMainFrame::onScaleEmitter() 
{ 
	ScaleDialogClass dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
	}
}
void CMainFrame::onToggleSorting() { }
void CMainFrame::onCameraBonePosX() { }
void CMainFrame::onViewPatchGapFill() { }
void CMainFrame::onViewSubdivision1() { }
void CMainFrame::onViewSubdivision2() { }
void CMainFrame::onViewSubdivision3() { }
void CMainFrame::onViewSubdivision4() { }
void CMainFrame::onViewSubdivision5() { }
void CMainFrame::onViewSubdivision6() { }
void CMainFrame::onViewSubdivision7() { }
void CMainFrame::onViewSubdivision8() { }
void CMainFrame::onMungeSortOnLoad() { }
void CMainFrame::onEnableGammaCorrection() { }
void CMainFrame::onSetGamma() 
{ 
	GammaDialogClass dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
	}
}
void CMainFrame::onEditAnimatedSoundsOptions()
{
    AnimatedSoundOptionsDialogClass dlg(this);
    dlg.exec();
}

