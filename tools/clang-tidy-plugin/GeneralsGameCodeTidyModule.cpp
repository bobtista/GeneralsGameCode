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

// Static registration using LLVM's registry system
// This is the "hack" mentioned in the documentation for out-of-tree plugins
// The static initializer should run when the library is loaded
static llvm::Registry<::clang::tidy::ClangTidyModule>::Add<
    ::clang::tidy::generalsgamecode::GeneralsGameCodeTidyModule>
    X("generalsgamecode", "GeneralsGameCode-specific checks");

// Anchor variable to force linker to include this file
// This ensures the static initializer above actually runs
volatile int GeneralsGameCodeTidyModuleAnchorSource = 0;

