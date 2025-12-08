# GeneralsGameCode Clang-Tidy Plugin

This is a custom clang-tidy plugin that provides checks specific to the GeneralsGameCode codebase.

## Checks

### `generals-use-is-empty`

Finds uses of `getLength() == 0` or `getLength() > 0` on `AsciiString` and `UnicodeString` and suggests using `isEmpty()` or `!isEmpty()` instead.

**Examples:**

```cpp
// Before
if (str.getLength() == 0) { ... }
if (str.getLength() > 0) { ... }

// After
if (str.isEmpty()) { ... }
if (!str.isEmpty()) { ... }
```

## Building

This plugin requires LLVM and Clang to be installed. Build it with CMake:

```bash
cd tools/clang-tidy-plugin
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
  -load tools/clang-tidy-plugin/build/lib/libGeneralsGameCodeClangTidyPlugin.so \
  file.cpp
```

### With Automatic Fixes

```bash
clang-tidy -p build/clang-tidy \
  --checks='-*,generals-use-is-empty' \
  -load tools/clang-tidy-plugin/build/lib/libGeneralsGameCodeClangTidyPlugin.so \
  -fix-errors \
  file.cpp
```

### On Multiple Files

```bash
find Core -name "*.cpp" -type f -exec clang-tidy \
  -p build/clang-tidy \
  --checks='-*,generals-use-is-empty' \
  -load tools/clang-tidy-plugin/build/lib/libGeneralsGameCodeClangTidyPlugin.so \
  -fix-errors {} \;
```

The `-p build/clang-tidy` flag tells clang-tidy to use the compile commands database to understand how to parse each file.

