# ParticleEditor Unification Plan

## Analysis Summary

### File Count
- **26 source files** in each version (Generals and GeneralsMD)
- **All 26 files differ** between versions
- **22 files differ only in copyright headers** ✅ Easy to unify
- **4 files have logic differences** ✅ Use GeneralsMD version (improvements/bug fixes)

### Differences Found

1. **Copyright Headers** (ALL files)
   - Generals: `Command & Conquer Generals(tm)`
   - GeneralsMD: `Command & Conquer Generals Zero Hour(tm)`
   - **Strategy**: Use Zero Hour header (more complete)

2. **CMakeLists.txt Differences**
   - Library name: `g_particleeditor` vs `z_particleeditor`
   - Link libraries: `gi_*` vs `zi_*` prefixes
   - GeneralsMD has additional libraries: `core_debug`, `core_profile`
   - `_AFXDLL` compile definition placement differs (line 39 vs line 57)
   - **Strategy**: Use conditional compilation based on build target

3. **Code Logic Differences** (4 files) - **SEE DETAILED ANALYSIS IN `particleeditor_logic_analysis.md`**
   - `ParticleEditorDialog.cpp` ⚠️ **CRITICAL**: GeneralsMD removes functionality for editing angleX/Y and angularRateX/Y min/max values. **MUST USE CONDITIONAL COMPILATION** to preserve Generals behavior.
   - `ParticleTypePanels.cpp` ✅ Safe: GeneralsMD adds "SMUDGE RESERVED" string (Zero Hour feature). **Use GeneralsMD version**.
   - `Resource.h` ✅ Safe: GeneralsMD adds copyright header. **Use GeneralsMD version**.
   - `VelocityTypePanels.cpp` ✅ Safe: GeneralsMD adds blank line (cosmetic). **Use GeneralsMD version**.
   - **Strategy**: Use GeneralsMD version as base, but add conditional compilation for `ParticleEditorDialog.cpp` to preserve Generals functionality

## Unification Strategy

### Phase 1: Unify All Source Files (26 files) ✅
**Goal**: Move all ParticleEditor files to Core using GeneralsMD (Zero Hour) version

**Process**:
1. Use `unify_file()` from `unify_move_files.py` to move GeneralsMD version to Core
2. Copyright will already be Zero Hour version
3. All files use GeneralsMD version (22 copyright-only, 4 with improvements)

**Files to unify** (26 files total):
- **22 files - copyright only**: CButtonShowColor.cpp/h, CColorAlphaDialog.cpp/h, CSwitchesDialog.cpp/h, EmissionTypePanels.cpp/h, MoreParmsDialog.cpp/h, ParticleEditor.cpp/h, ParticleEditorDialog.h, ParticleEditorExport.h, ParticleTypePanels.h, ShaderTypePanels.cpp/h, StdAfx.cpp/h, VelocityTypePanels.h, ISwapablePanel.h, CParticleEditorPage.h
- **3 files - logic differences (safe to use GeneralsMD)**: ParticleTypePanels.cpp, Resource.h, VelocityTypePanels.cpp
- **1 file - logic differences (NEEDS CONDITIONAL COMPILATION)**: ParticleEditorDialog.cpp

**Special Handling for ParticleEditorDialog.cpp**:
- Use GeneralsMD version as base
- Add `#ifdef RTS_BUILD_GENERALS` blocks to restore angleX/Y and angularRateX/Y editing functionality
- See `particleeditor_logic_analysis.md` for detailed code changes

### Phase 2: CMakeLists.txt Unification
**Goal**: Create unified CMakeLists.txt in Core

**Changes needed**:
1. Use conditional library naming:
   ```cmake
   if(RTS_BUILD_GENERALS)
       set(PARTICLEEDITOR_TARGET g_particleeditor)
       set(PARTICLEEDITOR_LIBS gi_gameengine_include gi_always gi_libraries_source_wwvegas)
   else()
       set(PARTICLEEDITOR_TARGET z_particleeditor)
       set(PARTICLEEDITOR_LIBS zi_gameengine_include zi_always zi_libraries_source_wwvegas core_debug core_profile)
   endif()
   ```

2. Handle `_AFXDLL` placement consistently

### Phase 3: Update Build System
**Goal**: Update parent CMakeLists.txt to reference Core version

**Changes**:
- Update `Generals/Code/Tools/CMakeLists.txt`
- Update `GeneralsMD/Code/Tools/CMakeLists.txt`
- Both should reference `Core/Tools/ParticleEditor`

## Execution Order

1. **Create Core/Tools/ParticleEditor directory structure**
2. **Move 25 files** using `unify_file()` from GeneralsMD (22 copyright-only + 3 safe logic differences)
3. **Move ParticleEditorDialog.cpp** and add conditional compilation:
   - Use GeneralsMD version as base
   - Add `#ifdef RTS_BUILD_GENERALS` blocks for angleX/Y and angularRateX/Y editing (8 locations total)
   - See `particleeditor_logic_analysis.md` for exact code changes
4. **Create unified CMakeLists.txt** in Core with conditional compilation for:
   - Library name: `g_particleeditor` vs `z_particleeditor`
   - Link libraries: `gi_*` vs `zi_*` prefixes
   - Additional libraries: `core_debug`, `core_profile` (GeneralsMD only)
   - `_AFXDLL` placement (Windows-only section)
5. **Update parent CMakeLists.txt** files to reference Core
6. **Test build** for both Generals and GeneralsMD
7. **Test functionality** - verify angleX/Y editing works in Generals, smudges work in GeneralsMD
8. **Remove old directories** after verification

## Quick Reference: Files to Unify

**26 files total**:
- **22 files**: Copyright-only differences → Use GeneralsMD version directly
- **3 files**: Safe logic differences → Use GeneralsMD version directly
- **1 file**: Needs conditional compilation → Use GeneralsMD version + add `#ifdef RTS_BUILD_GENERALS` blocks

Use `unify_file(Game.ZEROHOUR, "Tools/ParticleEditor/<filename>", Game.CORE, "Tools/ParticleEditor/<filename>")` for each file, then manually fix `ParticleEditorDialog.cpp`.

## Notes

- Use Zero Hour (GeneralsMD) version as base since it's more complete
- Library naming: Can use `z_particleeditor` for both, or make conditional
- Link libraries: Must be conditional (gi_ vs zi_ prefixes are different targets)
- Additional libraries in GeneralsMD: `core_debug`, `core_profile` - may need conditional inclusion

