//===-- SodiumAsmBackend.cpp - Sodium Assembler Backend -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SodiumMCExpr.h"
#include "SodiumAsmBackend.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCAsmLayout.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

#define DEBUG_TYPE "sodium-asmbackend"

using namespace llvm;

const MCFixupKindInfo &SodiumAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  const static MCFixupKindInfo Infos[Sodium::NumTargetFixupKinds] = {
      // This table *must* be in the order that the fixup_* kinds are defined in
      // SodiumFixupKinds.h.
      //
      // name offset bits flags
      {"fixup_sodium_hi19", 0, 32, 0},
      {"fixup_sodium_got_hi19", 0, 32, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_sodium_pcrel_hi19", 0, 32,
       MCFixupKindInfo::FKF_IsPCRel | MCFixupKindInfo::FKF_IsTarget},
      {"fixup_sodium_lo13", 0, 32, 0},
      {"fixup_sodium_lo13s", 0, 32, 0},
      {"fixup_sodium_pcrel_lo13", 0, 32,
       MCFixupKindInfo::FKF_IsPCRel | MCFixupKindInfo::FKF_IsTarget},
      {"fixup_sodium_pcrel_lo13s", 0, 32,
       MCFixupKindInfo::FKF_IsPCRel | MCFixupKindInfo::FKF_IsTarget},
      {"fixup_sodium_brcc20", 0, 32, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_sodium_brind20", 0, 32, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_sodium_jump25", 0, 32, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_sodium_relax", 0, 0, 0},
      {"fixup_sodium_call", 0, 64, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_sodium_call_plt", 0, 64, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_sodium_add_8", 0, 8, 0},
      {"fixup_sodium_sub_8", 0, 8, 0},
      {"fixup_sodium_add_16", 0, 16, 0},
      {"fixup_sodium_sub_16", 0, 16, 0},
      {"fixup_sodium_add_32", 0, 32, 0},
      {"fixup_sodium_sub_32", 0, 32, 0},
  };
  static_assert((std::size(Infos)) == Sodium::NumTargetFixupKinds,
                "Not all fixup kinds added to Infos array");
  // Fixup kinds from .reloc directive are like R_SODIUM_NONE. They
  // do not require any extra processing.
  if (Kind >= FirstLiteralRelocationKind)
    return MCAsmBackend::getFixupKindInfo(FK_NONE);
  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);
  assert(unsigned(Kind - FirstTargetFixupKind) < getNumFixupKinds() &&
    "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

bool SodiumAsmBackend::handleAddSubRelocations(const MCAsmLayout &Layout,
                                               const MCFragment &F,
                                               const MCFixup &Fixup,
                                               const MCValue &Target,
                                               uint64_t &FixedValue) const {
  uint64_t FixedValueA, FixedValueB;
  unsigned TA = 0, TB = 0;
  switch (Fixup.getKind()) {
  case llvm::FK_Data_1:
    TA = ELF::R_SODIUM_ADD8;
    TB = ELF::R_SODIUM_SUB8;
    break;
  case llvm::FK_Data_2:
    TA = ELF::R_SODIUM_ADD16;
    TB = ELF::R_SODIUM_SUB16;
    break;
  case llvm::FK_Data_4:
    TA = ELF::R_SODIUM_ADD32;
    TB = ELF::R_SODIUM_SUB32;
    break;
  default:
    llvm_unreachable("unsupported fixup size");
  }
  MCValue A = MCValue::get(Target.getSymA(), nullptr, Target.getConstant());
  MCValue B = MCValue::get(Target.getSymB());
  auto FA = MCFixup::create(
      Fixup.getOffset(), nullptr,
      static_cast<MCFixupKind>(FirstLiteralRelocationKind + TA));
  auto FB = MCFixup::create(
      Fixup.getOffset(), nullptr,
      static_cast<MCFixupKind>(FirstLiteralRelocationKind + TB));
  auto &Asm = Layout.getAssembler();
  Asm.getWriter().recordRelocation(Asm, Layout, &F, FA, A, FixedValueA);
  Asm.getWriter().recordRelocation(Asm, Layout, &F, FB, B, FixedValueB);
  FixedValue = FixedValueA - FixedValueB;
  return true;
}

#define ALIGN(x, y) if((x)&((1<<(y))-1)) Ctx.reportError(Fixup.getLoc(), "fixup value must be 2-byte aligned")

//bit field extract, return X[m:n]
#define GETBITS(x, n, m) ((x >> n) & ((1 << (m - n + 1)) - 1))

static uint64_t adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                 MCContext &Ctx) {
  switch (Fixup.getTargetKind()) {
  default:
    llvm_unreachable("Unknown fixup kind!");
  case Sodium::fixup_sodium_got_hi19:
    llvm_unreachable("Relocation should be unconditionally forced\n");
  case Sodium::fixup_sodium_add_8:
  case Sodium::fixup_sodium_sub_8:
  case Sodium::fixup_sodium_add_16:
  case Sodium::fixup_sodium_sub_16:
  case Sodium::fixup_sodium_add_32:
  case Sodium::fixup_sodium_sub_32:
  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
    return Value;
  case Sodium::fixup_sodium_lo13:
  case Sodium::fixup_sodium_pcrel_lo13:   //ADDI
    return (GETBITS(Value, 0, 0)   << 29) |
           (GETBITS(Value, 1, 12)  << 17);
  case Sodium::fixup_sodium_lo13s:
  case Sodium::fixup_sodium_pcrel_lo13s:  //LW
    return (GETBITS(Value, 0, 0)   << 29) |
           (GETBITS(Value, 1, 5)   << 4)  |
           (GETBITS(Value, 6, 12)  << 22);
  case Sodium::fixup_sodium_hi19:
  case Sodium::fixup_sodium_pcrel_hi19: { //AUIPC
    // Add 1 to test if bit 12 is 1, to compensate for low 13 bits being negative.
    Value = (Value + 0x1000) >> 13;
    return (GETBITS(Value, 0,  14) << 17) |
           (GETBITS(Value, 15, 19) << 12);
  }
  case Sodium::fixup_sodium_jump25: {     //B
    ALIGN(Value, 1);
    return (GETBITS(Value, 1,  5)  << 4)  |
           (GETBITS(Value, 6,  15) << 22) |
           (GETBITS(Value, 16, 25) << 12);
  }
  case Sodium::fixup_sodium_brcc20:
  case Sodium::fixup_sodium_brind20: {
    ALIGN(Value, 1);
    return (GETBITS(Value, 1,  5)  << 4)  |
           (GETBITS(Value, 6,  15) << 22) |
           (GETBITS(Value, 16, 20) << 12);
  }
  case Sodium::fixup_sodium_call:
  case Sodium::fixup_sodium_call_plt: {
    ALIGN(Value, 1);
    uint64_t LowerImm = (Value & 0x1fffULL);        //13 bits
    uint64_t UpperImm = (Value + 0x1000ULL) >> 13;  //19 bits
    //BLR
    LowerImm = (GETBITS(LowerImm, 1,  5)  << 4)  |
               (GETBITS(LowerImm, 6,  12) << 22);
    //AUIPC
    UpperImm = (GETBITS(UpperImm, 0,  14) << 17) |
               (GETBITS(UpperImm, 15, 19) << 12);
    return UpperImm | (LowerImm << 32);
  }
  }
}

void SodiumAsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                                  const MCValue &Target,
                                  MutableArrayRef<char> Data, uint64_t Value,
                                  bool IsResolved,
                                  const MCSubtargetInfo *STI) const {
  if (!Value) return; // Doesn't change encoding.
  MCFixupKind Kind = Fixup.getKind();
  if (Kind >= FirstLiteralRelocationKind) return;

  MCContext &Ctx = Asm.getContext();
  MCFixupKindInfo Info = getFixupKindInfo(Kind);
  Value = adjustFixupValue(Fixup, Value, Ctx);

  unsigned Offset = Fixup.getOffset();
  unsigned NumBytes = alignTo(Info.TargetSize + Info.TargetOffset, 8) / 8;
  assert(Offset + NumBytes <= Data.size() && "Invalid fixup offset!");

  // For each byte of the fragment that the fixup touches, mask in the
  // bits from the fixup value.
  for (unsigned i = 0; i != NumBytes; ++i) {
    Data[Offset + i] |= uint8_t((Value >> (i * 8)) & 0xff);
  }
}

bool SodiumAsmBackend::evaluateTargetFixup(
    const MCAssembler &Asm, const MCAsmLayout &Layout, const MCFixup &Fixup,
    const MCFragment *DF, const MCValue &Target, uint64_t &Value,
    bool &WasForced) {
  auto Kind = Fixup.getTargetKind();
  LLVM_DEBUG(errs() << "FIXME::evaluateTargetFixup: " << Kind << "\n");
  return false;
}

MCAsmBackend *llvm::createSodiumAsmBackend(const Target &T,
                                           const MCSubtargetInfo &STI,
                                           const MCRegisterInfo &MRI,
                                           const MCTargetOptions &Options) {
  const Triple &TT = STI.getTargetTriple();
  uint8_t OSABI = MCELFObjectTargetWriter::getOSABI(TT.getOS());
  return new SodiumAsmBackend(STI, OSABI, TT.isArch64Bit(), Options);
}
