//===-- SodiumMCAsmInfo.h - Sodium Asm Info --------------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the SodiumMCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_SODIUMMCASMINFO_H
#define LLVM_SODIUM_SODIUMMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class SodiumMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit SodiumMCAsmInfo(const Triple &TT, bool is32Bit);
};

} // namespace llvm

#endif
