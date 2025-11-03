//===-- SodiumTargetInfo.cpp - Sodium Target Implementation ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/SodiumTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

Target &llvm::getTheSodium16Target() {
  static Target TheSodium16Target;
  return TheSodium16Target;
}
Target &llvm::getTheSodium32Target() {
  static Target TheSodium32Target;
  return TheSodium32Target;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSodiumTargetInfo() {
  RegisterTarget<Triple::sodium16, /*HasJIT=*/false> X(
      getTheSodium16Target(), "sodium16", "Sodium16 Backend", "Sodium");
  RegisterTarget<Triple::sodium32, /*HasJIT=*/false> Y(
      getTheSodium32Target(), "sodium32", "Sodium32 Backend", "Sodium");
}
