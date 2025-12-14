//===--- UseIsEmptyCheck.cpp - Use isEmpty() instead of getLength() == 0 -===//
//
// This check finds patterns like:
//   - AsciiString::getLength() == 0  -> AsciiString::isEmpty()
//   - AsciiString::getLength() > 0   -> !AsciiString::isEmpty()
//   - UnicodeString::getLength() == 0 -> UnicodeString::isEmpty()
//   - UnicodeString::getLength() > 0  -> !UnicodeString::isEmpty()
//   - StringClass::Get_Length() == 0  -> StringClass::Is_Empty()
//   - WideStringClass::Get_Length() == 0 -> WideStringClass::Is_Empty()
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
  // Matcher for AsciiString/UnicodeString with getLength()
  auto GetLengthCall = cxxMemberCallExpr(
      callee(cxxMethodDecl(hasName("getLength"))),
      on(hasType(hasUnqualifiedDesugaredType(
          recordType(hasDeclaration(cxxRecordDecl(
              hasAnyName("AsciiString", "UnicodeString"))))))));

  // Matcher for StringClass/WideStringClass with Get_Length()
  auto GetLengthCallWWVegas = cxxMemberCallExpr(
      callee(cxxMethodDecl(hasName("Get_Length"))),
      on(hasType(hasUnqualifiedDesugaredType(
          recordType(hasDeclaration(cxxRecordDecl(
              hasAnyName("StringClass", "WideStringClass"))))))));

  // Helper function to add matchers for a given GetLength call matcher
  auto addMatchersForGetLength = [&](const auto &GetLengthMatcher) {
    Finder->addMatcher(
        binaryOperator(
            hasOperatorName("=="),
            hasLHS(ignoringParenImpCasts(GetLengthMatcher.bind("getLengthCall"))),
            hasRHS(integerLiteral(equals(0)).bind("zero")))
            .bind("comparison"),
        this);

    Finder->addMatcher(
        binaryOperator(
            hasOperatorName("!="),
            hasLHS(ignoringParenImpCasts(GetLengthMatcher.bind("getLengthCall"))),
            hasRHS(integerLiteral(equals(0)).bind("zero")))
            .bind("comparison"),
        this);

    Finder->addMatcher(
        binaryOperator(
            hasOperatorName(">"),
            hasLHS(ignoringParenImpCasts(GetLengthMatcher.bind("getLengthCall"))),
            hasRHS(integerLiteral(equals(0)).bind("zero")))
            .bind("comparison"),
        this);

    Finder->addMatcher(
        binaryOperator(
            hasOperatorName("<="),
            hasLHS(ignoringParenImpCasts(GetLengthMatcher.bind("getLengthCall"))),
            hasRHS(integerLiteral(equals(0)).bind("zero")))
            .bind("comparison"),
        this);

    Finder->addMatcher(
        binaryOperator(
            hasOperatorName("=="),
            hasLHS(integerLiteral(equals(0)).bind("zero")),
            hasRHS(ignoringParenImpCasts(GetLengthMatcher.bind("getLengthCall"))))
            .bind("comparison"),
        this);

    Finder->addMatcher(
        binaryOperator(
            hasOperatorName("!="),
            hasLHS(integerLiteral(equals(0)).bind("zero")),
            hasRHS(ignoringParenImpCasts(GetLengthMatcher.bind("getLengthCall"))))
            .bind("comparison"),
        this);
  };

  addMatchersForGetLength(GetLengthCall);
  addMatchersForGetLength(GetLengthCallWWVegas);
}

void UseIsEmptyCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Comparison = Result.Nodes.getNodeAs<BinaryOperator>("comparison");
  const auto *GetLengthCall =
      Result.Nodes.getNodeAs<CXXMemberCallExpr>("getLengthCall");

  if (!Comparison || !GetLengthCall)
    return;

  const Expr *ObjectExpr = GetLengthCall->getImplicitObjectArgument();
  if (!ObjectExpr)
    return;

  // Determine which method name to use based on the called method
  StringRef GetLengthMethodName = GetLengthCall->getMethodDecl()->getName();
  std::string IsEmptyMethodName;
  std::string GetLengthMethodNameStr;

  if (GetLengthMethodName == "Get_Length") {
    IsEmptyMethodName = "Is_Empty()";
    GetLengthMethodNameStr = "Get_Length()";
  } else {
    IsEmptyMethodName = "isEmpty()";
    GetLengthMethodNameStr = "getLength()";
  }

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
    return;
  }

  StringRef ObjectText = Lexer::getSourceText(
      CharSourceRange::getTokenRange(ObjectExpr->getSourceRange()),
      *Result.SourceManager, Result.Context->getLangOpts());

  std::string Replacement;
  if (ShouldNegate) {
    Replacement = "!" + ObjectText.str() + "." + IsEmptyMethodName;
  } else {
    Replacement = ObjectText.str() + "." + IsEmptyMethodName;
  }

  SourceLocation StartLoc = Comparison->getBeginLoc();
  SourceLocation EndLoc = Comparison->getEndLoc();

  diag(Comparison->getBeginLoc(),
       "use %0 instead of comparing %1 with 0")
      << Replacement << GetLengthMethodNameStr
      << FixItHint::CreateReplacement(
             CharSourceRange::getTokenRange(StartLoc, EndLoc), Replacement);
}

} // namespace clang::tidy::generalsgamecode::readability

