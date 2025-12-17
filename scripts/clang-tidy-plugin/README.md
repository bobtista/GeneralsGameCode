# GeneralsGameCode Clang-Tidy Plugin

This is a custom clang-tidy plugin that provides checks specific to the GeneralsGameCode codebase.

## Checks

### `generals-use-is-empty`

Finds uses of `getLength() == 0` or `getLength() > 0` on `AsciiString` and `UnicodeString`, and `Get_Length() == 0` on `StringClass` and `WideStringClass`, and suggests using `isEmpty()`/`Is_Empty()` or `!isEmpty()`/`!Is_Empty()` instead.

Also finds uses of `compare("") == 0`, `compare(TheEmptyString) == 0`, `compareNoCase("") == 0`, and similar patterns, suggesting `isEmpty()` instead.

**Examples:**

```cpp
// Before (AsciiString/UnicodeString)
if (str.getLength() == 0) { ... }
if (str.getLength() > 0) { ... }
if (str.compare("") == 0) { ... }
if (str.compare(AsciiString::TheEmptyString) == 0) { ... }
if (str.compareNoCase("") == 0) { ... }

// After (AsciiString/UnicodeString)
if (str.isEmpty()) { ... }
if (!str.isEmpty()) { ... }
if (str.isEmpty()) { ... }
if (str.isEmpty()) { ... }
if (str.isEmpty()) { ... }

// Before (StringClass/WideStringClass)
if (str.Get_Length() == 0) { ... }
if (str.Get_Length() > 0) { ... }

// After (StringClass/WideStringClass)
if (str.Is_Empty()) { ... }
if (!str.Is_Empty()) { ... }
```

## Building

This plugin requires LLVM and Clang to be installed. Build it with CMake:

```bash
cd scripts/clang-tidy-plugin
mkdir build && cd build
cmake .. -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm -DClang_DIR=/path/to/clang/lib/cmake/clang
make
```

The plugin will be built as a shared library (`.so` on Linux, `.dylib` on macOS, `.dll` on Windows) in the `build/lib/` directory.

## Prerequisites

Before using the plugin, you need to generate a compile commands database:

```bash
cmake -B build/clang-tidy -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -G Ninja
```

This creates `build/clang-tidy/compile_commands.json` which tells clang-tidy how to compile each file.

## Usage

### Basic Usage

```bash
clang-tidy -p build/clang-tidy \
  --checks='-*,generals-use-is-empty' \
  -load scripts/clang-tidy-plugin/build/lib/libGeneralsGameCodeClangTidyPlugin.so \
  file.cpp
```

### With Automatic Fixes

```bash
clang-tidy -p build/clang-tidy \
  --checks='-*,generals-use-is-empty' \
  -load scripts/clang-tidy-plugin/build/lib/libGeneralsGameCodeClangTidyPlugin.so \
  -fix-errors \
  file.cpp
```

### On Multiple Files

```bash
find Core -name "*.cpp" -type f -exec clang-tidy \
  -p build/clang-tidy \
  --checks='-*,generals-use-is-empty' \
  -load scripts/clang-tidy-plugin/build/lib/libGeneralsGameCodeClangTidyPlugin.so \
  -fix-errors {} \;
```

The `-p build/clang-tidy` flag tells clang-tidy to use the compile commands database to understand how to parse each file.

