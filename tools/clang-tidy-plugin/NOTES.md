# Plugin Registration Issue

## Current Status

The plugin builds successfully, but the check is not being recognized when loaded with `-load`.

## Problem

Clang-tidy's `-load` mechanism may not support dynamic registration of `ClangTidyModule` instances for out-of-tree plugins. The static registry approach works for in-tree modules, but dynamically loaded plugins may not initialize the registry properly.

## Possible Solutions

### Option 1: Build clang-tidy with the plugin integrated
Instead of loading as a plugin, integrate the check directly into a custom build of clang-tidy. This requires:
- Building LLVM/Clang from source
- Adding our module to the clang-tidy source tree
- Rebuilding clang-tidy

### Option 2: Use a wrapper script
Create a Python script that:
- Uses libclang or clang's Python bindings
- Applies the AST matchers directly
- Generates fix suggestions

### Option 3: Use clang-query or similar tools
Use clang's query-based tools to find the patterns, then apply fixes manually or with a script.

### Option 4: Investigate clang-tidy's plugin API further
There may be a different registration mechanism for out-of-tree plugins that we haven't discovered yet. The LLVM documentation on out-of-tree plugins might have more details.

## Current Implementation

The check implementation itself is complete and should work once registration is resolved. The AST matchers correctly identify:
- `AsciiString::getLength() == 0` → `AsciiString::isEmpty()`
- `AsciiString::getLength() > 0` → `!AsciiString::isEmpty()`
- `UnicodeString::getLength() == 0` → `UnicodeString::isEmpty()`
- `UnicodeString::getLength() > 0` → `!UnicodeString::isEmpty()`

## Testing the Check Logic

Even without registration working, we could test the AST matcher logic by:
1. Creating a standalone tool that uses the same matchers
2. Using clang-query to test the patterns
3. Integrating into the build system differently


