# WorldBuilder Unification Plan

## Overview
Unify the WorldBuilder tool from `Generals/Code/Tools/WorldBuilder` and `GeneralsMD/Code/Tools/WorldBuilder` into `Core/Tools/WorldBuilder`.

## Scale Analysis
- **Total files**: ~396 C++ files (headers + sources)
- **Files that differ**: 199 files
- **Files only in GeneralsMD**: 7 files
  - `RulerTool.h/cpp` - Zero Hour-specific tool
  - `RulerOptions.h/cpp` - Options for RulerTool
  - `TeamObjectProperties.h/cpp` - Zero Hour-specific team properties
  - `res/icon2.ico` - Additional icon resource

## Key Differences

### 1. CMakeLists.txt
- **Generals**: `g_worldbuilder` executable
  - Links: `g_gameengine`, `g_gameenginedevice`, `gi_always`
- **GeneralsMD**: `z_worldbuilder` executable
  - Links: `z_gameengine`, `z_gameenginedevice`, `zi_always`, `core_debug`, `core_profile`
- **Unified**: Use conditional compilation for target name and libraries

### 2. WorldBuilder.h
- **GeneralsMD**: Includes `RulerTool.h` (line 54)
- **GeneralsMD**: `NUM_VIEW_TOOLS=25` (includes RulerTool)
- **Generals**: `NUM_VIEW_TOOLS=24` (no RulerTool)
- **GeneralsMD**: Has `RulerTool m_rulerTool;` member (line 121)
- **Unified**: Conditional compilation for RulerTool inclusion and tool count

### 3. Copyright Headers
- All files have different copyright headers (Generals vs Zero Hour)
- **Strategy**: Use Zero Hour header (GeneralsMD version) as base

### 4. Library Naming
- Generals uses `g_` and `gi_` prefixes
- GeneralsMD uses `z_` and `zi_` prefixes
- **Strategy**: Use conditional compilation in CMakeLists.txt

### 5. Zero Hour-Specific Features
- **RulerTool**: Measurement tool only in Zero Hour
- **TeamObjectProperties**: Additional team property editing
- **Strategy**: Conditional compilation for these features

## Unification Strategy

### Phase 1: Analysis
1. Identify files that are functionally identical (only copyright differs)
2. Identify files with only naming differences (`g_` vs `z_`)
3. Identify files with logic differences
4. Identify files that need conditional compilation

### Phase 2: File Movement
1. Use `unify_move_files.py` script to move files from GeneralsMD to Core
2. Move all 7 GeneralsMD-only files to Core
3. Create unified CMakeLists.txt with conditional compilation
4. Update parent CMakeLists.txt files

### Phase 3: Conditional Compilation
1. Add conditional compilation for RulerTool in WorldBuilder.h
2. Add conditional compilation for TeamObjectProperties if needed
3. Ensure NUM_VIEW_TOOLS is correct for each build

### Phase 4: Verification
1. Verify all files compile
2. Test that Generals build works without RulerTool
3. Test that GeneralsMD build works with RulerTool

## Execution Order

1. **Update unify_move_files.py script** - Add WorldBuilder file list
2. **Move files to Core** - Use script to move all files from GeneralsMD to Core
3. **Create unified CMakeLists.txt** - With conditional compilation for target names and libraries
4. **Update parent CMakeLists.txt** - Remove from Generals/GeneralsMD, add to Core
5. **Add conditional compilation for RulerTool** - In WorldBuilder.h and related files
6. **Add conditional compilation for TeamObjectProperties** - If needed

## Detailed Analysis Results

### File Categorization
- **Copyright-only differences**: 148 files (no logic changes, just header)
- **RulerTool conditional compilation needed**: 5 files
  - `CMakeLists.txt` - Add RulerTool sources conditionally
  - `include/MainFrm.h` - Include `RulerOptions.h`, member `RulerOptions m_rulerOptions;`
  - `src/MainFrm.cpp` - Create and use `m_rulerOptions` dialog (4 locations)
  - `include/WorldBuilder.h` - Include, enum, member variable
  - `src/WorldBuilder.cpp` - Tool initialization `m_tools[24] = &m_rulerTool;`
