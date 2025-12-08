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
static llvm::Registry<::clang::tidy::ClangTidyModule>::Add<
    ::clang::tidy::generalsgamecode::GeneralsGameCodeTidyModule>
    X("generalsgamecode", "GeneralsGameCode-specific checks");

