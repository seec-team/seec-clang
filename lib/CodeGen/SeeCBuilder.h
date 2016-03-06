//===-- SeeCBuilder.h - SeeC IRBuilder --------------------------*- C++ -*-===//
//
//===----------------------------------------------------------------------===//

#ifndef CLANG_CODEGEN_SEECBUILDER_H
#define CLANG_CODEGEN_SEECBUILDER_H

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/AST/ASTTypeTraits.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"
#include "clang/CodeGen/SeeCMapping.h"

#include "CGValue.h"

#include <cassert>

#define SEEC_CLANG_DEBUG 0

namespace clang {

namespace CodeGen {

namespace seec {

/// \brief Helper for adding SeeC-Clang mapping information.
///
class MetadataInserter
{
private:
  //----------------------------------------------------------------------------
  // Members
  //----------------------------------------------------------------------------

  /// The llvm::Module being created.
  llvm::Module &Module;

  /// The LLVMContext we're working with.
  llvm::LLVMContext &Context;

  /// Kind of metadata for pointers to Stmt.
  unsigned MDKindIDForStmtPtr;

  /// Kind of metadata for pointers to Decl.
  unsigned MDKindIDForDeclPtr;

  /// Kind of metadata for Stmt completion pointers.
  unsigned MDKindIDForStmtCompletionPtrs;

  /// Kind of metadata for Decl completion pointers.
  unsigned MDKindIDForDeclCompletionPtrs;

  /// Stack of the current node references.
  llvm::SmallVector<ast_type_traits::DynTypedNode, 32> NodeStack;

  /// Creates metadata to describe statement mappings.
  ::seec::clang::MetadataWriter MDWriter;

  /// Metadata for all known mappings.
  std::vector< ::llvm::MDNode * > MDStmtMappings;

  /// Metadata for all param mappings.
  std::vector< ::llvm::MDNode * > MDParamMappings;

  /// Metadata for all local (non-param) mappings.
  std::vector< ::llvm::MDNode * > MDLocalMappings;

  /// Most recently seen llvm::Instruction.
  mutable llvm::Instruction *MostRecentInstruction;

  //----------------------------------------------------------------------------
  // Methods
  //----------------------------------------------------------------------------

  /// \brief Get a new MDNode, which has all the operands of Node, plus Value.
  ///
  llvm::MDNode *addOperand(llvm::MDNode *Node, llvm::Metadata *Value) {
    llvm::SmallVector<llvm::Metadata *, 8> Operands;

    if (Node) {
      unsigned const NumOperands = Node->getNumOperands();

      for (unsigned i = 0; i < NumOperands; ++i)
        Operands.push_back(Node->getOperand(i));
    }

    Operands.push_back(Value);

    return llvm::MDNode::get(Context, Operands);
  }

  /// \brief Create an llvm::Value holding the address of the given Decl.
  ///
  llvm::Metadata *makeDeclNode(clang::Decl const *Decl) const {
    return llvm::ConstantAsMetadata::get(
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(Context),
                             reinterpret_cast<uintptr_t const>(Decl)));
  }

  /// \brief Create an llvm::Value holding the address of the given Stmt.
  ///
  llvm::Metadata *makeStmtNode(clang::Stmt const *Stmt) const {
    return llvm::ConstantAsMetadata::get(
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(Context),
                             reinterpret_cast<uintptr_t const>(Stmt)));
  }

public:
  MetadataInserter(llvm::Module &TheModule)
  : Module(TheModule),
    Context(Module.getContext()),
    MDKindIDForStmtPtr(Context.getMDKindID("seec.clang.stmt.ptr")),
    MDKindIDForDeclPtr(Context.getMDKindID("seec.clang.decl.ptr")),
    MDKindIDForStmtCompletionPtrs(
      Context.getMDKindID("seec.clang.stmt.completion.ptrs")),
    MDKindIDForDeclCompletionPtrs(
      Context.getMDKindID("seec.clang.decl.completion.ptrs")),
    NodeStack(),
    MDWriter(Context),
    MDStmtMappings(),
    MDParamMappings(),
    MostRecentInstruction(nullptr)
  {
    if (SEEC_CLANG_DEBUG)
      llvm::errs() << "MetadataInserter()\n";
  }

