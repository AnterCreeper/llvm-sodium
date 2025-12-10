//===- SodiumLoadStoreOptimizer.cpp - Sodium load / store opt. pass -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that performs load / store pairs combine
// optimizations. This pass should be run before register allocation.
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

#define DEBUG_TYPE "sodium-ldst-opt"
#define PASS_NAME "Sodium load/store optimization pass"

/// This switch disables formation of pair load/store instructions that could
///  potentially lead to (new) misalignment.
/// This can be used to create libraries that are robust even when
///  users provoke undefined behaviour by supplying misaligned pointers.
static cl::opt<bool>
AssumeMisalignedLoadStore("sodium-misaligned-loadstore", cl::Hidden,
                          cl::init(false), cl::desc("Be more conservative in Sodium load/store opt"));

/// The LdStLimit limits how far we search for load/store pairs.
static cl::opt<unsigned>
LdStLimit("sodium-loadstore-scan-limit", cl::Hidden, cl::init(16));

namespace {

class SodiumLoadStoreOpt : public MachineFunctionPass {
public:
  static char ID;
  const SodiumSubtarget *STI;
  const SodiumInstrInfo *TII;

  SodiumLoadStoreOpt() : MachineFunctionPass(ID) {
    initializeSodiumLoadStoreOptPass(*PassRegistry::getPassRegistry());
  }
  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return PASS_NAME; }

private:
  bool optimizeBlock(MachineBasicBlock &MBB);
  bool tryToPairLdStInst(MachineBasicBlock::iterator &MBBI);
  MachineBasicBlock::iterator findMatchingInsn(MachineBasicBlock::iterator I);
  MachineBasicBlock::iterator mergePairedInsns(MachineBasicBlock::iterator I,
                                               MachineBasicBlock::iterator Paired);

};

char SodiumLoadStoreOpt::ID = 0;

bool SodiumLoadStoreOpt::runOnMachineFunction(MachineFunction &MF) {
  if (AssumeMisalignedLoadStore || skipFunction(MF.getFunction()))
    return false;

  STI = &MF.getSubtarget<SodiumSubtarget>();
  TII = STI->getInstrInfo();

  bool Modified = false;
  for (auto &MBB : MF) {
    Modified |= optimizeBlock(MBB);
  }
  return Modified;
}

/// Returns true if instruction is a memory operation that this pass is capable
/// of operating on.
static bool isCandidateToPair(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
    default:
      return false;
    case Sodium::LW:
    case Sodium::SW:
      break;
  }
  //TODO: can we combine together?
  //  LW %fixed-stack.0, 0 :: (load (s16) from %fixed-stack.0)
  //  LW %fixed-stack.1, 0 :: (load (s16) from %fixed-stack.1, align 16)
  //if (!MI.getOperand(1).isReg() &&
  //    !MI.getOperand(1).isFI())
  if (!MI.getOperand(1).isReg())
    return false;
  if (!MI.getOperand(2).isImm())
    return false;

  // When no memory operands are present, conservatively assume unaligned,
  // volatile, unfoldable.
  if (!MI.hasOneMemOperand())
    return false;

  const MachineMemOperand &MMO = **MI.memoperands_begin();

  // Don't touch volatile memory accesses - we may be changing their order.
  if (MMO.isVolatile() || MMO.isAtomic())
    return false;

  // str <undef> could probably be eliminated entirely, but for now we just want
  // to avoid making a mess of it.
  if (MI.getOperand(0).isReg() && MI.getOperand(0).isUndef())
    return false;

  // Likewise don't mess with references to undefined addresses.
  if (MI.getOperand(1).isUndef())
    return false;

  return true;
}

bool SodiumLoadStoreOpt::optimizeBlock(MachineBasicBlock &MBB) {
  bool Modified = false;
  for (MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
       MBBI != E;) {
    if (isCandidateToPair(*MBBI) && tryToPairLdStInst(MBBI))
      Modified = true;
    else
      ++MBBI;
  }
  return Modified;
}

bool SodiumLoadStoreOpt::tryToPairLdStInst(MachineBasicBlock::iterator &MBBI) {
  MachineInstr &MI = *MBBI;

  const MachineMemOperand &MMO = **MI.memoperands_begin();
  if (MMO.getAlign() < Align(4)) {
    LLVM_DEBUG(
      dbgs() << "Skip due to protential unaligned access with alignment "
             << MMO.getAlign().value() << " on ");
    LLVM_DEBUG(MI.print(dbgs()));
    return false;
  }

  // Look ahead instructions for a pairable instruction.
  MachineBasicBlock::iterator Paired = findMatchingInsn(MBBI);
  if (Paired != MI.getParent()->end()) {
    // Keeping the iterator straight is a pain, so we let the merge routine tell
    // us what the next instruction is after it's done mucking about.
    MBBI = mergePairedInsns(MBBI, Paired);
    return true;
  }

  return false;
}

