//===-- SodiumFrameLowering.cpp - Sodium Frame Information ----------------===//
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

#include "SodiumFrameLowering.h"
#include "SodiumInstrInfo.h"
#include "SodiumSubtarget.h"
#include "SodiumMachineFunctionInfo.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"

using namespace llvm;

#define DEBUG_TYPE "frame-info"

/*
// hasFP - Return true if the specified function should have a dedicated frame
// pointer register.  This is true if the function has variable sized allocas,
// if it needs dynamic stack realignment, if frame pointer elimination is
// disabled, or if the frame address is taken.
bool SodiumFrameLowering::hasFP(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();

  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
    MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken() ||
    TRI->needsStackRealignment(MF);
}
*/

bool SodiumFrameLowering::hasFP(const MachineFunction &MF) const {
  return false;
}

void SodiumFrameLowering::adjustReg(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MBBI,
                                    const DebugLoc &DL, Register DestReg,
                                    Register SrcReg, int64_t Val,
                                    MachineInstr::MIFlag Flag, bool is32Bit) const {
  if (DestReg == SrcReg && Val == 0)
    return;

  const SodiumInstrInfo &TII =
    *static_cast<const SodiumInstrInfo *>(STI.getInstrInfo());
  if (isInt<12>(Val)) {
  // addi $DstReg, $SrcReg, Val
    BuildMI(MBB, MBBI, DL, TII.get(Sodium::ADDI), DestReg)
      .addReg(SrcReg)
      .addImm(Val)
      .setMIFlag(Flag);
    return;
  }
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  Register ScratchReg = MRI.createVirtualRegister(&Sodium::IntRegsRegClass);
  unsigned Opc = Sodium::ADD;
  if (Val < 0) {
    Val = -Val;
    Opc = Sodium::SUB;
  }
  //li $Scratch, Val
  //add/sub $DstReg, $SrcReg, $Scratch
  TII.movImm(MBB, MBBI, DL, ScratchReg, Val, Flag, is32Bit);
  BuildMI(MBB, MBBI, DL, TII.get(Opc), DestReg)
    .addReg(SrcReg)
    .addReg(ScratchReg, RegState::Kill)
    .setMIFlag(Flag);
}

void SodiumFrameLowering::emitPrologue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  const SodiumSubtarget &ST = MF.getSubtarget<SodiumSubtarget>();
  const SodiumInstrInfo &TII =
    *static_cast<const SodiumInstrInfo *>(STI.getInstrInfo());

  // All calls are tail calls in GHC calling conv, and functions have no
  // prologue/epilogue.
  if (MF.getFunction().getCallingConv() == CallingConv::GHC)
    return;

  // Debug location must be unknown since the first debug location is used
  // to determine the end of the prologue.
  DebugLoc DL;
  MachineBasicBlock::iterator MBBI = MBB.begin();

  MachineFrameInfo &MFI = MF.getFrameInfo();
  uint64_t StackSize = MFI.getStackSize() + MFI.getOffsetAdjustment();

  // Early exit if there is no need to allocate on the stack
  if (StackSize == 0 && !MFI.adjustsStack())
    return;

  adjustReg(MBB, MBBI, DL, Sodium::X4, Sodium::X4, -StackSize, MachineInstr::FrameSetup, ST.is32Bit);

  // emit ".cfi_def_cfa_offset StackSize"
  unsigned CFIIndex =
      MF.addFrameInst(
      MCCFIInstruction::cfiDefCfaOffset(nullptr, StackSize));
  BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
      .addCFIIndex(CFIIndex);

  MachineModuleInfo &MMI = MF.getMMI();
  const MCRegisterInfo *MRI = MMI.getContext().getRegisterInfo();

  const auto &CSI = MFI.getCalleeSavedInfo();
  if (!CSI.empty()) {
    // Find the instruction past the last instruction that saves a callee-saved
    // register to the stack.
    for (unsigned i = 0; i < CSI.size(); ++i) ++MBBI;

    // Iterate over list of callee-saved registers and emit .cfi_offset
    // directives.
    for (std::vector<CalleeSavedInfo>::const_iterator I = CSI.begin(),
           E = CSI.end(); I != E; ++I) {
      int64_t Offset = MFI.getObjectOffset(I->getFrameIdx());
      unsigned Reg = I->getReg();
      {
        // Reg is in CPURegs.
        unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::createOffset(
            nullptr, MRI->getDwarfRegNum(Reg, true), Offset));
        BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
            .addCFIIndex(CFIIndex);
      }
    }
  }

  if (hasFP(MF)) {
    /*
    // emit instruction "mv $fp, $sp" at this location.
    BuildMI(MBB, MBBI, DL, TII.get(Sodium::ADDI), FP)
        .addReg(Sodium::X4)
        .addImm(0)
        .setMIFlag(MachineInstr::FrameSetup);

    // emit ".cfi_def_cfa_register $fp"
    unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::createDefCfaRegister(
        nullptr, MRI->getDwarfRegNum(FP, true)));
    BuildMI(MBB, MBBI, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);
    */
  }
}

