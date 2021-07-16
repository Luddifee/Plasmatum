#include "context.hh"
#include "defines.hh"
#include "lib.hh"

#include <iostream>
#include <llvm/IR/Verifier.h>

void PlsmContext::initLogicals() {
  auto nullLogical = getNullLogical();
  auto intLogical = getIntLogical();
  auto floatLogical = getFloatLogical();

  std::vector<llvm::Constant *> funcs = {nullLogical, intLogical, floatLogical};
  auto arrT =
      llvm::ArrayType::get(logicalFunctionType->getPointerTo(), funcs.size());
  auto arr = llvm::ConstantArray::get(arrT, funcs);

  logicalFuncs = new llvm::GlobalVariable(
      module, arrT, true, llvm::GlobalVariable::ExternalLinkage, arr);
}

llvm::Function *PlsmContext::getNullLogical() {
  auto name = "null_logical";

  llvm::Function *f = module.getFunction(name);
  if (f)
    return f;

  f = llvm::Function::Create(logicalFunctionType,
                             llvm::Function::ExternalLinkage, name, module);

  auto bb = llvm::BasicBlock::Create(context, "", f);
  builder.SetInsertPoint(bb);

  auto result = llvm::ConstantInt::get(i1Type, 0);
  builder.CreateRet(result);

  return f;
}

llvm::Function *PlsmContext::getIntLogical() {
  auto name = "int_logical";

  llvm::Function *f = module.getFunction(name);
  if (f)
    return f;

  f = llvm::Function::Create(logicalFunctionType,
                             llvm::Function::ExternalLinkage, name, module);

  auto bb = llvm::BasicBlock::Create(context, "", f);
  builder.SetInsertPoint(bb);

  auto alloca = builder.CreateAlloca(plsmType);
  builder.CreateStore(f->getArg(0), alloca);

  auto valuePtrEP = builder.CreateStructGEP(alloca, 1);
  auto valuePtr = (llvm::Value *)builder.CreateLoad(pointerType, valuePtrEP);

  valuePtr = builder.CreatePointerCast(valuePtr, intType->getPointerTo());
  auto intValue = builder.CreateLoad(intType, valuePtr);

  auto result = builder.CreateICmpNE(intValue, getInt(0));
  builder.CreateRet(result);

  return f;
}

llvm::Function *PlsmContext::getFloatLogical() {
  auto name = "float_logical";

  llvm::Function *f = module.getFunction(name);
  if (f)
    return f;

  f = llvm::Function::Create(logicalFunctionType,
                             llvm::Function::ExternalLinkage, name, module);

  auto bb = llvm::BasicBlock::Create(context, "", f);
  builder.SetInsertPoint(bb);

  auto alloca = builder.CreateAlloca(plsmType);
  builder.CreateStore(f->getArg(0), alloca);

  auto valuePtrEP = builder.CreateStructGEP(alloca, 1);
  auto valuePtr = (llvm::Value *)builder.CreateLoad(pointerType, valuePtrEP);

  valuePtr = builder.CreatePointerCast(valuePtr, floatingPointType->getPointerTo());
  auto floatValue = builder.CreateLoad(floatingPointType, valuePtr);

  auto result = builder.CreateFCmpUNE(floatValue, getFloat(0));
  builder.CreateRet(result);

  return f;
}

llvm::Function *PlsmContext::getStringLogical() { return nullptr; }

llvm::Constant *PlsmContext::getI64(int64_t value) {
  return llvm::ConstantInt::get(i64Type, value);
}

llvm::Constant *PlsmContext::getInt(int64_t value) {
  return llvm::ConstantInt::get(intType, value);
}

llvm::Constant *PlsmContext::getFloat(double value) {
  return llvm::ConstantFP::get(floatingPointType, value);
}

int64_t PlsmContext::getTypeSize(llvm::Type *type) {
  return dataLayout.getTypeAllocSize(type);
}

llvm::Value *PlsmContext::createMalloc(llvm::Type *resultType,
                                       int64_t numElements) {
  auto arg = getI64(getTypeSize(resultType) * numElements);

  auto result = (llvm::Value *)builder.CreateCall(mallocFunction, {arg});

  return builder.CreatePointerCast(result, resultType->getPointerTo());
}

llvm::Value *PlsmContext::getPlsmValue(int8_t type, llvm::Value *valuePointer) {
  auto alloca = builder.CreateAlloca(plsmType);

  auto typeEP = builder.CreateStructGEP(alloca, 0);
  auto valuePointerEP = builder.CreateStructGEP(alloca, 1);

  valuePointer = builder.CreatePointerCast(valuePointer, pointerType);

  builder.CreateStore(llvm::ConstantInt::get(typeType, type), typeEP);
  builder.CreateStore(valuePointer, valuePointerEP);

  return builder.CreateLoad(plsmType, alloca);
}

llvm::Value *PlsmContext::getPlsmNull() {
  auto typeValue = llvm::ConstantInt::get(typeType, TYPE_NULL);
  auto nullPtr = llvm::ConstantPointerNull::get(pointerType);

  std::vector<llvm::Constant *> attrs = {typeValue, nullPtr};
  return llvm::ConstantStruct::get(plsmType, attrs);
}

llvm::Value *PlsmContext::getPlsmInt(int64_t value) {
  auto ptr = createMalloc(intType);

  builder.CreateStore(getInt(value), ptr);

  return getPlsmValue(TYPE_INT, ptr);
}

llvm::Value *PlsmContext::getPlsmFloat(double value) {
  auto ptr = createMalloc(floatingPointType);

  builder.CreateStore(getFloat(value), ptr);

  return getPlsmValue(TYPE_FLOAT, ptr);
}