  ~MetadataInserter() {
    if (SEEC_CLANG_DEBUG)
      llvm::errs() << "~MetadataInserter()\n";

    typedef std::vector< ::llvm::MDNode * >::iterator IterTy;

    // Add all Stmt mappings.
    llvm::NamedMDNode *GlobalStmtMapMD
      = Module.getOrInsertNamedMetadata(
        ::seec::clang::StmtMapping::getGlobalMDNameForMapping());

    for (IterTy It = MDStmtMappings.begin(), End = MDStmtMappings.end();
         It != End; ++It) {
      GlobalStmtMapMD->addOperand(*It);
    }

    // Add all parameter mappings.
    llvm::NamedMDNode *GlobalParamMapMD
      = Module.getOrInsertNamedMetadata(
        ::seec::clang::ParamMapping::getGlobalMDNameForMapping());

    for (IterTy It = MDParamMappings.begin(), End = MDParamMappings.end();
         It != End; ++It) {
      GlobalParamMapMD->addOperand(*It);
    }

    // Add all local (non-parameter) mappings.
    llvm::NamedMDNode *GlobalLocalMapMD
      = Module.getOrInsertNamedMetadata(
        ::seec::clang::LocalMapping::getGlobalMDNameForMapping());

    for (IterTy It = MDLocalMappings.begin(), End = MDLocalMappings.end();
         It != End; ++It) {
      GlobalLocalMapMD->addOperand(*It);
    }
  }

  void pushDecl(Decl const *D) {
    assert(D && "Pushing null Decl.");

    if (SEEC_CLANG_DEBUG) {
      for (std::size_t i = 0; i < NodeStack.size(); ++i)
        llvm::outs() << " ";
      llvm::outs() << "Decl " << D->getDeclKindName() << "\n";
    }

    NodeStack.push_back(ast_type_traits::DynTypedNode::create(*D));
  }

  void popDecl() {
    assert(NodeStack.size() && NodeStack.back().get<clang::Decl>());

    if (auto const I = MostRecentInstruction) {
      auto const Decl = NodeStack.back().get<clang::Decl>();
      I->setMetadata(MDKindIDForDeclCompletionPtrs,
                     addOperand(I->getMetadata(MDKindIDForDeclCompletionPtrs),
                                makeDeclNode(Decl)));
    }

    NodeStack.pop_back();
  }

  void pushStmt(Stmt const *S) {
    assert(S && "Pushing null Stmt.");

    if (SEEC_CLANG_DEBUG) {
      for (std::size_t i = 0; i < NodeStack.size(); ++i)
        llvm::outs() << " ";
      llvm::outs() << "Stmt " << S->getStmtClassName() << "\n";
    }

    NodeStack.push_back(ast_type_traits::DynTypedNode::create(*S));
  }

  void popStmt() {
    assert(NodeStack.size() && NodeStack.back().get<clang::Stmt>());

    if (auto const I = MostRecentInstruction) {
      auto const Stmt = NodeStack.back().get<clang::Stmt>();
      I->setMetadata(MDKindIDForStmtCompletionPtrs,
                     addOperand(I->getMetadata(MDKindIDForStmtCompletionPtrs),
                                makeStmtNode(Stmt)));
    }

    NodeStack.pop_back();
  }

  void attachMetadata(llvm::Instruction *I) const {
    if (NodeStack.empty())
      return;

    auto const &Node = NodeStack.back();

    if (auto const Stmt = Node.get<clang::Stmt>())
      I->setMetadata(MDKindIDForStmtPtr,
                     llvm::MDNode::get(Context, makeStmtNode(Stmt)));
    else if (auto const Decl = Node.get<clang::Decl>())
      I->setMetadata(MDKindIDForDeclPtr,
                     llvm::MDNode::get(Context, makeDeclNode(Decl)));

    MostRecentInstruction = I;
  }

  /// \brief Mark an LValue produced by the given Stmt.
  ///
  void markLValue(LValue const &Value, Stmt const *S) {
    if (SEEC_CLANG_DEBUG) {
      llvm::errs() << "mark lvalue for " << S->getStmtClassName();
      if (clang::Expr const *E = llvm::dyn_cast<clang::Expr>(S)) {
        llvm::errs() << "  " << E->getType().getAsString();
      }
      llvm::errs() << " @" << S << "\n";
    }

    if (Value.isSimple()) {
      if (llvm::Value *Addr = Value.getAddress().getPointer()) {
        MDStmtMappings.push_back(
          MDWriter.getMetadataFor(
            ::seec::clang::StmtMapping::forLValSimple(S, Addr)));
      }
      else {
        if (SEEC_CLANG_DEBUG)
          llvm::errs() << "simple: null getAddress()!\n";
      }
    }
    else if (Value.isVectorElt()) {
      if (SEEC_CLANG_DEBUG)
        llvm::errs() << "VectorElt: not supported!\n";
    }
    else if (Value.isBitField()) {
      if (SEEC_CLANG_DEBUG)
        llvm::errs() << "BitField: not supported!\n";
    }
  }

