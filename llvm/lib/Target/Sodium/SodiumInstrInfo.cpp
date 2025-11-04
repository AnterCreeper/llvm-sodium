//===-- SodiumInstrInfo.cpp - Sodium Instruction Information --------------===//
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

#include "Sodium.h"
#include "SodiumInstrInfo.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "SodiumGenInstrInfo.inc"

// Pin the vtable to this file.
void SodiumInstrInfo::anchor() {}

SodiumInstrInfo::SodiumInstrInfo(SodiumSubtarget &ST)
    : SodiumGenInstrInfo(Sodium::ADJCALLSTACKDOWN, Sodium::ADJCALLSTACKUP),
      Subtarget(ST) {}

unsigned SodiumInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  // Meta-instructions emit no code.
  if (MI.isMetaInstruction())
    return 0;
  unsigned Opcode = MI.getOpcode();
  if (Opcode == TargetOpcode::BUNDLE)
    return getInstBundleLength(MI);

  // Size should be preferably set in XXXInstrInfo.td (default case).
  return MI.getDesc().getSize();
}

unsigned SodiumInstrInfo::getInstBundleLength(const MachineInstr &MI) const {
  unsigned Size = 0;
  MachineBasicBlock::const_instr_iterator I = MI.getIterator();
  MachineBasicBlock::const_instr_iterator E = MI.getParent()->instr_end();
  while (++I != E && I->isInsideBundle()) {
    assert(!I->isBundle() && "No nested bundle!");
    Size += getInstSizeInBytes(*I);
  }
  return Size;
}

void SodiumInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         Register SrcReg, bool isKill, int FI,
                                         const TargetRegisterClass *RC,
                                         const TargetRegisterInfo *TRI,
                                         Register VReg) const {
  DebugLoc DL;
  if (I != MBB.end()) DL = I->getDebugLoc();

  MachineFunction *MF = MBB.getParent();
  const MachineFrameInfo &MFI = MF->getFrameInfo();

  unsigned Opcode;
  Opcode = Sodium::SW;
  assert(Opcode && "Register class not handled!");

  MachineMemOperand *MMO = MF->getMachineMemOperand(
      MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOStore,
      MFI.getObjectSize(FI), MFI.getObjectAlign(FI));

  BuildMI(MBB, I, DL, get(Opcode))
    .addReg(SrcReg, getKillRegState(isKill))
    .addFrameIndex(FI)
    .addImm(0)
    .addMemOperand(MMO);
}

void SodiumInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator I,
                                          Register DstReg, int FI,
                                          const TargetRegisterClass *RC,
                                          const TargetRegisterInfo *TRI,
                                          Register VReg) const {
  DebugLoc DL;
  if (I != MBB.end()) DL = I->getDebugLoc();

  MachineFunction *MF = MBB.getParent();
  const MachineFrameInfo &MFI = MF->getFrameInfo();

  unsigned Opcode;
  Opcode = Sodium::LW;
  assert(Opcode && "Register class not handled!");

  MachineMemOperand *MMO = MF->getMachineMemOperand(
      MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOLoad,
      MFI.getObjectSize(FI), MFI.getObjectAlign(FI));

  BuildMI(MBB, I, DL, get(Opcode), DstReg)
    .addFrameIndex(FI)
    .addImm(0)
    .addMemOperand(MMO);
}

void SodiumInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator I,
                                  const DebugLoc &DL, MCRegister DstReg,
                                  MCRegister SrcReg, bool KillSrc) const {
  if (Sodium::IntRegsRegClass.contains(DstReg, SrcReg)) {
    BuildMI(MBB, I, DL, get(Sodium::ADDI), DstReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addImm(0);
    return;
  }
  assert("copyPhysReg failed!");
}

void SodiumInstrInfo::parseCondBranch(MachineInstr *LastInst,
                                      MachineBasicBlock *&TBB,
                                      SmallVectorImpl<MachineOperand> &Cond) const {
  // Block ends with fall-through condbranch.
  TBB = LastInst->getOperand(1).getMBB();
  Cond.push_back(MachineOperand::CreateImm(LastInst->getOpcode())); //Cond[0]
  Cond.push_back(LastInst->getOperand(0));                          //Cond[1]
}

void SodiumInstrInfo::instantiateCondBranch(MachineBasicBlock &MBB,
                                            MachineBasicBlock *TBB,
                                            ArrayRef<MachineOperand> Cond,
                                            const DebugLoc &DL) const {
  // ref: void parseCondBranch();
  BuildMI(&MBB, DL, get(Cond[0].getImm())).add(Cond[1]).addMBB(TBB);
}

// derived from AArch64 Target.
bool SodiumInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *&TBB,
                                    MachineBasicBlock *&FBB,
                                    SmallVectorImpl<MachineOperand> &Cond,
                                    bool AllowModify) const {
  // If the block has no terminators, it just falls into the block after it.
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end() || !isUnpredicatedTerminator(*I))
    return false;

  // Get the last instruction in the block.
  MachineInstr *LastInst = &*I;

  // If there is only one terminator instruction, process it.
  unsigned LastOpc = LastInst->getOpcode();
  if (I == MBB.begin() || !isUnpredicatedTerminator(*--I)) {
    if (isUncondBranchOpcode(LastOpc)) {
      TBB = LastInst->getOperand(0).getMBB();
      return false;
    }
    if (isCondBranchOpcode(LastOpc)) {
      // Block ends with fall-through condbranch.
      parseCondBranch(LastInst, TBB, Cond);
      return false;
    }
    return true; // Can't handle indirect branch.
  }

  // Get the instruction before it if it is a terminator.
  MachineInstr *SecondLastInst = &*I;
  unsigned SecondLastOpc = SecondLastInst->getOpcode();

  // If AllowModify is true and the block ends with two or more unconditional
  // branches, delete all but the first unconditional branch.
  if (AllowModify && isUncondBranchOpcode(LastOpc)) {
    while (isUncondBranchOpcode(SecondLastOpc)) {
      LastInst->eraseFromParent();
      LastInst = SecondLastInst;
      LastOpc = LastInst->getOpcode();
      if (I == MBB.begin() || !isUnpredicatedTerminator(*--I)) {
        // Return now the only terminator is an unconditional branch.
        TBB = LastInst->getOperand(0).getMBB();
        return false;
      } else {
        SecondLastInst = &*I;
        SecondLastOpc = SecondLastInst->getOpcode();
      }
    }
  }

  // If we're allowed to modify and the block ends in a unconditional branch
  // which could simply fallthrough, remove the branch.  (Note: This case only
  // matters when we can't understand the whole sequence, otherwise it's also
  // handled by BranchFolding.cpp.)
  if (AllowModify && isUncondBranchOpcode(LastOpc) &&
      MBB.isLayoutSuccessor(getBranchDestBlock(*LastInst))) {
    LastInst->eraseFromParent();
    LastInst = SecondLastInst;
    LastOpc = LastInst->getOpcode();
    if (I == MBB.begin() || !isUnpredicatedTerminator(*--I)) {
      assert(!isUncondBranchOpcode(LastOpc) &&
             "unreachable unconditional branches removed above");

      if (isCondBranchOpcode(LastOpc)) {
        // Block ends with fall-through condbranch.
        parseCondBranch(LastInst, TBB, Cond);
        return false;
      }
      return true; // Can't handle indirect branch.
    } else {
      SecondLastInst = &*I;
      SecondLastOpc = SecondLastInst->getOpcode();
    }
  }

  // If there are three terminators, we don't know what sort of block this is.
  if (SecondLastInst && I != MBB.begin() && isUnpredicatedTerminator(*--I))
    return true;

  // If the block ends with a B and a cond B, handle it.
  if (isCondBranchOpcode(SecondLastOpc) && isUncondBranchOpcode(LastOpc)) {
    parseCondBranch(SecondLastInst, TBB, Cond);
    FBB = LastInst->getOperand(0).getMBB();
    return false;
  }

  // If the block ends with two unconditional branches, handle it.  The second
  // one is not executed, so remove it.
  if (isUncondBranchOpcode(SecondLastOpc) && isUncondBranchOpcode(LastOpc)) {
    TBB = SecondLastInst->getOperand(0).getMBB();
    I = LastInst;
    if (AllowModify)
      I->eraseFromParent();
    return false;
  }

  // ...likewise if it ends with an indirect branch followed by an unconditional
  // branch.
  if (isIndirectBranchOpcode(SecondLastOpc) && isUncondBranchOpcode(LastOpc)) {
    I = LastInst;
    if (AllowModify)
      I->eraseFromParent();
    return true;
  }

  // Otherwise, can't handle this.
  return true;
}

// Inserts a branch into the end of the specific MachineBasicBlock, returning
// the number of instructions inserted.
unsigned SodiumInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                       MachineBasicBlock *TBB, MachineBasicBlock *FBB,
                                       ArrayRef<MachineOperand> Cond,
                                       const DebugLoc &DL, int *BytesAdded) const {
  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  // One-way branch.
  if (!FBB) {
    if (Cond.empty()) // Unconditional branch?
      BuildMI(&MBB, DL, get(Sodium::B)).addMBB(TBB);
    else
      instantiateCondBranch(MBB, TBB, Cond, DL);
    if (BytesAdded) *BytesAdded = 4;
    return 1;
  }
  // Two-way conditional branch.
  instantiateCondBranch(MBB, TBB, Cond, DL);
  BuildMI(&MBB, DL, get(Sodium::B)).addMBB(FBB);
  if (BytesAdded) *BytesAdded = 8;
  return 2;
}

unsigned SodiumInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                       int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();

  if (I == MBB.end())
    return 0;
  if (!I->getDesc().isBranch())
    return 0;
  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();
  I = MBB.end();

  if (I == MBB.begin())
    return 1;
  --I;
  if (!I->getDesc().isConditionalBranch())
    return 1;
  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();
  return 2;
}

/// This function generates the sequence of instructions needed to load
/// immediate.
void SodiumInstrInfo::movImm(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MBBI,
                             const DebugLoc &DL, Register DstReg, uint64_t Val,
                             MachineInstr::MIFlag Flag, bool is32Bit) const {
  if (isInt<13>(Val)) {
    BuildMI(MBB, MBBI, DL, get(Sodium::ADDI), DstReg)
      .addReg(Sodium::X0)
      .addImm(Val)
      .setMIFlag(Flag);
  } else {
    int64_t Hi19 = ((Val + 0x1000) >> 13) & (is32Bit ? 0x7FFFF : 0x7);
    int64_t Lo13 = SignExtend32<13>(Val);
    BuildMI(MBB, MBBI, DL, get(Sodium::LUI), DstReg)
      .addImm(Hi19)
      .setMIFlag(Flag);
    if (Lo13 == 0) return;
    BuildMI(MBB, MBBI, DL, get(Sodium::ADDI), DstReg)
      .addReg(DstReg, RegState::Kill)
      .addImm(Lo13)
      .setMIFlag(Flag);
  }
}

bool SodiumInstrInfo::isAsCheapAsAMove(const MachineInstr &MI) const {
  const unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
    default:
      break;
    //mv $?? $zero
    case Sodium::ADDI:
    case Sodium::ORI:
    case Sodium::XORI:
      return (MI.getOperand(1).isReg() &&
        MI.getOperand(1).getReg() == Sodium::X0) ||
        (MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0);
  }
  return MI.isAsCheapAsAMove();
}
