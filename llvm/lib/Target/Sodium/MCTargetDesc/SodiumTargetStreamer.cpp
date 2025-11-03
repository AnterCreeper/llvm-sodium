//===-- SodiumTargetStreamer.cpp - Sodium Target Streamer Methods ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides Sodium specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "SodiumTargetStreamer.h"
#include "SodiumInstPrinter.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

SodiumTargetStreamer::SodiumTargetStreamer(MCStreamer &S)
    : MCTargetStreamer(S) {}

void SodiumTargetStreamer::finish() {
    //MCELFStreamer &streamer = static_cast<MCELFStreamer&>(Streamer);
    //MCAssembler &MCA = streamer.getAssembler();
    //unsigned EFlags = MCA.getELFHeaderEFlags();
    //EFlags |= ELF::EF_SODIUM_ABI_32BIT;
    //MCA.setELFHeaderEFlags(EFlags);
}

SodiumTargetAsmStreamer::SodiumTargetAsmStreamer(MCStreamer &S,
                                                 formatted_raw_ostream &OS)
    : SodiumTargetStreamer(S), OS(OS) {}
