# Questions for LLVM Discord

## Context

I'm trying to create an out-of-tree clang-tidy plugin that provides custom checks for our codebase. The plugin builds successfully, but the checks aren't being recognized when loaded with `-load`.

## Technical Details

- **Plugin Type**: `ClangTidyModule` with custom checks
- **Build System**: CMake, building as MODULE (shared library)
- **Registration Attempts**:
  - Static registry using `llvm::Registry<ClangTidyModule>::Add<>`
  - Constructor attribute to force initialization
  - Extern C registration function
- **Issue**: Plugin loads without errors, but checks don't appear in `--list-checks`
- **Warnings**: Getting undefined template variable warnings for `Registry<ClangTidyModule>::Head` and `Tail`

## Questions to Ask

### Question 1: Plugin Loading Mechanism
**"Does clang-tidy's `-load` option support loading `ClangTidyModule` plugins, or is it only for other types of plugins (like AST consumers)? I've built an out-of-tree plugin following the documentation, but my checks aren't being registered when I use `-load`."**

### Question 2: Registry Template Instantiation
**"I'm getting warnings about undefined template variables (`Registry<ClangTidyModule>::Head` and `Tail`) when building my out-of-tree plugin. The static registry approach works for in-tree modules, but for dynamically loaded plugins, the registry template seems to only be instantiated in the clang-tidy binary. Is there a way to make this work, or do I need a different approach?"**

### Question 3: Out-of-Tree Module Registration
**"What's the correct way to register a `ClangTidyModule` in an out-of-tree plugin? I've tried:**
- Static registry with `llvm::Registry<ClangTidyModule>::Add<>`
- Constructor attribute to force initialization
- Extern C registration function

**None of these seem to work. Are there examples of working out-of-tree `ClangTidyModule` plugins I can reference?"**

### Question 4: Alternative Approaches
**"If out-of-tree `ClangTidyModule` plugins aren't supported via `-load`, what are the recommended alternatives? Should I:**
- Build clang-tidy from source with my module integrated?
- Use a different plugin mechanism?
- Use libclang Python bindings instead?"

### Question 5: Source Code Location
**"Where in the clang-tidy source code should I look to understand how `-load` processes plugins? I want to see the actual implementation to understand what's expected."**

## Code Snippets to Share

### Module Registration (Current Attempt)
```cpp
namespace clang::tidy::generalsgamecode {
void GeneralsGameCodeTidyModule::addCheckFactories(
    ClangTidyCheckFactories &CheckFactories) {
  CheckFactories.registerCheck<readability::UseIsEmptyCheck>(
      "generalsgamecode-use-is-empty");
}
}

// Static registration
static llvm::Registry<::clang::tidy::ClangTidyModule>::Add<
    ::clang::tidy::generalsgamecode::GeneralsGameCodeTidyModule>
    StaticReg("generalsgamecode", "GeneralsGameCode-specific checks");
```

### Build Configuration
```cmake
add_library(GeneralsGameCodeClangTidyPlugin MODULE ${SOURCES})
target_link_libraries(GeneralsGameCodeClangTidyPlugin
  PRIVATE
  clangTidy
  clangTidyReadabilityModule
  clangAST
  clangASTMatchers
  # ... other deps
)
```

## What I've Tried

1. ✅ Built plugin successfully as shared library
2. ✅ Implemented check with AST matchers
3. ✅ Tried static registry registration
4. ✅ Tried constructor-based initialization
5. ✅ Tried extern C registration function
6. ❌ Plugin loads but checks don't register
7. ❌ Checks don't appear in `--list-checks`

## Expected vs Actual Behavior

**Expected**: After `clang-tidy -load plugin.so --list-checks`, I should see `generalsgamecode-use-is-empty` in the list.

**Actual**: Plugin loads without errors, but the check doesn't appear in the list.

## Environment

- LLVM/Clang version: 21.1.5 (Homebrew on macOS)
- Build system: CMake 3.20+
- Platform: macOS (arm64)


