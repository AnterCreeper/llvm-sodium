//===-- SodiumExpandPseudoInsts.cpp - Expand pseudo instructions ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands pseudo instructions into target
// instructions. This pass should be run after register allocation but before
// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "Sodium.h"
#include "SodiumInstrInfo.h"
#include "SodiumTargetMachine.h"
#include "MCTargetDesc/SodiumBaseInfo.h"

#include "llvm/MC/MCContext.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define SODIUM_EXPAND_PSEUDO_NAME "Sodium pseudo instruction expansion pass"

namespace {

class SodiumExpandPseudo : public MachineFunctionPass {
public:
  static char ID;
  const SodiumSubtarget *STI;
  const SodiumInstrInfo *TII;

  SodiumExpandPseudo() : MachineFunctionPass(ID) {
    initializeSodiumExpandPseudoPass(*PassRegistry::getPassRegistry());
  }
  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return SODIUM_EXPAND_PSEUDO_NAME; }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadLocalAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadGlobalAddress(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator MBBI,
                               MachineBasicBlock::iterator &NextMBBI);
  bool expandAuipcInstPair(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI,
                           MachineBasicBlock::iterator &NextMBBI,
                           unsigned FlagsHi, unsigned SecondOpcode);

};

char SodiumExpandPseudo::ID = 0;

FunctionPass *createSodiumExpandPseudoPass() { return new SodiumExpandPseudo(); }

bool SodiumExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  STI = &MF.getSubtarget<SodiumSubtarget>();
  TII = STI->getInstrInfo();
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);
  return Modified;
}

bool SodiumExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;
  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }
  return Modified;
}

bool SodiumExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MBBI,
                                  MachineBasicBlock::iterator &NextMBBI) {
  // SodiumInstrInfo::getInstSizeInBytes expects that the total size of the
  // expanded instructions for each pseudo is correct in the Size field of the
  // tablegen definition for the pseudo.
  switch (MBBI->getOpcode()) {
  case Sodium::PseudoLLA:
    return expandLoadLocalAddress(MBB, MBBI, NextMBBI);
  case Sodium::PseudoLGA:
    return expandLoadGlobalAddress(MBB, MBBI, NextMBBI);
  }
  return false;
}

bool SodiumExpandPseudo::expandLoadLocalAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, SODIUMII::MO_PCREL_HI,
                             Sodium::ADDI);
}

bool SodiumExpandPseudo::expandLoadGlobalAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  unsigned SecondOpcode = STI->is32Bit ? Sodium::LD : Sodium::LW;
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, SODIUMII::MO_GOT_HI,
                             SecondOpcode);
}

bool SodiumExpandPseudo::expandAuipcInstPair(MachineBasicBlock &MBB,
                                             MachineBasicBlock::iterator MBBI,
                                             MachineBasicBlock::iterator &NextMBBI,
                                             unsigned FlagsHi, unsigned SecondOpcode) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();
  MachineFunction *MF = MBB.getParent();

  Register DestReg = MI.getOperand(0).getReg();
  MachineOperand &Symbol = MI.getOperand(1);

  Symbol.setTargetFlags(FlagsHi);
  MCSymbol *AUIPCSymbol = MF->getContext().createNamedTempSymbol("pcrel_hi");

  MachineInstr *MIAUIPC =
    BuildMI(MBB, MBBI, DL, TII->get(Sodium::AUIPC), DestReg).add(Symbol);
  MIAUIPC->setPreInstrSymbol(*MF, AUIPCSymbol);

  MachineInstr *SecondMI =
    BuildMI(MBB, MBBI, DL, TII->get(SecondOpcode), DestReg)
    .addReg(DestReg)
    .addSym(AUIPCSymbol, SODIUMII::MO_PCREL_LO);
  if (MI.hasOneMemOperand())
    SecondMI->addMemOperand(*MF, *MI.memoperands_begin());

  MI.eraseFromParent();
  return true;
}

} // end of anonymous namespace

INITIALIZE_PASS(SodiumExpandPseudo, "sodium-expand-pseudo",
                SODIUM_EXPAND_PSEUDO_NAME, false, false)

namespace llvm {
FunctionPass *createSodiumExpandPseudoPass() { return new SodiumExpandPseudo(); }
} // end of namespace llvm
