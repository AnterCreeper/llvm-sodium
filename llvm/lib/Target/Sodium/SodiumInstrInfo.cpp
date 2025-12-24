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
  if (Opcode == TargetOpcode::INLINEASM ||
      Opcode == TargetOpcode::INLINEASM_BR) {
    const MachineFunction &MF = *MI.getParent()->getParent();
    return getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                              *MF.getTarget().getMCAsmInfo());
  }
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
  unsigned Opcode = Sodium::IntPairRegClass.contains(SrcReg) ? Sodium::SD : Sodium::SW;

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
  unsigned Opcode = Sodium::IntPairRegClass.contains(DstReg) ? Sodium::LD : Sodium::LW;

  MachineMemOperand *MMO = MF->getMachineMemOperand(
    MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOLoad,
    MFI.getObjectSize(FI), MFI.getObjectAlign(FI));

  BuildMI(MBB, I, DL, get(Opcode), DstReg)
      .addFrameIndex(FI)
      .addImm(0)
      .addMemOperand(MMO);
}

void SodiumInstrInfo::parseCondBranch(MachineInstr &LastInst,
                                      MachineBasicBlock *&TBB,
                                      SmallVectorImpl<MachineOperand> &Cond) const {
  // Block ends with fall-through condbranch.
  TBB = LastInst.getOperand(1).getMBB();
  Cond.push_back(MachineOperand::CreateImm(LastInst.getOpcode())); //Cond[0]
  Cond.push_back(LastInst.getOperand(0));                          //Cond[1]
}

void SodiumInstrInfo::instantiateCondBranch(MachineBasicBlock &MBB,
                                            MachineBasicBlock *TBB,
                                            ArrayRef<MachineOperand> Cond,
                                            const DebugLoc &DL) const {
  // ref: void parseCondBranch();
  BuildMI(&MBB, DL, get(Cond[0].getImm())).add(Cond[1]).addMBB(TBB);
}

bool SodiumInstrInfo::reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const {
  unsigned CC = Cond[0].getImm();
  switch (CC) {
  default:
    llvm_unreachable("Unrecognized conditional branch");
  case Sodium::CBZ:
    CC = Sodium::CBNZ;
    break;
  case Sodium::CBNZ:
    CC = Sodium::CBZ;
    break;
  case Sodium::CBGE:
    CC = Sodium::CBLT;
    break;
  case Sodium::CBLT:
    CC = Sodium::CBGE;
    break;
  case Sodium::CBGT:
    CC = Sodium::CBLE;
    break;
  case Sodium::CBLE:
    CC = Sodium::CBGT;
    break;
  }
  // ref: void parseCondBranch();
  Cond[0].setImm(CC);
  return false;
}

bool SodiumInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *&TBB,
                                    MachineBasicBlock *&FBB,
                                    SmallVectorImpl<MachineOperand> &Cond,
                                    bool AllowModify) const {
  TBB = FBB = nullptr;
  Cond.clear();

  // If the block has no terminators, it just falls into the block after it.
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end() || !isUnpredicatedTerminator(*I))
    return false;

  // Count the number of terminators and find the first unconditional or
  // indirect branch.
  MachineBasicBlock::iterator FirstUncondOrIndirectBr = MBB.end();
  int NumTerminators = 0;
  for (auto J = I.getReverse(); J != MBB.rend() && isUnpredicatedTerminator(*J);
       J++) {
    NumTerminators++;
    if (J->getDesc().isUnconditionalBranch() ||
        J->getDesc().isIndirectBranch()) {
      FirstUncondOrIndirectBr = J.getReverse();
    }
  }

  // If AllowModify is true, we can erase any terminators after
  // FirstUncondOrIndirectBR.
  if (AllowModify && FirstUncondOrIndirectBr != MBB.end()) {
    while (std::next(FirstUncondOrIndirectBr) != MBB.end()) {
      std::next(FirstUncondOrIndirectBr)->eraseFromParent();
      NumTerminators--;
    }
    I = FirstUncondOrIndirectBr;
  }

  // We can't handle blocks that end in an indirect branch.
  if (I->getDesc().isIndirectBranch())
    return true;

  // We can't handle blocks with more than 2 terminators.
  if (NumTerminators > 2)
    return true;

  // Handle a single unconditional branch.
  if (NumTerminators == 1 && I->getDesc().isUnconditionalBranch()) {
    TBB = getBranchDestBlock(*I);
    return false;
  }

  // Handle a single conditional branch.
  if (NumTerminators == 1 && I->getDesc().isConditionalBranch()) {
    parseCondBranch(*I, TBB, Cond);
    return false;
  }

  // Handle a conditional branch followed by an unconditional branch.
  // e.g. bc.cc x, _flag0
  //      b     _flag1
  //      flag0:
  // =>
  //      bc.nc x, _flag1
  //      flag0:
  if (NumTerminators == 2 && std::prev(I)->getDesc().isConditionalBranch() &&
      I->getDesc().isUnconditionalBranch()) {
    parseCondBranch(*std::prev(I), TBB, Cond);
    FBB = getBranchDestBlock(*I);
    return false;
  }

  // Otherwise, we can't handle this.
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
      BuildMI(&MBB, DL, get(Sodium::PseudoBR)).addMBB(TBB);
    else
      instantiateCondBranch(MBB, TBB, Cond, DL);
    if (BytesAdded) *BytesAdded = 4;
    return 1;
  }
  // Two-way conditional branch.
  instantiateCondBranch(MBB, TBB, Cond, DL);
  BuildMI(&MBB, DL, get(Sodium::PseudoBR)).addMBB(FBB);
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

#define GETBITS(x, n, m) ((x >> n) & ((1 << (m - n + 1)) - 1))

/// This function generates the sequence of instructions needed to load
/// immediate.
void SodiumInstrInfo::movImm(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MBBI,
                             const DebugLoc &DL, Register DstReg, uint64_t Val,
                             MachineInstr::MIFlag Flag, bool is32Bit) const {
  int64_t Lo16 = GETBITS(Val, 0, 15);
  //int64_t Hi16 = GETBITS(Val, 16, 31);
  BuildMI(MBB, MBBI, DL, get(Sodium::LI), DstReg)
      .addImm(Lo16)
      .setMIFlag(Flag);
  if (is32Bit) {
    assert("Don't know how to deal with v2i16!");
  }
}

bool SodiumInstrInfo::isAsCheapAsAMove(const MachineInstr &MI) const {
  const unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  default:
    break;
  case Sodium::ADDI:
  case Sodium::ORI:
  case Sodium::XORI:
    return (MI.getOperand(1).isReg() && MI.getOperand(1).getReg() == Sodium::X0) ||
           (MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0);
  }
  return MI.isAsCheapAsAMove();
}
