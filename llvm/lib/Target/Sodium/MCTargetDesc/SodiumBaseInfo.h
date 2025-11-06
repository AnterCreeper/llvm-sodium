//===- SodiumBaseInfo.h - Top level definitions for Sodium MC ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains small standalone enum definitions for the Sodium target
// useful for the compiler back-end and the MC libraries.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_SODIUMBASEINFO_H
#define LLVM_SODIUM_SODIUMBASEINFO_H

#include "MCTargetDesc/SodiumMCTargetDesc.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/TargetParser/SubtargetFeature.h"

namespace llvm {

// SODIUMII - This namespace holds all of the target specific flags that
// instruction info tracks. All definitions must match SodiumInstrFormats.td.
namespace SODIUMII {
// Sodium Specific Machine Operand Flags
enum {
  MO_None = 0,
  MO_CALL = 1,
  MO_LO = 2,
  MO_PCREL_LO = 3,
  MO_HI = 4,
  MO_PCREL_HI = 5,
};

enum {
  InstFMT_Pseudo = 0,
  InstFMT_B   = 1,
  InstFMT_HT  = 3,
  InstFMT_LS  = 5,
  InstFMT_R   = 7,
  InstFMT_J   = 9,
  InstFMT_LI  = 11,
  InstFMT_SR  = 13,
  InstFMT_I   = 15,
  InstFMTMask = 15,
  InstFMTShift = 0,
};
// Helper functions to read TSFlags.
/// \returns the format of the instruction.
static inline unsigned getFormat(uint64_t TSFlags) {
  return (TSFlags & InstFMTMask) >> InstFMTShift;
}

} // namespace SODIUMII
} // namespace llvm

#endif
