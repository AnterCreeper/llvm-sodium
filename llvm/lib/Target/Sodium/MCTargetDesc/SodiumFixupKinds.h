//===-- SodiumFixupKinds.h - Sodium Specific Fixup Entries ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_SODIUMFIXUPKINDS_H
#define LLVM_SODIUM_SODIUMFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"
#include <utility>

#undef Sodium

namespace llvm::Sodium {
enum Fixups {
  //begin from FirstTargetFixupKind = 128
  fixup_sodium_hi19 = FirstTargetFixupKind,
  fixup_sodium_pcrel_hi19,
  fixup_sodium_lo13,
  fixup_sodium_lo13s,
  fixup_sodium_pcrel_lo13,
  fixup_sodium_pcrel_lo13s,
  fixup_sodium_brcc20,
  fixup_sodium_brind20,
  fixup_sodium_jump25,
  fixup_sodium_relax,
  fixup_sodium_call,
  fixup_sodium_add_8,
  fixup_sodium_sub_8,
  fixup_sodium_add_16,
  fixup_sodium_sub_16,
  fixup_sodium_add_32,
  fixup_sodium_sub_32,
  // Used as a sentinel, must be the last
  fixup_sodium_invalid,
  NumTargetFixupKinds = fixup_sodium_invalid - FirstTargetFixupKind
};

static inline std::pair<MCFixupKind, MCFixupKind>
getRelocPairForSize(unsigned Size) {
  switch (Size) {
    default:
      llvm_unreachable("unsupported fixup size");
    case 1:
      return std::make_pair(MCFixupKind(Sodium::fixup_sodium_add_8),
                            MCFixupKind(Sodium::fixup_sodium_sub_8));
    case 2:
      return std::make_pair(MCFixupKind(Sodium::fixup_sodium_add_16),
                            MCFixupKind(Sodium::fixup_sodium_sub_16));
    case 4:
      return std::make_pair(MCFixupKind(Sodium::fixup_sodium_add_32),
                            MCFixupKind(Sodium::fixup_sodium_sub_32));
  }
}

} // end namespace llvm::Sodium

#endif