/// Scan the instructions looking for a load/store that can be combined with the
/// current instruction into a wider equivalent or a load/store pair.
MachineBasicBlock::iterator
SodiumLoadStoreOpt::findMatchingInsn(MachineBasicBlock::iterator I) {
  MachineBasicBlock::iterator E = I->getParent()->end();
  MachineBasicBlock::iterator MBBI = next_nodbg(I, E);

  MachineInstr &FirstMI = *I;
  Register BaseReg = FirstMI.getOperand(1).getReg();
  int64_t  Offset  = FirstMI.getOperand(2).getImm();
  for (unsigned Count = 0; MBBI != E && Count < LdStLimit;
       MBBI = next_nodbg(MBBI, E)) {
    MachineInstr &MI = *MBBI;

    // Don't count transient instructions towards the search limit since there
    // may be different numbers of them if e.g. debug information is present.
    if (!MI.isTransient())
      ++Count;

    // Stop searching if we encounter a call instruction that might modify memory.
    if (MI.isCall())
      return E;

    // Test if match Pair Pattern.
    if (isCandidateToPair(MI) && FirstMI.getOpcode() == MI.getOpcode()) {
      // TODO: Make sure to check the new instruction offset is
      // actually an immediate and not a symbolic reference destined for
      // a relocation.
      Register MIBaseReg = MI.getOperand(1).getReg();
      int64_t  MIOffset  = MI.getOperand(2).getImm();
      if ((BaseReg == MIBaseReg) && (Offset + 2 == MIOffset))
        return MBBI;
    }
  }

  return E;
}

MachineBasicBlock::iterator
SodiumLoadStoreOpt::mergePairedInsns(MachineBasicBlock::iterator I,
                                     MachineBasicBlock::iterator Paired) {
  MachineBasicBlock::iterator E = I->getParent()->end();
  MachineBasicBlock::iterator NextI = next_nodbg(I, E);
  // If NextI is the second of the two instructions to be merged, we need
  // to skip one further. Either way we merge will invalidate the iterator,
  // and we don't need to scan the new instruction, as it's a pairwise
  // instruction, which we're not considering for further action anyway.
  if (NextI == Paired)
    NextI = next_nodbg(NextI, E);

  MachineBasicBlock &MBB = *I->getParent();
  MachineFunction *MF = MBB.getParent();

  // Construct the new instruction.
  DebugLoc DL  = I->getDebugLoc();
  DebugLoc DL2 = Paired->getDebugLoc();

  MachineInstr *Lo = &*I;
  MachineInstr *Hi = &*Paired;
  Register ScratchReg =
    MF->getRegInfo().createVirtualRegister(&Sodium::IntPairRegClass);

  MachineInstr *Res;
  if (Lo->getOpcode() == Sodium::LW) {
    MachineBasicBlock::iterator InsertionPoint = I;
    Res =
    BuildMI(MBB, InsertionPoint, DL,  TII->get(Sodium::LD), ScratchReg)
      .add(Lo->getOperand(1))
      .add(Lo->getOperand(2));
    BuildMI(MBB, InsertionPoint, DL,  TII->get(TargetOpcode::COPY), Lo->getOperand(0).getReg())
      .addReg(ScratchReg, 0, Sodium::sub_even);
    BuildMI(MBB, InsertionPoint, DL2, TII->get(TargetOpcode::COPY), Hi->getOperand(0).getReg())
      .addReg(ScratchReg, 0, Sodium::sub_odd);

  } else {
    MachineBasicBlock::iterator InsertionPoint = Paired;
    BuildMI(MBB, InsertionPoint, DL,  TII->get(TargetOpcode::REG_SEQUENCE), ScratchReg)
      .addReg(Lo->getOperand(0).getReg())
      .addImm(Sodium::sub_even)
      .addReg(Hi->getOperand(0).getReg())
      .addImm(Sodium::sub_odd);
    Res =
    BuildMI(MBB, InsertionPoint, DL,  TII->get(Sodium::SD))
      .addReg(ScratchReg)
      .add(Lo->getOperand(1))
      .add(Lo->getOperand(2));

  }

  LLVM_DEBUG(
      dbgs() << "Creating pair load/store. Replacing instructions:\n    ");
  LLVM_DEBUG(Lo->print(dbgs()));
  LLVM_DEBUG(dbgs() << "    ");
  LLVM_DEBUG(Hi->print(dbgs()));
  LLVM_DEBUG(dbgs() << "  with instruction:\n    ");
  LLVM_DEBUG(Res->print(dbgs()));
  LLVM_DEBUG(dbgs() << "\n");

  // Erase the old instructions.
  I->eraseFromParent();
  Paired->eraseFromParent();

  return NextI;
}

} // end of anonymous namespace

INITIALIZE_PASS(SodiumLoadStoreOpt, DEBUG_TYPE, PASS_NAME, false, false)

namespace llvm {
FunctionPass *createSodiumLoadStoreOptPass() { return new SodiumLoadStoreOpt(); }
} // end of namespace llvm