  /// \brief Mark an RValue produced by the given Stmt.
  ///
  void markRValue(RValue const &Value, Stmt const *S) {
    if (SEEC_CLANG_DEBUG) {
      llvm::errs() << "mark rvalue for " << S->getStmtClassName();
      if (clang::Expr const *E = llvm::dyn_cast<clang::Expr>(S)) {
        llvm::errs() << "  " << E->getType().getAsString();
      }
      llvm::errs() << " @" << S << "\n";
    }

    if (Value.isScalar()) {
      if (llvm::Value *Val = Value.getScalarVal()) {
        MDStmtMappings.push_back(
          MDWriter.getMetadataFor(
            ::seec::clang::StmtMapping::forRValScalar(S, Val)));
      }
      else {
        if (SEEC_CLANG_DEBUG)
          llvm::errs() << "scalar: null getScalarVal()!\n";
      }
    }
    else if (Value.isComplex()) {
      auto const Vals = Value.getComplexVal();
      if (Vals.first && Vals.second) {
        MDStmtMappings.push_back(
          MDWriter.getMetadataFor(
            ::seec::clang::StmtMapping::forRValScalar(S, Vals.first,
                                                         Vals.second)));
      }
      else if (SEEC_CLANG_DEBUG) {
        llvm::errs() << "complex: null values in getComplexVal()!\n";
      }
    }
    else if (Value.isAggregate()) {
      if (llvm::Value *Addr = Value.getAggregatePointer()) {
        MDStmtMappings.push_back(
          MDWriter.getMetadataFor(
            ::seec::clang::StmtMapping::forRValAggregate(S, Addr)));
      }
      else {
        if (SEEC_CLANG_DEBUG)
          llvm::errs() << "aggregate: null getAggregateAddr()!\n";
      }
    }
  }

  /// \brief Mark a parameter Decl.
  ///
  /// \param Param The parameter's declaration.
  /// \param Pointer Pointer to the parameter's storage.
  ///
  void markParameter(VarDecl const &Param, llvm::Value *Pointer) {
    MDParamMappings.push_back(
      MDWriter.getMetadataFor(
        ::seec::clang::ParamMapping(&Param, Pointer)));
  }
  
  /// \brief Mark a local variable Decl.
  ///
  /// \param TheDecl The local's declaration.
  /// \param Address The local's address.
  ///
  void markLocal(VarDecl const &TheDecl, llvm::Value *Pointer) {
    MDLocalMappings.push_back(
      MDWriter.getMetadataFor(
        ::seec::clang::LocalMapping(&TheDecl, Pointer)));
  }
};

/// \brief Convenience class that pushes a Stmt for the object's lifetime.
///
class PushStmtForScope {
private:
  MetadataInserter &MDInserter;

  bool const Pushed;

  PushStmtForScope(PushStmtForScope const &Other);
  PushStmtForScope & operator=(PushStmtForScope const &RHS);

public:
  PushStmtForScope(MetadataInserter &MDInserter, Stmt const *S)
  : MDInserter(MDInserter),
    Pushed(S)
  {
    if (Pushed)
      MDInserter.pushStmt(S);
  }

  ~PushStmtForScope() {
    if (Pushed)
      MDInserter.popStmt();
  }
};

/// \brief Convenience class that pushes a Decl for the object's lifetime.
///
class PushDeclForScope {
private:
  MetadataInserter &MDInserter;

  bool const Pushed;

  PushDeclForScope(PushDeclForScope const &Other);
  PushDeclForScope & operator=(PushDeclForScope const &RHS);

public:
  PushDeclForScope(MetadataInserter &MDInserter, Decl const *D)
  : MDInserter(MDInserter),
    Pushed(D)
  {
    if (Pushed)
      MDInserter.pushDecl(D);
  }

  ~PushDeclForScope() {
    if (Pushed)
      MDInserter.popDecl();
  }
};

} // namespace clang::CodeGen::seec

} // namespace clang::CodeGen

} // namespace clang

#endif // define CLANG_CODEGEN_SEECBUILDER_H