- **TeamObjectProperties conditional compilation needed**: 4 files
  - `CMakeLists.txt` - Add TeamObjectProperties sources conditionally
  - `res/WorldBuilder.rc` - Dialog resource definition
  - `res/resource.h` - Resource ID definition
  - `src/teamsdialog.cpp` - Include and AddPage call
- **Logic differences requiring analysis**: ~20 files
  - Need to review each to determine if conditional compilation needed or if can be unified

### WorldBuilder.cpp Key Differences
1. **RulerTool initialization**: `m_tools[24] = &m_rulerTool;` (Zero Hour only)
2. **Debug code**: `_exit(0);` (Zero Hour only)
3. **ApplicationHWnd**: `ApplicationHWnd = GetDesktopWindow();` (Zero Hour only)
4. **DEBUG_LOGGING**: Conditional console output setup (Zero Hour only)
5. **m_debugIgnoreAsserts**: `false` in Zero Hour vs `true` in Generals
6. **TheWritableGlobalData logging**: Additional DEBUG_LOG in Zero Hour
7. **TheScriptEngine->turnBreezeOff()**: Zero Hour only
8. **ini.loadFileDirectory()**: Additional script loading in Zero Hour
9. **m_isWorldBuilder flag**: Set in Zero Hour only
10. **TextureLoadTaskClass::shutdown()**: Removed in Zero Hour
11. **MEMORYPOOL_DEBUG**: Additional debug code in Zero Hour

### Conditional Compilation Requirements

#### RulerTool (Zero Hour Only)
- `include/WorldBuilder.h`: 
  - Conditional `#include "RulerTool.h"`
  - Conditional `NUM_VIEW_TOOLS` (24 vs 25)
  - Conditional `RulerTool m_rulerTool;` member
- `src/WorldBuilder.cpp`:
  - Conditional `m_tools[24] = &m_rulerTool;` initialization
- `include/MainFrm.h`:
  - Conditional `#include "RulerOptions.h"`
  - Conditional `RulerOptions m_rulerOptions;` member
- `src/MainFrm.cpp`:
  - Conditional `m_rulerOptions.Create(IDD_RULER_OPTIONS, this);` (window creation)
  - Conditional `m_rulerOptions.SetWindowPos(...)` (window positioning)
  - Conditional `m_rulerOptions.GetWindowRect(...)` (window rect retrieval)
  - Conditional `case IDD_RULER_OPTIONS: newOptions = &m_rulerOptions; break;` (dialog handling)
- `CMakeLists.txt`:
  - Conditional source files: `RulerTool.cpp`, `RulerOptions.cpp`

#### TeamObjectProperties (Zero Hour Only)
- `src/teamsdialog.cpp`:
  - Conditional `#include "TeamObjectProperties.h"`
  - Conditional `TeamObjectProperties object(...);` creation
  - Conditional `editDialog.AddPage(&object);` call
- `res/WorldBuilder.rc`:
  - Conditional `IDD_TeamObjectProperties` dialog definition
- `res/resource.h`:
  - Conditional `IDD_TeamObjectProperties` resource ID
- `CMakeLists.txt`:
  - Conditional source file: `TeamObjectProperties.cpp`

#### WorldBuilder.cpp Other Changes
- Most other differences appear to be debug/development code that may not need conditional compilation
- `m_debugIgnoreAsserts` difference may need conditional compilation if it affects behavior
- `TextureLoadTaskClass::shutdown()` removal may need investigation

## Notes
- This is a much larger unification than ParticleEditor (199 files vs 27 files)
- **148 files** only differ in copyright headers (easy unification)
- **9 files** need conditional compilation (5 for RulerTool + 4 for TeamObjectProperties)
- **~20 files** have logic differences that need individual review
- Most differences are likely copyright headers and naming prefixes
- Need to carefully handle the tool array initialization in WorldBuilder.cpp
- Need to review MainFrm.h for RulerTool menu/toolbar integration

