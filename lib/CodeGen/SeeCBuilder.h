//===-- SeeCBuilder.h - SeeC IRBuilder --------------------------*- C++ -*-===//
//
//===----------------------------------------------------------------------===//

#ifndef CLANG_CODEGEN_SEECBUILDER_H
#define CLANG_CODEGEN_SEECBUILDER_H

#include "llvm/IRBuilder.h"
#include "llvm/LLVMContext.h"
#include "llvm/Metadata.h"
#include "llvm/Module.h"
#include "llvm/ADT/SmallVector.h"

#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"
#include "clang/CodeGen/SeeCMapping.h"

#include "CGValue.h"

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

  /// Kind of metadata we attach to llvm::Instructions.
  unsigned MDMapKindID;

  /// Stack of the current clang::Stmt pointers.
  llvm::SmallVector<Stmt const *, 32> StmtStack;
  
  /// All known mappings.
  std::vector< ::seec::clang::StmtMapping> Mappings;
  
  
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
    MDMapKindID(Context.getMDKindID("seec.clang.stmt.ptr")),
    StmtStack(),
    Mappings()
  {
    if (SEEC_CLANG_DEBUG)
      llvm::errs() << "MetadataInserter()\n";
  }
  
  ~MetadataInserter() {
    if (SEEC_CLANG_DEBUG)
      llvm::errs() << "~MetadataInserter()\n";
    
    // Creates metadata to describe StmtMapping objects.
    ::seec::clang::StmtMapping::MetadataWriter MDWriter(Context);
    
    llvm::NamedMDNode *GlobalMD
      = Module.getOrInsertNamedMetadata(::seec::clang::StmtMapping::getGlobalMDNameForMapping());
    
    typedef std::vector< ::seec::clang::StmtMapping>::iterator IterTy;
    for (IterTy It = Mappings.begin(), End = Mappings.end(); It != End; ++It) {
      GlobalMD->addOperand(MDWriter.getMetadataFor(*It));
    }
  }

  void pushStmt(Stmt const *S) {
    StmtStack.push_back(S);
  }

  void popStmt() {
    StmtStack.pop_back();
  }

  void attachMetadata(llvm::Instruction *I) {
    if (!StmtStack.empty()) {
      // Make a constant int holding the address of the current Stmt
      uintptr_t PtrInt = reinterpret_cast<uintptr_t>(StmtStack.back());
      llvm::Type *i64 = llvm::Type::getInt64Ty(Context);
      llvm::Value *StmtAddr = llvm::ConstantInt::get(i64, PtrInt);
      I->setMetadata(MDMapKindID, llvm::MDNode::get(Context, StmtAddr));
    }
  }

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
        Mappings.push_back(::seec::clang::StmtMapping::forLValSimple(S, Addr));
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
        Mappings.push_back(::seec::clang::StmtMapping::forRValScalar(S, Val));
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
        Mappings.push_back(::seec::clang::StmtMapping::forRValAggregate(S,
                                                                        Addr));
      }
      else {
        if (SEEC_CLANG_DEBUG)
          llvm::errs() << "aggregate: null getAggregateAddr()!\n";
      }
    }
  }
};

class PushStmtForScope {
private:
  MetadataInserter &MDInserter;

  PushStmtForScope(PushStmtForScope const &Other);
  PushStmtForScope & operator= (PushStmtForScope const &RHS);

public:
  PushStmtForScope(MetadataInserter &MDInserter, Stmt const *S)
  : MDInserter(MDInserter)
  {
    MDInserter.pushStmt(S);
  }

  ~PushStmtForScope() {
    MDInserter.popStmt();
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
