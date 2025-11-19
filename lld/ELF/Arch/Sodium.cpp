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
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
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

TargetInfo *elf::getSodiumTargetInfo() {
  static Sodium target;
  return &target;
}

RelExpr Sodium::getRelExpr(const RelType type, const Symbol &s,
                           const uint8_t *loc) const {
  switch (type) {
  case R_SODIUM_NONE:
    return R_NONE;
  case R_SODIUM_RELAX:
    return R_NONE; //return config->relax ? R_RELAX_HINT : R_NONE;
  case R_SODIUM_16:
  case R_SODIUM_32:
  case R_SODIUM_LO16:
  case R_SODIUM_HI16:
    return R_ABS;
  case R_SODIUM_BR20:
  case R_SODIUM_BR25:
  case R_SODIUM_CALL:
  case R_SODIUM_PCREL_ADD:
  case R_SODIUM_PCREL_ADD12:
  case R_SODIUM_PCREL_ADD20:
    return R_PC;
  // The Sodium relocs behave like the RISCV counterparts; reuse
  // the RelExpr to avoid code duplication.
  case R_SODIUM_PCREL_LO13I:
  case R_SODIUM_PCREL_LO13L:
    return R_RISCV_PC_INDIRECT;
  case R_SODIUM_ADD8:
  case R_SODIUM_ADD16:
  case R_SODIUM_ADD32:
  case R_SODIUM_SUB8:
  case R_SODIUM_SUB16:
  case R_SODIUM_SUB32:
    return R_RISCV_ADD;
  default:
    error(getErrorLocation(loc) + "unknown relocation (" + Twine(type) +
          ") against symbol " + toString(s));
    return R_NONE;
  }
}

#define BIT(n)           (1 << (n))
#define GENMASK(n, m)    ((BIT(m - n + 1) - 1) << n)
#define GETBITS(x, n, m) (((x) << (63 - m)) >> (63 - m + n))

#include "SodiumRelocate.h"

void Sodium::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  default:
    write32le(loc, getReloc(loc, rel, val));
    return;
  case R_SODIUM_RELAX:
    return; // Ignored (for now)
  case R_SODIUM_16:
    write16le(loc, val);
    return;
  case R_SODIUM_32:
    write32le(loc, val);
    return;
  case R_SODIUM_ADD8:
    *loc += val;
    return;
  case R_SODIUM_SUB8:
    *loc -= val;
    return;
  case R_SODIUM_ADD16:
    write16le(loc, read16le(loc) + val);
    return;
  case R_SODIUM_SUB16:
    write16le(loc, read16le(loc) - val);
    return;
  case R_SODIUM_ADD32:
    write32le(loc, read32le(loc) + val);
    return;
  case R_SODIUM_SUB32:
    write32le(loc, read32le(loc) - val);
    return;
  case R_SODIUM_CALL: {
    relocateNoSym(loc, R_SODIUM_PCREL_ADD20, val);
    relocateNoSym(loc + 4, R_SODIUM_BR20, GETBITS((int64_t)val, 0, 20));
    return;
  }
  }
}

/*
static void relaxCall(const InputSection &sec, size_t i, uint64_t loc,
                      Relocation &r, uint32_t &remove) {
  const Symbol &sym = *r.sym;
  const uint64_t dest = sym.getVA() + r.addend;
  if (isInt<26>(dest - loc)) {
    sec.relaxAux->relocTypes[i] = R_SODIUM_BR25;
    sec.relaxAux->writes.push_back(0x201); //BL
    remove = 4;
  }
}
*/
