//===-- SodiumTargetMachine.cpp - Define TargetMachine for Sodium ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the info about Sodium target spec.
//
//===----------------------------------------------------------------------===//

#include "Sodium.h"
#include "SodiumISelDAGToDAG.h"
#include "SodiumTargetMachine.h"
#include "SodiumMachineFunctionInfo.h"
#include "TargetInfo/SodiumTargetInfo.h"
#include "SodiumTargetObjectFile.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/CodeGen/MIRParser/MIParser.h"
#include "llvm/CodeGen/MIRYamlMapping.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "sodium"

//refer to clang/lib/Basic/Targets/Sodium.h
StringRef DataLayout16 = "e"
  // ELF name mangling
  "-m:e"
  // 16-bit pointers, 16-bit aligned
  "-p:16:16"
  // 32-bit integers, 16-bit aligned
  "-i32:16"
  // 16-bit and 32-bit native integer width
  "-n16:32"
  // 128-bit natural stack alignment, in 16 Bytes
  "-S128";
StringRef DataLayout32 = "e"
  // ELF name mangling
  "-m:e"
  // 32-bit pointers, 16-bit aligned
  "-p:32:16"
  // 32-bit integers, 16-bit aligned
  "-i32:16"
  // 16-bit and 32-bit native integer width
  "-n16:32"
  // 128-bit natural stack alignment, in 16 Bytes
  "-S128";
StringRef computeDataLayout(bool is32Bit){
  return is32Bit ? DataLayout32 : DataLayout16;
}

static Reloc::Model getEffectiveRelocModel(const Triple &TT,
                                           std::optional<Reloc::Model> RM) {
  // On ELF platforms the default static relocation model has a smart enough
  // linker to cope with referencing external symbols defined in a shared
  // library. Hence DynamicNoPIC doesn't need to be promoted to PIC.
  if (!RM || *RM == Reloc::DynamicNoPIC)
    return Reloc::Static;
  return *RM;
}

SodiumTargetMachine::SodiumTargetMachine(const Target &T, const Triple &TT,
                                         StringRef CPU, StringRef FS,
                                         const TargetOptions &Options,
                                         std::optional<Reloc::Model> RM,
                                         std::optional<CodeModel::Model> CM,
                                         CodeGenOpt::Level OL, bool JIT,
                                         bool is32Bit)
    : LLVMTargetMachine(T, computeDataLayout(is32Bit), TT,
                        CPU, FS, Options, getEffectiveRelocModel(TT, RM),
                        getEffectiveCodeModel(CM, CodeModel::Small), OL),
      is32Bit(is32Bit),
      TLOF(std::make_unique<SodiumELFTargetObjectFile>()),
      Subtarget(TT, std::string(CPU), std::string(CPU), std::string(FS), *this) {
  initAsmInfo();
}
SodiumTargetMachine::~SodiumTargetMachine() {}

class SodiumPassConfig : public TargetPassConfig {
public:
  SodiumPassConfig(SodiumTargetMachine &TM, PassManagerBase &PM)
    : TargetPassConfig(TM, PM) {}
  void addPreRegAlloc() override {
    addPass(createSodiumExpandPseudoPass());
  }
  bool addInstSelector() override {
    addPass(createSodiumISelDag(getTM<SodiumTargetMachine>(), getOptLevel()));
    return false;
  }
};

TargetPassConfig *SodiumTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new SodiumPassConfig(*this, PM);
}

yaml::MachineFunctionInfo *
SodiumTargetMachine::createDefaultFuncInfoYAML() const {
  return new yaml::SodiumMachineFunctionInfo();
}

yaml::MachineFunctionInfo *
SodiumTargetMachine::convertFuncInfoToYAML(const MachineFunction &MF) const {
  const auto *MFI = MF.getInfo<SodiumMachineFunctionInfo>();
  return new yaml::SodiumMachineFunctionInfo(*MFI);
}

MachineFunctionInfo *SodiumTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return SodiumMachineFunctionInfo::create<SodiumMachineFunctionInfo>(Allocator,
                                                                      F, STI);
}

bool SodiumTargetMachine::parseMachineFunctionInfo(
    const yaml::MachineFunctionInfo &MFI, PerFunctionMIParsingState &PFS,
    SMDiagnostic &Error, SMRange &SourceRange) const {
  const auto &YamlMFI =
      static_cast<const yaml::SodiumMachineFunctionInfo &>(MFI);
  PFS.MF.getInfo<SodiumMachineFunctionInfo>()->initializeBaseYamlFields(YamlMFI);
  return false;
}

//16bit and 32bit mode of Sodium
void Sodium16TargetMachine::anchor() {}
Sodium16TargetMachine::Sodium16TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                                             StringRef FS, const TargetOptions &Options,
                                             std::optional<Reloc::Model> RM, std::optional<CodeModel::Model> CM,
                                             CodeGenOpt::Level OL, bool JIT)
    : SodiumTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL, JIT, false) {
}
void Sodium32TargetMachine::anchor() {}
Sodium32TargetMachine::Sodium32TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                                             StringRef FS, const TargetOptions &Options,
                                             std::optional<Reloc::Model> RM, std::optional<CodeModel::Model> CM,
                                             CodeGenOpt::Level OL, bool JIT)
    : SodiumTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL, JIT, true) {
}

//Register the target.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSodiumTarget() {
    //- 16bit Target Machine
    RegisterTargetMachine<Sodium16TargetMachine> X(getTheSodium16Target());
    //- 32bit Target Machine
    RegisterTargetMachine<Sodium32TargetMachine> Y(getTheSodium32Target());
    auto *PR = PassRegistry::getPassRegistry();
    initializeSodiumExpandPseudoPass(*PR);
    initializeSodiumDAGToDAGISelPass(*PR);
}
