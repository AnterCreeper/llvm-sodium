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

SodiumMCAsmInfo::SodiumMCAsmInfo(const Triple &TT) {
  CodePointerSize = CalleeSaveStackSlotSize = 2; //FIXME
  CommentString = "#";
  AlignmentIsInBytes = false;
  SupportsDebugInformation = true;
  ExceptionsType = ExceptionHandling::DwarfCFI;
  Data16bitsDirective = "\t.half\t";
  Data32bitsDirective = "\t.word\t";
}
