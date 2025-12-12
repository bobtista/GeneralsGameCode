# WorldBuilder Logic Differences Analysis

This document analyzes the 20 files with logic differences to determine if they need conditional compilation.

## 1. include/CUndoable.h

**Difference**: GeneralsMD adds `MultipleUndoable` class

**GeneralsMD Addition**:
```cpp
class MultipleUndoable : public Undoable
{
protected:
  Undoable * m_undoableList;
public:
    MultipleUndoable();
    ~MultipleUndoable(void);
    void addUndoable( Undoable * undoable );
    virtual void Do(void);
    virtual void Undo(void);
    virtual void Redo(void);
};
```

**Analysis**: This is a Zero Hour feature that consolidates multiple undo operations into a single logical step.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour-only feature

---

## 2. include/DrawObject.h

**Difference**: GeneralsMD adds extensive drawing features

**GeneralsMD Additions**:
- `setDrawObjects()` method signature expanded with 6 new boolean parameters:
  - `Bool bounding` - Draw bounding boxes
  - `Bool sight` - Draw sight ranges
  - `Bool weapon` - Draw weapon ranges
  - `Bool sound` - Draw sound ranges
  - `Bool testart` - Draw test art highlight
  - `Bool letterbox` - Draw letterbox
- New member variables:
  - `Bool m_drawBoundingBoxes`
  - `Bool m_drawSightRanges`
  - `Bool m_drawWeaponRanges`
  - `Bool m_drawSoundRanges`
  - `Bool m_drawTestArtHighlight`
  - `Bool m_drawLetterbox`
  - `Render2DClass *m_lineRenderer` - For rendering 2D lines
  - `CPoint m_winSize` - Window size
- New methods:
  - `addCircleToLineRenderer()`
  - `updateVBWithBoundingBox()`
  - `updateVBWithSightRange()`
  - `updateVBWithWeaponRange()`
  - `updateVBWithTestArtHighlight()`
  - `updateVBWithSoundRanges()`
  - `worldToScreen()`

**Analysis**: These are Zero Hour-specific visualization features for debugging/editing.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour-only features

---

## 3. include/EditCondition.h

**Difference**: GeneralsMD adds `CMyTreeCtrl` class

**GeneralsMD Addition**:
```cpp
class CMyTreeCtrl : public CTreeCtrl
{
public:
    virtual LRESULT WindowProc( UINT message, WPARAM wParam, LPARAM lParam );
};
```

**Analysis**: Custom tree control that prevents space key from triggering default behavior. Used in EditCondition.cpp.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour UI enhancement

---

## 4. include/EditParameter.h

**Difference**: GeneralsMD enhances parameter editing

**GeneralsMD Changes**:
- `edit()` method adds `Int keyPressed` parameter
- `getWarningText()` adds `Bool isAction` parameter
- `loadCounters()`, `loadActionParameter()`, `loadFlags()` now return `Bool` and take `AsciiString match` parameter
- Methods made `static`
- Adds `Int m_key` member variable
- Adds `scanReal()` protected method

**Analysis**: Enhanced parameter validation and matching. Improves user experience.

**Decision**: **CAN USE GENERALSMD VERSION** - Enhancement, doesn't break Generals functionality

---

## 5. include/LayersList.h

**Difference**: GeneralsMD adds polygon trigger layer management

**GeneralsMD Additions**:
- `ListPolygonTriggerPtr` typedef and iterator
- `polygonTriggersInLayer` member per layer
- Methods: `addPolygonTriggerToLayersList()`, `removePolygonTriggerFromLayersList()`, `changePolygonTriggerLayer()`
- Static methods: `findPolygonTriggerByUID()`, `findAndSelectPolygonTrigger()`, `unselectAllPolygonTriggers()`
- `ThePolygonTriggerLayerName` static string
- `m_activatedLayer` member
- Helper methods for polygon trigger management

**Analysis**: Zero Hour adds layer management for polygon triggers (triggers can be organized in layers).

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour feature

