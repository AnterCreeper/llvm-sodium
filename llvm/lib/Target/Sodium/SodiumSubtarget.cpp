//===-- SodiumSubtarget.cpp - Sodium Subtarget Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Sodium specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "SodiumSubtarget.h"
#include "SodiumTargetMachine.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "sodium-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "SodiumGenSubtargetInfo.inc"

void SodiumSubtarget::anchor() {}

SodiumSubtarget &
SodiumSubtarget::initializeSubtargetDependencies(const Triple &TT, StringRef CPU,
                                                StringRef TuneCPU, StringRef FS) {
  // Determine default and user-specified characteristics
  if (CPU.empty()) CPU = "generic";
  if (TuneCPU.empty())
    TuneCPU = CPU;
  ParseSubtargetFeatures(CPU, TuneCPU, FS);
  return *this;
}

SodiumSubtarget::SodiumSubtarget(const Triple &TT, StringRef CPU,
                                 StringRef TuneCPU, StringRef FS,
                                 const SodiumTargetMachine &TM)
    : SodiumGenSubtargetInfo(TT, CPU, /*TuneCPU*/ CPU, FS),
      TLInfo(TM, *this), FrameLowering(*this), InstrInfo(*this), RegInfo(*this),
      is32Bit(TM.is32Mode()) {
  if (TM.is32Mode()) {
    XLenVT = MVT::v2i16;
  }
  LLVM_DEBUG(dbgs() << "\nis32Bit:" << TM.is32Mode() << "\n");
}
