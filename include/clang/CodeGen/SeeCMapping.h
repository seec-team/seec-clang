//===-- SeeCMapping.h - SeeC Mapping ----------------------------*- C++ -*-===//
//
//===----------------------------------------------------------------------===//

#ifndef CLANG_CODEGEN_SEECMAPPING_H
#define CLANG_CODEGEN_SEECMAPPING_H

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

#include "clang/AST/Stmt.h"

#include <utility>

namespace seec {
  
namespace clang {

class StmtMapping {
public:
  //----------------------------------------------------------------------------
  // Types
  //----------------------------------------------------------------------------
  
  /// \brief Describes the type of mapping from clang::Stmt to LLVM IR.
  enum MapType {
    LValSimple,   ///< A simple lvalue (an pointer to an object).
    RValScalar,   ///< A scalar rvalue (a value).
    RValAggregate ///< An aggregate rvalue (a pointer to the aggregate).
  };
  
private:
  //----------------------------------------------------------------------------
  // Members
  //----------------------------------------------------------------------------
  
  /// The type of this mapping.
  MapType Type;
  
  /// The mapped clang::Stmt.
  ::clang::Stmt const *Statement;
  
  /// The first llvm::Value that is mapped to.
  llvm::Value *Val1;
  
  /// The (optional) second llvm::Value that is mapped to.
  llvm::Value *Val2;
  
public:
  /// \name Constructors
  /// @{
  
  /// \brief Create a mapping from a clang::Stmt to a single llvm::Value.
  StmtMapping(MapType TheType,
              ::clang::Stmt const *TheStmt,
              llvm::Value *Value)
  : Type(TheType),
    Statement(TheStmt),
    Val1(Value),
    Val2(0)
  {}
  
  /// \brief Create a mapping from a clang::Stmt to two llvm::Value objects.
  StmtMapping(MapType TheType,
              ::clang::Stmt const *TheStmt,
              llvm::Value *Value1,
              llvm::Value *Value2)
  : Type(TheType),
    Statement(TheStmt),
    Val1(Value1),
    Val2(Value2)
  {}
  
  /// \brief Create a mapping for a simple lvalue.
  static StmtMapping forLValSimple(::clang::Stmt const *TheStmt,
                                   llvm::Value *Value1) {
    return StmtMapping(LValSimple, TheStmt, Value1);
  }
  
  /// \brief Create a mapping for a scalar rvalue.
  static StmtMapping forRValScalar(::clang::Stmt const *TheStmt,
                                   llvm::Value *Value1,
                                   llvm::Value *Value2 = 0) {
    return StmtMapping(RValScalar, TheStmt, Value1, Value2);
  }
  
  /// \brief Create a mapping for an aggregate rvalue.
  static StmtMapping forRValAggregate(::clang::Stmt const *TheStmt,
                                      llvm::Value *Value1) {
    return StmtMapping(RValAggregate, TheStmt, Value1);
  }
  
  /// @}
  
  
  /// \name Static information
  /// @{
  
  static char const *getGlobalMDNameForMapping() {
    return "seec.clang.map.ptr";
  }
  
  /// @}
  
  
  /// \name Accessors
  /// @{
  
  MapType getType() const { return Type; }
  
  ::clang::Stmt const *getStmt() const { return Statement; }
  
  llvm::Value *getValue() const { return Val1; }
  
  std::pair<llvm::Value *, llvm::Value *> getValues() const {
    return std::make_pair(Val1, Val2);
  }
  
  /// @} (Accessors)
  
  
  /// \brief Write StmtMapping objects to metadata.
  class MetadataWriter {
    /// The LLVMContext for the metadata this writer will create.
    ::llvm::LLVMContext &Context;
    
    /// String that identifies Argument-type values.
    llvm::MDString *ValueTypeArgument;
    
    /// String that identifies Instruction-type values.
    llvm::MDString *ValueTypeInstruction;
    
    /// String that identifies all other Values.
    llvm::MDString *ValueTypeValue;
    
    /// \brief Get an identifier for V that can be stored in an MDNode.
    ::llvm::Value *getMapForValue(::llvm::Value *V) const {
      if (!V)
        return V;
      
      // We identify Arguments by storing their argument number.
      if (::llvm::Argument *Arg = ::llvm::dyn_cast< ::llvm::Argument>(V)) {
        ::llvm::Type *i32 = ::llvm::Type::getInt32Ty(Context);
        
        ::llvm::Value *Operands[] = {
          ValueTypeArgument,
          Arg->getParent(), // Containing ::llvm::Function.
          ::llvm::ConstantInt::get(i32, Arg->getArgNo())
        };
        
        return ::llvm::MDNode::get(Context, Operands);
      }
      
      // We identify the Instructions by storing their memory address as a 64
      // bit constant integer. After the compilation has been completed, we'll
      // find this and update it to use the Instruction's index in the Function.
      if (::llvm::Instruction *I = ::llvm::dyn_cast< ::llvm::Instruction>(V)) {
        ::llvm::Type *i64 = ::llvm::Type::getInt64Ty(Context);
        
        ::llvm::Value *Operands[] = {
          ValueTypeInstruction,
          I->getParent()->getParent(), // Containing ::llvm::Function.
          ::llvm::ConstantInt::get(i64, reinterpret_cast<uintptr_t>(I))
        };
        
        return ::llvm::MDNode::get(Context, Operands);
      }
      
      // All other Value types should be safe to store as they are.
      ::llvm::Value *Operands[] = { ValueTypeValue, V };
      return ::llvm::MDNode::get(Context, Operands);
    }
    
    /// \brief Get a string identifying the given MapType.
    ::llvm::MDString *getMapTypeString(MapType Type) const {
      switch (Type) {
        case LValSimple:
          return ::llvm::MDString::get(Context, "lvalsimple");
        case RValScalar:
          return ::llvm::MDString::get(Context, "rvalscalar");
        case RValAggregate:
          return ::llvm::MDString::get(Context, "rvalaggregate");
      }
      
      return ::llvm::MDString::get(Context, "invalid");
    }
    
  public:
    /// \brief Construct a new MetadataWriter for the given LLVMContext.
    MetadataWriter(::llvm::LLVMContext &ForContext)
    : Context(ForContext),
      ValueTypeArgument(llvm::MDString::get(Context, "argument")),
      ValueTypeInstruction(llvm::MDString::get(Context, "instruction")),
      ValueTypeValue(llvm::MDString::get(Context, "value"))
    {}
    
    /// \brief Get an ::llvm::MDNode describing the given StmtMapping.
    ::llvm::MDNode *getMetadataFor(StmtMapping const &Mapping) {
      // Get a string identifying the mapping type.
      ::llvm::MDString *MapStr = getMapTypeString(Mapping.getType());
      
      // Make a constant int holding the address of the Stmt.
      uintptr_t PtrInt = reinterpret_cast<uintptr_t>(Mapping.getStmt());
      ::llvm::Type *i64 = ::llvm::Type::getInt64Ty(Context);
      ::llvm::Value *StmtAddr = ::llvm::ConstantInt::get(i64, PtrInt);
      
      ::llvm::Value *Operands[] = {
        MapStr,
        StmtAddr,
        getMapForValue(Mapping.getValue()),
        getMapForValue(Mapping.getValues().second)
      };
      
      return llvm::MDNode::get(Context, Operands);
    }
  };
};

} // namespace clang

} // namespace seec

#endif // CLANG_CODEGEN_SEECMAPPING_H
