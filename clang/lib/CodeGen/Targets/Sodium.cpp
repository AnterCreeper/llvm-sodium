//===- Sodium.cpp ---------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"
#include <cstdio>

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// Sodium ABI Implementation
//===----------------------------------------------------------------------===//

namespace {

class SodiumABIInfo : public DefaultABIInfo {
public:
  SodiumABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

};

class SodiumTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  SodiumTargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<SodiumABIInfo>(CGT)) {}
  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &M) const override;

};

} // end namespace

void SodiumTargetCodeGenInfo::setTargetAttributes(
    const Decl *D, llvm::GlobalValue *GV, CodeGen::CodeGenModule &M) const {
  if (const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(D)) {
    const auto *InterruptAttr = FD->getAttr<SodiumInterruptAttr>();
    if (!InterruptAttr)
      return;
    llvm::Function *Fn = cast<llvm::Function>(GV);
    Fn->addFnAttr("interrupt");
  }
}

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createSodiumTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<SodiumTargetCodeGenInfo>(CGM.getTypes());
}
