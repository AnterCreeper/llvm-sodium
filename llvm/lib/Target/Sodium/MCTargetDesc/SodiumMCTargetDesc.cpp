//===-- SodiumMCTargetDesc.cpp - Sodium Target Descriptions ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides Sodium specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "SodiumMCTargetDesc.h"
#include "SodiumInstPrinter.h"
#include "SodiumMCAsmInfo.h"
#include "TargetInfo/SodiumTargetInfo.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MachineLocation.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "SodiumGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "SodiumGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "SodiumGenRegisterInfo.inc"

static MCRegisterInfo *createSodiumMCRegisterInfo(const Triple &TT) {
    MCRegisterInfo *X = new MCRegisterInfo();
    InitSodiumMCRegisterInfo(X, Sodium::X2); //defined in generated SodiumGenRegisterInfo.inc, Sodium:X2 stands for $ra
    return X;
}

static MCInstrInfo *createSodiumMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitSodiumMCInstrInfo(X); //defined in generated SodiumGenInstrInfo.inc
  return X;
}

static MCSubtargetInfo *createSodiumMCSubtargetInfo(const Triple &TT,
                                                    StringRef CPU, StringRef FS) {
  return createSodiumMCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS); //defined in generated SodiumGenSubtargetInfo.inc
}

static MCAsmInfo *createSodiumMCAsmInfo(const MCRegisterInfo &MRI,
                                        const Triple &TT, const MCTargetOptions &Options) {
  MCAsmInfo *MAI = new SodiumMCAsmInfo(TT);
  unsigned SP = MRI.getDwarfRegNum(Sodium::X4, true);
  MCCFIInstruction Inst = MCCFIInstruction::createDefCfaRegister(nullptr, SP);
  MAI->addInitialFrameState(Inst);
  return MAI;
}

static MCInstPrinter *createSodiumMCInstPrinter(const Triple &T,
                                                unsigned SyntaxVariant,
                                                const MCAsmInfo &MAI,
                                                const MCInstrInfo &MII,
                                                const MCRegisterInfo &MRI) {
  return new SodiumInstPrinter(MAI, MII, MRI);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSodiumTargetMC() {
  for (Target *T : {&getTheSodium16Target(), &getTheSodium32Target()}) {
    TargetRegistry::RegisterMCRegInfo(*T, createSodiumMCRegisterInfo);
    TargetRegistry::RegisterMCInstrInfo(*T, createSodiumMCInstrInfo);
    TargetRegistry::RegisterMCSubtargetInfo(*T, createSodiumMCSubtargetInfo);
    TargetRegistry::RegisterMCAsmInfo(*T, createSodiumMCAsmInfo);
    TargetRegistry::RegisterMCInstPrinter(*T, createSodiumMCInstPrinter);
    TargetRegistry::RegisterMCCodeEmitter(*T, createSodiumMCCodeEmitter);
    TargetRegistry::RegisterMCAsmBackend(*T, createSodiumAsmBackend);
  }
}
