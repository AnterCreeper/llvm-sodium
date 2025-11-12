//===-- SodiumAsmPrinter.cpp - Sodium LLVM assembly writer ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to GAS-format Sodium assembly language.
//
//===----------------------------------------------------------------------===//

#include "Sodium.h"
#include "SodiumTargetMachine.h"
#include "TargetInfo/SodiumTargetInfo.h"
#include "MCTargetDesc/SodiumMCExpr.h"
#include "MCTargetDesc/SodiumBaseInfo.h"
#include "MCTargetDesc/SodiumInstPrinter.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
class SodiumAsmPrinter : public AsmPrinter {
  const SodiumSubtarget *Subtarget;

public:
  explicit SodiumAsmPrinter(TargetMachine &TM,
                            std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return "Sodium Assembly Printer"; }

  void emitInstruction(const MachineInstr *MI) override;
  bool emitPseudoExpansionLowering(MCStreamer &OutStreamer, const MachineInstr *MI);

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override;
  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &OS) override;

private:
  bool lowerOperand(const MachineOperand &MO, MCOperand &MCOp) const;
  bool LowerToMCInst(const MachineInstr *MI,
                                       MCInst &OutMI);

};
} // end of anonymous namespace

bool SodiumAsmPrinter::runOnMachineFunction(MachineFunction &MF) {
  Subtarget = &MF.getSubtarget<SodiumSubtarget>();
  SetupMachineFunction(MF);
  emitFunctionBody();
  return false;
}

static MCOperand LowerSymbolOperand(const MachineOperand &MO, MCSymbol *Sym,
                                    const AsmPrinter &AP) {
  MCContext &Ctx = AP.OutContext;
  SodiumMCExpr::VariantKind Kind;

  switch (MO.getTargetFlags()) {
  default:
    llvm_unreachable("Unknown target flag on GV operand");
  case SODIUMII::MO_None:
    Kind = SodiumMCExpr::VK_SODIUM_None;
    break;
  case SODIUMII::MO_CALL:
    Kind = SodiumMCExpr::VK_SODIUM_CALL;
    break;
  case SODIUMII::MO_LO:
    Kind = SodiumMCExpr::VK_SODIUM_LO;
    break;
  case SODIUMII::MO_HI:
    Kind = SodiumMCExpr::VK_SODIUM_HI;
    break;
  case SODIUMII::MO_PCREL_LO:
    Kind = SodiumMCExpr::VK_SODIUM_PCREL_LO;
    break;
  case SODIUMII::MO_PCREL_HI:
    Kind = SodiumMCExpr::VK_SODIUM_PCREL_HI;
    break;
  }

  const MCExpr *ME =
      MCSymbolRefExpr::create(Sym, MCSymbolRefExpr::VK_None, Ctx);

  if (!MO.isJTI() && !MO.isMBB() && MO.getOffset())
    ME = MCBinaryExpr::createAdd(
      ME, MCConstantExpr::create(MO.getOffset(), Ctx), Ctx);

  if (Kind != SodiumMCExpr::VK_SODIUM_None)
    ME = SodiumMCExpr::create(ME, Kind, Ctx);
  return MCOperand::createExpr(ME);
}

// Wrapper needed for tblgenned pseudo lowering.
bool SodiumAsmPrinter::lowerOperand(const MachineOperand &MO,
                                    MCOperand &MCOp) const {
  switch (MO.getType()) {
  default:
    report_fatal_error("lowerOperand: unknown operand type");
  case MachineOperand::MO_Register:
    // Ignore all implicit register operands.
    if (MO.isImplicit())
      return false;
    MCOp = MCOperand::createReg(MO.getReg());
    break;
  case MachineOperand::MO_RegisterMask:
    // Regmasks are like implicit defs.
    return false;
  case MachineOperand::MO_Immediate:
    MCOp = MCOperand::createImm(MO.getImm());
    break;
  case MachineOperand::MO_MachineBasicBlock:
    MCOp = LowerSymbolOperand(MO, MO.getMBB()->getSymbol(), *this);
    break;
  case MachineOperand::MO_GlobalAddress:
    MCOp = LowerSymbolOperand(MO, getSymbolPreferLocal(*MO.getGlobal()), *this);
    break;
  case MachineOperand::MO_BlockAddress:
    MCOp = LowerSymbolOperand(MO, GetBlockAddressSymbol(MO.getBlockAddress()),
                              *this);
    break;
  case MachineOperand::MO_ExternalSymbol:
    MCOp = LowerSymbolOperand(MO, GetExternalSymbolSymbol(MO.getSymbolName()),
                              *this);
    break;
  case MachineOperand::MO_ConstantPoolIndex:
    MCOp = LowerSymbolOperand(MO, GetCPISymbol(MO.getIndex()), *this);
    break;
  case MachineOperand::MO_JumpTableIndex:
    MCOp = LowerSymbolOperand(MO, GetJTISymbol(MO.getIndex()), *this);
    break;
  case MachineOperand::MO_MCSymbol:
    MCOp = LowerSymbolOperand(MO, MO.getMCSymbol(), *this);
    break;
  }
  return true;
}

