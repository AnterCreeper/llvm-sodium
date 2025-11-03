//===-- SodiumMCTargetDesc.h - Sodium Target Descriptions -------*- C++ -*-===//
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

#ifndef LLVM_SODIUM_MCTARGET_DESC_H
#define LLVM_SODIUM_MCTARGET_DESC_H

#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;
class Target;

MCAsmBackend *createSodiumAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                    const MCRegisterInfo &MRI,
                                    const MCTargetOptions &Options);
MCCodeEmitter *createSodiumMCCodeEmitter(const MCInstrInfo &MCII,
                                         MCContext &Ctx);
std::unique_ptr<MCObjectTargetWriter> createSodiumELFObjectWriter(uint8_t OSABI,
                                                                  bool Is64Bit);
} // End llvm namespace

// Defines symbolic names for Sodium registers.  This defines a mapping from
// register name to register number.
//
#define GET_REGINFO_ENUM
#include "SodiumGenRegisterInfo.inc"

// Defines symbolic names for the Sodium instructions.
//
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "SodiumGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "SodiumGenSubtargetInfo.inc"

#endif // LLVM_SODIUM_MCTARGET_DESC_H
