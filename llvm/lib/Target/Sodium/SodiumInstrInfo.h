//===-- SodiumInstrInfo.h - Sodium Instruction Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Sodium implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_SODIUMINSTRINFO_H
#define LLVM_SODIUM_SODIUMINSTRINFO_H

#include "SodiumRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/MachineTraceMetrics.h"
#include <cstdint>

#define GET_INSTRINFO_HEADER
#include "SodiumGenInstrInfo.inc"

namespace llvm {
class SodiumSubtarget;
class SodiumInstrInfo : public SodiumGenInstrInfo {
  virtual void anchor();
  const SodiumSubtarget& Subtarget;

public:
  explicit SodiumInstrInfo(SodiumSubtarget &STI);

  unsigned getInstSizeInBytes(const MachineInstr &MI) const override;

  void storeRegToStackSlot(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI, Register SrcReg,
                           bool IsKill, int FrameIndex,
                           const TargetRegisterClass *RC,
                           const TargetRegisterInfo *TRI,
                           Register VReg) const override;
  void loadRegFromStackSlot(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MBBI, Register DstReg,
                            int FrameIndex, const TargetRegisterClass *RC,
                            const TargetRegisterInfo *TRI,
                            Register VReg) const override;

  void copyPhysReg(MachineBasicBlock &MBB,
                   MachineBasicBlock::iterator MBBI,
                   const DebugLoc &DL, MCRegister DstReg,
                    MCRegister SrcReg, bool KillSrc) const override;

  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const override;
  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL, int *BytesAdded) const override;
  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;

  MachineBasicBlock *getBranchDestBlock(const MachineInstr &MI) const override {
    assert(MI.getDesc().isBranch() && "Unexpected opcode!");
    // The branch target is always the last operand.
    int NumOp = MI.getNumExplicitOperands();
    return MI.getOperand(NumOp - 1).getMBB();
  }

  bool isAsCheapAsAMove(const MachineInstr &MI) const override;

  void movImm(MachineBasicBlock &MBB,
              MachineBasicBlock::iterator MBBI,
              const DebugLoc &DL, Register DstReg, uint64_t Val,
              MachineInstr::MIFlag Flag, bool is32Bit) const;

  MachineTraceStrategy getMachineCombinerTraceStrategy() const override {
    return MachineTraceStrategy::TS_Local;
  }

private:
  unsigned getInstBundleLength(const MachineInstr &MI) const;

  void parseCondBranch(MachineInstr &LastInst, MachineBasicBlock *&TBB,
                       SmallVectorImpl<MachineOperand> &Cond) const;
  void instantiateCondBranch(MachineBasicBlock &MBB,
                             MachineBasicBlock *TBB,
                             ArrayRef<MachineOperand> Cond,
                             const DebugLoc &DL) const;

};

}

#endif