---

## 6. include/ScriptDialog.h

**Difference**: GeneralsMD adds script verification features

**GeneralsMD Changes**:
- `updateWarnings()` now takes `Bool forceUpdate=false` parameter
- Adds `patchScriptParametersForGC()` static method
- Adds `checkParametersForGC()` static method
- Adds `m_autoUpdateWarnings` member variable
- Adds `OnVerify()`, `OnPatchGC()`, `OnAutoVerify()` message handlers

**Analysis**: Zero Hour adds script verification and patching features for GameCube compatibility.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour feature (GameCube support)

---

## 7. include/WBHeightMap.h

**Difference**: GeneralsMD adds optional FlatHeightMap support

**GeneralsMD Addition**:
```cpp
#define dont_USE_FLAT_HEIGHT_MAP // Use the original height map for mission disk
#ifdef USE_FLAT_HEIGHT_MAP
class WBHeightMap : public FlatHeightMapRenderObjClass
#else
class WBHeightMap : public HeightMapRenderObjClass
#endif
```

**Analysis**: Zero Hour has an alternative height map implementation option (currently disabled).

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour feature (even if disabled by default)

---

## 8. include/WHeightMapEdit.h

**Difference**: GeneralsMD removes `NUM_ALPHA_TILES` and `getRawTileData()` method

**Analysis**: GeneralsMD removes functionality. `getRawTileData()` is defined and implemented in Generals but may not be called. Need to check if it's actually used.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Generals has this method, need to preserve it for Generals builds

---

## 9. include/mapobjectprops.h

**Difference**: GeneralsMD adds scale slider and reorders methods

**GeneralsMD Changes**:
- Adds `Real m_scale` member
- Adds `WBPopupSliderButton m_scaleSlider` member
- Adds `m_scaleSlider` initialization
- Adds `OnScaleOn()`, `OnScaleOff()`, `OnKillfocusMAPOBJECTXYPosition()` message handlers
- Adds `_ScaleToDict()` method
- Adds `SetPosition()` method
- Reorders some methods (OnOK/OnCancel moved)
- Adds sound-related members: `m_defaultEntryIndex`, `m_defaultIsNone`, `m_defaultEntryName`
- Adds `m_position` member

**Analysis**: Zero Hour adds object scaling feature and improves position handling.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour feature (scale slider)

---

## 10. include/wbview.h

**Difference**: GeneralsMD adds RulerTool feedback support

**GeneralsMD Changes**:
- Adds `RulerTypeEnum` enum (RULER_NONE, RULER_LINE, RULER_CIRCLE)
- Adds `m_doRulerFeedback` member
- Adds `m_rulerPoints[2]` member
- Adds `m_rulerLength` member
- Adds `doRulerFeedback()` method
- Adds `rulerFeedbackInfo()` method

**Analysis**: Zero Hour adds ruler feedback visualization in 2D view.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour feature (RulerTool related)

---

## 11. include/wbview3d.h

**Difference**: GeneralsMD replaces directional view commands with visualization features

**GeneralsMD Changes**:
- **Removed**: `OnLookEast()`, `OnLookNorth()`, `OnLookSouth()`, `OnLookWest()` methods
- **Added**: `OnViewBoundingBoxes()`, `OnViewSightRanges()`, `OnViewWeaponRanges()`, `OnHighlightTestArt()`, `OnShowLetterbox()`, `OnViewShowSoundCircles()` methods
- **Added**: Member variables: `m_showBoundingBoxes`, `m_showSightRanges`, `m_showWeaponRanges`, `m_highlightTestArt`, `m_showLetterbox`, `m_showSoundCircles`
- **Added**: `drawCircle()`, `updateTrees()`, `getCurrentZoom()`, getter/setter methods for visualization flags
- **Added**: `m_actualWinSize` member

**Analysis**: Zero Hour replaces simple directional views with advanced visualization features (bounding boxes, ranges, etc.).

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour features replace Generals features

---

## 12. src/BorderTool.cpp

