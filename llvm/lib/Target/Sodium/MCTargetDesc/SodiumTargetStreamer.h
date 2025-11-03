//===-- SodiumTargetStreamer.h - Sodium Target Streamer --------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_SODIUMTARGETSTREAMER_H
#define LLVM_SODIUM_SODIUMTARGETSTREAMER_H

#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCStreamer.h"

namespace llvm {

class formatted_raw_ostream;

class SodiumTargetStreamer : public MCTargetStreamer {
public:
  SodiumTargetStreamer(MCStreamer &S);
  void finish() override;
};

// This part is for ascii assembly output
class SodiumTargetAsmStreamer : public SodiumTargetStreamer {
  formatted_raw_ostream &OS;

public:
  SodiumTargetAsmStreamer(MCStreamer &S, formatted_raw_ostream &OS);
};

} // end namespace llvm

#endif
