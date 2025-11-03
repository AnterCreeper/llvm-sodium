//===-- Sodium.td - Describe the Sodium Target Machine -----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Sodium Subtarget features.
//===----------------------------------------------------------------------===//

def FeatureSoftMul
  : SubtargetFeature<"soft-mul", "UseSoftMul", "true",
                     "Use software emulation for integer multiply">;

def FeatureSoftDiv
  : SubtargetFeature<"soft-div", "UseSoftDiv", "true",
                     "Use software emulation for integer divide">;

def FeatureSoftFloat
  : SubtargetFeature<"soft-float", "UseSoftFloat", "true",
                     "Use software emulation for floating point">;

//===----------------------------------------------------------------------===//
// Register File, Calling Conv, Instruction Descriptions
//===----------------------------------------------------------------------===//

def Feature32Bit
  : SubtargetFeature<"32bit", "IsSodium32", "true", "Implements Sodium32">;
def IsSodium32 : Predicate<"Subtarget->is32Bit">,
                 AssemblerPredicate<(all_of Feature32Bit),
                 "Sodium32 Base Instruction Set">;

defvar Sodium16 = DefaultMode;
def Sodium32 : HwMode<"+32bit", [IsSodium32]>;

include "SodiumSchedule.td"
include "SodiumRegisterInfo.td"
include "SodiumInstrInfo.td"
include "SodiumCallingConv.td"

def SodiumInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Sodium processors supported.
//===----------------------------------------------------------------------===//

def : ProcessorModel<"generic", NoSchedModel, [FeatureSoftDiv,
                                               FeatureSoftFloat]>;
def : ProcessorModel<"nomul",   NoSchedModel, [FeatureSoftMul,
                                               FeatureSoftDiv,
                                               FeatureSoftFloat]>;

def SodiumAsmParser : AsmParser {
  bit ShouldEmitMatchRegisterName = 0;
}

def SodiumAsmParserVariant : AsmParserVariant {
  int Variant = 0;
  // Recognize hard coded registers.
  string RegisterPrefix = "$";
}

def SodiumAsmWriter : AsmWriter {
  int PassSubtarget = 1;
}

def Sodium : Target {
  // Pull in Instruction Info:
  let InstructionSet = SodiumInstrInfo;
  let AssemblyParsers = [SodiumAsmParser];
  let AssemblyWriters = [SodiumAsmWriter];
  let AssemblyParserVariants = [SodiumAsmParserVariant];
  let AllowRegisterRenaming = 1;
}
