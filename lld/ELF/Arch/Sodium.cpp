//===- Sodium.cpp ---------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Symbols.h"
#include "Target.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class Sodium final : public TargetInfo {
public:
  Sodium();
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};
} // namespace

Sodium::Sodium() {
  // ebreak
  trapInstr = {0x43, 0x43, 0x43, 0x43};
  pltEntrySize  = 16;
  pltHeaderSize = 32;
  gotPltHeaderEntriesNum = 2;
  defaultMaxPageSize = 65536;
  gotBaseSymInGotPlt = false;
}

RelExpr Sodium::getRelExpr(RelType type, const Symbol &s,
                           const uint8_t *loc) const {
  switch (type) {
  case R_MSP430_10_PCREL:
  case R_MSP430_16_PCREL:
  case R_MSP430_16_PCREL_BYTE:
  case R_MSP430_2X_PCREL:
  case R_MSP430_RL_PCREL:
  case R_MSP430_SYM_DIFF:
    return R_PC;
  default:
    return R_ABS;
  }
}

void Sodium::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_MSP430_8:
    checkIntUInt(loc, val, 8, rel);
    *loc = val;
    break;
  case R_MSP430_16:
  case R_MSP430_16_PCREL:
  case R_MSP430_16_BYTE:
  case R_MSP430_16_PCREL_BYTE:
    checkIntUInt(loc, val, 16, rel);
    write16le(loc, val);
    break;
  case R_MSP430_32:
    checkIntUInt(loc, val, 32, rel);
    write32le(loc, val);
    break;
  case R_MSP430_10_PCREL: {
    int16_t offset = ((int16_t)val >> 1) - 1;
    checkInt(loc, offset, 10, rel);
    write16le(loc, (read16le(loc) & 0xFC00) | (offset & 0x3FF));
    break;
  }
  default:
    error(getErrorLocation(loc) + "unrecognized relocation " +
          toString(rel.type));
  }
}

TargetInfo *elf::getSodiumTargetInfo() {
  static Sodium target;
  return &target;
}
