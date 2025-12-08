//===--- GeneralsGameCodeTidyModule.cpp - GeneralsGameCode Tidy Module ---===//
//
// Custom clang-tidy module for GeneralsGameCode
// Provides checks for custom types like AsciiString and UnicodeString
//
//===----------------------------------------------------------------------===//

#include "GeneralsGameCodeTidyModule.h"
#include "readability/UseIsEmptyCheck.h"
#include "llvm/Support/Registry.h"

namespace clang::tidy::generalsgamecode {

void GeneralsGameCodeTidyModule::addCheckFactories(
    ClangTidyCheckFactories &CheckFactories) {
  CheckFactories.registerCheck<readability::UseIsEmptyCheck>(
      "generalsgamecode-use-is-empty");
}

} // namespace clang::tidy::generalsgamecode

// Force static initialization by using constructor attribute
// This ensures the registry entry is created when the library is loaded
__attribute__((constructor)) static void registerModule() {
  // Force the static registry to initialize by creating the Add object
  static llvm::Registry<::clang::tidy::ClangTidyModule>::Add<
      ::clang::tidy::generalsgamecode::GeneralsGameCodeTidyModule>
      X("generalsgamecode", "GeneralsGameCode-specific checks");
  
  // Access the registry to ensure it's initialized
  (void)llvm::Registry<::clang::tidy::ClangTidyModule>::begin();
}

// Alternative: Static registration at file scope (original approach)
// Keep this as a fallback in case constructor doesn't work
static llvm::Registry<::clang::tidy::ClangTidyModule>::Add<
    ::clang::tidy::generalsgamecode::GeneralsGameCodeTidyModule>
    StaticReg("generalsgamecode", "GeneralsGameCode-specific checks");

