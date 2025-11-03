//===-- SodiumELFObjectWriter.cpp - Sodium ELF Writer ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/SodiumMCTargetDesc.h"
#include "SodiumFixupKinds.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

#define DEBUG_TYPE "sodium-elfobjwriter"

using namespace llvm;

namespace {
class SodiumELFObjectWriter : public MCELFObjectTargetWriter {
public:
  SodiumELFObjectWriter(uint8_t OSABI, bool Is64Bit);
  ~SodiumELFObjectWriter() override;

  // Return true if the given relocation must be with a symbol rather than
  // section plus offset.
  bool needsRelocateWithSymbol(const MCSymbol &Sym,
                               unsigned Type) const override {
    // TODO: This is extremely conservative. This really needs to use an
    // explicit list with a clear explanation for why each realocation needs to
    // point to the symbol, not to the section.
    return true;
  }

protected:
  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override;
};
}

SodiumELFObjectWriter::SodiumELFObjectWriter(uint8_t OSABI, bool Is64Bit)
    : MCELFObjectTargetWriter(Is64Bit, OSABI, ELF::EM_SODIUM,
                              /*HasRelocationAddend*/ true) {}

SodiumELFObjectWriter::~SodiumELFObjectWriter() = default;

std::unique_ptr<MCObjectTargetWriter>
llvm::createSodiumELFObjectWriter(uint8_t OSABI, bool Is64Bit) {
  return std::make_unique<SodiumELFObjectWriter>(OSABI, Is64Bit);
}

unsigned SodiumELFObjectWriter::getRelocType(MCContext &Ctx,
                                             const MCValue &Target,
                                             const MCFixup &Fixup,
                                             bool IsPCRel) const {
  // Determine the type of the relocation
  unsigned Kind = Fixup.getTargetKind();
  if (Kind >= FirstLiteralRelocationKind)
    return Kind - FirstLiteralRelocationKind;

  if (IsPCRel)
  switch (Kind) {
  // FIXME:
  //case FK_Data_4:
  //case FK_PCRel_4:
  }
  switch (Kind) {
  default:
    Ctx.reportError(Fixup.getLoc(), "unsupported relocation type");
    return ELF::R_SODIUM_NONE;
  case FK_Data_2:                     return ELF::R_SODIUM_16;
  case FK_Data_4:                     return ELF::R_SODIUM_32;
  case Sodium::fixup_sodium_add_8:    return ELF::R_SODIUM_ADD8;
  case Sodium::fixup_sodium_sub_8:    return ELF::R_SODIUM_SUB8;
  case Sodium::fixup_sodium_add_16:   return ELF::R_SODIUM_ADD16;
  case Sodium::fixup_sodium_sub_16:   return ELF::R_SODIUM_SUB16;
  case Sodium::fixup_sodium_add_32:   return ELF::R_SODIUM_ADD32;
  case Sodium::fixup_sodium_sub_32:   return ELF::R_SODIUM_SUB32;
  case Sodium::fixup_sodium_call:     return ELF::R_SODIUM_CALL;
  case Sodium::fixup_sodium_call_plt: return ELF::R_SODIUM_CALL_PLT;
  case Sodium::fixup_sodium_hi19:     return ELF::R_SODIUM_HI19;
  case Sodium::fixup_sodium_got_hi19: return ELF::R_SODIUM_GOT_HI19;
  case Sodium::fixup_sodium_pcrel_hi19:   return ELF::R_SODIUM_PCREL_HI19;
  case Sodium::fixup_sodium_lo13:     return ELF::R_SODIUM_LO13;
  case Sodium::fixup_sodium_lo13s:    return ELF::R_SODIUM_LO13S;
  case Sodium::fixup_sodium_pcrel_lo13:   return ELF::R_SODIUM_PCREL_LO13;
  case Sodium::fixup_sodium_pcrel_lo13s:  return ELF::R_SODIUM_PCREL_LO13S;
  case Sodium::fixup_sodium_jump25:   return ELF::R_SODIUM_BR25;
  case Sodium::fixup_sodium_brcc20:
  case Sodium::fixup_sodium_brind20:  return ELF::R_SODIUM_BR20;
  case Sodium::fixup_sodium_relax:    return ELF::R_SODIUM_RELAX;
  }
  return ELF::R_SODIUM_NONE;
}
