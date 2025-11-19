//=---- SodiumMCCodeEmitter.cpp - Convert Sodium code to machine code -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the SodiumMCCodeEmitter class.
//
//===----------------------------------------------------------------------===//

#include "SodiumMCExpr.h"
#include "SodiumBaseInfo.h"
#include "SodiumFixupKinds.h"
#include "SodiumMCTargetDesc.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/EndianStream.h"

using namespace llvm;

#define DEBUG_TYPE "mccodeemitter"

namespace {
class SodiumMCCodeEmitter : public MCCodeEmitter {
  SodiumMCCodeEmitter(const SodiumMCCodeEmitter &) = delete;
  void operator=(const SodiumMCCodeEmitter &) = delete;
  MCContext &Ctx;
  MCInstrInfo const &MCII;

public:
  SodiumMCCodeEmitter(MCContext &Ctx, const MCInstrInfo &MCII)
      : Ctx(Ctx), MCII(MCII) {}
  ~SodiumMCCodeEmitter() override {}

  unsigned getExprOpValue(const MCInst &MI, const MCExpr *Expr,
                          SmallVectorImpl<MCFixup> &Fixups,
                          const MCSubtargetInfo &STI) const;

#include "SodiumEncoderMethod.h"

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

  // NOTE: TableGen'erated function in "SodiumGenMCCodeEmitter.inc"
  // for getting the binary encoding for an instruction.
  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

private:
  void expandFunctionCall(const MCInst &MI,
                          SmallVectorImpl<char> &CB,
                          SmallVectorImpl<MCFixup> &Fixups,
                          const MCSubtargetInfo &STI,
                          bool IsTailCall = false,
                          bool IsIndJmp = false) const;

};
} // namespace llvm

MCCodeEmitter *llvm::createSodiumMCCodeEmitter(const MCInstrInfo &MCII,
                                               MCContext &Ctx) {
  return new SodiumMCCodeEmitter(Ctx, MCII);
}

void SodiumMCCodeEmitter::expandFunctionCall(const MCInst &MI,
                                             SmallVectorImpl<char> &CB,
                                             SmallVectorImpl<MCFixup> &Fixups,
                                             const MCSubtargetInfo &STI,
                                             bool IsTailCall, bool IsIndJmp) const {
  MCInst TmpInst;
  uint32_t Binary;
  unsigned Opcode = IsIndJmp ? Sodium::BR : Sodium::BLR;

  MCOperand Func = MI.getOperand(0);
  assert(Func.isExpr() && "Expected expression");

  const MCExpr *CallExpr = Func.getExpr();

  // Emit pcadd20i $ra, $func with R_SODIUM_CALL relocation type.
  TmpInst = MCInstBuilder(Sodium::PCADD20I).addReg(Sodium::X2).addExpr(CallExpr);
  Binary = getBinaryCodeForInstr(TmpInst, Fixups, STI);
  support::endian::write(CB, Binary, support::little);

  TmpInst = MCInstBuilder(Opcode).addReg(Sodium::X2).addImm(0);
  Binary = getBinaryCodeForInstr(TmpInst, Fixups, STI);
  support::endian::write(CB, Binary, support::little);
}

void SodiumMCCodeEmitter::encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
  LLVM_DEBUG(errs() << MI);
  const MCInstrDesc &Desc = MCII.get(MI.getOpcode());
  switch (MI.getOpcode()) {
  default:
    break;
  case Sodium::PseudoCall:
    expandFunctionCall(MI, CB, Fixups, STI, false);
    return;
  case Sodium::PseudoTail:
    expandFunctionCall(MI, CB, Fixups, STI, true);
    return;
  case Sodium::PseudoJump:
    expandFunctionCall(MI, CB, Fixups, STI, false, true);
    return;
  }
  // Get byte count of instruction.
  unsigned Size = Desc.getSize();
  switch (Size) {
    default:
      llvm_unreachable("Unhandled encodeInstruction length!");
    case 4: {
      uint32_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
      support::endian::write(CB, Bits, support::little);
      break;
    }
  }
  return;
}