void SodiumFrameLowering::emitEpilogue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  const SodiumSubtarget &ST = MF.getSubtarget<SodiumSubtarget>();
  //const SodiumInstrInfo &TII =
  //  *static_cast<const SodiumInstrInfo *>(STI.getInstrInfo());

  // All calls are tail calls in GHC calling conv, and functions have no
  // prologue/epilogue.
  if (MF.getFunction().getCallingConv() == CallingConv::GHC)
    return;

  // Get the insert location for the epilogue. If there were no terminators in
  // the block, get the last instruction.
  DebugLoc DL;
  MachineBasicBlock::iterator MBBI = MBB.end();
  if (!MBB.empty()) {
    MBBI = MBB.getLastNonDebugInstr();
    if (MBBI != MBB.end()) DL = MBBI->getDebugLoc();

    MBBI = MBB.getFirstTerminator();
    // If callee-saved registers are saved via libcall, place stack adjustment
    // before this call.
    while (MBBI != MBB.begin() &&
           std::prev(MBBI)->getFlag(MachineInstr::FrameDestroy))
      --MBBI;
  }

  MachineFrameInfo &MFI = MF.getFrameInfo();
  // Get the number of bytes from FrameInfo.
  uint64_t StackSize = MFI.getStackSize() + MFI.getOffsetAdjustment();

  if (hasFP(MF)) {
    /*
    // Find the first instruction that restores a callee-saved register.
    MachineBasicBlock::iterator I = MBBI;
    for (unsigned i = 0; i < MFI.getCalleeSavedInfo().size(); ++i)
      --I;
    // Insert instruction "mv $sp, $fp" at this location.
    BuildMI(MBB, I, DL, TII.get(Sodium::ADDI), SP).addReg(FP).addImm(0);
    */
  }

  adjustReg(MBB, MBBI, DL, Sodium::X4, Sodium::X4, StackSize, MachineInstr::FrameDestroy, ST.is32Bit);
}

// Not preserve stack space within prologue for outgoing variables when the
// function contains variable size objects.
// Let eliminateCallFramePseudoInstr preserve stack space for it.
bool SodiumFrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  return !MF.getFrameInfo().hasVarSizedObjects();
}

void SodiumFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                               BitVector &SavedRegs,
                                               RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  auto &Subtarget = MF.getSubtarget<SodiumSubtarget>();

  if (MF.getFrameInfo().hasCalls()) {
    SavedRegs.set(Sodium::X2);
    if(Subtarget.is32Bit) SavedRegs.set(Sodium::X3);
    if (hasFP(MF)) {
      SavedRegs.set(Sodium::X8);
      if(Subtarget.is32Bit) SavedRegs.set(Sodium::X9);
    }
  }

  // If interrupt is enabled and there are calls in the handler,
  // unconditionally save all Caller-saved registers, regardless whether they are used.
  MachineFrameInfo &MFI = MF.getFrameInfo();
  if (MF.getFunction().hasFnAttribute("interrupt") && MFI.hasCalls()) {
    static const MCPhysReg CSRegs[] = {
      Sodium::X2, Sodium::X3,                               //$ra32
      Sodium::X10, Sodium::X11,                             //$a0-$a7
      Sodium::X12, Sodium::X13, Sodium::X14, Sodium::X15, Sodium::X16, Sodium::X17,
      Sodium::X28, Sodium::X29, Sodium::X30, Sodium::X31, 0 //$t0-$t3
    };
    for (unsigned i = 0; CSRegs[i]; ++i) SavedRegs.set(CSRegs[i]);
  }
}

