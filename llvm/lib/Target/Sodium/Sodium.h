//===-- Sodium.h - Top-level interface for Sodium representation *- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// Sodium back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SODIUM_SODIUM_H
#define LLVM_LIB_TARGET_SODIUM_SODIUM_H

#include "MCTargetDesc/SodiumMCTargetDesc.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
class AsmPrinter;
class FunctionPass;
class MCInst;
class MachineInstr;
class PassRegistry;
class SodiumTargetMachine;

void initializeSodiumExpandPseudoPass(PassRegistry &);
void initializeSodiumLoadStoreOptPass(PassRegistry &);
void initializeSodiumDAGToDAGISelPass(PassRegistry &);
FunctionPass *createSodiumExpandPseudoPass();
FunctionPass *createSodiumLoadStoreOptPass();
FunctionPass *createSodiumISelDag(SodiumTargetMachine &TM,
                                  CodeGenOpt::Level OptLevel);

} // namespace llvm

#endif
