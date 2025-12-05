//===-- SodiumFrameLowering.h - Define frame lowering for Sodium *- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Sodium implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_SODIUMFRAMELOWERING_H
#define LLVM_SODIUM_SODIUMFRAMELOWERING_H

#include "Sodium.h"
#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {
class SodiumSubtarget;
class SodiumFrameLowering : public TargetFrameLowering {
protected:
  const SodiumSubtarget &STI;

public:
  explicit SodiumFrameLowering(const SodiumSubtarget &STI)
  : TargetFrameLowering(StackGrowsDown,
                        /*StackAlignment=*/Align(16),
                        /*LocalAreaOffset=*/0,
                        /*TransientStackAlignment=*/Align(16)),
                        STI(STI) {}

  /// emitProlog/emitEpilog - These methods insert prolog and epilog code into
  /// the function.
  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  /*
  // custom Spill and Restore to generate pair loadstore ops
  bool spillCalleeSavedRegisters(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 ArrayRef<CalleeSavedInfo> CSI,
                                 const TargetRegisterInfo *TRI) const override;
  bool restoreCalleeSavedRegisters(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MI,
                                   MutableArrayRef<CalleeSavedInfo> CSI,
                                   const TargetRegisterInfo *TRI) const override;
  */

  bool hasReservedCallFrame(const MachineFunction &MF) const override;
  void determineCalleeSaves(MachineFunction &MF, BitVector &SavedRegs,
                            RegScavenger *RS = nullptr) const override;

  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF,
                                MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I) const override;

  bool hasFP(const MachineFunction &MF) const override;

private:
  void adjustReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                 const DebugLoc &DL, Register DestReg, Register SrcReg,
                 int64_t Val, MachineInstr::MIFlag Flag, bool is32Bit) const;

};
} // End llvm namespace

#endif