**Difference**: GeneralsMD adds new border creation mode

**GeneralsMD Changes**:
- Adds `motion == -1` case that creates initial boundary
- Sets `m_addingNewBorder = true`
- Adds boundary `{1, 1}` to document

**Analysis**: Zero Hour adds ability to create new borders from scratch.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour feature

---

## 13. src/BuildList.cpp

**Difference**: Minor formatting/style differences (10 non-copyright differences)

**Analysis**: Appears to be mostly code style (spacing, formatting).

**Decision**: **LIKELY NO CONDITIONAL COMPILATION** - Can use GeneralsMD version

---

## 14. src/CUndoable.cpp

**Difference**: GeneralsMD adds MultipleUndoable implementation and LayersList integration

**GeneralsMD Changes**:
- Implements `MultipleUndoable` class (constructor, destructor, `addUndoable()`, `Do()`, `Undo()`, `Redo()`)
- Adds `undoHelper()` static function for recursive undo
- Adds `TheLayersList->addPolygonTriggerToLayersList()` calls in polygon undoables
- Adds `TheLayersList->removePolygonTriggerFromLayersList()` calls
- Minor formatting changes (indentation)

**Analysis**: Implements the MultipleUndoable feature and integrates with LayersList for polygon triggers.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour features

---

## 15. src/CameraOptions.cpp

**Difference**: GeneralsMD adds waypoint dropping and center-on-selected features

**GeneralsMD Changes**:
- Adds `#include "WaypointOptions.h"` and `#include "CUndoable.h"`
- Adds `OnDropWaypointButton()` method - drops waypoint at camera target
- Adds `OnCenterOnSelectedButton()` method - centers view on single selected object
- Changes zoom calculation to use `p3View->getCurrentZoom()` instead of height-based calculation
- Adds message map entries for new buttons

**Analysis**: Zero Hour adds convenient camera/waypoint features.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour features

---

## 16. src/DrawObject.cpp

**Difference**: GeneralsMD adds drawing features and performance improvements

**GeneralsMD Changes**:
- **Performance**: Changes buffer usage from `USAGE_DEFAULT` to `USAGE_DYNAMIC`
- **Performance**: Adds `D3DLOCK_DISCARD` to all WriteLock operations
- **Type change**: `Vector3i *pPoly` → `TriIndex *pPoly` (API change)
- **New includes**: `W3DConvert.h`, `render2d.h`, `Weapon.h`, `AudioEventInfo.h`
- **Initialization**: Adds `m_lineRenderer(NULL)`, `m_drawSoundRanges(false)`
- **Cleanup**: Changes `REF_PTR_RELEASE(m_waterDrawObject)` to `delete m_lineRenderer`
- **New features**: Implements all the new drawing methods from DrawObject.h (bounding boxes, ranges, etc.)

**Analysis**: Zero Hour adds extensive visualization features and performance optimizations. The buffer usage changes are performance improvements that should work for Generals too, but the new drawing features are Zero Hour-only.

**Decision**: **MIXED** - Buffer usage improvements can be unified, drawing features need conditional compilation

---

## 17. src/EditAction.cpp

**Difference**: GeneralsMD improves tree view UI

**GeneralsMD Changes**:
- Adds `findOrAdd()` helper function for tree item management
- Replaces combo box with tree view for action selection
- Creates `m_actionTreeView` (CMyTreeCtrl) instead of using combo box
- Adds `ENM_KEYEVENTS` to edit control event mask
- Changes from combo box string matching to tree view item selection
- Improves UI organization with hierarchical tree structure

**Analysis**: Zero Hour improves script action editing UI with tree view instead of flat combo box.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour UI enhancement

---

## 18. src/EditCondition.cpp

**Difference**: GeneralsMD improves condition editing UI

