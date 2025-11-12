//===-- SodiumMCExpr.h - Sodium specific MC expression classes---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes Sodium specific MCExprs, used for modifiers like
// "%hi" or "%lo" etc.,
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_SODIUMMCEXPR_H
#define LLVM_SODIUM_SODIUMMCEXPR_H

#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCStreamer.h"

namespace llvm {
class StringRef;

class SodiumMCExpr : public MCTargetExpr {
public:
  enum VariantKind {
    VK_SODIUM_None,
    VK_SODIUM_LO,
    VK_SODIUM_HI,
    VK_SODIUM_PCREL_LO,
    VK_SODIUM_PCREL_HI,
    VK_SODIUM_CALL,
    VK_SODIUM_Invalid // Must be the last item
  };

private:
  const MCExpr *Expr;
  const VariantKind Kind;

  explicit SodiumMCExpr(const MCExpr *Expr, VariantKind Kind)
      : Expr(Expr), Kind(Kind) {}

public:
  static const SodiumMCExpr *create(const MCExpr *Expr, VariantKind Kind,
                                    MCContext &Ctx);

  VariantKind getKind() const { return Kind; }
  const MCExpr *getSubExpr() const { return Expr; }

  void printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const override;

  void visitUsedExpr(MCStreamer &Streamer) const override {
    Streamer.visitUsedExpr(*getSubExpr());
  }
  MCFragment *findAssociatedFragment() const override {
    return getSubExpr()->findAssociatedFragment();
  }

  bool evaluateAsConstant(int64_t &Res) const;
  bool evaluateAsRelocatableImpl(MCValue &Res, const MCAsmLayout *Layout,
                                 const MCFixup *Fixup) const override;

  // There are no TLS SodiumMCExprs at the moment.
  void fixELFSymbolsInTLSFixups(MCAssembler &Asm) const override {};

  static StringRef getVariantKindName(VariantKind Kind);
  static VariantKind getVariantKindForName(StringRef name);

};

} // end namespace llvm.

#endif
