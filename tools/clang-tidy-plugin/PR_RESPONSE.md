# Response to PR Review Comment

## Original Comment

> I suspect clang-tidy only finds std:: related cases, unless it can be configured for other types?
>
> We still need the same kind of change for AsciiString and other internal types (in another change).

## Response

You're absolutely correct! The `readability-container-size-empty` check only works with standard library containers (std::vector, std::string, etc.) and doesn't handle custom types like `AsciiString` and `UnicodeString`.

I've created a custom clang-tidy plugin (`tools/clang-tidy-plugin/`) that includes a check specifically for these internal types. The `generals-use-is-empty` check will find patterns like:

- `AsciiString::getLength() == 0` → `AsciiString::isEmpty()`
- `AsciiString::getLength() > 0` → `!AsciiString::isEmpty()`
- `UnicodeString::getLength() == 0` → `UnicodeString::isEmpty()`
- `UnicodeString::getLength() > 0` → `!UnicodeString::isEmpty()`

This plugin can be built and loaded with clang-tidy to automatically find and fix these patterns across the codebase. Once we have the plugin built and tested, we can run it to create a follow-up PR that addresses all the `AsciiString` and `UnicodeString` cases.

The plugin is structured as an out-of-tree clang-tidy module, so it doesn't require modifying LLVM/Clang itself. See `tools/clang-tidy-plugin/README.md` for build and usage instructions.

