//===-- SeeCBuilder.h - SeeC IRBuilder --------------------------*- C++ -*-===//
//
//===----------------------------------------------------------------------===//

#ifndef CLANG_CODEGEN_SEECBUILDER_H
#define CLANG_CODEGEN_SEECBUILDER_H

#include "llvm/LLVMContext.h"
#include "llvm/Metadata.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IRBuilder.h"

namespace clang {

namespace CodeGen {

namespace seec {

class MetadataInserter
{
private:
  llvm::LLVMContext &Context;
  unsigned MDMapKindID;
  llvm::SmallVector<Stmt const *, 32> StmtStack;

public:
  MetadataInserter(llvm::LLVMContext &Context)
  : Context(Context),
    MDMapKindID(Context.getMDKindID("seec.clang.stmt.ptr")),
    StmtStack()
  {}

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
                               llvm::MDNode *TBAATag = 0) {
    llvm::CallInst *I = BaseBuilder::CreateMemCpy(Dst, Src, Size, Align,
                                                  isVolatile, TBAATag);
    MDInserter.attachMetadata(I);
    return I;
  }

  llvm::CallInst *CreateMemCpy(llvm::Value *Dst, llvm::Value *Src,
                               llvm::Value *Size, unsigned Align,
                               bool isVolatile = false,
                               llvm::MDNode *TBAATag = 0) {
    llvm::CallInst *I = BaseBuilder::CreateMemCpy(Dst, Src, Size, Align,
                                                  isVolatile, TBAATag);
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