unsigned
SodiumMCCodeEmitter::getExprOpValue(const MCInst &MI, const MCExpr *Expr,
                                    SmallVectorImpl<MCFixup> &Fixups,
                                    const MCSubtargetInfo &STI) const {
  unsigned MIFrm = SODIUMII::getFormat(MCII.get(MI.getOpcode()).TSFlags);

  bool RelaxCandidate = false;
  MCExpr::ExprKind Kind = Expr->getKind();
  Sodium::Fixups FixupKind = Sodium::fixup_sodium_invalid;

  if (Kind == MCExpr::Target) {
    const SodiumMCExpr *SodiumExpr = cast<SodiumMCExpr>(Expr);
    switch (SodiumExpr->getKind()) {
    default:
      llvm_unreachable("Missing fixup kind!");
    case SodiumMCExpr::VK_SODIUM_None:
    case SodiumMCExpr::VK_SODIUM_Invalid:
      llvm_unreachable("Invalid fixup kind!");
    case SodiumMCExpr::VK_SODIUM_LO16:
      FixupKind = Sodium::Fixups::fixup_sodium_lo16;
      break;
    case SodiumMCExpr::VK_SODIUM_HI16:
      FixupKind = Sodium::Fixups::fixup_sodium_hi16;
      break;
    case SodiumMCExpr::VK_SODIUM_PCREL_ADD:
      FixupKind = Sodium::Fixups::fixup_sodium_pcrel_add;
      break;
    case SodiumMCExpr::VK_SODIUM_PCREL_ADD12:
      RelaxCandidate = true;
      FixupKind = Sodium::Fixups::fixup_sodium_pcrel_add12;
      break;
    case SodiumMCExpr::VK_SODIUM_PCREL_ADD20:
      RelaxCandidate = true;
      FixupKind = Sodium::Fixups::fixup_sodium_pcrel_add20;
      break;
    case SodiumMCExpr::VK_SODIUM_CALL:
      RelaxCandidate = true;
      FixupKind = Sodium::Fixups::fixup_sodium_call;
      break;
    case SodiumMCExpr::VK_SODIUM_PCREL_LO:
      RelaxCandidate = true;
      if      (MIFrm == SODIUMII::InstFMT_I)
        FixupKind = Sodium::Fixups::fixup_sodium_pcrel_lo13i;
      else if (MIFrm == SODIUMII::InstFMT_LS)
        FixupKind = Sodium::Fixups::fixup_sodium_pcrel_lo13l;
      else llvm_unreachable("unexpected MIFrm for VK_SODIUM_PCREL_LO");
      break;
    }
  } else
  if (Kind == MCExpr::SymbolRef &&
      cast<MCSymbolRefExpr>(Expr)->getKind() == MCSymbolRefExpr::VK_None) {
    switch (MIFrm) {
    case SODIUMII::InstFMT_J:
      if     (MI.getOpcode() == Sodium::B)
        FixupKind = Sodium::fixup_sodium_jump25;
      else if(MI.getOpcode() == Sodium::BR)
        FixupKind = Sodium::fixup_sodium_brind20;
      break;
    case SODIUMII::InstFMT_B:
        FixupKind = Sodium::fixup_sodium_brcc20;
      break;
    }
  }

  if (FixupKind == Sodium::fixup_sodium_invalid) {
    unsigned num;
    switch(Kind) {
    default:
      num = 0;
      break;
    case MCExpr::Binary:
      num = 1;
      break;
    case MCExpr::Constant:
      num = 2;
      break;
    case MCExpr::SymbolRef:
      num = 3;
      break;
    case MCExpr::Unary:
      num = 4;
      break;
    case MCExpr::Target:
      num = 5;
      break;
    }
    LLVM_DEBUG(errs() << "FIXME::Kind: " << num << "\n");
    LLVM_DEBUG(errs() << "FIXME::Format: " << MIFrm << "\n");
    llvm_unreachable("Unhandled expression!");
  }
  //assert(FixupKind != Sodium::fixup_sodium_invalid && "Unhandled expression!");

  Fixups.push_back(MCFixup::create(0, Expr, MCFixupKind(FixupKind), MI.getLoc()));

  if (!RelaxCandidate) return 0;
  const MCConstantExpr *Dummy = MCConstantExpr::create(0, Ctx);
  Fixups.push_back(MCFixup::create(0, Dummy, MCFixupKind(Sodium::fixup_sodium_relax),
                   MI.getLoc()));
  return 0;
}

#include "SodiumGenMCCodeEmitter.inc"
