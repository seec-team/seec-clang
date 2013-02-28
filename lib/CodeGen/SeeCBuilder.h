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

#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"
#include "clang/CodeGen/SeeCMapping.h"

#include "CGValue.h"

#include <cassert>

#define SEEC_CLANG_DEBUG 1

namespace clang {

namespace CodeGen {

namespace seec {

/// \brief Helper for adding SeeC-Clang mapping information.
///
class MetadataInserter
{
private:
  /// \brief A reference to a single Stmt or Decl.
  ///
  class NodeRef {
    enum NodeKind { NRDecl, NRStmt };
    
    NodeKind Kind;
    
    union {
      ::clang::Decl const *Declaration;
      
      ::clang::Stmt const *Statement;
    };
    
  public:
    /// \brief Construct a NodeRef for a Decl.
    NodeRef(::clang::Decl const *D)
    : Kind(NRDecl),
      Declaration(D)
    {}
    
    /// \brief Construct a NodeRef for a Stmt.
    NodeRef(::clang::Stmt const *S)
    : Kind(NRStmt),
      Statement(S)
    {}
    
    /// \brief Check if this NodeRef is a Decl.
    bool isDecl() const { return Kind == NRDecl; }
    
    /// \brief Check if this NodeRef is a Stmt.
    bool isStmt() const { return Kind == NRStmt; }
    
    /// \brief Get the Decl from this NodeRef.
    ::clang::Decl const *getDecl() const {
      assert(Kind == NRDecl);
      return Declaration;
    }
    
    /// \brief Get the Stmt from this NodeRef.
    ::clang::Stmt const *getStmt() const {
      assert(Kind == NRStmt);
      return Statement;
    }
  };
    
  //----------------------------------------------------------------------------
  // Members
  //----------------------------------------------------------------------------
  
  /// The llvm::Module being created.
  llvm::Module &Module;
  
  /// The LLVMContext we're working with.
  llvm::LLVMContext &Context;

  /// Kind of metadata for pointers to Stmt.
  unsigned MDKindIDForStmtPtr;
  
  /// Kind of metadata for pointers to Delc.
  unsigned MDKindIDForDeclPtr;

  /// Stack of the current node references.
  llvm::SmallVector<NodeRef, 32> NodeStack;
  
  /// Creates metadata to describe statement mappings.
  ::seec::clang::StmtMapping::MetadataWriter MDWriter;
  
  /// Metadata for all known mappings.
  std::vector< ::llvm::MDNode *> MDMappings;
  
  
  //----------------------------------------------------------------------------
  // Methods
  //----------------------------------------------------------------------------

  /// \brief Get a new MDNode, which has all the operands of Node, plus Value.
  static llvm::MDNode *addOperand(llvm::MDNode *Node, llvm::Value *Value) {
    llvm::SmallVector<llvm::Value *, 8> Operands;

    unsigned NumOperands = Node->getNumOperands();

    for (unsigned i = 0; i < NumOperands; ++i)
      Operands.push_back(Node->getOperand(i));

    Operands.push_back(Value);

    return llvm::MDNode::get(Node->getContext(), Operands);
  }
  
public:
  MetadataInserter(llvm::Module &TheModule)
  : Module(TheModule),
    Context(Module.getContext()),
    MDKindIDForStmtPtr(Context.getMDKindID("seec.clang.stmt.ptr")),
    MDKindIDForDeclPtr(Context.getMDKindID("seec.clang.decl.ptr")),
    NodeStack(),
    MDWriter(Context),
    MDMappings()
  {
    if (SEEC_CLANG_DEBUG)
      llvm::errs() << "MetadataInserter()\n";
  }
  
  ~MetadataInserter() {
    if (SEEC_CLANG_DEBUG)
      llvm::errs() << "~MetadataInserter()\n";
    
    llvm::NamedMDNode *GlobalMD
      = Module.getOrInsertNamedMetadata(
        ::seec::clang::StmtMapping::getGlobalMDNameForMapping());
    
    typedef std::vector< ::llvm::MDNode *>::iterator IterTy;
    for (IterTy It = MDMappings.begin(), End = MDMappings.end();
         It != End; ++It) {
      GlobalMD->addOperand(*It);
    }
  }
  
