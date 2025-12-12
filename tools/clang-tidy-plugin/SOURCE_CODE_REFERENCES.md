# Source Code References for Plugin Loading

## Key Files to Examine in LLVM Project

Based on the LLVM project structure at https://github.com/llvm/llvm-project, here are the key files to examine:

### 1. Plugin Loading Implementation
**Location**: `clang-tools-extra/clang-tidy/tool/ClangTidyMain.cpp`

This file likely contains the main entry point and `-load` option handling. Look for:
- How `-load` is parsed
- How plugins are dynamically loaded
- How `ClangTidyModule` instances are discovered/registered

### 2. Module Registry
**Location**: `clang-tools-extra/clang-tidy/ClangTidyModuleRegistry.h` and `.cpp`

This defines how modules are registered. Key things to look for:
- `ClangTidyModuleRegistry` class definition
- How static registry entries are processed
- Whether there's special handling for dynamically loaded modules

### 3. Frontend Plugin Registry
**Location**: `clang/include/clang/Frontend/FrontendPluginRegistry.h`

This is the underlying plugin mechanism. The `-load` option might use this. Look for:
- How plugins register themselves
- The difference between `FrontendPluginRegistry` and `ClangTidyModuleRegistry`

### 4. Example In-Tree Modules
**Location**: `clang-tools-extra/clang-tidy/`

Examine how in-tree modules register themselves:
- `readability/ReadabilityTidyModule.cpp` - Example module
- `misc/MiscTidyModule.cpp` - Another example
- Look for the registration pattern: `static Registry::Add<...>`

### 5. Tool Implementation
**Location**: `clang-tools-extra/clang-tidy/tool/`

Files that handle command-line options and plugin loading:
- `ClangTidyMain.cpp` - Main entry point
- Look for `LoadLibraryPermanently` or `sys::DynamicLibrary::LoadLibraryPermanently`

## Specific Code Patterns to Search For

### Search for `-load` handling:
```bash
# In clang-tools-extra/clang-tidy/
grep -r "load" tool/ --include="*.cpp" --include="*.h"
grep -r "LoadLibrary" tool/
grep -r "DynamicLibrary" tool/
```

### Search for module registration:
```bash
# In clang-tools-extra/clang-tidy/
grep -r "ClangTidyModuleRegistry" --include="*.cpp" --include="*.h"
grep -r "Registry.*Add.*ClangTidyModule" --include="*.cpp"
```

### Search for plugin loading:
```bash
# In clang/include/clang/Frontend/
grep -r "FrontendPluginRegistry" --include="*.h"
```

## GitHub Links to Examine

1. **ClangTidyMain.cpp**:
   https://github.com/llvm/llvm-project/tree/main/clang-tools-extra/clang-tidy/tool/ClangTidyMain.cpp

2. **ClangTidyModuleRegistry**:
   https://github.com/llvm/llvm-project/tree/main/clang-tools-extra/clang-tidy/ClangTidyModuleRegistry.h

3. **Example Module** (readability):
   https://github.com/llvm/llvm-project/tree/main/clang-tools-extra/clang-tidy/readability/ReadabilityTidyModule.cpp

4. **Frontend Plugin Registry**:
   https://github.com/llvm/llvm-project/tree/main/clang/include/clang/Frontend/FrontendPluginRegistry.h

## What to Look For

1. **How `-load` is processed**: Does it call `dlopen`/`LoadLibrary`? Does it look for specific symbols?

2. **Module discovery**: After loading a library, how does clang-tidy discover `ClangTidyModule` instances?

3. **Registry initialization**: When are static registry entries processed? Is this different for dynamically loaded libraries?

4. **Symbol requirements**: Are there specific function names or symbols that must be exported?

5. **Registration timing**: When does module registration happen relative to library loading?

## Potential Issues to Investigate

1. **Static initialization order**: Static registry entries might not initialize when a library is loaded via `dlopen`

2. **Symbol visibility**: The registry template might need to be in the same binary as the registry itself

3. **Plugin type mismatch**: `-load` might be for `FrontendAction` plugins, not `ClangTidyModule` plugins

4. **Version compatibility**: The plugin API might have changed between LLVM versions


