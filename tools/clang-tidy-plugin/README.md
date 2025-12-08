# GeneralsGameCode Clang-Tidy Plugin

This is a custom clang-tidy plugin that provides checks specific to the GeneralsGameCode codebase.

## Checks

### `generalsgamecode-use-is-empty`

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

This plugin requires LLVM and Clang to be installed. You can build it with CMake:

```bash
cd tools/clang-tidy-plugin
mkdir build
cd build
cmake .. -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm -DClang_DIR=/path/to/clang/lib/cmake/clang
make
```

The plugin will be built as a shared library (`.so` on Linux, `.dylib` on macOS, `.dll` on Windows) in the `build/lib/` directory.

## Usage

Load the plugin when running clang-tidy:

```bash
clang-tidy --checks=-*,generalsgamecode-use-is-empty \
  -load build/lib/GeneralsGameCodeClangTidyPlugin.so \
  source.cpp
```

Or add it to your `.clang-tidy` configuration file:

```yaml
Checks: '-*,generalsgamecode-use-is-empty'
```

And load it with:

```bash
clang-tidy -load build/lib/GeneralsGameCodeClangTidyPlugin.so source.cpp
```

