#include "UseThisInsteadOfSingletonCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Lexer.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace clang::tidy::generalsgamecode::readability {


static const CXXMethodDecl *getEnclosingMethod(ASTContext *Context,
                                                const Stmt *S) {
  const CXXMethodDecl *Method = nullptr;
  
  auto Parents = Context->getParents(*S);
  while (!Parents.empty()) {
    if (const auto *M = Parents[0].get<CXXMethodDecl>()) {
      if (!M->isStatic()) {
        Method = M;
        break;
      }
    }
    Parents = Context->getParents(Parents[0]);
  }
  
  return Method;
}

static bool typesMatch(const QualType &SingletonType,
                       const CXXRecordDecl *EnclosingClass) {
  if (!EnclosingClass) {
    return false;
  }

  const Type *TypePtr = SingletonType.getTypePtrOrNull();
  if (!TypePtr) {
    return false;
  }

  if (const PointerType *PtrType = TypePtr->getAs<PointerType>()) {
    QualType PointeeType = PtrType->getPointeeType();
    if (const RecordType *RecordTy = PointeeType->getAs<RecordType>()) {
      if (const CXXRecordDecl *RecordDecl =
              dyn_cast<CXXRecordDecl>(RecordTy->getDecl())) {
        return RecordDecl->getCanonicalDecl() ==
               EnclosingClass->getCanonicalDecl();
      }
    }
  }

  return false;
}

void UseThisInsteadOfSingletonCheck::registerMatchers(MatchFinder *Finder) {
  auto SingletonVarMatcher = varDecl(
      hasGlobalStorage(),
      matchesName("^The[A-Z]"),
      hasType(pointerType(pointee(recordType()))))
      .bind("singletonVar");

  auto SingletonDeclRef = declRefExpr(to(SingletonVarMatcher));

  auto SingletonMemberExpr = memberExpr(
      hasObjectExpression(ignoringParenImpCasts(SingletonDeclRef)),
      hasDeclaration(anyOf(cxxMethodDecl(), fieldDecl())))
      .bind("memberExpr");

  Finder->addMatcher(SingletonMemberExpr, this);
}

void UseThisInsteadOfSingletonCheck::check(
    const MatchFinder::MatchResult &Result) {
  const auto *MemberExpr = Result.Nodes.getNodeAs<MemberExpr>("memberExpr");
  const auto *SingletonVar =
      Result.Nodes.getNodeAs<VarDecl>("singletonVar");

  if (!MemberExpr || !SingletonVar) {
    return;
  }

  StringRef SingletonName = SingletonVar->getName();
  if (!SingletonName.startswith("The") || SingletonName.size() <= 3 ||
      (SingletonName[3] < 'A' || SingletonName[3] > 'Z')) {
    return;
  }

  const ASTContext *Context = Result.Context;

  const CXXMethodDecl *EnclosingMethod = getEnclosingMethod(
      const_cast<ASTContext *>(Context), MemberExpr);
  if (!EnclosingMethod) {
    return;
  }

  const CXXRecordDecl *EnclosingClass = EnclosingMethod->getParent();
  if (!EnclosingClass) {
    return;
  }

  QualType SingletonType = SingletonVar->getType();
  if (!typesMatch(SingletonType, EnclosingClass)) {
    return;
  }

  const ValueDecl *Member = MemberExpr->getMemberDecl();
  if (!Member) {
    return;
  }

  StringRef MemberName = Member->getName();
  
  SourceManager &SM = *Result.SourceManager;
  const LangOptions &LangOpts = Result.Context->getLangOpts();

  SourceLocation StartLoc = MemberExpr->getBeginLoc();
  SourceLocation EndLoc = MemberExpr->getEndLoc();

  std::string Replacement = std::string(MemberName);
  
  if (isa<CXXMethodDecl>(Member)) {
    Replacement += "()";
  }

  diag(StartLoc, "use '%0' instead of '%1->%2' when inside a member function")
      << Replacement << SingletonName << MemberName
      << FixItHint::CreateReplacement(
             CharSourceRange::getTokenRange(StartLoc, EndLoc), Replacement);
}

} // namespace clang::tidy::generalsgamecode::readability

