#include "ast.hh"
#include "context.hh"
#include "lib.hh"
#include "parser.hh"

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

#include <llvm/Support/TargetSelect.h>

#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>

#include <iostream>
#include <vector>

void initLLVM() {
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
}

void printUsage(char *arg0) {
  std::cout << "usage: " << arg0 << " <file>" << std::endl;
}

int main(int argc, char **argv) {
  if (argc <= 0) {
    return 1;
  } else if (argc <= 1) {
    printUsage(argv[0]);
    return 1;
  }

  std::u32string text = readFile(argv[1]);

  Parser parser(text);

  std::vector<Stmt *> stmts;
  Stmt *stmt = nullptr;
  while ((stmt = parser.parseStmt())) {
    stmts.push_back(stmt);
    // std::cout << to_str(stmt->to_string()) << std::endl;
  }

  // int32_t plsm_str[] = {72, 101, 108, 108, 111, 32, 87, 111, 114, 108, 100,
  // 33, 0}; plsm_val args[] = {(plsm_val){TYPE_STRING, (int8_t *)plsm_str}};
  // println(1, args);

  initLLVM();

  PlsmContext context;
  for (auto &tl_stmt : stmts) {
    tl_stmt->genCode(context);
  }

  auto mainFunc = context.getMain();

  // std::cout << "------------------------------------------------" << std::endl;
  context.optimize();
  // context.printLLVMIR();
  // std::cout << "------------------------------------------------" << std::endl;

  auto &engine = context.getExecutionEngine();

  auto address = engine.getFunctionAddress("main");
  auto finalFunc = (int8_t(*)(int))address;
  finalFunc(20);

  return 0;
}
