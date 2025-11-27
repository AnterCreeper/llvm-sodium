//===-- SodiumMCAsmInfo.cpp - Sodium Asm properties -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the SodiumMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "SodiumMCAsmInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void SodiumMCAsmInfo::anchor() {}

// ref: SodiumMCTargetDesc.cpp
SodiumMCAsmInfo::SodiumMCAsmInfo(const Triple &TT, bool is32Bit) {
  CodePointerSize = CalleeSaveStackSlotSize = is32Bit ? 4 : 2;
  CommentString = "#";
  AlignmentIsInBytes = false;
  SupportsDebugInformation = true;
  ExceptionsType = ExceptionHandling::DwarfCFI;
  Data16bitsDirective = "\t.word\t";
  Data32bitsDirective = "\t.dword\t";
}