bool SodiumAsmPrinter::LowerToMCInst(const MachineInstr *MI,
                                     MCInst &OutMI) {
  OutMI.setOpcode(MI->getOpcode());
  for (const MachineOperand &MO : MI->operands()) {
    MCOperand MCOp;
    if (lowerOperand(MO, MCOp))
      OutMI.addOperand(MCOp);
  }
  return false;
}

#include "SodiumGenMCPseudoLowering.inc"

void SodiumAsmPrinter::emitInstruction(const MachineInstr *MI) {
  // Do any auto-generated pseudo lowerings.
  if (emitPseudoExpansionLowering(*OutStreamer, MI))
    return;

  MachineBasicBlock::const_instr_iterator I = MI->getIterator();
  MachineBasicBlock::const_instr_iterator E = MI->getParent()->instr_end();
  do {
    MCInst TmpInst;
    LowerToMCInst(&*I, TmpInst);
    OutStreamer->emitInstruction(TmpInst, getSubtargetInfo());
  } while (++I != E && I->isInsideBundle());
}

bool SodiumAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                       const char *ExtraCode, raw_ostream &OS) {
  // First try the generic code, which knows about modifiers like 'c' and 'n'.
  if (!AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS))
    return false;

  const MachineOperand &MO = MI->getOperand(OpNo);
  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1] != 0)
      return true; // Unknown modifier.

    switch (ExtraCode[0]) {
    default:
      return true; // Unknown modifier.
    case 'z':      // Print zero register if zero, regular printing otherwise.
      if (MO.isImm() && MO.getImm() == 0) {
        OS << SodiumInstPrinter::getRegisterName(Sodium::X0);
        return false;
      }
      break;
    case 'i': // Literal 'i' if operand is not a register.
      if (!MO.isReg())
        OS << 'i';
      return false;
    }
  }

  switch (MO.getType()) {
  case MachineOperand::MO_Immediate:
    OS << MO.getImm();
    return false;
  case MachineOperand::MO_Register:
    OS << SodiumInstPrinter::getRegisterName(MO.getReg());
    return false;
  case MachineOperand::MO_GlobalAddress:
    PrintSymbolOperand(MO, OS);
    return false;
  case MachineOperand::MO_BlockAddress: {
    MCSymbol *Sym = GetBlockAddressSymbol(MO.getBlockAddress());
    Sym->print(OS, MAI);
    return false;
  }
  default:
    break;
  }

  return true;
}

bool SodiumAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                             unsigned OpNo,
                                             const char *ExtraCode,
                                             raw_ostream &OS) {
  if (ExtraCode)
    return AsmPrinter::PrintAsmMemoryOperand(MI, OpNo, ExtraCode, OS);

  const MachineOperand &AddrReg = MI->getOperand(OpNo);
  assert(MI->getNumOperands() > OpNo + 1 && "Expected additional operand");
  const MachineOperand &Offset = MI->getOperand(OpNo + 1);
  // All memory operands should have a register and an immediate operand (see
  // SodiumDAGToDAGISel::SelectInlineAsmMemoryOperand).
  if (!AddrReg.isReg())
    return true;
  if (!Offset.isImm() && !Offset.isGlobal())
    return true;

  MCOperand MCO;
  if (!lowerOperand(Offset, MCO))
    return true;

  if (Offset.isImm())
    OS << MCO.getImm();
  else if (Offset.isGlobal())
    OS << *MCO.getExpr();
  OS << "(" << SodiumInstPrinter::getRegisterName(AddrReg.getReg()) << ")";
  return false;
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSodiumAsmPrinter() {
  RegisterAsmPrinter<SodiumAsmPrinter> X(getTheSodium16Target());
  RegisterAsmPrinter<SodiumAsmPrinter> Y(getTheSodium32Target());
}
