//===------ SodiumAsmBackend.h - Sodium Assembler Backend -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_SODIUMASMBACKEND_H
#define LLVM_SODIUM_SODIUMASMBACKEND_H

#include "MCTargetDesc/SodiumFixupKinds.h"
#include "MCTargetDesc/SodiumMCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/TargetParser/Triple.h"

namespace llvm {
class MCAssembler;
class MCObjectTargetWriter;

class SodiumAsmBackend : public MCAsmBackend {
  uint8_t OSABI;
  bool Is64Bit;
  const MCSubtargetInfo &STI;
  const MCTargetOptions &TargetOptions;

public:
  SodiumAsmBackend(const MCSubtargetInfo &STI, uint8_t OSABI, bool Is64Bit,
                   const MCTargetOptions &Options)
      : MCAsmBackend(support::little), OSABI(OSABI), Is64Bit(Is64Bit),
        STI(STI), TargetOptions(Options) {
  }
  ~SodiumAsmBackend() override = default;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createSodiumELFObjectWriter(OSABI, Is64Bit);
  }

  const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const override;
  unsigned getNumFixupKinds() const override {
    return Sodium::NumTargetFixupKinds;
  }

  void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                  const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsResolved,
                  const MCSubtargetInfo *STI) const override;
  bool evaluateTargetFixup(const MCAssembler &Asm, const MCAsmLayout &Layout,
                           const MCFixup &Fixup, const MCFragment *DF,
                           const MCValue &Target, uint64_t &Value,
                           bool &WasForced) override;
  bool handleAddSubRelocations(const MCAsmLayout &Layout, const MCFragment &F,
                               const MCFixup &Fixup, const MCValue &Target,
                               uint64_t &FixedValue) const override;

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    return true;
  }
  bool shouldForceRelocation(const MCAssembler &Asm, const MCFixup &Fixup,
                             const MCValue &Target) override {
    return true;
  }

  bool mayNeedRelaxation(const MCInst &Inst,
                         const MCSubtargetInfo &STI) const override {
    return false;
  }
  bool fixupNeedsRelaxation(const MCFixup &Fixup, uint64_t Value,
                            const MCRelaxableFragment *DF,
                            const MCAsmLayout &Layout) const override {
    llvm_unreachable("RelaxInstruction() unimplemented");
    return false;
  }
};
}

#endif
