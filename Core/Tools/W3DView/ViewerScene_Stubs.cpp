// TheSuperHackers @refactor bobtista 01/01/2025 Stub implementations for ViewerSceneClass on non-Windows platforms

#ifndef _WIN32

#include "ViewerScene.h"
#include <QtCore/QtGlobal>  // For Q_UNUSED

bool ViewerSceneClass::Can_Line_Up (RenderObjClass *obj)
{
	Q_UNUSED(obj);
	return false;
}

bool ViewerSceneClass::Can_Line_Up (int class_id)
{
	Q_UNUSED(class_id);
	return false;
}

#endif