// Eliminate ADJCALLSTACKDOWN, ADJCALLSTACKUP pseudo instructions.
MachineBasicBlock::iterator
SodiumFrameLowering::eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                                   MachineBasicBlock::iterator I) const {
  Register SPReg = Sodium::X4;
  DebugLoc DL = I->getDebugLoc();
  const auto &ST = MF.getSubtarget<SodiumSubtarget>();
  if (!hasReservedCallFrame(MF)) {
    // If space has not been reserved for a call frame, ADJCALLSTACKDOWN and
    // ADJCALLSTACKUP must be converted to instructions manipulating the stack
    // pointer. This is necessary when there is a variable length stack
    // allocation (e.g. alloca), which means it's not possible to allocate
    // space for outgoing arguments from within the function prologue.
    int64_t Amount = I->getOperand(0).getImm();
    if (Amount != 0) {
      // Ensure the stack remains aligned after adjustment.
      Amount = alignSPAdjust(Amount);
      if (I->getOpcode() == Sodium::ADJCALLSTACKDOWN)
        Amount = -Amount;
      //const SodiumRegisterInfo &RI = *STI.getRegisterInfo();
      adjustReg(MBB, I, DL, SPReg, SPReg, Amount, MachineInstr::NoFlags, ST.is32Bit);
    }
  }
  return MBB.erase(I);
}

static Register convertIntRegsToIntPair(Register Reg) {
  assert(Reg >= Sodium::X0 && Reg <= Sodium::X31 && "Invalid register");
  return Sodium::D0 + ((Reg - Sodium::X0) >> 1);
}

// Test if valid adjacent register pairs with [odd, even]
static bool invalidRegisterPairing(unsigned Reg1, unsigned Reg2) {
  assert(Sodium::IntRegsRegClass.contains(Reg1) && Sodium::IntRegsRegClass.contains(Reg2) &&
         "IntPair callee-saved regs to spill!");
  if (Reg1 < Reg2)
    return ((Reg1 - Sodium::X0) & 0x1) || (Reg2 != Reg1 + 1);
  else
    return ((Reg2 - Sodium::X0) & 0x1) || (Reg1 != Reg2 + 1);
}

bool
SodiumFrameLowering::assignCalleeSavedSpillSlots(MachineFunction &MF,
                                                 const TargetRegisterInfo *TRI,
                                                 std::vector<CalleeSavedInfo> &CSI) const {
  if (CSI.empty())
    return true; // Early exit if no callee saved registers are modified!

  MachineFrameInfo &MFI = MF.getFrameInfo();

  // Now that we know which registers need to be saved and restored, allocate
  // stack slots for them.
  for (std::vector<CalleeSavedInfo>::iterator I = CSI.begin(); I != CSI.end(); ++I) {
    MCRegister Reg = I->getReg();

    auto Next = std::next(I);
    if (Next != CSI.end() && !invalidRegisterPairing(Reg, Next->getReg())) {
      Reg = convertIntRegsToIntPair(Reg);
      I->setReg(Reg);
      CSI.erase(Next);
    }

    const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(Reg);
    unsigned Size = TRI->getSpillSize(*RC);
    Align Alignment(TRI->getSpillAlign(*RC));

    int FrameIdx = MFI.CreateStackObject(Size, Alignment, true);
    I->setFrameIdx(FrameIdx);
  }

  return true;
}
