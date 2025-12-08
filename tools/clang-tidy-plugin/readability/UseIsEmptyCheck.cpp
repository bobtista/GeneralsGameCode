//===--- UseIsEmptyCheck.cpp - Use isEmpty() instead of getLength() == 0 -===//
//
// This check finds patterns like:
//   - AsciiString::getLength() == 0  -> AsciiString::isEmpty()
//   - AsciiString::getLength() > 0   -> !AsciiString::isEmpty()
//   - UnicodeString::getLength() == 0 -> UnicodeString::isEmpty()
//   - UnicodeString::getLength() > 0  -> !UnicodeString::isEmpty()
//
//===----------------------------------------------------------------------===//

#include "UseIsEmptyCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::generalsgamecode::readability {

void UseIsEmptyCheck::registerMatchers(MatchFinder *Finder) {
  // Match member calls to getLength() on AsciiString or UnicodeString
  // followed by comparison with 0
  auto GetLengthCall = cxxMemberCallExpr(
      callee(cxxMethodDecl(hasName("getLength"))),
      on(hasType(hasUnqualifiedDesugaredType(
          recordType(hasDeclaration(cxxRecordDecl(
              hasAnyName("AsciiString", "UnicodeString"))))))));

  // Match: obj.getLength() == 0
  Finder->addMatcher(
      binaryOperator(
          hasOperatorName("=="),
          hasLHS(ignoringParenImpCasts(GetLengthCall.bind("getLengthCall"))),
          hasRHS(integerLiteral(equals(0)).bind("zero")))
          .bind("comparison"),
      this);

  // Match: obj.getLength() != 0
  Finder->addMatcher(
      binaryOperator(
          hasOperatorName("!="),
          hasLHS(ignoringParenImpCasts(GetLengthCall.bind("getLengthCall"))),
          hasRHS(integerLiteral(equals(0)).bind("zero")))
          .bind("comparison"),
      this);

  // Match: obj.getLength() > 0
  Finder->addMatcher(
      binaryOperator(
          hasOperatorName(">"),
          hasLHS(ignoringParenImpCasts(GetLengthCall.bind("getLengthCall"))),
          hasRHS(integerLiteral(equals(0)).bind("zero")))
          .bind("comparison"),
      this);

  // Match: obj.getLength() <= 0
  Finder->addMatcher(
      binaryOperator(
          hasOperatorName("<="),
          hasLHS(ignoringParenImpCasts(GetLengthCall.bind("getLengthCall"))),
          hasRHS(integerLiteral(equals(0)).bind("zero")))
          .bind("comparison"),
      this);

  // Match: 0 == obj.getLength()
  Finder->addMatcher(
      binaryOperator(
          hasOperatorName("=="),
          hasLHS(integerLiteral(equals(0)).bind("zero")),
          hasRHS(ignoringParenImpCasts(GetLengthCall.bind("getLengthCall"))))
          .bind("comparison"),
      this);

  // Match: 0 != obj.getLength()
  Finder->addMatcher(
      binaryOperator(
          hasOperatorName("!="),
          hasLHS(integerLiteral(equals(0)).bind("zero")),
          hasRHS(ignoringParenImpCasts(GetLengthCall.bind("getLengthCall"))))
          .bind("comparison"),
      this);
}

void UseIsEmptyCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Comparison = Result.Nodes.getNodeAs<BinaryOperator>("comparison");
  const auto *GetLengthCall =
      Result.Nodes.getNodeAs<CXXMemberCallExpr>("getLengthCall");

  if (!Comparison || !GetLengthCall)
    return;

  // Get the object on which getLength() is called
  const Expr *ObjectExpr = GetLengthCall->getImplicitObjectArgument();
  if (!ObjectExpr)
    return;

  // Determine the replacement based on the operator
  StringRef Operator = Comparison->getOpcodeStr();
  bool ShouldNegate = false;

  if (Operator == "==") {
    ShouldNegate = false;
  } else if (Operator == "!=") {
    ShouldNegate = true;
  } else if (Operator == ">") {
    ShouldNegate = true;
  } else if (Operator == "<=") {
    ShouldNegate = false;
  } else {
    return; // Unsupported operator
  }

  // Get the source text for the object expression
  StringRef ObjectText = Lexer::getSourceText(
      CharSourceRange::getTokenRange(ObjectExpr->getSourceRange()),
      *Result.SourceManager, Result.Context->getLangOpts());

  // Build the replacement text
  std::string Replacement;
  if (ShouldNegate) {
    Replacement = "!" + ObjectText.str() + ".isEmpty()";
  } else {
    Replacement = ObjectText.str() + ".isEmpty()";
  }

  // Create the fix - replace the entire comparison
  SourceLocation StartLoc = Comparison->getBeginLoc();
  SourceLocation EndLoc = Comparison->getEndLoc();

  diag(Comparison->getBeginLoc(),
       "use %0 instead of comparing getLength() with 0")
      << Replacement
      << FixItHint::CreateReplacement(
             CharSourceRange::getTokenRange(StartLoc, EndLoc), Replacement);
}

} // namespace clang::tidy::generalsgamecode::readability