llvm::Value *PlsmContext::getPlsmString(const std::u32string &string) {
  std::vector<llvm::Constant *> chars;
  for (auto &c : string) {
    chars.push_back(llvm::ConstantInt::get(charType, c));
  }

  chars.push_back(llvm::ConstantInt::get(charType, 0));

  auto arrT = llvm::ArrayType::get(charType, string.size() + 1);
  auto str = llvm::ConstantArray::get(arrT, chars);

  auto ptr = createMalloc(arrT);

  builder.CreateStore(str, ptr);

  return getPlsmValue(TYPE_STRING, ptr);
}

llvm::Value *PlsmContext::getPlsmLogicalValue(llvm::Value *value) {
  auto alloca = builder.CreateAlloca(plsmType);
  builder.CreateStore(value, alloca);
  auto typeEP = builder.CreateStructGEP(alloca, 0);
  auto logicalIdx = builder.CreateLoad(typeType, typeEP);

  auto logicalFuncEP = builder.CreateGEP(logicalFuncs, {getI64(0), logicalIdx});
  auto tmpFunc =
      builder.CreateLoad(logicalFunctionType->getPointerTo(), logicalFuncEP);

  auto func = llvm::FunctionCallee(logicalFunctionType, tmpFunc);
  return builder.CreateCall(func, {value});
}

llvm::Function *PlsmContext::getMain() {
  if (mainFunction)
    return mainFunction;
  else if (!functions.count("main"))
    return nullptr;

  auto retT = llvm::IntegerType::getInt8Ty(context);
  auto ft = llvm::FunctionType::get(retT, false);
  auto f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "main",
                                  module);

  auto bb = llvm::BasicBlock::Create(context, "", f);

  builder.SetInsertPoint(bb);

  auto callExpr = new CallExpr("main", {});
  callExpr->genCode(*this);

  builder.CreateRet(llvm::ConstantInt::get(retT, 42));

  return f;
}

llvm::Value *PlsmContext::createRet(llvm::Value *value) {
  return builder.CreateRet(value);
}

llvm::Value *PlsmContext::createPlsmFunction(const std::string &id,
                                             std::vector<Stmt *> body) {
  auto f = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage,
                                  "", module);

  auto bb = llvm::BasicBlock::Create(context, "", f);

  builder.SetInsertPoint(bb);

  for (auto &stmt : body) {
    stmt->genCode(*this);
  }

  if (llvm::verifyFunction(*f)) {
    builder.CreateRet(getPlsmNull());
  }

  functions[id] = f;

  return f;
}

llvm::Value *PlsmContext::createPlsmCall(const std::string &id,
                                         std::vector<Expr *> args) {
  auto f = functions[id];
  if (!f) {
    std::cout << "unable to find function '" << id << "'" << std::endl;
  }

  auto argCount = llvm::ConstantInt::get(intType, args.size());
  std::vector<llvm::Value *> callArgs = {argCount};

  if (args.size() > 0) {
    auto indexType = llvm::Type::getInt64Ty(context);

    auto arraySize = llvm::ConstantInt::get(indexType, args.size());
    auto array = builder.CreateAlloca(plsmType, arraySize);

    for (size_t i = 0; i < args.size(); i++) {
      auto idx = llvm::ConstantInt::get(indexType, i);
      auto ptr = builder.CreateGEP(array, idx);

      auto value = args[i]->genCode(*this);

      builder.CreateStore(value, ptr);
    }
    callArgs.push_back(array);
  } else {
    callArgs.push_back(
        llvm::ConstantPointerNull::get(plsmType->getPointerTo()));
  }

  return builder.CreateCall(f, callArgs);
}

llvm::Value *PlsmContext::createPlsmIf(Expr *condExpr, Expr *trueExpr,
                                       Expr *falseExpr) {
  auto condV = condExpr->genCode(*this);
  condV = getPlsmLogicalValue(condV);

  auto f = builder.GetInsertBlock()->getParent();

  auto trueBB = llvm::BasicBlock::Create(context, "", f);
  auto falseBB = llvm::BasicBlock::Create(context, "", f);
  auto mergeBB = llvm::BasicBlock::Create(context, "", f);

  builder.CreateCondBr(condV, trueBB, falseBB);

  builder.SetInsertPoint(trueBB);
  auto trueV = trueExpr->genCode(*this);
  builder.CreateBr(mergeBB);

  builder.SetInsertPoint(falseBB);
  auto falseV = falseExpr->genCode(*this);
  builder.CreateBr(mergeBB);

  falseBB = builder.GetInsertBlock();

  builder.SetInsertPoint(mergeBB);

  auto phiNode = builder.CreatePHI(plsmType, 2);
  phiNode->addIncoming(trueV, trueBB);
  phiNode->addIncoming(falseV, falseBB);

  return phiNode;
}

llvm::ExecutionEngine &PlsmContext::getExecutionEngine() {
  auto &result = *(llvm::EngineBuilder(std::unique_ptr<llvm::Module>(&module))
                       .setEngineKind(llvm::EngineKind::JIT)
                       .create());

  result.DisableSymbolSearching();

  result.addGlobalMapping(mallocFunction, (void *)&malloc);

  result.addGlobalMapping(functions["print"], (void *)&print);
  result.addGlobalMapping(functions["println"], (void *)&println);

  result.finalizeObject();
  return result;
}

void PlsmContext::printLLVMIR() { module.print(llvm::errs(), nullptr); }
