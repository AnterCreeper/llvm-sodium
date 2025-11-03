//===-- SodiumMCExpr.cpp - Sodium specific MC expression classes ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the implementation of the assembly expression modifiers
// accepted by the Sodium architecture.
//
//===----------------------------------------------------------------------===//

#include "SodiumMCExpr.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmLayout.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "sodium-mcexpr"

const SodiumMCExpr *SodiumMCExpr::create(const MCExpr *Expr, VariantKind Kind,
                                         MCContext &Ctx) {
  return new (Ctx) SodiumMCExpr(Expr, Kind);
}

StringRef SodiumMCExpr::getVariantKindName(VariantKind Kind) {
  switch (Kind) {
  case VK_SODIUM_None:
  case VK_SODIUM_Invalid:
    llvm_unreachable("Invalid ELF symbol kind");
  case VK_SODIUM_CALL:
    return "call";
  case VK_SODIUM_CALL_PLT:
    return "call_plt";
  case VK_SODIUM_LO:
    return "lo";
  case VK_SODIUM_PCREL_LO:
    return "pcrel_lo";
  case VK_SODIUM_HI:
    return "hi";
  case VK_SODIUM_GOT_HI:
    return "got_pcrel_hi";
  case VK_SODIUM_PCREL_HI:
    return "pcrel_hi";
  }
  llvm_unreachable("Invalid ELF symbol kind");
}

void SodiumMCExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  VariantKind Kind = getKind();
  bool HasVariant = ((Kind != VK_SODIUM_None) && (Kind != VK_SODIUM_CALL) &&
                    (Kind != VK_SODIUM_CALL_PLT));
  if (HasVariant)
    OS << '%' << getVariantKindName(getKind()) << '(';
  Expr->print(OS, MAI);
  if (Kind == VK_SODIUM_CALL_PLT)
    OS << "@plt";
  if (HasVariant)
    OS << ')';
}

bool SodiumMCExpr::evaluateAsRelocatableImpl(MCValue &Res,
                                             const MCAsmLayout *Layout,
                                             const MCFixup *Fixup) const {
  // Explicitly drop the layout and assembler to prevent any symbolic folding in
  // the expression handling.  This is required to preserve symbolic difference
  // expressions to emit the paired relocations.
  if (!getSubExpr()->evaluateAsRelocatable(Res, nullptr, nullptr))
    return false;
  Res =
      MCValue::get(Res.getSymA(), Res.getSymB(), Res.getConstant(), getKind());
  // Custom fixup types are not valid with symbol difference expressions.
  return Res.getSymB() ? getKind() == VK_SODIUM_None : true;
}