  void pushDecl(Decl const *D) {
    assert(D && "Pushing null Decl.");
    
    if (SEEC_CLANG_DEBUG) {
      for (std::size_t i = 0; i < NodeStack.size(); ++i)
        llvm::outs() << " ";
      llvm::outs() << "Decl " << D->getDeclKindName() << "\n";
    }

    NodeStack.push_back(NodeRef(D));
  }
  
  void popDecl() {
    assert(NodeStack.size() && NodeStack.back().isDecl());
    NodeStack.pop_back();
  }

  void pushStmt(Stmt const *S) {
    assert(S && "Pushing null Stmt.");
    
    if (SEEC_CLANG_DEBUG) {
      for (std::size_t i = 0; i < NodeStack.size(); ++i)
        llvm::outs() << " ";
      llvm::outs() << "Stmt " << S->getStmtClassName() << "\n";
    }

    NodeStack.push_back(NodeRef(S));
  }

  void popStmt() {
    assert(NodeStack.size() && NodeStack.back().isStmt());
    NodeStack.pop_back();
  }

  void attachMetadata(llvm::Instruction *I) {
    if (NodeStack.empty())
      return;
    
    NodeRef const &Node = NodeStack.back();
    
    if (Node.isStmt()) {
      // Make a constant int holding the address of the Stmt.
      uintptr_t const PtrInt = reinterpret_cast<uintptr_t>(Node.getStmt());
      llvm::Type *i64 = llvm::Type::getInt64Ty(Context);
      llvm::Value *StmtAddr = llvm::ConstantInt::get(i64, PtrInt);
      I->setMetadata(MDKindIDForStmtPtr, llvm::MDNode::get(Context, StmtAddr));
    }
    else if (Node.isDecl()) {
      // Make a constant int holding the address of the Decl.
      uintptr_t const PtrInt = reinterpret_cast<uintptr_t>(Node.getDecl());
      llvm::Type *i64 = llvm::Type::getInt64Ty(Context);
      llvm::Value *DeclAddr = llvm::ConstantInt::get(i64, PtrInt);
      I->setMetadata(MDKindIDForDeclPtr, llvm::MDNode::get(Context, DeclAddr));
    }
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
      if (llvm::Value *Addr = Value.getAddress()) {
        MDMappings.push_back(
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
        MDMappings.push_back(
          MDWriter.getMetadataFor(
            ::seec::clang::StmtMapping::forRValScalar(S, Val)));
      }
      else {
        if (SEEC_CLANG_DEBUG)
          llvm::errs() << "scalar: null getScalarVal()!\n";
      }
    }
    else if (Value.isComplex()) {
      if (SEEC_CLANG_DEBUG)
        llvm::errs() << "complex: not supported!\n";
    }
    else if (Value.isAggregate()) {
      if (llvm::Value *Addr = Value.getAggregateAddr()) {
        MDMappings.push_back(
          MDWriter.getMetadataFor(
            ::seec::clang::StmtMapping::forRValAggregate(S, Addr)));
      }
      else {
        if (SEEC_CLANG_DEBUG)
          llvm::errs() << "aggregate: null getAggregateAddr()!\n";
      }
    }
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
    Pushed(S != nullptr)
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
    Pushed(D != nullptr)
  {
    if (Pushed)
      MDInserter.pushDecl(D);
  }

  ~PushDeclForScope() {
    if (Pushed)
      MDInserter.popDecl();
  }
};

template<bool preserveNames = true>
class IRBuilderInserter
: public llvm::IRBuilderDefaultInserter<preserveNames>
{
private:
  MetadataInserter *MDInserter;

protected:
  void InsertHelper(llvm::Instruction *I,
                    const llvm::Twine &Name,
                    llvm::BasicBlock *BB,
                    llvm::BasicBlock::iterator InsertPt) const {
    llvm::IRBuilderDefaultInserter<preserveNames>::InsertHelper(I, Name, BB,
                                                                InsertPt);
    if (MDInserter)
      MDInserter->attachMetadata(I);
  }

public:
  IRBuilderInserter()
  : MDInserter(0)
  {}

  IRBuilderInserter(MetadataInserter &MDInserter)
  : MDInserter(&MDInserter)
  {}
};

template<bool preserveNames = true>
class SeeCIRBuilder
: public llvm::IRBuilder<preserveNames, llvm::ConstantFolder,
                         IRBuilderInserter<preserveNames> >
{
private:
  typedef llvm::IRBuilder<preserveNames, llvm::ConstantFolder,
                          IRBuilderInserter<preserveNames> > BaseBuilder;

  MetadataInserter &MDInserter;

public:
  SeeCIRBuilder(llvm::LLVMContext &Context,
                MetadataInserter &MDInserter)
  : BaseBuilder(Context, llvm::ConstantFolder(),
                IRBuilderInserter<preserveNames>(MDInserter)),
    MDInserter(MDInserter)
  {}

  explicit SeeCIRBuilder(llvm::BasicBlock *TheBB, MetadataInserter &MDInserter)
  : BaseBuilder(TheBB->getContext(), llvm::ConstantFolder(),
                IRBuilderInserter<preserveNames>(MDInserter)),
    MDInserter(MDInserter)
  {
    BaseBuilder::SetInsertPoint(TheBB);
  }

  llvm::CallInst *CreateMemSet(llvm::Value *Ptr, llvm::Value *Val,
                               uint64_t Size, unsigned Align,
                               bool isVolatile = false,
                               llvm::MDNode *TBAATag = 0) {
    llvm::CallInst *I = BaseBuilder::CreateMemSet(Ptr, Val, Size, Align,
                                                  isVolatile, TBAATag);
    MDInserter.attachMetadata(I);
    return I;
  }

  llvm::CallInst *CreateMemSet(llvm::Value *Ptr, llvm::Value *Val,
                               llvm::Value *Size, unsigned Align,
                               bool isVolatile = false,
                               llvm::MDNode *TBAATag = 0) {
    llvm::CallInst *I = BaseBuilder::CreateMemSet(Ptr, Val, Size, Align,
                                                  isVolatile, TBAATag);
    MDInserter.attachMetadata(I);
    return I;
  }

  llvm::CallInst *CreateMemCpy(llvm::Value *Dst, llvm::Value *Src,
                               uint64_t Size, unsigned Align,
                               bool isVolatile = false,
                               llvm::MDNode *TBAATag = 0,
                               llvm::MDNode *TBAAStructTag = 0) {
    llvm::CallInst *I = BaseBuilder::CreateMemCpy(Dst, Src, Size, Align,
                                                  isVolatile, TBAATag,
                                                  TBAAStructTag);
    MDInserter.attachMetadata(I);
    return I;
  }

  llvm::CallInst *CreateMemCpy(llvm::Value *Dst, llvm::Value *Src,
                               llvm::Value *Size, unsigned Align,
                               bool isVolatile = false,
                               llvm::MDNode *TBAATag = 0,
                               llvm::MDNode *TBAAStructTag = 0) {
    llvm::CallInst *I = BaseBuilder::CreateMemCpy(Dst, Src, Size, Align,
                                                  isVolatile, TBAATag,
                                                  TBAAStructTag);
    MDInserter.attachMetadata(I);
    return I;
  }

  llvm::CallInst *CreateMemMove(llvm::Value *Dst, llvm::Value *Src,
                                uint64_t Size, unsigned Align,
                                bool isVolatile = false,
                               llvm::MDNode *TBAATag = 0) {
    llvm::CallInst *I = BaseBuilder::CreateMemMove(Dst, Src, Size, Align,
                                                   isVolatile, TBAATag);
    MDInserter.attachMetadata(I);
    return I;
  }

  llvm::CallInst *CreateMemMove(llvm::Value *Dst, llvm::Value *Src,
                                llvm::Value *Size, unsigned Align,
                                bool isVolatile = false,
                               llvm::MDNode *TBAATag = 0) {
    llvm::CallInst *I = BaseBuilder::CreateMemMove(Dst, Src, Size, Align,
                                                   isVolatile, TBAATag);
    MDInserter.attachMetadata(I);
    return I;
  }
};

} // namespace clang::CodeGen::seec

} // namespace clang::CodeGen

} // namespace clang

#endif // define CLANG_CODEGEN_SEECBUILDER_H
