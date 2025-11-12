//===-- SodiumRegisterInfo.cpp - Sodium Register Information ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Sodium implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "SodiumRegisterInfo.h"
#include "Sodium.h"
#include "SodiumSubtarget.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#define GET_REGINFO_TARGET_DESC
#include "SodiumGenRegisterInfo.inc"

using namespace llvm;

#define DEBUG_TYPE "sodium-reginfo"

SodiumRegisterInfo::SodiumRegisterInfo(const SodiumSubtarget &ST)
  : SodiumGenRegisterInfo(ST.is32Bit ? Sodium::D1 : Sodium::X2),
    Subtarget(ST) {}

const MCPhysReg
*SodiumRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  if (MF->getFunction().hasFnAttribute("interrupt")) {
    return CSR_Interrupt_SaveList;
  }
  return CSR_Default_SaveList;
}

const uint32_t
*SodiumRegisterInfo::getNoPreservedMask() const {
  return CSR_NoReg_RegMask;
}
const uint32_t
*SodiumRegisterInfo::getCallPreservedMask(const MachineFunction & MF,
                                          CallingConv::ID CC) const {
  return CSR_Default_RegMask;
}

BitVector SodiumRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  const TargetFrameLowering *TFI = getFrameLowering(MF);

  BitVector Reserved(getNumRegs());
  // Use markSuperRegs to ensure any register aliases are also reserved
  markSuperRegs(Reserved, Sodium::X0); // zero
  markSuperRegs(Reserved, Sodium::X2); // ra
  markSuperRegs(Reserved, Sodium::X4); // sp
  markSuperRegs(Reserved, Sodium::X6); // gp
  if(Subtarget.is32Bit) {
  markSuperRegs(Reserved, Sodium::X3); // ra
  markSuperRegs(Reserved, Sodium::X5); // sp
  markSuperRegs(Reserved, Sodium::X7); // gp
  }
  if (TFI->hasFP(MF)) {
  markSuperRegs(Reserved, Sodium::X8); // fp
  if(Subtarget.is32Bit)
  markSuperRegs(Reserved, Sodium::X9); // fp
  }
  // Also reserve the register pair aliases covering the above
  // registers, with the same conditions.
  markSuperRegs(Reserved, Sodium::D0); // zero32
  markSuperRegs(Reserved, Sodium::D1); // ra32
  markSuperRegs(Reserved, Sodium::D2); // sp32
  markSuperRegs(Reserved, Sodium::D3); // gp32
  if (TFI->hasFP(MF)) {
  markSuperRegs(Reserved, Sodium::D4); // fp32
  }

  assert(checkAllSuperRegsMarked(Reserved));
  return Reserved;
}

bool SodiumRegisterInfo::eliminateFI(MachineBasicBlock::iterator II,
                                     unsigned OpNo, int FrameIndex,
                                     uint64_t StackSize,
                                     int64_t SPOffset) const {
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  //const SodiumSubtarget &STI = MF.getSubtarget<SodiumSubtarget>();
  //const SodiumInstrInfo *TII = STI.getInstrInfo();

  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  int MinCSFI = 0;
  int MaxCSFI = -1;
  if (CSI.size()) {
    MinCSFI = CSI[0].getFrameIdx();
    MaxCSFI = CSI[CSI.size() - 1].getFrameIdx();
  }

  // The following stack frame objects are always referenced relative to $sp:
  // 1. Outgoing arguments.
  // 2. Pointer to dynamically allocated stack space.
  // 3. Locations for callee-saved registers.
  // Everything else is referenced relative to whatever register
  // getFrameRegister() returns.
  Register FrameReg;
  if ((FrameIndex >= MinCSFI && FrameIndex <= MaxCSFI))// ||
  //FI->isOutArgFI(FrameIndex) || FI->isDynAllocFI(FrameIndex))
    FrameReg = Sodium::X4;
  else
    FrameReg = getFrameRegister(MF);

  // Calculate final offset.
  // - There is no need to change the offset if the frame object is one of the
  //   following: an outgoing argument, pointer to a dynamically allocated
  //   stack space or a $gp restore location,
  // - If the frame object is any of the following, its offset must be adjusted
  //   by adding the size of the stack:
  //   incoming argument, callee-saved register location or local variable.
  int64_t Offset;

  Offset = SPOffset + (int64_t)StackSize;
  Offset += MI.getOperand(OpNo + 1).getImm();

  LLVM_DEBUG(errs() << "Offset     : " << Offset << "\n"
                    << "<--------->\n");

  bool IsKill = false;
  //unsigned MIOpc = MI.getOpcode();
  if (!MI.isDebugValue() && !isInt<12>(Offset)) {
    assert("Doesn't support right now!");
    /*
    // The offset won't fit in an immediate, so use a scratch register instead.
    // Modify Offset and FrameReg appropriately.
    Register ScratchReg = MRI.createVirtualRegister(&Sodium::IntRegsRegClass);
    TII->movImm(MBB, II, DL, ScratchReg, Offset);
    if (MIOpc == Sodium::ADDI) {
      BuildMI(MBB, II, DL, TII->get(Sodium::ADD), MI.getOperand(0).getReg())
        .addReg(FrameReg)
        .addReg(ScratchReg, RegState::Kill);
      MI.eraseFromParent();
      return true;
    }
    BuildMI(MBB, II, DL, TII->get(Sodium::ADD), ScratchReg)
    .addReg(FrameReg)
    .addReg(ScratchReg, RegState::Kill);
    Offset = 0;
    FrameReg = ScratchReg;
    IsKill = true;
    */
  }

  MI.getOperand(OpNo).ChangeToRegister(FrameReg, false, false, IsKill);
  MI.getOperand(OpNo + 1).ChangeToImmediate(Offset);
  return false;
}

bool SodiumRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                             int SPAdj, unsigned FIOperandNum,
                                             RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected non-zero SPAdj value");
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();

  LLVM_DEBUG(errs() << "\nFunction : " << MF.getName() << "\n";
             errs() << "<--------->\n"
                    << MI);

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  uint64_t stackSize = MF.getFrameInfo().getStackSize();
  int64_t spOffset = MF.getFrameInfo().getObjectOffset(FrameIndex);

  LLVM_DEBUG(errs() << "FrameIndex : " << FrameIndex << "\n"
                    << "spOffset   : " << spOffset << "\n"
                    << "stackSize  : " << stackSize << "\n"
                    << "alignment  : "
                    << DebugStr(MF.getFrameInfo().getObjectAlign(FrameIndex))
                    << "\n");

  return eliminateFI(MI, FIOperandNum, FrameIndex, stackSize, spOffset);
}

Register SodiumRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  const TargetFrameLowering *TFI = getFrameLowering(MF);
  return TFI->hasFP(MF) ? (Subtarget.is32Bit ? Sodium::D4 : Sodium::X8)   //$fp32; $fp
                        : (Subtarget.is32Bit ? Sodium::D2 : Sodium::X4);  //$sp32; $sp
}