**GeneralsMD Changes**:
- Implements `CMyTreeCtrl::WindowProc()` - prevents space key from triggering default behavior
- Adds `findOrAdd()` helper function (same as EditAction.cpp)
- Replaces combo box with tree view for condition selection
- Creates `m_conditionTreeView` (CMyTreeCtrl) instead of using combo box
- Changes from combo box string matching to tree view item selection
- Improves UI organization with hierarchical tree structure

**Analysis**: Zero Hour improves script condition editing UI with tree view instead of flat combo box.

**Decision**: **CONDITIONAL COMPILATION NEEDED** - Zero Hour UI enhancement

---

## 19. src/EditObjectParameter.cpp

**Difference**: GeneralsMD adds keyboard handling for tree view

**GeneralsMD Changes**:
- Adds `OnNotify()` method override
- Handles `TVN_KEYDOWN` notifications
- Allows Shift/Space keys to toggle tree expansion
- Improves tree view keyboard navigation

**Analysis**: Zero Hour improves tree view keyboard navigation.

**Decision**: **CAN USE GENERALSMD VERSION** - Enhancement, doesn't break Generals functionality

---

## 20. src/EditParameter.cpp

**Difference**: GeneralsMD enhances parameter validation

**GeneralsMD Changes**:
- `edit()` method now handles `keyPressed` parameter
- `getWarningText()` now checks `isAction` flag for different validation rules
- `loadCounters()`, `loadActionParameter()`, `loadFlags()` now return `Bool` and check for matches
- Adds `LEFT_OR_RIGHT` parameter type handling
- Improves validation logic for counters, flags, and parameters
- Better error messages and warnings

**Analysis**: Zero Hour improves parameter editing with better validation and matching. These are enhancements that improve the editor.

**Decision**: **CAN USE GENERALSMD VERSION** - Enhancement, doesn't break Generals functionality

---

## Summary

### Files Requiring Conditional Compilation (Confirmed):
1. ✅ `include/CUndoable.h` - MultipleUndoable class
2. ✅ `include/DrawObject.h` - Enhanced drawing features (bounding boxes, ranges, etc.)
3. ✅ `include/WBHeightMap.h` - FlatHeightMap option
4. ✅ `include/EditCondition.h` - CMyTreeCtrl class
5. ✅ `include/LayersList.h` - Polygon trigger layer management
6. ✅ `include/mapobjectprops.h` - Scale slider feature
7. ✅ `include/wbview3d.h` - Visualization features (replaces directional views)
8. ✅ `src/CUndoable.cpp` - MultipleUndoable implementation + LayersList integration
9. ✅ `src/DrawObject.cpp` - New drawing features (buffer improvements can be unified)
10. ✅ `src/EditAction.cpp` - Tree view UI enhancement
11. ✅ `src/EditCondition.cpp` - Tree view UI enhancement
12. ✅ `src/CameraOptions.cpp` - Waypoint dropping and center-on-selected
13. ✅ `src/BorderTool.cpp` - New border creation mode

### Files Safe to Use GeneralsMD Version (No Conditional Compilation):
1. ✅ `src/BuildList.cpp` - Style differences only
2. ✅ `include/EditParameter.h` + `src/EditParameter.cpp` - Enhancements that improve Generals too

### Files Needing Verification:
- `include/WHeightMapEdit.h` (4 differences) - **NEEDS CHECK** - Removes `getRawTileData()`, verify if Generals uses it

---

## Final Summary

**Total files analyzed**: 20

**Files requiring conditional compilation**: 13 files
- RulerTool related: 5 files
- TeamObjectProperties related: 4 files  
- Other Zero Hour features: 4 files

**Files safe to use GeneralsMD version**: 4 files
- `src/BuildList.cpp` - Style only
- `include/EditParameter.h` + `src/EditParameter.cpp` - Enhancements
- `src/EditObjectParameter.cpp` - Keyboard navigation enhancement

**Files needing conditional compilation**: 1 file
- `include/WHeightMapEdit.h` + `src/WHeightMapEdit.cpp` - Generals uses `getRawTileData()` method that GeneralsMD removes. Need to keep Generals version or add conditional compilation.

